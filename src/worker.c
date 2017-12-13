#include "logpipe_in.h"

#define MAX_EPOLL_EVENTS	10000

int worker( struct LogpipeEnv *p_env )
{
	int				quit_flag ;
	
	struct epoll_event		event ;
	struct epoll_event		events[ MAX_EPOLL_EVENTS ] ;
	int				epoll_nfds ;
	int				i ;
	struct epoll_event		*p_event = NULL ;
	struct LogpipeInputPlugin	*p_logpipe_input_plugin = NULL ;
	
	int				nret = 0 ;
	
	signal( SIGTERM , SIG_DFL );
	
	SetLogFile( p_env->log_file );
	SetLogLevel( p_env->log_level );
	
	/* 第二次初始化环境 */
	nret = InitEnvironment2( p_env ) ;
	if( nret )
	{
		ERRORLOG( "InitEnvironment2 failed[%d]" , nret );
		return -1;
	}
	
	/* 创建epoll */
	p_env->epoll_fd = epoll_create( 1024 ) ;
	if( p_env->epoll_fd == -1 )
	{
		ERRORLOG( "epoll_create failed , errno[%d]" , errno );
		return -1;
	}
	else
	{
		INFOLOG( "epoll_create ok" )
	}
	
	/* 所有输入端描述字加入epoll */
	list_for_each_entry( p_logpipe_input_plugin , & (p_env->logpipe_input_plugins_list.this_node) , struct LogpipeInputPlugin , this_node )
	{
		if( p_logpipe_input_plugin->fd >= 0 )
		{
			/* 加入订阅可读事件到epoll */
			memset( & event , 0x00 , sizeof(struct epoll_event) );
			event.events = EPOLLIN | EPOLLERR ;
			event.data.ptr = p_logpipe_input_plugin ;
			nret = epoll_ctl( p_env->epoll_fd , EPOLL_CTL_ADD , p_logpipe_input_plugin->fd , & event ) ;
			if( nret == -1 )
			{
				ERRORLOG( "epoll_ctl[%d] add input plugin fd[%d] EPOLLIN failed , errno[%d]" , p_env->epoll_fd , p_logpipe_input_plugin->fd , errno );
				return -1;
			}
		}
	}
	
	/* 管道描述字加入epoll */
	memset( & event , 0x00 , sizeof(struct epoll_event) );
	event.events = EPOLLIN | EPOLLERR ;
	event.data.ptr = p_env->quit_pipe ;
	nret = epoll_ctl( p_env->epoll_fd , EPOLL_CTL_ADD , p_env->quit_pipe[0] , & event ) ;
	if( nret == -1 )
	{
		ERRORLOG( "epoll_ctl[%d] add quit pipe fd[%d] failed , errno[%d]" , p_env->epoll_fd , p_env->quit_pipe[0] , errno );
		return -1;
	}
	else
	{
		INFOLOG( "epoll_ctl[%d] add quit pipe fd[%d] ok" , p_env->epoll_fd , p_env->quit_pipe[0] );
	}
	
	/* 工作主循环 */
	quit_flag = 0 ;
	while( quit_flag == 0 )
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
			if( p_event->data.ptr == p_env->quit_pipe )
			{
				quit_flag = 1 ;
			}
			else
			{
				char		so_filename[ sizeof(((struct LogpipeInputPlugin *)0)->so_filename) ] ;
				
				p_logpipe_input_plugin = (struct LogpipeInputPlugin *)(p_event->data.ptr) ;
				strcpy( so_filename , p_logpipe_input_plugin->so_filename );
				
				/* 可读事件 */
				if( p_event->events & EPOLLIN )
				{
					nret = p_logpipe_input_plugin->pfuncOnLogpipeInputEvent( p_env , p_logpipe_input_plugin , p_logpipe_input_plugin->context ) ;
					if( nret < 0 )
					{
						FATALLOG( "[%s]->pfuncOnLogpipeInputEvent failed[%d]" , so_filename , nret )
						return -1;
					}
					else if( nret > 0 )
					{
						INFOLOG( "[%s]->pfuncOnLogpipeInputEvent return[%d]" , so_filename , nret )
					}
					else
					{
						DEBUGLOG( "[%s]->pfuncOnLogpipeInputEvent ok" , so_filename )
					}
				}
				/* 其它事件 */
				else
				{
					FATALLOG( "[%s]->pfuncOnLogpipeInputEvent unknow event[0x%X]" , so_filename , p_event->events )
					return -1;
				}
			}
		}
	}
	
	/* 销毁epoll */
	INFOLOG( " close epoll" )
	close( p_env->epoll_fd );
	
	return 0;
}

