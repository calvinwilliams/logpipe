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
	char			*magic = NULL ;
	uint16_t		*filename_len_htons = NULL ;
	char			comm_buf[ 1 + sizeof(uint16_t) + PATH_MAX ] ;
	uint16_t		filename_len ;
	int			len ;
	
	int			nret = 0 ;
	
	len = readn( p_accepted_session->accepted_sock , comm_buf , 1+sizeof(uint16_t) ) ;
	if( len == -1 )
	{
		ERRORLOG( "recv comm magic and filename len failed , errno[%d]" , errno );
		return 1;
	}
	else if( len == 0 )
	{
		INFOLOG( "remote socket closed on recv comm magic and filename len" );
		return 1;
	}
	else
	{
		INFOLOG( "recv comm magic and filename len ok , [%d]bytes" , len )
		DEBUGHEXLOG( comm_buf , len , "comm magic and filename len [%d]bytes" , len )
	}
	
	magic = comm_buf ;
	if( (*magic) != LOGPIPE_COMM_MAGIC )
	{
		ERRORLOG( "magic[%c][%d] invalid" , (*magic) , (*magic) );
		return 1;
	}
	
	filename_len_htons = (uint16_t*)(comm_buf+1) ;
	filename_len = ntohs(*filename_len_htons) ;
	
	if( filename_len > PATH_MAX )
	{
		ERRORLOG( "filename length[%d] too long" , filename_len );
		return -1;
	}
	
	len = readn( p_accepted_session->accepted_sock , comm_buf+1+sizeof(uint16_t) , filename_len ) ;
	if( len == -1 )
	{
		ERRORLOG( "recv filename failed , errno[%d]" , errno );
		return 1;
	}
	else if( len == 0 )
	{
		INFOLOG( "remote socket closed on recv filename" );
		return 1;
	}
	else
	{
		INFOLOG( "recv filename from socket ok , [%d]bytes" , len )
		DEBUGHEXLOG( comm_buf+1+sizeof(uint16_t) , len , "filename [%d]bytes" , len )
	}
	
	/* 导出所有输出端 */
	nret = ToOutputs( p_env , comm_buf , 1+sizeof(uint16_t)+filename_len , comm_buf+1+sizeof(uint16_t) , filename_len , p_accepted_session->accepted_sock , -1 ) ;
	if( nret )
	{
		ERRORLOG( "ToOutputs failed[%d]" , nret )
	}
	
	return 0;
}

