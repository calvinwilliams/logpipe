#include "logpipe_api.h"

/* communication protocol :
	|(log)|key=(key)|file=(pathfile)|byteoffset=(byteoffset)|
*/

/* compile and install command
make logpipe-output-ingeek.so && cp logpipe-output-ingeek.so ~/so/
*/

char	*__LOGPIPE_OUTPUT_INGEEK_VERSION = "0.1.0" ;

struct ForwardSession
{
	char			*ip ;
	int			port ;
	struct sockaddr_in   	addr ;
	int			sock ;
	time_t			enable_timestamp ;
} ;

/* 插件环境结构 */
#define IP_PORT_MAXCNT		8

#define DISABLE_TIMEOUT		60

#define PARSE_BUFFER_SIZE	LOGPIPE_BLOCK_BUFSIZE*2

struct OutputPluginContext
{
	/*
	char			*key ;
	*/
	char			*path ;
	struct ForwardSession	forward_session_array[IP_PORT_MAXCNT] ;
	int			forward_session_count ;
	struct ForwardSession	*p_forward_session ;
	int			forward_session_index ;
	int			disable_timeout ;
	
	char			*filename ;
	uint32_t		file_line ;
	uint32_t		block_len ;
	char			*block_buf ;
	
	char			parse_buffer[ PARSE_BUFFER_SIZE + 1 ] ; /* 解析缓冲区 */
	int			parse_buflen ;
} ;

funcLoadOutputPluginConfig LoadOutputPluginConfig ;
int LoadOutputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , struct LogpipePluginConfigItem *p_plugin_config_items , void **pp_context )
{
	struct OutputPluginContext	*p_plugin_ctx = NULL ;
	int				i ;
	struct ForwardSession		*p_forward_session = NULL ;
	char				*p = NULL ;
	
	/* 申请内存以存放插件上下文 */
	p_plugin_ctx = (struct OutputPluginContext *)malloc( sizeof(struct OutputPluginContext) ) ;
	if( p_plugin_ctx == NULL )
	{
		ERRORLOG( "malloc failed , errno[%d]" , errno );
		return -1;
	}
	memset( p_plugin_ctx , 0x00 , sizeof(struct OutputPluginContext) );
	
	/*
	p_plugin_ctx->key = QueryPluginConfigItem( p_plugin_config_items , "key" ) ;
	INFOLOG( "key[%s]" , p_plugin_ctx->key )
	if( p_plugin_ctx->key == NULL || p_plugin_ctx->key[0] == '\0' )
	{
		ERRORLOG( "expect config for 'key'" );
		return -1;
	}
	*/
	
	p_plugin_ctx->path = QueryPluginConfigItem( p_plugin_config_items , "path" ) ;
	INFOLOG( "path[%s]" , p_plugin_ctx->path )
	if( p_plugin_ctx->path == NULL || p_plugin_ctx->path[0] == '\0' )
	{
		ERRORLOG( "expect config for 'path'" );
		return -1;
	}
	
	/* 解析插件配置 */
	for( i = 0 , p_forward_session = p_plugin_ctx->forward_session_array ; i < IP_PORT_MAXCNT ; i++ , p_forward_session++ )
	{
		if( i == 0 )
		{
			p_forward_session->ip = QueryPluginConfigItem( p_plugin_config_items , "ip" ) ;
			INFOLOG( "ip[%s]" , p_forward_session->ip )
		}
		else
		{
			p_forward_session->ip = QueryPluginConfigItem( p_plugin_config_items , "ip%d" , i+1 ) ;
			INFOLOG( "ip%d[%s]" , i+1 , p_forward_session->ip )
		}
		if( p_forward_session->ip == NULL || p_forward_session->ip[0] == '\0' )
			break;
		
		if( i == 0 )
			p = QueryPluginConfigItem( p_plugin_config_items , "port" ) ;
		else
			p = QueryPluginConfigItem( p_plugin_config_items , "port%d" , i+1 ) ;
		if( p == NULL || p[0] == '\0' )
		{
			ERRORLOG( "expect config for 'port'" );
			return -1;
		}
		p_forward_session->port = atoi(p) ;
		if( i == 0 )
		{
			INFOLOG( "port[%d]" , p_forward_session->port )
		}
		else
		{
			INFOLOG( "port%d[%d]" , i+1 , p_forward_session->port )
		}
		if( p_forward_session->port <= 0 )
		{
			ERRORLOG( "port[%s] invalid" , p );
			return -1;
		}
		
		p_plugin_ctx->forward_session_count++;
	}
	if( p_plugin_ctx->forward_session_count == 0 )
	{
		ERRORLOG( "expect config for 'ip'" );
		return -1;
	}
	
	p = QueryPluginConfigItem( p_plugin_config_items , "disable_timeout" ) ;
	if( p )
	{
		p_plugin_ctx->disable_timeout = atoi(p) ;
	}
	else
	{
		p_plugin_ctx->disable_timeout = DISABLE_TIMEOUT ;
	}
	
	/* 设置插件环境上下文 */
	(*pp_context) = p_plugin_ctx ;
	
	return 0;
}

static int CheckAndConnectForwardSocket( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , struct OutputPluginContext *p_plugin_ctx , int forward_session_index )
{
	struct ForwardSession	*p_forward_session = NULL ;
	
	int			i ;
	
	int			nret = 0 ;
	
	DEBUGLOG( "forward_session_index[%d]" , forward_session_index )
	
	while(1)
	{
		for( i = 0 ; i < p_plugin_ctx->forward_session_count ; i++ )
		{
			if( forward_session_index >= 0 )
			{
				p_forward_session = p_plugin_ctx->forward_session_array + forward_session_index ;
			}
			else
			{
				p_plugin_ctx->forward_session_index++;
				if( p_plugin_ctx->forward_session_index >= p_plugin_ctx->forward_session_count )
					p_plugin_ctx->forward_session_index = 0 ;
				DEBUGLOG( "p_plugin_ctx->forward_session_index[%d]" , p_plugin_ctx->forward_session_index )
				p_plugin_ctx->p_forward_session = p_forward_session = p_plugin_ctx->forward_session_array + p_plugin_ctx->forward_session_index ;
			}
			
			if( forward_session_index < 0 )
			{
				if( p_forward_session->enable_timestamp > 0 )
				{
					if( time(NULL) < p_forward_session->enable_timestamp )
						continue;
					else
						p_forward_session->enable_timestamp = 0 ;
				}
				
				if( p_forward_session->sock >= 0 )
					return 0;
			}
			
			/* 创建套接字 */
			p_forward_session->sock = socket( AF_INET , SOCK_STREAM , IPPROTO_TCP ) ;
			if( p_forward_session->sock == -1 )
			{
				ERRORLOG( "socket failed , errno[%d]" , errno );
				return -1;
			}
			
			/* 设置套接字选项 */
			{
				int	onoff = 1 ;
				setsockopt( p_forward_session->sock , SOL_SOCKET , SO_REUSEADDR , (void *) & onoff , sizeof(int) );
			}
			
			{
				int	onoff = 1 ;
				setsockopt( p_forward_session->sock , IPPROTO_TCP , TCP_NODELAY , (void*) & onoff , sizeof(int) );
			}
			
			/* 连接到服务端侦听端口 */
			nret = connect( p_forward_session->sock , (struct sockaddr *) & (p_forward_session->addr) , sizeof(struct sockaddr) ) ;
			if( nret == -1 )
			{
				ERRORLOG( "connect[%s:%d] failed , errno[%d]" , p_forward_session->ip , p_forward_session->port , errno );
				close( p_forward_session->sock ); p_forward_session->sock = -1 ;
				if( forward_session_index < 0 )
					p_forward_session->enable_timestamp = time(NULL) + p_plugin_ctx->disable_timeout ;
			}
			else
			{
				INFOLOG( "connect[%s:%d] ok , sock[%d]" , p_forward_session->ip , p_forward_session->port , p_forward_session->sock );
				/* 设置输入描述字 */
				AddOutputPluginEvent( p_env , p_logpipe_output_plugin , p_forward_session->sock );
				return 0;
			}
		}
		
		sleep(1);
	}
}

funcInitOutputPluginContext InitOutputPluginContext ;
int InitOutputPluginContext( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	int				i ;
	struct ForwardSession		*p_forward_session = NULL ;
	
	int				nret = 0 ;
	
	/* 初始化插件环境内部数据 */
	
	/* 连接所有对端logpipe服务器 */
	for( i = 0 , p_forward_session = p_plugin_ctx->forward_session_array ; i < p_plugin_ctx->forward_session_count ; i++ , p_forward_session++ )
	{
		memset( & (p_forward_session->addr) , 0x00 , sizeof(struct sockaddr_in) );
		p_forward_session->addr.sin_family = AF_INET ;
		if( p_forward_session->ip[0] == '\0' )
			p_forward_session->addr.sin_addr.s_addr = INADDR_ANY ;
		else
			p_forward_session->addr.sin_addr.s_addr = inet_addr(p_forward_session->ip) ;
		p_forward_session->addr.sin_port = htons( (unsigned short)(p_forward_session->port) );
		
		/* 连接服务端 */
		p_forward_session->sock = -1 ;
		nret = CheckAndConnectForwardSocket( p_env , p_logpipe_output_plugin , p_plugin_ctx , i ) ;
		if( nret )
			return -1;
	}
	
	p_plugin_ctx->forward_session_index = -1 ;
	
	return 0;
}

funcOnOutputPluginEvent OnOutputPluginEvent;
int OnOutputPluginEvent( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	int				i ;
	struct ForwardSession		*p_forward_session = NULL ;
	fd_set				read_fds , except_fds ;
	int				max_fd ;
	struct timeval			timeout ;
	
	int				nret = 0 ;
	
	DEBUGLOG( "OnOutputPluginEvent" )
	
	/* 侦测所有对端logpipe服务器可用 */
	FD_ZERO( & read_fds );
	FD_ZERO( & except_fds );
	max_fd = -1 ;
	for( i = 0 , p_forward_session = p_plugin_ctx->forward_session_array ; i < p_plugin_ctx->forward_session_count ; i++ , p_forward_session++ )
	{
		if( p_forward_session->sock < 0 )
			continue;
		
		FD_SET( p_forward_session->sock , & read_fds );
		FD_SET( p_forward_session->sock , & except_fds );
		if( p_forward_session->sock > max_fd )
			max_fd = p_forward_session->sock ;
		DEBUGLOG( "add fd[%d] to select fds" , p_forward_session->sock )
	}
	
	timeout.tv_sec = 0 ;
	timeout.tv_usec = 0 ;
	nret = select( max_fd+1 , & read_fds , NULL , & except_fds , & timeout ) ;
	if( nret > 0 )
	{
		for( i = 0 , p_forward_session = p_plugin_ctx->forward_session_array ; i < p_plugin_ctx->forward_session_count ; i++ , p_forward_session++ )
		{
			if( p_forward_session->sock < 0 )
				continue;
			
			if( FD_ISSET( p_forward_session->sock , & read_fds ) || FD_ISSET( p_forward_session->sock , & except_fds ) )
			{
				DEBUGLOG( "select fd[%d] hited" , p_forward_session->sock )
				
				/* 关闭连接 */
				DeleteOutputPluginEvent( p_env , p_logpipe_output_plugin , p_forward_session->sock );
				ERRORLOG( "remote socket closed , close forward sock[%d]" , p_forward_session->sock )
				close( p_forward_session->sock ); p_forward_session->sock = -1 ;
			}
		}
	}
	
	return 0;
}

funcBeforeWriteOutputPlugin BeforeWriteOutputPlugin ;
int BeforeWriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint16_t filename_len , char *filename )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	int				nret = 0 ;
	
	/* 检查连接，如果连接失效则重连 */
	nret = CheckAndConnectForwardSocket( p_env , p_logpipe_output_plugin , p_plugin_ctx , -1 ) ;
	if( nret < 0 )
		return nret;
	
	/* 保存文件名指针 */
	p_plugin_ctx->filename = filename ;
	
	return 0;
}

/* 分列解析缓冲区 */
static int ParseCombineBuffer( struct OutputPluginContext *p_plugin_ctx , int line_len , int line_add )
{
	char		mainfilename[ PATH_MAX + 1 ] ;
	char		*p = NULL ;
	char		tail_buffer[ 1024 + 1 ] ;
	uint32_t	tail_buffer_len ;
	
	int		len ;
	
	INFOLOG( "parse [%d][%.*s]" , line_len , line_len , p_plugin_ctx->parse_buffer )
	
	/* 填充尾巴 */
	memset( mainfilename , 0x00 , sizeof(mainfilename) );
	strncpy( mainfilename , p_plugin_ctx->filename , sizeof(mainfilename)-1 );
	p = strrchr( mainfilename , '.' ) ;
	if( p )
		(*p) = '\0' ;
	memset( tail_buffer , 0x00 , sizeof(tail_buffer) );
	tail_buffer_len = snprintf( tail_buffer , sizeof(tail_buffer)-1 , "[key=%s][file=%s/%s][byteoffset=%d]\n" , mainfilename , p_plugin_ctx->path , p_plugin_ctx->filename , p_plugin_ctx->file_line+line_add ) ;
	
	/* 发送数据块到TCP */
	len = writen( p_plugin_ctx->p_forward_session->sock , p_plugin_ctx->parse_buffer , line_len ) ;
	if( len == -1 )
	{
		ERRORLOG( "send block data to socket failed , errno[%d]" , errno )
		close( p_plugin_ctx->p_forward_session->sock ); p_plugin_ctx->p_forward_session->sock = -1 ;
		return 1;
	}
	else
	{
		INFOLOG( "send block data to socket ok , [%d]bytes" , line_len )
		DEBUGHEXLOG( p_plugin_ctx->parse_buffer , line_len , NULL )
	}
	
	len = writen( p_plugin_ctx->p_forward_session->sock , tail_buffer , tail_buffer_len ) ;
	if( len == -1 )
	{
		ERRORLOG( "send block len to socket failed , errno[%d]" , errno )
		close( p_plugin_ctx->p_forward_session->sock ); p_plugin_ctx->p_forward_session->sock = -1 ;
		return 1;
	}
	else
	{
		INFOLOG( "send block len to socket ok , [%d]bytes" , tail_buffer_len )
		DEBUGHEXLOG( tail_buffer , tail_buffer_len , NULL )
	}
	
	return 0;
}

/* 数据块合并到解析缓冲区 */
static int CombineToParseBuffer( struct OutputPluginContext *p_plugin_ctx , char *block_buf , uint32_t block_len )
{
	char		*p_newline = NULL ;
	int		line_len ;
	int		remain_len ;
	int		line_add ;
	
	int		nret = 0 ;
	
	INFOLOG( "before combine , [%d][%.*s]" , p_plugin_ctx->parse_buflen , p_plugin_ctx->parse_buflen , p_plugin_ctx->parse_buffer )
	INFOLOG( "block_buf [%d][%.*s]" , block_len , block_len , block_buf )
	
	/* 如果遗留数据+当前数据块放的下解析缓冲区 */
	if( p_plugin_ctx->parse_buflen + block_len <= sizeof(p_plugin_ctx->parse_buffer)-1 )
	{
		strncpy( p_plugin_ctx->parse_buffer+p_plugin_ctx->parse_buflen , block_buf , block_len );
		p_plugin_ctx->parse_buflen += block_len ;
	}
	else
	{
		/* 先强制把遗留数据都处理掉 */
		nret = ParseCombineBuffer( p_plugin_ctx , p_plugin_ctx->parse_buflen , 0 ) ;
		if( nret < 0 )
		{
			ERRORLOG( "ParseCombineBuffer failed[%d]" , nret )
			return nret;
		}
		else if( nret > 0 )
		{
			WARNLOG( "ParseCombineBuffer return[%d]" , nret )
		}
		else
		{
			DEBUGLOG( "ParseCombineBuffer ok" )
		}
		
		strncpy( p_plugin_ctx->parse_buffer , block_buf , block_len );
		p_plugin_ctx->parse_buflen = block_len ;
	}
	
	/* 把解析缓冲区中有效行都处理掉 */
	line_add = 0 ;
	while(1)
	{
		p_newline = memchr( p_plugin_ctx->parse_buffer , '\n' , p_plugin_ctx->parse_buflen ) ;
		if( p_newline == NULL )
			break;
		
		line_len = p_newline - p_plugin_ctx->parse_buffer ;
		remain_len = p_plugin_ctx->parse_buflen - line_len - 1 ;
		(*p_newline) = '\0' ;
		nret = ParseCombineBuffer( p_plugin_ctx , line_len , line_add ) ;
		if( nret < 0 )
		{
			ERRORLOG( "ParseCombineBuffer failed[%d]" , nret )
			return nret;
		}
		else if( nret > 0 )
		{
			WARNLOG( "ParseCombineBuffer return[%d]" , nret )
		}
		else
		{
			DEBUGLOG( "ParseCombineBuffer ok" )
		}
		
		memmove( p_plugin_ctx->parse_buffer , p_newline+1 , remain_len );
		p_plugin_ctx->parse_buflen = remain_len ;
		line_add++;
	}
	
	INFOLOG( "after combine , [%d][%.*s]" , p_plugin_ctx->parse_buflen , p_plugin_ctx->parse_buflen , p_plugin_ctx->parse_buffer )
	
	return 0;
}

funcWriteOutputPlugin WriteOutputPlugin ;
int WriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint32_t file_offset , uint32_t file_line , uint32_t block_len , char *block_buf )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	int				nret = 0 ;
	
	/* 保存信息 */
	p_plugin_ctx->file_line = file_line ;
	p_plugin_ctx->block_len = block_len ;
	p_plugin_ctx->block_buf = block_buf ;
	
	/* 数据块合并到解析缓冲区 */
	nret = CombineToParseBuffer( p_plugin_ctx , block_buf , block_len ) ;
	if( nret < 0 )
	{
		ERRORLOG( "CombineToParseBuffer failed[%d]" , nret )
		return nret;
	}
	else
	{
		DEBUGLOG( "CombineToParseBuffer ok" )
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
	
	if( p_plugin_ctx->p_forward_session->sock >= 0 )
	{
		INFOLOG( "close forward sock[%d]" , p_plugin_ctx->p_forward_session->sock )
		close( p_plugin_ctx->p_forward_session->sock ); p_plugin_ctx->p_forward_session->sock = -1 ;
	}
	
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

