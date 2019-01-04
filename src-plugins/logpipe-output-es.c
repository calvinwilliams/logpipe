#include "logpipe_api.h"

#include "fasterhttp.h"

/* command for compile && install
make logpipe-output-es.so && cp -f logpipe-output-es.so ~/so/
*/

int	__LOGPIPE_OUTPUT_ES_VERSION_0_1_0 = 1 ;

/* 分列信息结构 */
struct FieldSeparatorInfo
{
	char			*begin ; /* 开始偏移量 */
	char			*end ; /* 结束偏移量 */
	int			len ; /* 列长度 */
} ;

/* 插件环境结构 */
#define PARSE_BUFFER_SIZE		LOGPIPE_OUTPUT_BUFSIZE*2
#define FORMAT_BUFFER_SIZE		LOGPIPE_OUTPUT_BUFSIZE*2
#define POST_BUFFER_SIZE_DEFAULT	4*1024*1024
#define POST_BUFFER_SIZE_INCREASE	4*1024*1024

struct OutputPluginContext
{
	char				*uncompress_algorithm ;
	char				*translate_charset ;
	char				*separator_charset ;
	char				*grep ;
	char				*fields_strictly ;
	char				*iconv_from ;
	char				*iconv_to ;
	char				output_template_buffer[ FORMAT_BUFFER_SIZE + 1 ] ;
	char				*output_template ;
	int				output_template_len ;
	char				*ip ;
	int				port ;
	char				*index ;
	char				*type ;
	char				*bulk ;
	char				*bulk_uri ;
	char				*bulk_head ;
	int				bulk_head_len ;
	
	struct HttpEnv			*http_env ;
	
	int				field_separator_array_size ;
	struct FieldSeparatorInfo	*field_separator_array ; /* 分列信息结构 结构数组 */
	
	char				*filename ;
	int				filename_len ;
	
	char				parse_buffer[ PARSE_BUFFER_SIZE + 1 ] ; /* 解析缓冲区 */
	int				parse_buflen ;
	char				format_buffer[ FORMAT_BUFFER_SIZE + 1 ] ; /* 模板实例化缓冲区 */
	int				format_buflen ;
	char				format_buffer_iconv[ FORMAT_BUFFER_SIZE + 1 ] ;
	int				format_buflen_iconv ;
	char				*p_format_buffer ;
	char				*post_buffer ; /* HTTP提交缓冲区 */
	int				post_bufsize ;
	int				post_buflen ;
	
	int				sock ;
	struct sockaddr_in		addr ;
} ;

funcLoadOutputPluginConfig LoadOutputPluginConfig ;
int LoadOutputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , struct LogpipePluginConfigItem *p_plugin_config_items , void **pp_context )
{
	struct OutputPluginContext	*p_plugin_ctx = NULL ;
	char				*p = NULL ;
	
	/* 申请内存以存放插件上下文 */
	p_plugin_ctx = (struct OutputPluginContext *)malloc( sizeof(struct OutputPluginContext) ) ;
	if( p_plugin_ctx == NULL )
	{
		ERRORLOGC( "malloc failed , errno[%d]" , errno )
		return -1;
	}
	memset( p_plugin_ctx , 0x00 , sizeof(struct OutputPluginContext) );
	
	p_plugin_ctx->uncompress_algorithm = QueryPluginConfigItem( p_plugin_config_items , "uncompress_algorithm" ) ;
	if( p_plugin_ctx->uncompress_algorithm )
	{
		if( STRCMP( p_plugin_ctx->uncompress_algorithm , == , "deflate" ) )
		{
			;
		}
		else
		{
			ERRORLOGC( "uncompress_algorithm[%s] invalid" , p_plugin_ctx->uncompress_algorithm )
			return -1;
		}
	}
	INFOLOGC( "uncompress_algorithm[%s]" , p_plugin_ctx->uncompress_algorithm )
	
	p_plugin_ctx->translate_charset = QueryPluginConfigItem( p_plugin_config_items , "translate_charset" ) ;
	INFOLOGC( "translate_charset[%s]" , p_plugin_ctx->translate_charset )
	
	p_plugin_ctx->separator_charset = QueryPluginConfigItem( p_plugin_config_items , "separator_charset" ) ;
	INFOLOGC( "separator_charset[%s]" , p_plugin_ctx->separator_charset )
	if( p_plugin_ctx->separator_charset == NULL )
		p_plugin_ctx->separator_charset = " " ;
	
	p_plugin_ctx->grep = QueryPluginConfigItem( p_plugin_config_items , "grep" ) ;
	INFOLOGC( "grep[%s]" , p_plugin_ctx->grep )
	
	p_plugin_ctx->fields_strictly = QueryPluginConfigItem( p_plugin_config_items , "fields_strictly" ) ;
	if( p_plugin_ctx->fields_strictly && ( STRICMP( p_plugin_ctx->fields_strictly , == , "false" ) || STRICMP( p_plugin_ctx->fields_strictly , == , "no" ) ) )
		p_plugin_ctx->fields_strictly = NULL ;
	INFOLOGC( "fields_strictly[%s]" , p_plugin_ctx->fields_strictly )
	
	p_plugin_ctx->iconv_from = QueryPluginConfigItem( p_plugin_config_items , "iconv_from" ) ;
	INFOLOGC( "iconv_from[%s]" , p_plugin_ctx->iconv_from )
	
	p_plugin_ctx->iconv_to = QueryPluginConfigItem( p_plugin_config_items , "iconv_to" ) ;
	INFOLOGC( "iconv_to[%s]" , p_plugin_ctx->iconv_to )
	
	if( p_plugin_ctx->iconv_from && p_plugin_ctx->iconv_to == NULL )
	{
		ERRORLOGC( "expect config for 'iconv_to'" )
		return -1;
	}
	
	if( p_plugin_ctx->iconv_from == NULL && p_plugin_ctx->iconv_to )
	{
		ERRORLOGC( "expect config for 'iconv_from'" )
		return -1;
	}
	
	p = QueryPluginConfigItem( p_plugin_config_items , "output_template" ) ;
	if( p == NULL || p[0] == '\0' )
	{
		ERRORLOGC( "expect config for 'output_template'" )
		return -1;
	}
	
	p_plugin_ctx->output_template_len = strlen(p) ;
	if( p_plugin_ctx->output_template_len > FORMAT_BUFFER_SIZE )
	{
		ERRORLOGC( "'output_template' too long" )
		return -1;
	}
	
	{
		int		buffer_len = 0 ;
		int		remain_len = sizeof(p_plugin_ctx->output_template_buffer)-1 ;
		
		memset( p_plugin_ctx->output_template_buffer , 0x00 , sizeof(p_plugin_ctx->output_template_buffer) );
		JSONUNESCAPE_FOLD( p , p_plugin_ctx->output_template_len , p_plugin_ctx->output_template_buffer , buffer_len , remain_len )
		if( buffer_len == -1 )
		{
			ERRORLOGC( "output_tempalte[%s] invalid" , p )
			return -1;
		}
		
		p_plugin_ctx->output_template = p_plugin_ctx->output_template_buffer ;
		p_plugin_ctx->output_template_len = buffer_len ;
	}
	INFOLOGC( "output_template[%s]" , p_plugin_ctx->output_template )
	
	p_plugin_ctx->ip = QueryPluginConfigItem( p_plugin_config_items , "ip" ) ;
	INFOLOGC( "ip[%s]" , p_plugin_ctx->ip )
	if( p_plugin_ctx->ip == NULL || p_plugin_ctx->ip[0] == '\0' )
	{
		ERRORLOGC( "expect config for 'ip'" )
		return -1;
	}
	
	p = QueryPluginConfigItem( p_plugin_config_items , "port" ) ;
	if( p == NULL || p[0] == '\0' )
	{
		ERRORLOGC( "expect config for 'port'" )
		return -1;
	}
	p_plugin_ctx->port = atoi(p) ;
	INFOLOGC( "port[%d]" , p_plugin_ctx->port )
	if( p_plugin_ctx->port <= 0 )
	{
		ERRORLOGC( "port[%s] invalid" , p )
		return -1;
	}
	
	p_plugin_ctx->index = QueryPluginConfigItem( p_plugin_config_items , "index" ) ;
	INFOLOGC( "index[%s]" , p_plugin_ctx->index )
	if( p_plugin_ctx->index == NULL || p_plugin_ctx->index[0] == '\0' )
	{
		ERRORLOGC( "expect config for 'index'" )
		return -1;
	}
	
	p_plugin_ctx->type = QueryPluginConfigItem( p_plugin_config_items , "type" ) ;
	INFOLOGC( "type[%s]" , p_plugin_ctx->type )
	if( p_plugin_ctx->type == NULL || p_plugin_ctx->type[0] == '\0' )
	{
		ERRORLOGC( "expect config for 'type'" )
		return -1;
	}
	
	p_plugin_ctx->bulk = QueryPluginConfigItem( p_plugin_config_items , "bulk" ) ;
	if( p_plugin_ctx->bulk && ( STRICMP( p_plugin_ctx->bulk , == , "false" ) || STRICMP( p_plugin_ctx->bulk , == , "no" ) ) )
		p_plugin_ctx->bulk = NULL ;
	INFOLOGC( "bulk[%s]" , p_plugin_ctx->bulk )
	if( p_plugin_ctx->bulk == NULL )
	{
		p_plugin_ctx->bulk_uri = "" ;
		p_plugin_ctx->bulk_head = "" ;
	}
	else
	{
		p_plugin_ctx->bulk_uri = "/_bulk" ;
		p_plugin_ctx->bulk_head = "{ \"index\":{} }\r\n" ;
	}
	p_plugin_ctx->bulk_head_len = strlen(p_plugin_ctx->bulk_head) ;
	
	/* 设置插件环境上下文 */
	(*pp_context) = p_plugin_ctx ;
	
	return 0;
}

static int ConnectElasticSearchServer( struct OutputPluginContext *p_plugin_ctx )
{
	int		nret = 0 ;
	
	/* 创建套接字 */
	p_plugin_ctx->sock = socket( AF_INET , SOCK_STREAM , IPPROTO_TCP ) ;
	if( p_plugin_ctx->sock == -1 )
	{
		ERRORLOGC( "socket failed , errno[%d]" , errno )
		return -1;
	}
	
	/* 设置套接字选项 */
	{
		int	onoff = 1 ;
		setsockopt( p_plugin_ctx->sock , SOL_SOCKET , SO_REUSEADDR , (void *) & onoff , sizeof(int) );
	}
	
	{
		int	onoff = 1 ;
		setsockopt( p_plugin_ctx->sock , IPPROTO_TCP , TCP_NODELAY , (void*) & onoff , sizeof(int) );
	}
	
	/* 连接到服务端侦听端口 */
	nret = connect( p_plugin_ctx->sock , (struct sockaddr *) & (p_plugin_ctx->addr) , sizeof(struct sockaddr) ) ;
	if( nret == -1 )
	{
		ERRORLOGC( "connect[%s:%d][%d] failed , errno[%d]" , p_plugin_ctx->ip , p_plugin_ctx->port , p_plugin_ctx->sock , errno )
		close( p_plugin_ctx->sock ); p_plugin_ctx->sock = -1 ;
		sleep(1);
		return 1;
	}
	else
	{
		INFOLOGC( "connect[%s:%d][%d] ok" , p_plugin_ctx->ip , p_plugin_ctx->port , p_plugin_ctx->sock )
	}
	
	return 0;
}
	
funcInitOutputPluginContext InitOutputPluginContext ;
int InitOutputPluginContext( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	int				max_field_index ;
	int				field_index ;
	char				*p = NULL ;
	char				*p2 = NULL ;
	
	int				nret = 0 ;
	
	/* 初始化插件环境内部数据 */
	max_field_index = 0 ;
	p = p_plugin_ctx->output_template ;
	while(1)
	{
		p = strchr( p , '$' ) ;
		if( p == NULL )
			break;
		p++;
		field_index = strtol( p , & p2 , 10 ) ;
		if( p2 == p )
		{
			ERRORLOGC( "output_template[%s] invalid" , p_plugin_ctx->output_template )
			return -1;
		}
		if( field_index > max_field_index )
			max_field_index = field_index ;
		
		p = p2 ;
	}
	
	p_plugin_ctx->field_separator_array_size = max_field_index + 1 ; /* 第一个单元放filename */
	p_plugin_ctx->field_separator_array = (struct FieldSeparatorInfo *)malloc( sizeof(struct FieldSeparatorInfo) * p_plugin_ctx->field_separator_array_size ) ;
	if( p_plugin_ctx->field_separator_array == NULL )
	{
		ERRORLOGC( "malloc faild , size[%d]" , sizeof(struct FieldSeparatorInfo) * p_plugin_ctx->field_separator_array_size )
		return -1;
	}
	memset( p_plugin_ctx->field_separator_array , 0x00 , sizeof(struct FieldSeparatorInfo) * p_plugin_ctx->field_separator_array_size );
	INFOLOGC( "field_separator_array_size[%d]" , p_plugin_ctx->field_separator_array_size )
	
	p_plugin_ctx->http_env = CreateHttpEnv() ;
	if( p_plugin_ctx->http_env == NULL )
	{
		ERRORLOGC( "CreateHttpEnv faild" )
		return -1;
	}
	
	p_plugin_ctx->sock = -1 ;
	
	memset( & (p_plugin_ctx->addr) , 0x00 , sizeof(struct sockaddr_in) );
	p_plugin_ctx->addr.sin_family = AF_INET ;
	if( p_plugin_ctx->ip[0] == '\0' )
		p_plugin_ctx->addr.sin_addr.s_addr = INADDR_ANY ;
	else
		p_plugin_ctx->addr.sin_addr.s_addr = inet_addr(p_plugin_ctx->ip) ;
	p_plugin_ctx->addr.sin_port = htons( (unsigned short)(p_plugin_ctx->port) );
	
	while(1)
	{
		nret = ConnectElasticSearchServer( p_plugin_ctx ) ;
		if( nret < 0 )
			return nret;
		else if( nret == 0 )
			break;
	}
	
	/* 设置输入描述字 */
	AddOutputPluginEvent( p_env , p_logpipe_output_plugin , p_plugin_ctx->sock );
	
	return 0;
}

funcOnOutputPluginEvent OnOutputPluginEvent;
int OnOutputPluginEvent( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	int				nret = 0 ;
	
	DEBUGLOGC( "close sock" )
	close( p_plugin_ctx->sock ); p_plugin_ctx->sock = -1 ;
	sleep(1);
	
	while(1)
	{
		nret = ConnectElasticSearchServer( p_plugin_ctx ) ;
		if( nret < 0 )
			return nret;
		else if( nret == 0 )
			break;
	}
	
	/* 设置输入描述字 */
	AddOutputPluginEvent( p_env , p_logpipe_output_plugin , p_plugin_ctx->sock );
	
	return 0;
}

funcBeforeWriteOutputPlugin BeforeWriteOutputPlugin ;
int BeforeWriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint16_t filename_len , char *filename )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	p_plugin_ctx->filename = filename ;
	p_plugin_ctx->filename_len = filename_len ;
	
	return 0;
}

/* 发送HTTP请求到ES */
static int PostToEs( struct OutputPluginContext *p_plugin_ctx )
{
	struct HttpBuffer	*http_req = NULL ;
	struct HttpBuffer	*http_rsp = NULL ;
	struct timeval		tv_begin_send_http ;
	struct timeval		tv_end_send_http ;
	struct timeval		tv_diff_send_http ;
	int			status_code ;
	
	int			nret = 0 ;
	
_GOTO_RESEND :
	
	/* 重置HTTP环境 */
	ResetHttpEnv( p_plugin_ctx->http_env );
	
	/* 组织HTTP请求 */
	DEBUGLOGC( "StrcpyfHttpBuffer http body [%d][%.*s]" , p_plugin_ctx->post_buflen , p_plugin_ctx->post_buflen , p_plugin_ctx->post_buffer )
	http_req = GetHttpRequestBuffer( p_plugin_ctx->http_env ) ;
	nret = StrcpyfHttpBuffer( http_req , "POST /%s/%s%s HTTP/1.0" HTTP_RETURN_NEWLINE
						"Content-Type: application/json%s%s" HTTP_RETURN_NEWLINE
						"Connection: Keep-alive" HTTP_RETURN_NEWLINE
						"Content-length: %d" HTTP_RETURN_NEWLINE
						HTTP_RETURN_NEWLINE
						"%.*s"
						, p_plugin_ctx->index , p_plugin_ctx->type , p_plugin_ctx->bulk_uri
						, p_plugin_ctx->iconv_to?"; charset=":"" , p_plugin_ctx->iconv_to?p_plugin_ctx->iconv_to:""
						, p_plugin_ctx->post_buflen
						, p_plugin_ctx->post_buflen , p_plugin_ctx->post_buffer ) ;
	if( nret )
	{
		ERRORLOGC( "StrcpyfHttpBuffer failed[%d]" , nret )
		return 1;
	}
	else
	{
		INFOLOGC( "StrcpyfHttpBuffer ok" )
	}
	
	INFOLOGC( "RequestHttp ..." )
	INFOLOGC( "HTTP REQ[%.*s]" , GetHttpBufferLength(http_req) , GetHttpBufferBase(http_req,NULL) )
	/* 发送HTTP请求 */
	gettimeofday( & tv_begin_send_http , NULL );
	nret = RequestHttp( p_plugin_ctx->sock , NULL , p_plugin_ctx->http_env ) ;
	gettimeofday( & tv_end_send_http , NULL );
	http_rsp = GetHttpResponseBuffer( p_plugin_ctx->http_env ) ;
	if( nret )
	{
		ERRORLOGC( "RequestHttp failed[%d]" , nret )
		ERRORLOGC( "HTTP RSP[%.*s]" , GetHttpBufferLength(http_rsp) , GetHttpBufferBase(http_rsp,NULL) )
		close( p_plugin_ctx->sock ); p_plugin_ctx->sock = -1 ;
		while(1)
		{
			nret = ConnectElasticSearchServer( p_plugin_ctx ) ;
			if( nret < 0 )
				return nret;
			else if( nret == 0 )
				break;
		}
		goto _GOTO_RESEND;
	}
	else
	{
		INFOLOGC( "RequestHttp ok" )
		INFOLOGC( "HTTP RSP[%.*s]" , GetHttpBufferLength(http_rsp) , GetHttpBufferBase(http_rsp,NULL) )
	}
	
	DiffTimeval( & tv_begin_send_http , & tv_end_send_http , & tv_diff_send_http );
	INFOLOGC( "SEND-HTTP[%ld.%06ld]" , tv_diff_send_http.tv_sec , tv_diff_send_http.tv_usec )
	
	/* 检查HTTP响应头 */
	status_code = GetHttpStatusCode( p_plugin_ctx->http_env ) ;
	if( status_code/100 != HTTP_OK/100 )
	{
		ERRORLOGC( "status code[%d]" , status_code )
	}
	else
	{
		INFOLOGC( "status code[%d]" , status_code )
	}
	
	p_plugin_ctx->post_buflen = 0 ;
	
	return 0;
}

/* 输出模板实例化处理 */
static int FormatOutputTemplate( struct OutputPluginContext *p_plugin_ctx )
{
	char				*p = NULL ;
	char				*p_endptr = NULL ;
	int				field_index ;
	struct FieldSeparatorInfo	*p_field_separator = NULL ;
	int				d_len ;
	
	int				nret = 0 ;
	
	/* 把模板配置复制到模板实例化缓冲区 */
	strcpy( p_plugin_ctx->format_buffer , p_plugin_ctx->output_template );
	p_plugin_ctx->format_buflen = p_plugin_ctx->output_template_len ;
	INFOLOGC( "before format [%d][%.*s]" , p_plugin_ctx->format_buflen , p_plugin_ctx->format_buflen , p_plugin_ctx->format_buffer )
	
	/* 模板实例化 */
	p = p_plugin_ctx->format_buffer ;
	while(1)
	{
		p = strchr( p , '$' ) ;
		if( p == NULL )
			break;
		
		/*
			$1 => "123"
			$2 => "4"
			$3 => "56789"
			
			abc$1def$2ghi
                           ^ ^
                           | |
                           | p_endptr
                           p
		*/
		p_endptr = NULL ;
		field_index = strtol( p+1 , & p_endptr , 10 ) ;
		p_field_separator = p_plugin_ctx->field_separator_array + field_index ;
		if( p_field_separator->end )
		{
			d_len = p_field_separator->len - ( p_endptr-p ) ;
			if( p_plugin_ctx->format_buflen + d_len > sizeof(p_plugin_ctx->format_buffer)-1 )
			{
				ERRORLOGC( "format output template overflow" )
				return 1;
			}
			memmove( p_endptr + d_len , p_endptr , strlen(p_endptr)+1 );
			memcpy( p , p_field_separator->begin , p_field_separator->len );
			p_plugin_ctx->format_buflen += d_len ;
		}
		
		DEBUGLOGC( "       format [%d][%.*s]" , p_plugin_ctx->format_buflen , p_plugin_ctx->format_buflen , p_plugin_ctx->format_buffer )
		
		p = p_endptr ;
	}
	
	/* 如果需要字符编码转换 */
	if( p_plugin_ctx->iconv_from && p_plugin_ctx->iconv_to )
	{
		memset( p_plugin_ctx->format_buffer_iconv , 0x00 , sizeof(p_plugin_ctx->format_buffer_iconv) );
		p_plugin_ctx->format_buflen_iconv = sizeof(p_plugin_ctx->format_buffer_iconv) - 1 ;
		p = ConvertContentEncodingEx( p_plugin_ctx->iconv_from , p_plugin_ctx->iconv_to , p_plugin_ctx->format_buffer , & (p_plugin_ctx->format_buflen) , p_plugin_ctx->format_buffer_iconv , & (p_plugin_ctx->format_buflen_iconv) ) ;
		if( p == NULL )
		{
			ERRORLOGC( "convert content encoding failed" )
			return 1;
		}
		else
		{
			INFOLOGC( "convert content encoding ok" )
		}
		
		p_plugin_ctx->p_format_buffer = p_plugin_ctx->format_buffer_iconv ;
		p_plugin_ctx->format_buflen = p_plugin_ctx->format_buflen_iconv ;
	}
	else
	{
		p_plugin_ctx->p_format_buffer = p_plugin_ctx->format_buffer ;
	}
	
	/* 从模板实例化缓冲区复制合并到HTTP提交缓冲区 */
	if( p_plugin_ctx->post_buffer == NULL || p_plugin_ctx->post_buflen + p_plugin_ctx->bulk_head_len+p_plugin_ctx->format_buflen > p_plugin_ctx->post_bufsize-1 )
	{
		char	*tmp = NULL ;
		int	new_post_bufsize ;
		
		if( p_plugin_ctx->post_bufsize == 0 )
		{
			new_post_bufsize = POST_BUFFER_SIZE_DEFAULT ;
		}
		else
		{
			new_post_bufsize = p_plugin_ctx->post_buflen + p_plugin_ctx->bulk_head_len+p_plugin_ctx->format_buflen + 1 ;
		}
		tmp = (char*)realloc( p_plugin_ctx->post_buffer , new_post_bufsize ) ;
		if( tmp == NULL )
		{
			FATALLOGC( "realloc failed , errno[%d]" , errno )
			return -1;
		}
		else
		{
			DEBUGLOGC( "realloc post buffer [%d]bytes to [%d]bytes" , p_plugin_ctx->post_bufsize , new_post_bufsize )
		}
		memset( tmp+p_plugin_ctx->post_buflen , 0x00 , new_post_bufsize-1-p_plugin_ctx->post_buflen );
		p_plugin_ctx->post_buffer = tmp ;
		p_plugin_ctx->post_bufsize = new_post_bufsize ;
	}
	
	if( p_plugin_ctx->bulk )
	{
		memcpy( p_plugin_ctx->post_buffer+p_plugin_ctx->post_buflen , p_plugin_ctx->bulk_head , p_plugin_ctx->bulk_head_len ); p_plugin_ctx->post_buflen += p_plugin_ctx->bulk_head_len ;
	}
	
	memcpy( p_plugin_ctx->post_buffer+p_plugin_ctx->post_buflen , p_plugin_ctx->p_format_buffer , p_plugin_ctx->format_buflen ); p_plugin_ctx->post_buflen += p_plugin_ctx->format_buflen ;
	DEBUGLOGC( "post_buffer[%.*s]" , p_plugin_ctx->post_buflen , p_plugin_ctx->post_buffer )
	
	/* 如果单条提交（非批量），发送HTTP请求到ES */
	if( p_plugin_ctx->bulk == NULL )
	{
		nret = PostToEs( p_plugin_ctx ) ;
		if( nret )
		{
			ERRORLOGC( "PostToEs failed[%d]" , nret )
			return nret;
		}
		else
		{
			INFOLOGC( "PostToEs ok" )
		}
	}
	
	return 0;
}

/* 分列解析缓冲区 */
static int ParseCombineBuffer( struct OutputPluginContext *p_plugin_ctx , int line_len )
{
	char				*p = NULL ;
	int				field_index ;
	struct FieldSeparatorInfo	*p_field_separator = NULL ;
	
	int				nret = 0 ;
	
	INFOLOGC( "parse [%d][%.*s]" , line_len , line_len , p_plugin_ctx->parse_buffer )
	
	/* 丢弃不符合过滤条件的行 */
	if( p_plugin_ctx->grep )
	{
		if( strstr( p_plugin_ctx->parse_buffer , p_plugin_ctx->grep ) == NULL )
		{
			INFOLOGC( "grep return false" )
			return 0;
		}
	}
	
	/* 替换字符，参照linux命令tr，如tr '[]' ' ' */
	if( p_plugin_ctx->translate_charset && p_plugin_ctx->translate_charset[0] )
	{
		for( p = p_plugin_ctx->parse_buffer ; (*p) ; p++ )
			if( strchr( p_plugin_ctx->translate_charset , (*p) ) )
				(*p) = p_plugin_ctx->separator_charset[0] ;
		INFOLOGC( "translated [%d][%.*s]" , p_plugin_ctx->parse_buflen , p_plugin_ctx->parse_buflen , p_plugin_ctx->parse_buffer )
	}
	
	if( p_plugin_ctx->field_separator_array_size > 0 )
	{
		memset( p_plugin_ctx->field_separator_array , 0x00 , sizeof(struct FieldSeparatorInfo) * p_plugin_ctx->field_separator_array_size );
	}
	
	/* 当前有效行分列，分列信息写入分列信息结构数组 */
	/* 第0列为 文件名 */
	field_index = 0 ;
	p_field_separator = p_plugin_ctx->field_separator_array ;
	p_field_separator->begin = p_plugin_ctx->filename ;
	p_field_separator->end = p_plugin_ctx->filename + p_plugin_ctx->filename_len ;
	p_field_separator->len = p_plugin_ctx->filename_len ;
	DEBUGLOGC( "parse field [%d]-[%d][%.*s]" , field_index , p_field_separator->len , p_field_separator->len , p_field_separator->begin )
	
	p = p_plugin_ctx->parse_buffer ;
	for( field_index++ , p_field_separator++ ; field_index < p_plugin_ctx->field_separator_array_size ; field_index++ , p_field_separator++ )
	{
		/* 寻找第一个非分隔字符 */
		for( p_field_separator->begin = p ; *(p_field_separator->begin) ; p_field_separator->begin++ )
			if( ! strchr( p_plugin_ctx->separator_charset , p_field_separator->begin[0] ) )
				break;
		if( *(p_field_separator->begin) == '\0' )
			break;
		
		/* 寻找第一个分隔字符 */
		for( p_field_separator->end = p_field_separator->begin ; *(p_field_separator->end) ; p_field_separator->end++ )
			if( strchr( p_plugin_ctx->separator_charset , p_field_separator->end[0] ) )
				break;
		
		p_field_separator->len = p_field_separator->end - p_field_separator->begin ;
		DEBUGLOGC( "parse field [%d]-[%d][%.*s]" , field_index , p_field_separator->len , p_field_separator->len , p_field_separator->begin )
		
		p = p_field_separator->end + 1 ;
	}
	
	/* 如果启用了列数严谨开关，列数不符合要求，丢弃本行处理 */
	if( p_plugin_ctx->fields_strictly )
	{
		if( p_plugin_ctx->field_separator_array[p_plugin_ctx->field_separator_array_size-1].end == NULL )
			return 0;
	}
	
	/* 输出模板实例化处理 */
	nret = FormatOutputTemplate( p_plugin_ctx ) ;
	if( nret )
	{
		ERRORLOGC( "FormatOutputTemplate failed[%d]" , nret )
		return nret;
	}
	else
	{
		INFOLOGC( "FormatOutputTemplate ok" )
	}
	
	return 0;
}

/* 数据块合并到解析缓冲区 */
static int CombineToParseBuffer( struct OutputPluginContext *p_plugin_ctx , char *block_buf , uint64_t block_len )
{
	char		*p_newline = NULL ;
	int		line_len ;
	int		remain_len ;
	
	int		nret = 0 ;
	
	INFOLOGC( "before combine , [%d][%.*s]" , p_plugin_ctx->parse_buflen , p_plugin_ctx->parse_buflen , p_plugin_ctx->parse_buffer )
	INFOLOGC( "block_buf [%d][%.*s]" , block_len , block_len , block_buf )
	
	/* 如果遗留数据+当前数据块放的下解析缓冲区 */
	if( p_plugin_ctx->parse_buflen + block_len <= sizeof(p_plugin_ctx->parse_buffer)-1 )
	{
		strncpy( p_plugin_ctx->parse_buffer+p_plugin_ctx->parse_buflen , block_buf , block_len );
		p_plugin_ctx->parse_buflen += block_len ;
	}
	else
	{
		/* 先强制把遗留数据都处理掉 */
		nret = ParseCombineBuffer( p_plugin_ctx , p_plugin_ctx->parse_buflen ) ;
		if( nret < 0 )
		{
			ERRORLOGC( "ParseCombineBuffer failed[%d]" , nret )
			return nret;
		}
		else if( nret > 0 )
		{
			WARNLOGC( "ParseCombineBuffer return[%d]" , nret )
		}
		else
		{
			DEBUGLOGC( "ParseCombineBuffer ok" )
		}
		
		strncpy( p_plugin_ctx->parse_buffer , block_buf , block_len );
		p_plugin_ctx->parse_buflen = block_len ;
	}
	
	/* 把解析缓冲区中有效行都处理掉 */
	while(1)
	{
		p_newline = memchr( p_plugin_ctx->parse_buffer , '\n' , p_plugin_ctx->parse_buflen ) ;
		if( p_newline == NULL )
			break;
		
		line_len = p_newline - p_plugin_ctx->parse_buffer ;
		remain_len = p_plugin_ctx->parse_buflen - line_len - 1 ;
		(*p_newline) = '\0' ;
		nret = ParseCombineBuffer( p_plugin_ctx , line_len ) ;
		if( nret < 0 )
		{
			ERRORLOGC( "ParseCombineBuffer failed[%d]" , nret )
			return nret;
		}
		else if( nret > 0 )
		{
			WARNLOGC( "ParseCombineBuffer return[%d]" , nret )
		}
		else
		{
			INFOLOGC( "ParseCombineBuffer ok" )
		}
		
		memmove( p_plugin_ctx->parse_buffer , p_newline+1 , remain_len );
		p_plugin_ctx->parse_buflen = remain_len ;
	}
	
	/* 如果启用批量HTTP提交 */
	if( p_plugin_ctx->bulk )
	{
		nret = PostToEs( p_plugin_ctx ) ;
		if( nret )
		{
			ERRORLOGC( "PostToEs failed[%d]" , nret )
			return nret;
		}
		else
		{
			INFOLOGC( "PostToEs ok" )
		}
	}
	
	INFOLOGC( "after combine , [%d][%.*s]" , p_plugin_ctx->parse_buflen , p_plugin_ctx->parse_buflen , p_plugin_ctx->parse_buffer )
	
	return 0;
}

funcWriteOutputPlugin WriteOutputPlugin ;
int WriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint64_t file_offset , uint64_t file_line , uint64_t block_len , char *block_buf , uint64_t block_buf_size )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	int				nret = 0 ;
	
	/* 如果未启用解压 */
	if( p_plugin_ctx->uncompress_algorithm == NULL )
	{
		/* 数据块合并到解析缓冲区 */
		nret = CombineToParseBuffer( p_plugin_ctx , block_buf , block_len ) ;
		if( nret < 0 )
		{
			ERRORLOGC( "CombineToParseBuffer failed[%d]" , nret )
			return nret;
		}
		else
		{
			INFOLOGC( "CombineToParseBuffer ok" )
		}
	}
	/* 如果启用了解压 */
	else
	{
		if( STRCMP( p_plugin_ctx->uncompress_algorithm , == , "deflate" ) )
		{
			char			block_out_buf[ LOGPIPE_OUTPUT_BUFSIZE + 1 ] ;
			uint64_t		block_out_len ;
			
			memset( block_out_buf , 0x00 , sizeof(block_out_buf) );
			nret = UncompressInputPluginData( p_plugin_ctx->uncompress_algorithm , block_buf , block_len , block_out_buf , & block_out_len , LOGPIPE_OUTPUT_BUFSIZE ) ;
			if( nret )
			{
				ERRORLOGC( "UncompressInputPluginData failed[%d]" , nret )
				return -1;
			}
			else
			{
				DEBUGLOGC( "UncompressInputPluginData ok" )
			}
			
			/* 数据块合并到解析缓冲区 */
			nret = CombineToParseBuffer( p_plugin_ctx , block_out_buf , block_out_len ) ;
			if( nret < 0 )
			{
				ERRORLOGC( "CombineToParseBuffer failed[%d]" , nret )
				return nret;
			}
			else
			{
				INFOLOGC( "CombineToParseBuffer ok" )
			}
		}
		else
		{
			ERRORLOGC( "uncompress_algorithm[%s] invalid" , p_plugin_ctx->uncompress_algorithm )
			return -1;
		}
	}
	
	return 0;
}

funcAfterWriteOutputPlugin AfterWriteOutputPlugin ;
int AfterWriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint16_t filename_len , char *filename )
{
	return 0;
}

funcCleanOutputPluginContext CleanOutputPluginContext ;
int CleanOutputPluginContext( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	free( p_plugin_ctx->field_separator_array );
	
	INFOLOGC( "DestroyHttpEnv" )
	DestroyHttpEnv( p_plugin_ctx->http_env );
	
	return 0;
}

funcUnloadOutputPluginConfig UnloadOutputPluginConfig ;
int UnloadOutputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void **pp_context )
{
	struct OutputPluginContext	**pp_plugin_ctx = (struct OutputPluginContext **)pp_context ;
	
	/* 释放内存以存放插件上下文 */
	free( (*pp_plugin_ctx) ); (*pp_plugin_ctx) = NULL ;
	
	return 0;
}

