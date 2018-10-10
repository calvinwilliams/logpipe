/*
 * logpipe - Distribute log collector
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "logpipe_in.h"

/* 每轮最大可接收epoll事件数量 */
#define MAX_EPOLL_EVENTS	10000

/* 子进程主函数 */
int worker( struct LogpipeEnv *p_env )
{
	time_t			tt ;
	struct tm		stime ;
	struct tm		old_stime ;
	
	int			quit_flag ;
	
	struct epoll_event	event ;
	struct epoll_event	events[ MAX_EPOLL_EVENTS ] ;
	int			epoll_nfds ;
	int			timeout ;
	int			i ;
	struct epoll_event	*p_event = NULL ;
	
	int			nret = 0 ;
	
	/* 清除TERM信号回调函数设置 */
	signal( SIGTERM , SIG_DFL );
	
	/* 设置子进程日志文件名 */
	time( & tt );
	memset( & old_stime , 0x00 , sizeof(struct tm) );
	localtime_r( & tt , & old_stime );
	SetLogcFile( "%s.%d" , p_env->log_file , old_stime.tm_hour );
	SetLogcLevel( p_env->log_level );
	
	/* 创建事件总线 */
	p_env->epoll_fd = epoll_create( 1024 ) ;
	if( p_env->epoll_fd == -1 )
	{
		ERRORLOGC( "epoll_create failed , errno[%d]" , errno )
		return -1;
	}
	else
	{
		INFOLOGC( "epoll_create ok , epoll_fd[%d]" , p_env->epoll_fd )
	}
	
	/* 初始化插件环境 */
	nret = InitEnvironment( p_env ) ;
	if( nret )
	{
		ERRORLOGC( "InitEnvironment failed[%d]" , nret )
		return -1;
	}
	else
	{
		INFOLOGC( "InitEnvironment ok" )
	}
	
	/* 管道描述字加入epoll */
	memset( & event , 0x00 , sizeof(struct epoll_event) );
	event.events = EPOLLIN | EPOLLERR ;
	event.data.ptr = p_env->quit_pipe ;
	nret = epoll_ctl( p_env->epoll_fd , EPOLL_CTL_ADD , p_env->quit_pipe[0] , & event ) ;
	if( nret == -1 )
	{
		ERRORLOGC( "epoll_ctl[%d] add quit pipe fd[%d] failed , errno[%d]" , p_env->epoll_fd , p_env->quit_pipe[0] , errno )
		return -1;
	}
	else
	{
		INFOLOGC( "epoll_ctl[%d] add quit pipe fd[%d] ok" , p_env->epoll_fd , p_env->quit_pipe[0] )
	}
	
#if 0
	/* 创建内部状态输出有名管道 */
	memset( p_env->logpipe_fifo_path_filename , 0x00 , sizeof(p_env->logpipe_fifo_path_filename) );
	snprintf( p_env->logpipe_fifo_path_filename , sizeof(p_env->logpipe_fifo_path_filename)-1 , "%s/etc/logpipe.fifo" , getenv("HOME") );
	nret = CreateLogpipeFifo( p_env ) ;
	if( nret )
	{
		ERRORLOGC( "CreateLogpipeFifo failed[%d]" , nret )
		return -1;
	}
	else
	{
		INFOLOGC( "CreateLogpipeFifo ok" )
	}
#endif
	
	/* 工作主循环 */
	quit_flag = 0 ;
	while( quit_flag == 0 )
	{
		/* 如果当前时点与上一次不一致，则切换日志文件 */
		time( & tt );
		memset( & stime , 0x00 , sizeof(struct tm) );
		localtime_r( & tt , & stime );
		if( stime.tm_hour != old_stime.tm_hour )
		{
			SetLogcFile( "%s.%d" , p_env->log_file , stime.tm_hour );
			unlink( GetLogcFilePtr() );
			memcpy( & old_stime , & stime , sizeof(struct tm) );
		}
		
		/* 等待epoll事件，或者1秒超时 */
		INFOLOGC( "epoll_wait[%d] ..." , p_env->epoll_fd )
		memset( events , 0x00 , sizeof(events) );
		if( p_env->idle_processing_flag )
			timeout = 1000 ;
		else
			timeout = -1 ;
		epoll_nfds = epoll_wait( p_env->epoll_fd , events , MAX_EPOLL_EVENTS , timeout ) ;
		if( epoll_nfds == -1 )
		{
			if( errno == EINTR )
			{
				INFOLOGC( "epoll_wait[%d] interrupted" , p_env->epoll_fd )
			}
			else
			{
				ERRORLOGC( "epoll_wait[%d] failed , errno[%d]" , p_env->epoll_fd , errno )
				return -1;
			}
		}
		else if( epoll_nfds == 0 )
		{
			nret = ProcessOnIdle( p_env ) ;
			if( nret )
			{
				ERRORLOGC( "ProcessOnIdle failed[%d] , errno[%d]" , nret , errno )
				return -1;
			}
			else
			{
				DEBUGLOGC( "ProcessOnIdle ok" )
			}
		}
		else
		{
			INFOLOGC( "epoll_wait[%d] return[%d]events" , p_env->epoll_fd , epoll_nfds )
		}
		
		/* 循环处理所有epoll事件 */
		for( i = 0 , p_event = events ; i < epoll_nfds ; i++ , p_event++ )
		{
			/* 如果是父子进程命令管道 */
			if( p_event->data.ptr == p_env->quit_pipe )
			{
				DEBUGLOGC( "p_event->data.ptr[%p] quit_pipe" , p_event->data.ptr )
				quit_flag = 1 ;
			}
#if 0
			else if( p_event->data.ptr == & (p_env->logpipe_fifo_inotify_fd) )
			{
				/* 处理内部状态输出有名管道事件 */
				nret = ProcessLogpipeFifoEvents( p_env ) ;
				if( nret )
				{
					ERRORLOGC( "ProcessLogpipeFifoEvents failed[%d]\n" , nret )
					return -1;
				}
				else
				{
					DEBUGLOGC( "ProcessLogpipeFifoEvents ok\n" )
				}
			}
#endif
			else
			{
				struct LogpipePlugin	*p_logpipe_plugin = NULL ;
				
				p_logpipe_plugin = (struct LogpipePlugin *)(p_event->data.ptr) ;
				/* 如果是输入插件 */
				if( p_logpipe_plugin->type == LOGPIPE_PLUGIN_TYPE_INPUT )
				{
					struct LogpipeInputPlugin	*p_logpipe_input_plugin = NULL ;
					char				so_filename[ sizeof(((struct LogpipeInputPlugin *)0)->so_filename) ] ;
					
					DEBUGLOGC( "p_event->data.ptr[%p] p_logpipe_input_plugin" , p_event->data.ptr )
					
					p_logpipe_input_plugin = (struct LogpipeInputPlugin *)(p_event->data.ptr) ;
					strcpy( so_filename , p_logpipe_input_plugin->so_filename );
					
					/* 可读事件 */
					if( p_event->events & EPOLLIN )
					{
						/* 调用输入插件事件回调函数 */
						nret = p_logpipe_input_plugin->pfuncOnInputPluginEvent( p_env , p_logpipe_input_plugin , p_logpipe_input_plugin->context ) ;
						if( nret < 0 )
						{
							FATALLOGC( "[%s]->pfuncOnInputPluginEvent failed[%d]" , so_filename , nret )
							return -1;
						}
						else if( nret > 0 )
						{
							WARNLOGC( "[%s]->pfuncOnInputPluginEvent return[%d]" , so_filename , nret )
						}
						else
						{
							DEBUGLOGC( "[%s]->pfuncOnInputPluginEvent ok" , so_filename )
						}
					}
					/* 其它事件 */
					else
					{
						FATALLOGC( "[%s]->pfuncOnInputPluginEvent unknow event[0x%X]" , so_filename , p_event->events )
						return -1;
					}
				}
				/* 如果是输出插件 */
				else if( p_logpipe_plugin->type == LOGPIPE_PLUGIN_TYPE_OUTPUT )
				{
					struct LogpipeOutputPlugin	*p_logpipe_output_plugin = NULL ;
					char				so_filename[ sizeof(((struct LogpipeOutputPlugin *)0)->so_filename) ] ;
					
					DEBUGLOGC( "p_event->data.ptr[%p] p_logpipe_output_plugin" , p_event->data.ptr )
					
					p_logpipe_output_plugin = (struct LogpipeOutputPlugin *)(p_event->data.ptr) ;
					strcpy( so_filename , p_logpipe_output_plugin->so_filename );
					
					/* 可读事件 */
					if( p_event->events & EPOLLIN )
					{
						/* 调用输出插件事件回调函数 */
						nret = p_logpipe_output_plugin->pfuncOnOutputPluginEvent( p_env , p_logpipe_output_plugin , p_logpipe_output_plugin->context ) ;
						if( nret < 0 )
						{
							FATALLOGC( "[%s]->pfuncOnOutputPluginEvent failed[%d]" , so_filename , nret )
							return -1;
						}
						else if( nret > 0 )
						{
							WARNLOGC( "[%s]->pfuncOnOutputPluginEvent return[%d]" , so_filename , nret )
						}
						else
						{
							DEBUGLOGC( "[%s]->pfuncOnOutputPluginEvent ok" , so_filename )
						}
					}
					/* 其它事件 */
					else
					{
						FATALLOGC( "[%s]->pfuncOnOutputPluginEvent unknow event[0x%X]" , so_filename , p_event->events )
						return -1;
					}
				}
				else
				{
					FATALLOGC( "unknow plugin[%p] type[%c]" , p_event->data.ptr , p_logpipe_plugin->type )
					return -1;
				}
				
			}
		}
	}
	
	/* 清理插件环境 */
	CleanEnvironment( p_env );
	INFOLOGC( "CleanEnvironment" )
	
	/* 关闭事件总线 */
	INFOLOGC( " close epoll_fd[%d]" , p_env->epoll_fd )
	close( p_env->epoll_fd );
	
	return 0;
}

