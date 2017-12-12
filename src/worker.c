#include "logpipe_in.h"

#define MAX_EPOLL_EVENTS	10000

int worker( struct LogpipeEnv *p_env )
{
	struct epoll_event		events[ MAX_EPOLL_EVENTS ] ;
	int				epoll_nfds ;
	int				i ;
	struct epoll_event		*p_event = NULL ;
	struct LogpipeInputPlugin	*p_logpipe_input_plugin = NULL ;
	
	int				nret = 0 ;
	
	signal( SIGTERM , SIG_DFL );
	
	SetLogFile( p_env->log_file );
	SetLogLevel( p_env->log_level );
	
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
			/* 可读事件 */
			if( p_event->events & EPOLLIN )
			{
				p_logpipe_input_plugin = (struct LogpipeInputPlugin *)(p_event->data.ptr) ;
				nret = p_logpipe_input_plugin->pfuncOnLogpipeInputEvent( p_env , p_logpipe_input_plugin , p_logpipe_input_plugin->context ) ;
				if( nret < 0 )
				{
					FATALLOG( "[%s]p_logpipe_input_plugin->pfuncOnLogpipeInputEvent failed[%d]" , p_logpipe_input_plugin->so_filename , nret )
					return -1;
				}
				else if( nret > 0 )
				{
					INFOLOG( "[%s]p_logpipe_input_plugin->pfuncOnLogpipeInputEvent return[%d]" , p_logpipe_input_plugin->so_filename , nret )
				}
				else
				{
					DEBUGLOG( "[%s]p_logpipe_input_plugin->pfuncOnLogpipeInputEvent ok" , p_logpipe_input_plugin->so_filename )
				}
			}
			/* 其它事件 */
			else
			{
				FATALLOG( "[%s]p_logpipe_input_plugin->pfuncOnLogpipeInputEvent unknow event[0x%X]" , p_logpipe_input_plugin->so_filename , p_event->events )
				return -1;
			}
		}
	}
	
	return 0;
}

