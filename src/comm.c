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
		int	opts ;
		opts = fcntl( p_accepted_session->accepted_sock , F_GETFL ) ;
		opts |= O_NONBLOCK ;
		fcntl( p_accepted_session->accepted_sock , F_SETFL , opts ) ;
	}
	
	{
		int	onoff = 1 ;
		setsockopt( p_accepted_session->accepted_sock , SOL_SOCKET , SO_REUSEADDR , (void *) & onoff , sizeof(int) );
	}
	
	{
		int	onoff = 1 ;
		setsockopt( p_accepted_session->accepted_sock , IPPROTO_TCP , TCP_NODELAY , (void*) & onoff , sizeof(int) );
	}
	
	DEBUGLOG( "[%d]accept ok[%d] session[%p]" , p_listen_session->listen_sock , p_accepted_session->accepted_sock , p_accepted_session )
	
	/* 申请通讯接收缓冲区在会话结构中 */
	p_accepted_session->comm_buf = (char*)malloc( LOGPIPE_COMM_BUFFER_INIT_SIZE + 1 ) ;
	if( p_accepted_session->comm_buf == NULL )
	{
		ERRORLOG( "malloc failed , errno[%d]" , errno )
		free( p_accepted_session );
		return 1;
	}
	memset( p_accepted_session->comm_buf , 0x00 , LOGPIPE_COMM_BUFFER_INIT_SIZE + 1 );
	p_accepted_session->comm_buf_size = LOGPIPE_COMM_BUFFER_INIT_SIZE + 1 ;
	p_accepted_session->comm_data_len = 0 ;
	p_accepted_session->comm_body_len = 0 ;
	
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
		free( p_accepted_session->comm_buf );
		free( p_accepted_session );
	}
	
	return;
}

/* 通讯接收数据 */
int OnReceivingSocket( struct LogPipeEnv *p_env , struct AcceptedSession *p_accepted_session )
{
	int			len ;
	
	int			nret = 0 ;
	
	/* 如果通讯接收缓冲区满了，扩张该缓冲区 */
	if( p_accepted_session->comm_data_len == p_accepted_session->comm_buf_size-1 )
	{
		char	*tmp = NULL ;
		
		tmp = (char*)realloc( p_accepted_session->comm_buf , p_accepted_session->comm_buf_size+LOGPIPE_COMM_BUFFER_INCREASE_SIZE ) ;
		if( tmp == NULL )
		{
			ERRORLOG( "malloc failed , errno[%d]" , errno )
			return 1;
		}
		p_accepted_session->comm_buf = tmp ;
		p_accepted_session->comm_buf_size += LOGPIPE_COMM_BUFFER_INCREASE_SIZE ;
	}
	
	/* 非堵塞的读一把客户端连接套接字 */
	DEBUGLOG( "read [%d] bytes at most ..." , p_accepted_session->comm_buf_size-1-p_accepted_session->comm_data_len )
	len = read( p_accepted_session->accepted_sock , p_accepted_session->comm_buf+p_accepted_session->comm_data_len , p_accepted_session->comm_buf_size-1-p_accepted_session->comm_data_len ) ;
	if( len == -1 )
	{
		ERRORLOG( "recv failed[%d] , errno[%d]" , len , errno );
		return 1;
	}
	else if( len == 0 )
	{
		INFOLOG( "remote socket closed" );
		return 1;
	}
	else
	{
		DEBUGHEXLOG( p_accepted_session->comm_buf+p_accepted_session->comm_data_len , len , "recv [%d]bytes" , len )
		p_accepted_session->comm_data_len += len ;
	}
	
	/* 如果已接收到数据长度大于等于4字节 */
	while( p_accepted_session->comm_data_len >= sizeof(uint32_t) )
	{
		/* 如果4字节通讯头未解析，则解析之 */
		if( p_accepted_session->comm_body_len == 0 )
		{
			p_accepted_session->comm_body_len = ntohl( *(uint32_t*)(p_accepted_session->comm_buf) ) ;
			DEBUGLOG( "parse comm total length [%d]bytes" , p_accepted_session->comm_body_len )
			if( p_accepted_session->comm_body_len <= 0 )
			{
				ERRORLOG( "comm_body_len[%d] invalid" , p_accepted_session->comm_body_len )
				return 1;
			}
		}
		
		/* 如果已读到的通讯数据大于等于当前通讯头+通讯体 */
		if( p_accepted_session->comm_data_len >= sizeof(uint32_t) + p_accepted_session->comm_body_len )
		{
			char		bak ;
			
			/* 按字符串截断，交由应用层处理 */
			bak = p_accepted_session->comm_buf[sizeof(uint32_t)+p_accepted_session->comm_body_len] ;
			p_accepted_session->comm_buf[sizeof(uint32_t)+p_accepted_session->comm_body_len] = '\0' ;
			DEBUGHEXLOG( p_accepted_session->comm_buf , sizeof(uint32_t)+p_accepted_session->comm_body_len , "processing [%ld]bytes" , sizeof(uint32_t)+p_accepted_session->comm_body_len )
			nret = CommToOutput( p_env , p_accepted_session ) ;
			p_accepted_session->comm_buf[sizeof(uint32_t)+p_accepted_session->comm_body_len] = bak ;
			if( nret )
			{
				ERRORLOG( "CommToOutput failed[%d]" , nret )
				return 1;
			}
			
			/* 修正尾部粘包 */
			memmove( p_accepted_session->comm_buf , p_accepted_session->comm_buf+sizeof(uint32_t)+p_accepted_session->comm_body_len , p_accepted_session->comm_data_len-sizeof(uint32_t)-p_accepted_session->comm_body_len );
			p_accepted_session->comm_data_len -= sizeof(uint32_t)+p_accepted_session->comm_body_len ;
			p_accepted_session->comm_body_len = 0  ;
		}
		else
		{
			break;
		}
	}
	
	return 0;
}

