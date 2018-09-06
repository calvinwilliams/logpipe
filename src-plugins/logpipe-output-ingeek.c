#include "logpipe_api.h"

/* communication protocol :
	|(log)|key=(key)|file=(pathfile)|byteoffset=(byteoffset)|
*/

/* compile and install command
make logpipe-output-ingeek.so && cp logpipe-output-ingeek.so ~/so/
*/

int	__LOGPIPE_OUTPUT_INGEEK_VERSION_0_1_0 = 1 ;

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

#define MAX_LOG_BLOCK		1024
#define MAX_LOG_LINE		256

struct OutputPluginContext
{
	char			*key ;
	char			*path ;
	struct ForwardSession	forward_session_array[IP_PORT_MAXCNT] ;
	int			forward_session_count ;
	struct ForwardSession	*p_forward_session ;
	int			forward_session_index ;
	int			disable_timeout ;
	
	char			*filename ;
	uint64_t		file_line ;
	uint64_t		block_len ;
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
		ERRORLOGC( "malloc failed , errno[%d]" , errno )
		return -1;
	}
	memset( p_plugin_ctx , 0x00 , sizeof(struct OutputPluginContext) );
	
	p_plugin_ctx->key = QueryPluginConfigItem( p_plugin_config_items , "key" ) ;
	INFOLOGC( "key[%s]" , p_plugin_ctx->key )
	
	p_plugin_ctx->path = QueryPluginConfigItem( p_plugin_config_items , "path" ) ;
	INFOLOGC( "path[%s]" , p_plugin_ctx->path )
	if( p_plugin_ctx->path == NULL || p_plugin_ctx->path[0] == '\0' )
	{
		ERRORLOGC( "expect config for 'path'" )
		return -1;
	}
	
	/* 解析插件配置 */
	for( i = 0 , p_forward_session = p_plugin_ctx->forward_session_array ; i < IP_PORT_MAXCNT ; i++ , p_forward_session++ )
	{
		if( i == 0 )
		{
			p_forward_session->ip = QueryPluginConfigItem( p_plugin_config_items , "ip" ) ;
			INFOLOGC( "ip[%s]" , p_forward_session->ip )
		}
		else
		{
			p_forward_session->ip = QueryPluginConfigItem( p_plugin_config_items , "ip%d" , i+1 ) ;
			INFOLOGC( "ip%d[%s]" , i+1 , p_forward_session->ip )
		}
		if( p_forward_session->ip == NULL || p_forward_session->ip[0] == '\0' )
			break;
		
		if( i == 0 )
			p = QueryPluginConfigItem( p_plugin_config_items , "port" ) ;
		else
			p = QueryPluginConfigItem( p_plugin_config_items , "port%d" , i+1 ) ;
		if( p == NULL || p[0] == '\0' )
		{
			ERRORLOGC( "expect config for 'port'" )
			return -1;
		}
		p_forward_session->port = atoi(p) ;
		if( i == 0 )
		{
			INFOLOGC( "port[%d]" , p_forward_session->port )
		}
		else
		{
			INFOLOGC( "port%d[%d]" , i+1 , p_forward_session->port )
		}
		if( p_forward_session->port <= 0 )
		{
			ERRORLOGC( "port[%s] invalid" , p )
			return -1;
		}
		
		p_plugin_ctx->forward_session_count++;
	}
	if( p_plugin_ctx->forward_session_count == 0 )
	{
		ERRORLOGC( "expect config for 'ip'" )
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
	
	DEBUGLOGC( "forward_session_index[%d]" , forward_session_index )
	
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
				DEBUGLOGC( "p_plugin_ctx->forward_session_index[%d]" , p_plugin_ctx->forward_session_index )
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
				ERRORLOGC( "socket failed , errno[%d]" , errno )
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
				ERRORLOGC( "connect[%s:%d] failed , errno[%d]" , p_forward_session->ip , p_forward_session->port , errno )
				close( p_forward_session->sock ); p_forward_session->sock = -1 ;
				if( forward_session_index < 0 )
					p_forward_session->enable_timestamp = time(NULL) + p_plugin_ctx->disable_timeout ;
			}
			else
			{
				INFOLOGC( "connect[%s:%d] ok , sock[%d]" , p_forward_session->ip , p_forward_session->port , p_forward_session->sock )
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
		{
			ERRORLOGC( "CheckAndConnectForwardSocket failed[%d]" , nret )
			return -1;
		}
	}
	
	p_plugin_ctx->forward_session_index = -1 ;
	
	/* 检查连接，如果连接失效则重连 */
	nret = CheckAndConnectForwardSocket( p_env , p_logpipe_output_plugin , p_plugin_ctx , -1 ) ;
	if( nret )
	{
		ERRORLOGC( "CheckAndConnectForwardSocket failed[%d]" , nret )
		return nret;
	}
	
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
	
	DEBUGLOGC( "OnOutputPluginEvent" )
	
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
		DEBUGLOGC( "add fd[%d] to select fds" , p_forward_session->sock )
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
				DEBUGLOGC( "select fd[%d] hited" , p_forward_session->sock )
				
				/* 关闭连接 */
				DeleteOutputPluginEvent( p_env , p_logpipe_output_plugin , p_forward_session->sock );
				ERRORLOGC( "remote socket closed , close forward sock[%d]" , p_forward_session->sock )
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
	if( nret )
	{
		ERRORLOGC( "CheckAndConnectForwardSocket failed[%d]" , nret )
		return nret;
	}
	
	/* 保存文件名指针 */
	p_plugin_ctx->filename = filename ;
	
	return 0;
}

/* 分列解析缓冲区 */
static int ParseCombineBuffer( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , struct OutputPluginContext *p_plugin_ctx , uint64_t line_len , uint64_t line_add )
{
	char		mainfilename[ PATH_MAX + 1 ] ;
	char		*p = NULL ;
	int		line_short_len ;
	char		tail_buffer[ MAX_LOG_LINE + 1 ] ;
	uint64_t	tail_buffer_len ;
	struct timeval	tv_begin_send_log ;
	struct timeval	tv_end_send_log ;
	struct timeval	tv_diff_send_log ;
	struct timeval	tv_begin_send_tail ;
	struct timeval	tv_end_send_tail ;
	struct timeval	tv_diff_send_tail ;
	int		len ;
	
	int		nret = 0 ;
	
	DEBUGLOGC( "parse [%d][%.*s]" , line_len , line_len , p_plugin_ctx->parse_buffer )
	
	/* 填充尾巴 */
	memset( tail_buffer , 0x00 , sizeof(tail_buffer) );
	if( p_plugin_ctx->key )
	{
		tail_buffer_len = snprintf( tail_buffer , sizeof(tail_buffer)-1 , "[key=%s][file=%s/%s][byteoffset=%"PRIu64"]\n" , p_plugin_ctx->key , p_plugin_ctx->path , p_plugin_ctx->filename , p_plugin_ctx->file_line+line_add ) ;
	}
	else
	{
		memset( mainfilename , 0x00 , sizeof(mainfilename) );
		strncpy( mainfilename , p_plugin_ctx->filename , sizeof(mainfilename)-1 );
		p = strrchr( mainfilename , '.' ) ;
		if( p )
			(*p) = '\0' ;
		p = strchr( mainfilename , '_' ) ;
		if( p )
		{
			p = strchr( p+1 , '_' ) ;
			if( p )
				(*p) = '\0' ;
		}
		tail_buffer_len = snprintf( tail_buffer , sizeof(tail_buffer)-1 , "[key=%s][file=%s/%s][byteoffset=%"PRIu64"]\n" , mainfilename , p_plugin_ctx->path , p_plugin_ctx->filename , p_plugin_ctx->file_line+line_add ) ;
	}
	
	if( line_len > MAX_LOG_LINE )
		line_short_len = MAX_LOG_LINE ;
	else
		line_short_len = line_len ;
	
	/* 发送数据块到TCP */
_GOTO_WRITEN_LOG :
	gettimeofday( & tv_begin_send_log , NULL );
	len = writen( p_plugin_ctx->p_forward_session->sock , p_plugin_ctx->parse_buffer , line_len ) ;
	gettimeofday( & tv_end_send_log , NULL );
	if( len == -1 )
	{
		ERRORLOGC( "send-log [%d][%.*s] to socket failed , errno[%d]" , line_len , line_short_len , p_plugin_ctx->parse_buffer , errno )
		close( p_plugin_ctx->p_forward_session->sock ); p_plugin_ctx->p_forward_session->sock = -1 ;
		nret = CheckAndConnectForwardSocket( p_env , p_logpipe_output_plugin , p_plugin_ctx , -1 ) ;
		if( nret )
		{
			ERRORLOGC( "CheckAndConnectForwardSocket failed[%d]" , nret )
			return nret;
		}
		goto _GOTO_WRITEN_LOG;
	}
	else
	{
		DEBUGLOGC( "send log data to socket ok , [%d]bytes" , line_len )
		DEBUGHEXLOGC( p_plugin_ctx->parse_buffer , line_len , NULL )
	}
	
_GOTO_WRITEN_TAIL :
	gettimeofday( & tv_begin_send_tail , NULL );
	len = writen( p_plugin_ctx->p_forward_session->sock , tail_buffer , tail_buffer_len ) ;
	gettimeofday( & tv_end_send_tail , NULL );
	if( len == -1 )
	{
		ERRORLOGC( "send-tail [%d][%.*s] to socket failed , errno[%d]" , tail_buffer_len , tail_buffer_len , tail_buffer , errno )
		close( p_plugin_ctx->p_forward_session->sock ); p_plugin_ctx->p_forward_session->sock = -1 ;
		nret = CheckAndConnectForwardSocket( p_env , p_logpipe_output_plugin , p_plugin_ctx , -1 ) ;
		if( nret )
		{
			ERRORLOGC( "CheckAndConnectForwardSocket failed[%d]" , nret )
			return nret;
		}
		goto _GOTO_WRITEN_TAIL;
	}
	else
	{
		DEBUGLOGC( "send tail data to socket ok , [%d]bytes" , tail_buffer_len )
		DEBUGHEXLOGC( tail_buffer , tail_buffer_len , NULL )
	}
	
	DiffTimeval( & tv_begin_send_log , & tv_end_send_log , & tv_diff_send_log );
	DiffTimeval( & tv_begin_send_tail , & tv_end_send_tail , & tv_diff_send_tail );
	
	INFOLOGC( "send [%ld.%06ld] [%d][%.*s] [%ld.%06ld] [%d][%.*s]"
		, tv_diff_send_tail.tv_sec , tv_diff_send_tail.tv_usec
		, tail_buffer_len , tail_buffer_len , tail_buffer
		, tv_diff_send_log.tv_sec , tv_diff_send_log.tv_usec
		, line_len , line_short_len , p_plugin_ctx->parse_buffer )
	
	return 0;
}

/* 数据块合并到解析缓冲区 */
static int CombineToParseBuffer( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , struct OutputPluginContext *p_plugin_ctx , char *block_buf , uint64_t block_len )
{
	char		*p_newline = NULL ;
	uint64_t	line_len ;
	uint64_t	remain_len ;
	uint64_t	line_add ;
	
	int		nret = 0 ;
	
	INFOLOGC( "before combine , [%d][%.*s]" , p_plugin_ctx->parse_buflen , (p_plugin_ctx->parse_buflen>MAX_LOG_BLOCK?MAX_LOG_BLOCK:p_plugin_ctx->parse_buflen) , p_plugin_ctx->parse_buffer )
	INFOLOGC( "block_buf [%d][%.*s]" , block_len , (block_len>MAX_LOG_BLOCK?MAX_LOG_BLOCK:block_len) , block_buf )
	
	/* 如果遗留数据+当前数据块放的下解析缓冲区 */
	if( p_plugin_ctx->parse_buflen + block_len <= sizeof(p_plugin_ctx->parse_buffer)-1 )
	{
		strncpy( p_plugin_ctx->parse_buffer+p_plugin_ctx->parse_buflen , block_buf , block_len );
		p_plugin_ctx->parse_buflen += block_len ;
	}
	else
	{
		/* 先强制把遗留数据都处理掉 */
		nret = ParseCombineBuffer( p_env , p_logpipe_output_plugin , p_plugin_ctx , p_plugin_ctx->parse_buflen , 0 ) ;
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
	line_add = 0 ;
	while(1)
	{
		p_newline = memchr( p_plugin_ctx->parse_buffer , '\n' , p_plugin_ctx->parse_buflen ) ;
		if( p_newline == NULL )
			break;
		
		line_len = p_newline - p_plugin_ctx->parse_buffer ;
		remain_len = p_plugin_ctx->parse_buflen - line_len - 1 ;
		(*p_newline) = '\0' ;
		nret = ParseCombineBuffer( p_env , p_logpipe_output_plugin , p_plugin_ctx , line_len , line_add ) ;
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
		
		memmove( p_plugin_ctx->parse_buffer , p_newline+1 , remain_len );
		p_plugin_ctx->parse_buflen = remain_len ;
		line_add++;
	}
	
	INFOLOGC( "after combine , [%d][%.*s]" , p_plugin_ctx->parse_buflen , (p_plugin_ctx->parse_buflen>MAX_LOG_BLOCK?MAX_LOG_BLOCK:p_plugin_ctx->parse_buflen) , p_plugin_ctx->parse_buffer )
	
	return 0;
}

funcWriteOutputPlugin WriteOutputPlugin ;
int WriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint64_t file_offset , uint64_t file_line , uint64_t block_len , char *block_buf )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	int				nret = 0 ;
	
	/* 保存信息 */
	p_plugin_ctx->file_line = file_line ;
	p_plugin_ctx->block_len = block_len ;
	p_plugin_ctx->block_buf = block_buf ;
	
	/* 数据块合并到解析缓冲区 */
	nret = CombineToParseBuffer( p_env , p_logpipe_output_plugin , p_plugin_ctx , block_buf , block_len ) ;
	if( nret < 0 )
	{
		ERRORLOGC( "CombineToParseBuffer failed[%d]" , nret )
		return nret;
	}
	else
	{
		DEBUGLOGC( "CombineToParseBuffer ok" )
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
		INFOLOGC( "close forward sock[%d]" , p_plugin_ctx->p_forward_session->sock )
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

