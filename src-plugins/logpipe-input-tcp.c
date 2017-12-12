#include "logpipe_api.h"

char	*__LOGPIPE_INPUT_TCP_VERSION = "0.1.0" ;

struct LogpipeInputPlugin_tcp_accepted
{
	struct sockaddr_in   	 accepted_addr ;
	int			accepted_sock ;
} ;

funcOnLogpipeInputEvent OnLogpipeInputEvent_tcp_accepted ;
int OnLogpipeInputEvent_tcp_accepted( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	struct LogpipeInputPlugin_tcp_accepted		*p_plugin_input_plugin_tcp_accepted = (struct LogpipeInputPlugin_tcp_accepted *)p_context ;
	
	uint16_t		*filename_len_htons = NULL ;
	char			comm_buf[ 1 + sizeof(uint16_t) + PATH_MAX ] ;
	uint16_t		filename_len ;
	int			len ;
	
	int			nret = 0 ;
	
	len = readn( p_plugin_input_plugin_tcp_accepted->accepted_sock , comm_buf , 1+sizeof(uint16_t) ) ;
	if( len == -1 )
	{
		ERRORLOG( "recv comm magic and filename len failed , errno[%d]" , errno );
		return 1;
	}
	else if( len == 0 )
	{
		WARNLOG( "remote socket closed on recv comm magic and filename len" );
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
		return 1;
	}
	
	filename_len_htons = (uint16_t*)(comm_buf+1) ;
	filename_len = ntohs(*filename_len_htons) ;
	
	if( filename_len > PATH_MAX )
	{
		ERRORLOG( "filename length[%d] too long" , filename_len );
		return -1;
	}
	
	len = readn( p_plugin_input_plugin_tcp_accepted->accepted_sock , comm_buf+1+sizeof(uint16_t) , filename_len ) ;
	if( len == -1 )
	{
		ERRORLOG( "recv filename failed , errno[%d]" , errno );
		return 1;
	}
	else if( len == 0 )
	{
		WARNLOG( "remote socket closed on recv filename" );
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
		return 0;
	}
	
	return 0;
}

funcBeforeReadLogpipeInput BeforeReadLogpipeInput_tcp_accepted ;
int BeforeReadLogpipeInput_tcp_accepted( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	return 0;
}

funcReadLogpipeInput ReadLogpipeInput_tcp_accepted ;
int ReadLogpipeInput_tcp_accepted( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context , uint32_t *p_block_len , char *block_buf , int block_bufsize )
{
	struct LogpipeInputPlugin_tcp_accepted	*p_plugin_input_plugin_tcp_accepted = (struct LogpipeInputPlugin_tcp_accepted *)p_context ;
	uint32_t				block_len_htonl ;
	int					len ;
	
	len = readn( p_plugin_input_plugin_tcp_accepted->accepted_sock , & block_len_htonl , sizeof(uint32_t) ) ;
	if( len == -1 )
	{
		ERRORLOG( "recv block length failed , errno[%d]" , errno );
		return 1;
	}
	else if( len == 0 )
	{
		WARNLOG( "remote socket closed on recv block length" );
		return 1;
	}
	else
	{
		INFOLOG( "recv filename from socket ok , [%d]bytes" , sizeof(uint32_t) )
		DEBUGHEXLOG( (char*) & block_len_htonl , len , NULL )
	}
	
	(*p_block_len) = ntohl( block_len_htonl ) ;
	if( (*p_block_len) == 0 )
		return LOGPIPE_READ_END_OF_INPUT;
	if( (*p_block_len) > block_bufsize-1 )
	{
		ERRORLOG( "block length[%d] too lone" , (*p_block_len) );
		return 1;
	}
	
	len = readn( p_plugin_input_plugin_tcp_accepted->accepted_sock , block_buf , (*p_block_len) ) ;
	if( len == -1 )
	{
		ERRORLOG( "recv filename failed , errno[%d]" , errno );
		return 1;
	}
	else if( len == 0 )
	{
		WARNLOG( "remote socket closed on recv filename" );
		return 1;
	}
	else
	{
		INFOLOG( "recv filename from socket ok , [%d]bytes" , (*p_block_len) )
		DEBUGHEXLOG( block_buf , len , NULL )
	}
	
	return 0;
}

funcAfterReadLogpipeInput AfterReadLogpipeInput_tcp_accepted ;
int AfterReadLogpipeInput_tcp_accepted( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	return 0;
}

funcCleanLogpipeInputPlugin CleanLogpipeInputPlugin_tcp_accepted ;
int CleanLogpipeInputPlugin_tcp_accepted( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	struct LogpipeInputPlugin_tcp_accepted	*p_plugin_input_plugin_tcp_accepted = (struct LogpipeInputPlugin_tcp_accepted *)p_context ;
	
	if( p_plugin_input_plugin_tcp_accepted->accepted_sock >= 0 )
	{
		INFOLOG( "close accepted sock" )
		close( p_plugin_input_plugin_tcp_accepted->accepted_sock ); p_plugin_input_plugin_tcp_accepted->accepted_sock = -1 ;
	}
	
	INFOLOG( "free p_plugin_input_plugin_tcp_accepted" )
	free( p_plugin_input_plugin_tcp_accepted );
	
	return 0;
}

/****************** 以上为子环境 ******************/

struct LogpipeInputPlugin_tcp
{
	char			*ip ;
	int			port ;
	
	struct sockaddr_in   	 listen_addr ;
	int			listen_sock ;
} ;

funcInitLogpipeInputPlugin InitLogpipeInputPlugin ;
int InitLogpipeInputPlugin( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , struct LogpipePluginConfigItem *p_plugin_config_items , void **pp_context , int *p_fd )
{
	struct LogpipeInputPlugin_tcp	*p_plugin_env = NULL ;
	char				*p = NULL ;
	
	int				nret = 0 ;
	
	p_plugin_env = (struct LogpipeInputPlugin_tcp *)malloc( sizeof(struct LogpipeInputPlugin_tcp) ) ;
	if( p_plugin_env == NULL )
	{
		ERRORLOG( "malloc failed , errno[%d]" , errno );
		return -1;
	}
	memset( p_plugin_env , 0x00 , sizeof(struct LogpipeInputPlugin_tcp) );
	
	/* 解析插件配置 */
	p_plugin_env->ip = QueryPluginConfigItem( p_plugin_config_items , "ip" ) ;
	INFOLOG( "ip[%s]" , p_plugin_env->ip )
	
	p = QueryPluginConfigItem( p_plugin_config_items , "port" ) ;
	if( p )
		p_plugin_env->port = atoi(p) ;
	else
		p_plugin_env->port = 0 ;
	INFOLOG( "port[%d]" , p_plugin_env->port )
	
	/* 初始化插件环境内部数据 */
	memset( & (p_plugin_env->listen_addr) , 0x00 , sizeof(struct sockaddr_in) );
	p_plugin_env->listen_addr.sin_family = AF_INET ;
	if( p_plugin_env->ip[0] == '\0' )
		p_plugin_env->listen_addr.sin_addr.s_addr = INADDR_ANY ;
	else
		p_plugin_env->listen_addr.sin_addr.s_addr = inet_addr(p_plugin_env->ip) ;
	p_plugin_env->listen_addr.sin_port = htons( (unsigned short)(p_plugin_env->port) );
	
	/* 创建侦听端 */
	p_plugin_env->listen_sock = socket( AF_INET , SOCK_STREAM , IPPROTO_TCP ) ;
	if( p_plugin_env->listen_sock == -1 )
	{
		ERRORLOG( "socket failed , errno[%d]" , errno );
		return -1;
	}
	
	/* 设置套接字选项 */
	{
		int	onoff = 1 ;
		setsockopt( p_plugin_env->listen_sock , SOL_SOCKET , SO_REUSEADDR , (void *) & onoff , sizeof(int) );
	}
	
	{
		int	onoff = 1 ;
		setsockopt( p_plugin_env->listen_sock , IPPROTO_TCP , TCP_NODELAY , (void*) & onoff , sizeof(int) );
	}
	
	/* 绑定套接字到侦听端口 */
	nret = bind( p_plugin_env->listen_sock , (struct sockaddr *) & (p_plugin_env->listen_addr) , sizeof(struct sockaddr) ) ;
	if( nret == -1 )
	{
		ERRORLOG( "bind[%s:%d][%d] failed , errno[%d]" , p_plugin_env->ip , p_plugin_env->port , p_plugin_env->listen_sock , errno );
		return -1;
	}
	
	/* 处于侦听状态了 */
	nret = listen( p_plugin_env->listen_sock , 10240 ) ;
	if( nret == -1 )
	{
		ERRORLOG( "listen[%s:%d][%d] failed , errno[%d]" , p_plugin_env->ip , p_plugin_env->port , p_plugin_env->listen_sock , errno );
		return -1;
	}
	else
	{
		INFOLOG( "listen[%s:%d][%d] ok" , p_plugin_env->ip , p_plugin_env->port , p_plugin_env->listen_sock )
	}
	
	/* 设置插件环境上下文 */
	(*pp_context) = p_plugin_env ;
	(*p_fd) = p_plugin_env->listen_sock ;
	
	return 0;
}

funcOnLogpipeInputEvent OnLogpipeInputEvent ;
int OnLogpipeInputEvent( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	struct LogpipeInputPlugin_tcp		*p_plugin_env = (struct LogpipeInputPlugin_tcp *)p_context ;
	struct LogpipeInputPlugin_tcp_accepted	*p_logpipe_input_plugin_tcp_accepted = NULL ;
	socklen_t				accept_addr_len ;
	
	/* 申请内存以存放客户端连接会话 */
	p_logpipe_input_plugin_tcp_accepted = (struct LogpipeInputPlugin_tcp_accepted *)malloc( sizeof(struct LogpipeInputPlugin_tcp_accepted) ) ;
	if( p_logpipe_input_plugin_tcp_accepted == NULL )
	{
		ERRORLOG( "malloc failed , errno[%d]" , errno )
		return 1;
	}
	memset( p_logpipe_input_plugin_tcp_accepted , 0x00 , sizeof(struct LogpipeInputPlugin_tcp_accepted) );
	
	/* 接受新连接 */
	accept_addr_len = sizeof(struct sockaddr) ;
	p_logpipe_input_plugin_tcp_accepted->accepted_sock = accept( p_plugin_env->listen_sock , (struct sockaddr *) & (p_logpipe_input_plugin_tcp_accepted->accepted_addr) , & accept_addr_len ) ;
	if( p_logpipe_input_plugin_tcp_accepted->accepted_sock == -1 )
	{
		ERRORLOG( "accept failed , errno[%d]" , errno )
		free( p_logpipe_input_plugin_tcp_accepted );
		return 1;
	}
	
	/* 设置套接字选项 */
	{
		int	onoff = 1 ;
		setsockopt( p_logpipe_input_plugin_tcp_accepted->accepted_sock , SOL_SOCKET , SO_REUSEADDR , (void *) & onoff , sizeof(int) );
	}
	
	{
		int	onoff = 1 ;
		setsockopt( p_logpipe_input_plugin_tcp_accepted->accepted_sock , IPPROTO_TCP , TCP_NODELAY , (void*) & onoff , sizeof(int) );
	}
	
	DEBUGLOG( "[%d]accept[%d] ok" , p_plugin_env->listen_sock , p_logpipe_input_plugin_tcp_accepted->accepted_sock )
	
	p_logpipe_input_plugin = AddLogpipeInputPlugin( p_env , "accepted_socket" , & OnLogpipeInputEvent_tcp_accepted , & BeforeReadLogpipeInput_tcp_accepted , & ReadLogpipeInput_tcp_accepted , & AfterReadLogpipeInput_tcp_accepted , & CleanLogpipeInputPlugin_tcp_accepted , p_logpipe_input_plugin_tcp_accepted->accepted_sock , p_logpipe_input_plugin_tcp_accepted ) ;
	if( p_logpipe_input_plugin == NULL )
	{
		close( p_logpipe_input_plugin_tcp_accepted->accepted_sock );
		free( p_logpipe_input_plugin_tcp_accepted );
		ERRORLOG( "AddLogpipeInputPlugin failed" )
		return 0;
	}
	
	return 0;
}

funcBeforeReadLogpipeInput BeforeReadLogpipeInput ;
int BeforeReadLogpipeInput( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	return 0;
}

funcReadLogpipeInput ReadLogpipeInput ;
int ReadLogpipeInput( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context , uint32_t *p_block_len , char *block_buf , int block_bufsize )
{
	return 0;
}

funcAfterReadLogpipeInput AfterReadLogpipeInput ;
int AfterReadLogpipeInput( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	return 0;
}

funcCleanLogpipeInputPlugin CleanLogpipeInputPlugin ;
int CleanLogpipeInputPlugin( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	struct LogpipeInputPlugin_tcp	*p_plugin_env = (struct LogpipeInputPlugin_tcp *)p_context ;
	
	if( p_plugin_env->listen_sock >= 0 )
	{
		INFOLOG( "close listen sock" )
		close( p_plugin_env->listen_sock ); p_plugin_env->listen_sock = -1 ;
	}
	
	INFOLOG( "free p_plugin_env" )
	free( p_plugin_env );
	
	return 0;
}

