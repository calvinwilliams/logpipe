#include "logpipe_api.h"

/* communication protocol :
	|'@'(1byte)|filename_len(2bytes)|file_name|file_block_len(2bytes)|file_block_data|...(other file blocks)...|\0\0\0\0|
*/

char	*__LOGPIPE_INPUT_TCP_VERSION = "0.1.0" ;

struct InputPluginContext_AcceptedSession
{
	struct sockaddr_in	accepted_addr ;
	int			accepted_sock ;
} ;

funcOnInputPluginEvent OnInputPluginEvent_accepted_session ;
int OnInputPluginEvent_accepted_session( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	struct InputPluginContext_AcceptedSession	*p_accepted_session_context = (struct InputPluginContext_AcceptedSession *)p_context ;
	
	uint16_t					*filename_len_htons = NULL ;
	char						comm_buf[ 1 + sizeof(uint16_t) + PATH_MAX ] ;
	uint16_t					filename_len ;
	int						len ;
	
	int						nret = 0 ;
	
	len = readn( p_accepted_session_context->accepted_sock , comm_buf , 1+sizeof(uint16_t) ) ;
	if( len == -1 )
	{
		ERRORLOG( "recv comm magic and filename len failed , errno[%d]" , errno );
		RemoveInputPluginSession( p_env , p_logpipe_input_plugin );
		return 1;
	}
	else if( len == 0 )
	{
		INFOLOG( "remote socket closed on recv comm magic and filename len" );
		RemoveInputPluginSession( p_env , p_logpipe_input_plugin );
		return 1;
	}
	else
	{
		INFOLOG( "recv comm magic and filename len ok , [%d]bytes" , len )
		DEBUGHEXLOG( comm_buf , len , NULL )
	}
	
	if( comm_buf[0] != LOGPIPE_COMM_HEAD_MAGIC )
	{
		ERRORLOG( "magic[%c][%d] invalid" , comm_buf[0] , comm_buf[0] );
		RemoveInputPluginSession( p_env , p_logpipe_input_plugin );
		return 1;
	}
	
	filename_len_htons = (uint16_t*)(comm_buf+1) ;
	filename_len = ntohs(*filename_len_htons) ;
	
	if( filename_len > PATH_MAX )
	{
		ERRORLOG( "filename length[%d] too long" , filename_len );
		RemoveInputPluginSession( p_env , p_logpipe_input_plugin );
		return 1;
	}
	
	len = readn( p_accepted_session_context->accepted_sock , comm_buf+1+sizeof(uint16_t) , filename_len ) ;
	if( len == -1 )
	{
		ERRORLOG( "recv accepted session sock failed , errno[%d]" , errno );
		RemoveInputPluginSession( p_env , p_logpipe_input_plugin );
		return 1;
	}
	else if( len == 0 )
	{
		ERRORLOG( "remote socket closed on recv accepted session sock" );
		RemoveInputPluginSession( p_env , p_logpipe_input_plugin );
		return 1;
	}
	else
	{
		INFOLOG( "recv filename from socket ok , [%d]bytes" , len )
		DEBUGHEXLOG( comm_buf+1+sizeof(uint16_t) , len , NULL )
	}
	
	/* 导出所有输出端 */
	nret = WriteAllOutputPlugins( p_env , p_logpipe_input_plugin , filename_len , comm_buf+1+sizeof(uint16_t) ) ;
	if( nret )
	{
		ERRORLOG( "WriteAllOutputPlugins failed[%d]" , nret )
		return nret;
	}
	
	return 0;
}

funcBeforeReadInputPlugin BeforeReadInputPlugin_accepted_session ;
int BeforeReadInputPlugin_accepted_session( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	return 0;
}

funcReadInputPlugin ReadInputPlugin_accepted_session ;
int ReadInputPlugin_accepted_session( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context , uint32_t *p_block_len , char *block_buf , int block_bufsize )
{
	struct InputPluginContext_AcceptedSession	*p_accepted_session_context = (struct InputPluginContext_AcceptedSession *)p_context ;
	uint32_t					block_len_htonl ;
	int						len ;
	
	len = readn( p_accepted_session_context->accepted_sock , & block_len_htonl , sizeof(uint32_t) ) ;
	if( len == -1 )
	{
		ERRORLOG( "recv block length from accepted session sock failed , errno[%d]" , errno )
		RemoveInputPluginSession( p_env , p_logpipe_input_plugin );
		return 1;
	}
	else if( len == 0 )
	{
		ERRORLOG( "accepted sessio sock closed on recv block length" )
		RemoveInputPluginSession( p_env , p_logpipe_input_plugin );
		return 1;
	}
	else
	{
		INFOLOG( "recv block length from accepted session sock ok , [%d]bytes" , sizeof(uint32_t) )
		DEBUGHEXLOG( (char*) & block_len_htonl , len , NULL )
	}
	
	(*p_block_len) = ntohl( block_len_htonl ) ;
	if( (*p_block_len) == 0 )
		return LOGPIPE_READ_END_OF_INPUT;
	if( (*p_block_len) > block_bufsize-1 )
	{
		ERRORLOG( "block length[%d] too long" , (*p_block_len) )
		RemoveInputPluginSession( p_env , p_logpipe_input_plugin );
		return 1;
	}
	
	len = readn( p_accepted_session_context->accepted_sock , block_buf , (*p_block_len) ) ;
	if( len == -1 )
	{
		ERRORLOG( "recv block from accepted session sock failed , errno[%d]" , errno )
		RemoveInputPluginSession( p_env , p_logpipe_input_plugin );
		return 1;
	}
	else if( len == 0 )
	{
		ERRORLOG( "accepted session socket closed on recv block" )
		RemoveInputPluginSession( p_env , p_logpipe_input_plugin );
		return 1;
	}
	else
	{
		INFOLOG( "recv block from accepted session sock ok , [%d]bytes" , (*p_block_len) )
		DEBUGHEXLOG( block_buf , len , NULL )
	}
	
	return 0;
}

funcAfterReadInputPlugin AfterReadInputPlugin_accepted_session ;
int AfterReadInputPlugin_accepted_session( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	return 0;
}

funcCleanInputPluginContext CleanInputPluginContext_accepted_session ;
int CleanInputPluginContext_accepted_session( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	struct InputPluginContext_AcceptedSession	*p_accepted_session_context = (struct InputPluginContext_AcceptedSession *)p_context ;
	
	if( p_accepted_session_context->accepted_sock >= 0 )
	{
		INFOLOG( "close accepted sock[%d]" , p_accepted_session_context->accepted_sock )
		close( p_accepted_session_context->accepted_sock ); p_accepted_session_context->accepted_sock = -1 ;
	}
	
	return 0;
}

funcUnloadInputPluginConfig UnloadInputPluginConfig_accepted_session ;
int UnloadInputPluginConfig_accepted_session( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void **pp_context )
{
	struct InputPluginContext_AcceptedSession	**pp_accepted_session_context = (struct InputPluginContext_AcceptedSession **)pp_context ;
	
	free( (*pp_accepted_session_context) ); (*pp_accepted_session_context) = NULL ;
	
	return 0;
}

/****************** 以上为已连接会话 ******************/

struct InputPluginContext
{
	char			*ip ;
	int			port ;
	
	struct sockaddr_in	listen_addr ;
	int			listen_sock ;
} ;

funcLoadInputPluginConfig LoadInputPluginConfig ;
int LoadInputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , struct LogpipePluginConfigItem *p_plugin_config_items , void **pp_context , int *p_fd )
{
	struct InputPluginContext	*p_plugin_ctx = NULL ;
	char				*p = NULL ;
	
	p_plugin_ctx = (struct InputPluginContext *)malloc( sizeof(struct InputPluginContext) ) ;
	if( p_plugin_ctx == NULL )
	{
		ERRORLOG( "malloc failed , errno[%d]" , errno );
		return -1;
	}
	memset( p_plugin_ctx , 0x00 , sizeof(struct InputPluginContext) );
	
	/* 解析插件配置 */
	p_plugin_ctx->ip = QueryPluginConfigItem( p_plugin_config_items , "ip" ) ;
	INFOLOG( "ip[%s]" , p_plugin_ctx->ip )
	
	p = QueryPluginConfigItem( p_plugin_config_items , "port" ) ;
	if( p )
		p_plugin_ctx->port = atoi(p) ;
	else
		p_plugin_ctx->port = 0 ;
	INFOLOG( "port[%d]" , p_plugin_ctx->port )
	
	/* 初始化插件环境内部数据 */
	memset( & (p_plugin_ctx->listen_addr) , 0x00 , sizeof(struct sockaddr_in) );
	p_plugin_ctx->listen_addr.sin_family = AF_INET ;
	if( p_plugin_ctx->ip[0] == '\0' )
		p_plugin_ctx->listen_addr.sin_addr.s_addr = INADDR_ANY ;
	else
		p_plugin_ctx->listen_addr.sin_addr.s_addr = inet_addr(p_plugin_ctx->ip) ;
	p_plugin_ctx->listen_addr.sin_port = htons( (unsigned short)(p_plugin_ctx->port) );
	
	/* 设置插件环境上下文 */
	(*pp_context) = p_plugin_ctx ;
	
	return 0;
}

funcInitInputPluginContext InitInputPluginContext ;
int InitInputPluginContext( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context , int *p_fd )
{
	struct InputPluginContext	*p_plugin_ctx = (struct InputPluginContext *)p_context ;
	
	int				nret = 0 ;
	
	/* 创建侦听端 */
	p_plugin_ctx->listen_sock = socket( AF_INET , SOCK_STREAM , IPPROTO_TCP ) ;
	if( p_plugin_ctx->listen_sock == -1 )
	{
		ERRORLOG( "socket failed , errno[%d]" , errno );
		return -1;
	}
	
	/* 设置套接字选项 */
	{
		int	onoff = 1 ;
		setsockopt( p_plugin_ctx->listen_sock , SOL_SOCKET , SO_REUSEADDR , (void *) & onoff , sizeof(int) );
	}
	
	{
		int	onoff = 1 ;
		setsockopt( p_plugin_ctx->listen_sock , IPPROTO_TCP , TCP_NODELAY , (void*) & onoff , sizeof(int) );
	}
	
	/* 绑定套接字到侦听端口 */
	nret = bind( p_plugin_ctx->listen_sock , (struct sockaddr *) & (p_plugin_ctx->listen_addr) , sizeof(struct sockaddr) ) ;
	if( nret == -1 )
	{
		ERRORLOG( "bind[%s:%d][%d] failed , errno[%d]" , p_plugin_ctx->ip , p_plugin_ctx->port , p_plugin_ctx->listen_sock , errno );
		return -1;
	}
	
	/* 处于侦听状态了 */
	nret = listen( p_plugin_ctx->listen_sock , 10240 ) ;
	if( nret == -1 )
	{
		ERRORLOG( "listen[%s:%d][%d] failed , errno[%d]" , p_plugin_ctx->ip , p_plugin_ctx->port , p_plugin_ctx->listen_sock , errno );
		return -1;
	}
	else
	{
		INFOLOG( "listen[%s:%d][%d] ok" , p_plugin_ctx->ip , p_plugin_ctx->port , p_plugin_ctx->listen_sock )
	}
	
	/* 设置输入描述字 */
	(*p_fd) = p_plugin_ctx->listen_sock ;
	
	return 0;
}

funcOnInputPluginEvent OnInputPluginEvent ;
int OnInputPluginEvent( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	struct InputPluginContext			*p_plugin_ctx = (struct InputPluginContext *)p_context ;
	struct InputPluginContext_AcceptedSession	*p_accepted_session_ctx = NULL ;
	struct LogpipeInputPlugin			*p_accepted_session = NULL ;
	socklen_t					accept_addr_len ;
	
	/* 申请内存以存放客户端连接会话 */
	p_accepted_session_ctx = (struct InputPluginContext_AcceptedSession *)malloc( sizeof(struct InputPluginContext_AcceptedSession) ) ;
	if( p_accepted_session_ctx == NULL )
	{
		ERRORLOG( "malloc failed , errno[%d]" , errno )
		return -1;
	}
	memset( p_accepted_session_ctx , 0x00 , sizeof(struct InputPluginContext_AcceptedSession) );
	
	/* 接受新连接 */
	accept_addr_len = sizeof(struct sockaddr) ;
	p_accepted_session_ctx->accepted_sock = accept( p_plugin_ctx->listen_sock , (struct sockaddr *) & (p_accepted_session_ctx->accepted_addr) , & accept_addr_len ) ;
	if( p_accepted_session_ctx->accepted_sock == -1 )
	{
		ERRORLOG( "accept failed , errno[%d]" , errno )
		free( p_accepted_session_ctx );
		return -1;
	}
	
	/* 设置套接字选项 */
	{
		int	onoff = 1 ;
		setsockopt( p_accepted_session_ctx->accepted_sock , SOL_SOCKET , SO_REUSEADDR , (void *) & onoff , sizeof(int) );
	}
	
	{
		int	onoff = 1 ;
		setsockopt( p_accepted_session_ctx->accepted_sock , IPPROTO_TCP , TCP_NODELAY , (void*) & onoff , sizeof(int) );
	}
	
	DEBUGLOG( "[%d]accept[%d] ok" , p_plugin_ctx->listen_sock , p_accepted_session_ctx->accepted_sock )
	
	p_accepted_session = AddInputPluginSession( p_env , "accepted_session"
					, & OnInputPluginEvent_accepted_session
					, & BeforeReadInputPlugin_accepted_session , & ReadInputPlugin_accepted_session , & AfterReadInputPlugin_accepted_session
					, & CleanInputPluginContext_accepted_session , & UnloadInputPluginConfig_accepted_session
					, p_accepted_session_ctx->accepted_sock , p_accepted_session_ctx ) ;
	if( p_accepted_session == NULL )
	{
		close( p_accepted_session_ctx->accepted_sock );
		free( p_accepted_session_ctx );
		ERRORLOG( "AddInputPluginSession failed" )
		return -1;
	}
	
	return 0;
}

funcBeforeReadInputPlugin BeforeReadInputPlugin ;
int BeforeReadInputPlugin( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	return 0;
}

funcReadInputPlugin ReadInputPlugin ;
int ReadInputPlugin( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context , uint32_t *p_block_len , char *block_buf , int block_bufsize )
{
	return 0;
}

funcAfterReadInputPlugin AfterReadInputPlugin ;
int AfterReadInputPlugin( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	return 0;
}

funcCleanInputPluginContext CleanInputPluginContext ;
int CleanInputPluginContext( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	struct InputPluginContext	*p_plugin_ctx = (struct InputPluginContext *)p_context ;
	
	if( p_plugin_ctx->listen_sock >= 0 )
	{
		INFOLOG( "close listen sock[%d]" , p_plugin_ctx->listen_sock )
		close( p_plugin_ctx->listen_sock ); p_plugin_ctx->listen_sock = -1 ;
	}
	
	return 0;
}

funcUnloadInputPluginConfig UnloadInputPluginConfig ;
int UnloadInputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void **pp_context )
{
	struct InputPluginContext	**pp_plugin_ctx = (struct InputPluginContext **)pp_context ;
	
	free( (*pp_plugin_ctx) ); (*pp_plugin_ctx) = NULL ;
	
	return 0;
}

