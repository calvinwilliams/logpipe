#include "logpipe_in.h"

/* 接受新连接 */
int OnAcceptingSocket( struct LogPipeEnv *p_env , struct ListenSession *p_listen_session )
{
	struct AcceptedSession	*p_accepted_session = NULL ;
	socklen_t		accept_addr_len ;
	
	struct epoll_event	event ;
	
	int			nret = 0 ;
	
	/* 申请内存以存放客户端连接会话 */
	p_accepted_session = (struct AcceptedSession *)malloc( sizeof(struct AcceptedSession) ) ;
	if( p_accepted_session == NULL )
	{
		ERRORLOG( "malloc failed , errno[%d]" , errno )
		return 1;
	}
	memset( p_accepted_session , 0x00 , sizeof(struct AcceptedSession) );
	
	p_accepted_session->session_type = LOGPIPE_SESSION_TYPE_ACCEPTED ;
	p_accepted_session->p_listen_session = p_listen_session ;
	
	/* 接受新连接 */
	accept_addr_len = sizeof(struct sockaddr) ;
	p_accepted_session->accepted_sock = accept( p_listen_session->listen_sock , (struct sockaddr *) & (p_accepted_session->accepted_addr) , & accept_addr_len ) ;
	if( p_accepted_session->accepted_sock == -1 )
	{
		ERRORLOG( "accept failed , errno[%d]" , errno )
		free( p_accepted_session );
		return 1;
	}
	
	/* 设置套接字选项 */
	{
		int	onoff = 1 ;
		setsockopt( p_accepted_session->accepted_sock , SOL_SOCKET , SO_REUSEADDR , (void *) & onoff , sizeof(int) );
	}
	
	{
		int	onoff = 1 ;
		setsockopt( p_accepted_session->accepted_sock , IPPROTO_TCP , TCP_NODELAY , (void*) & onoff , sizeof(int) );
	}
	
	DEBUGLOG( "[%d]accept ok[%d] session[%p]" , p_listen_session->listen_sock , p_accepted_session->accepted_sock , p_accepted_session )
	
	/* 加入新连接套接字到epoll */
	memset( & event , 0x00 , sizeof(struct epoll_event) );
	event.events = EPOLLIN | EPOLLERR ;
	event.data.ptr = p_accepted_session ;
	nret = epoll_ctl( p_env->epoll_fd , EPOLL_CTL_ADD , p_accepted_session->accepted_sock , & event ) ;
	if( nret == -1 )
	{
		ERRORLOG( "epoll_ctl[%d] add[%d] failed , errno[%d]" , p_env->epoll_fd , p_accepted_session->accepted_sock , errno )
		close( p_accepted_session->accepted_sock );
		free( p_accepted_session );
		return 1;
	}
	else
	{
		DEBUGLOG( "epoll_ctl[%d] add[%d] ok" , p_env->epoll_fd , p_accepted_session->accepted_sock )
	}
	
	list_add_tail( & (p_accepted_session->this_node) , & (p_listen_session->accepted_session_list.this_node) );
	
	return 0;
}

/* 关闭连接 */
void OnClosingSocket( struct LogPipeEnv *p_env , struct AcceptedSession *p_accepted_session )
{
	if( p_accepted_session )
	{
		DEBUGLOG( "close socket[%d] session[%p]" , p_accepted_session->accepted_sock , p_accepted_session )
		epoll_ctl( p_env->epoll_fd , EPOLL_CTL_DEL , p_accepted_session->accepted_sock , NULL );
		close( p_accepted_session->accepted_sock );
		list_del( & (p_accepted_session->this_node) );
		free( p_accepted_session );
	}
	
	return;
}

/* 通讯接收数据 */
int OnReceivingSocket( struct LogPipeEnv *p_env , struct AcceptedSession *p_accepted_session )
{
	uint32_t		comm_head_len_ntohl ;
	uint32_t		*comm_head_len_htonl = NULL ;
	char			*magic = NULL ;
	uint16_t		*filename_len_htons = NULL ;
	char			comm_buffer[ sizeof(uint32_t) + 1 + sizeof(uint16_t) ] ;
	
	uint16_t		filename_len_ntohs ;
	char			filename[ PATH_MAX + 1 ] ;
	int			appender_len ;
	int			len ;
	
	int			nret = 0 ;
	
	DEBUGLOG( "receiving comm head and magic and filename len ..." , p_accepted_session->accepted_sock )
	len = readn( p_accepted_session->accepted_sock , comm_buffer , sizeof(comm_buffer) ) ;
	if( len == -1 )
	{
		ERRORLOG( "receiving comm head and magic and filename len failed , errno[%d]" , errno );
		return 1;
	}
	else if( len == 0 )
	{
		INFOLOG( "remote socket closed on receiving comm head and magic and filename len" );
		return 1;
	}
	else
	{
		DEBUGHEXLOG( comm_buffer , len , "received comm head and magic and filename len [%d]bytes" , len )
	}
	
	comm_head_len_htonl = (uint32_t*)comm_buffer ;
	comm_head_len_ntohl = ntohl( (*comm_head_len_htonl) ) ;
	
	magic = comm_buffer + sizeof(uint32_t) ;
	if( magic[0] != LOGPIPE_COMM_MAGIC[0] )
	{
		ERRORLOG( "magic[%c][%d] invalid" , (*magic) , (*magic) );
		return 1;
	}
	
	filename_len_htons = (uint16_t*)(comm_buffer+sizeof(uint32_t)+1) ;
	filename_len_ntohs = ntohs(*filename_len_htons) ;
	
	if( filename_len_ntohs > PATH_MAX )
	{
		ERRORLOG( "filename length[%d] too long" , filename_len_ntohs );
		return -1;
	}
	
	DEBUGLOG( "receiving filename ..." , p_accepted_session->accepted_sock )
	memset( filename , 0x00 , sizeof(filename) );
	len = readn( p_accepted_session->accepted_sock , filename , filename_len_ntohs ) ;
	if( len == -1 )
	{
		ERRORLOG( "receiving filename failed , errno[%d]" , errno );
		return 1;
	}
	else if( len == 0 )
	{
		INFOLOG( "remote socket closed on receiving filename" );
		return 1;
	}
	else
	{
		DEBUGHEXLOG( filename , len , "received filename [%d]bytes" , len )
	}
	
	appender_len = comm_head_len_ntohl - 1 - sizeof(filename_len_ntohs) - filename_len_ntohs ;
	
	/* 导出所有输出端 */
	nret = ToOutputs( p_env , filename , filename_len_ntohs , p_accepted_session->accepted_sock , appender_len ) ;
	if( nret )
	{
		ERRORLOG( "ToOutputs failed[%d]" , nret )
	}
	
	return 0;
}

