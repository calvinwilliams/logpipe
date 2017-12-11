#include "logpipe_in.h"

#define MAX_EPOLL_EVENTS	10000

int worker( struct LogpipeEnv *p_env )
{
	struct epoll_event	events[ MAX_EPOLL_EVENTS ] ;
	int			epoll_nfds ;
	int			i ;
	struct epoll_event	*p_event = NULL ;
	struct Session		*p_session = NULL ;
	struct InotifySession	*p_inotify_session = NULL ;
	struct AcceptedSession	*p_accepted_session = NULL ;
	struct ListenSession	*p_listen_session = NULL ;
	struct ForwardSession	*p_forward_session = NULL ;
	
	int			nret = 0 ;
	
	signal( SIGTERM , SIG_DFL );
	
	SetLogFile( p_env->conf.log.log_file );
	SetLogLevel( p_env->log_level );
	
	/* 连接所有转发端 */
	list_for_each_entry( p_forward_session , & (p_env->forward_session_list.this_node) , struct ForwardSession , this_node )
	{
		p_forward_session->forward_sock = -1 ;
		nret = ConnectForwardSocket( p_env , p_forward_session ) ;
		if( nret < 0 )
		{
			ERRORLOG( "ConnectForwardSocket[%s:%d] failed , errno[%d]" , p_forward_session->forward_ip , p_forward_session->forward_port , errno );
			return -1;
		}
	}
	
	while(1)
	{
		/* 等待epoll事件，或者1秒超时 */
		INFOLOG( "epoll_wait[%d] ..." , p_env->epoll_fd );
		memset( events , 0x00 , sizeof(events) );
		epoll_nfds = epoll_wait( p_env->epoll_fd , events , MAX_EPOLL_EVENTS , -1 ) ;
		if( epoll_nfds == -1 )
		{
			if( errno == EINTR )
			{
				INFOLOG( "epoll_wait[%d] interrupted" , p_env->epoll_fd )
			}
			else
			{
				ERRORLOG( "epoll_wait[%d] failed , errno[%d]" , p_env->epoll_fd , errno )
				return -1;
			}
		}
		else
		{
			INFOLOG( "epoll_wait[%d] return[%d]events" , p_env->epoll_fd , epoll_nfds );
		}
		
		for( i = 0 , p_event = events ; i < epoll_nfds ; i++ , p_event++ )
		{
			p_session = (struct Session *)(p_event->data.ptr) ;
			if( p_session->session_type == LOGPIPE_SESSION_TYPE_INOTIFY )
			{
				p_inotify_session = (struct InotifySession *)p_session ;
				
				/* 可读事件 */
				if( p_event->events & EPOLLIN )
				{
					nret = OnInotifyHandler( p_env , p_inotify_session ) ;
					if( nret < 0 )
					{
						FATALLOG( "OnInotifyHandler failed[%d]" , nret )
						return -1;
					}
					else if( nret > 0 )
					{
						INFOLOG( "OnInotifyHandler return[%d]" , nret )
					}
					else
					{
						DEBUGLOG( "OnInotifyHandler ok" )
					}
				}
				/* 其它事件 */
				else
				{
					FATALLOG( "Unknow inotify session event[0x%X]" , p_event->events )
					exit(4);
				}
			}
			else if( p_session->session_type == LOGPIPE_SESSION_TYPE_LISTEN )
			{
				p_listen_session = (struct ListenSession *)p_session ;
				
				/* 可读事件 */
				if( p_event->events & EPOLLIN )
				{
					nret = OnAcceptingSocket( p_env , p_listen_session ) ;
					if( nret < 0 )
					{
						FATALLOG( "OnAcceptingSocket failed[%d]" , nret )
						return -1;
					}
					else if( nret > 0 )
					{
						INFOLOG( "OnAcceptingSocket return[%d]" , nret )
					}
					else
					{
						DEBUGLOG( "OnAcceptingSocket ok" )
					}
				}
				/* 出错事件 */
				else if( ( p_event->events & EPOLLERR ) || ( p_event->events & EPOLLHUP ) )
				{
					FATALLOG( "listen session err or hup event[0x%X]" , p_event->events )
					return -1;
				}
				/* 其它事件 */
				else
				{
					FATALLOG( "Unknow listen session event[0x%X]" , p_event->events )
					return -1;
				}
			}
			else if( p_session->session_type == LOGPIPE_SESSION_TYPE_ACCEPTED )
			{
				p_accepted_session = (struct AcceptedSession *)p_session ;
				
				/* 可读事件 */
				if( p_event->events & EPOLLIN )
				{
					nret = OnReceivingSocket( p_env , p_accepted_session ) ;
					if( nret < 0 )
					{
						FATALLOG( "OnReceivingSocket failed[%d]" , nret )
						return -1;
					}
					else if( nret > 0 )
					{
						INFOLOG( "OnReceivingSocket return[%d]" , nret )
						OnClosingSocket( p_env , p_accepted_session );
					}
					else
					{
						DEBUGLOG( "OnReceivingSocket ok" )
					}
				}
				/* 可写事件 */
				else if( p_event->events & EPOLLOUT )
				{
					FATALLOG( "unexpect accepted session EPOLLOUT event" )
					OnClosingSocket( p_env , p_accepted_session );
				}
				/* 出错事件 */
				else if( ( p_event->events & EPOLLERR ) || ( p_event->events & EPOLLHUP ) )
				{
					FATALLOG( "accepted session err or hup event[0x%X]" , p_event->events )
					OnClosingSocket( p_env , p_accepted_session );
				}
				/* 其它事件 */
				else
				{
					FATALLOG( "Unknow accepted session event[0x%X]" , p_event->events )
					return -1;
				}
			}
			else
			{
				FATALLOG( "Unknow session type[%c]" , p_session->session_type )
				exit(4);
			}
		}
	}
	
	return 0;
}

