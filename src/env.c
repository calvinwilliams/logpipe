/*
 * logpipe - Distribute log collector
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "logpipe_in.h"

int InitEnvironment( struct LogpipeEnv *p_env )
{
	struct LogpipeInputPlugin	*p_logpipe_input_plugin = NULL ;
	struct LogpipeOutputPlugin	*p_logpipe_output_plugin = NULL ;
	struct epoll_event		event ;
	
	int				nret = 0 ;
	
	/* 执行所有输出端初始化函数 */
	list_for_each_entry( p_logpipe_output_plugin , & (p_env->logpipe_output_plugins_list.this_node) , struct LogpipeOutputPlugin , this_node )
	{
		nret = p_logpipe_output_plugin->pfuncInitOutputPluginContext( p_env , p_logpipe_output_plugin , p_logpipe_output_plugin->context ) ;
		if( nret )
		{
			ERRORLOG( "[%s]->pfuncInitOutputPluginContext failed , errno[%d]" , p_logpipe_output_plugin->so_filename , errno );
			return -1;
		}
		else
		{
			DEBUGLOG( "[%s]->pfuncInitOutputPluginContext ok" , p_logpipe_output_plugin->so_filename );
		}
	}
	
	/* 执行所有输入端初始化函数 */
	list_for_each_entry( p_logpipe_input_plugin , & (p_env->logpipe_input_plugins_list.this_node) , struct LogpipeInputPlugin , this_node )
	{
		nret = p_logpipe_input_plugin->pfuncInitInputPluginContext( p_env , p_logpipe_input_plugin , p_logpipe_input_plugin->context , & (p_logpipe_input_plugin->fd) ) ;
		if( nret )
		{
			ERRORLOG( "[%s]->pfuncInitInputPluginContext failed , errno[%d]" , p_logpipe_input_plugin->so_filename , errno );
			return -1;
		}
		else
		{
			DEBUGLOG( "[%s]->pfuncInitInputPluginContext ok" , p_logpipe_input_plugin->so_filename );
		}
		
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
			else
			{
				INFOLOG( "epoll_ctl[%d] add input plugin fd[%d] EPOLLIN ok" , p_env->epoll_fd , p_logpipe_input_plugin->fd );
			}
		}
		else
		{
			p_env->p_block_input_plugin = p_logpipe_input_plugin ;
		}
	}
	
	return 0;
}

void CleanEnvironment( struct LogpipeEnv *p_env )
{
	struct LogpipeInputPlugin	*p_logpipe_input_plugin = NULL ;
	struct LogpipeOutputPlugin	*p_logpipe_output_plugin = NULL ;
	
	int				nret = 0 ;
	
	/* 执行所有输入端初始化函数 */
	list_for_each_entry( p_logpipe_input_plugin , & (p_env->logpipe_input_plugins_list.this_node) , struct LogpipeInputPlugin , this_node )
	{
		if( p_env->epoll_fd >= 0 )
		{
			if( p_logpipe_input_plugin->fd >= 0 )
			{
				INFOLOG( "epoll_ctl[%d] del input plugin fd[%d]" , p_env->epoll_fd , p_logpipe_input_plugin->fd );
				epoll_ctl( p_env->epoll_fd , EPOLL_CTL_DEL , p_logpipe_input_plugin->fd , NULL );
			}
		}
		
		nret = p_logpipe_input_plugin->pfuncCleanInputPluginContext( p_env , p_logpipe_input_plugin , p_logpipe_input_plugin->context ) ;
		if( nret )
		{
			ERRORLOG( "[%s]->pfuncCleanInputPluginContext failed , errno[%d]" , p_logpipe_input_plugin->so_filename , errno );
			return;
		}
		else
		{
			DEBUGLOG( "[%s]->pfuncCleanInputPluginContext ok" , p_logpipe_input_plugin->so_filename );
		}
	}
	
	/* 执行所有输出端初始化函数 */
	list_for_each_entry( p_logpipe_output_plugin , & (p_env->logpipe_output_plugins_list.this_node) , struct LogpipeOutputPlugin , this_node )
	{
		nret = p_logpipe_output_plugin->pfuncCleanOutputPluginContext( p_env , p_logpipe_output_plugin , p_logpipe_output_plugin->context ) ;
		if( nret )
		{
			ERRORLOG( "[%s]->pfuncCleanOutputPluginContext failed , errno[%d]" , p_logpipe_output_plugin->so_filename , errno );
			return;
		}
		else
		{
			DEBUGLOG( "[%s]->pfuncCleanOutputPluginContext ok" , p_logpipe_output_plugin->so_filename );
		}
	}
	
	return;
}

struct LogpipeInputPlugin *AddInputPluginSession( struct LogpipeEnv *p_env , char *so_filename
						, funcOnInputPluginEvent *pfuncOnInputPluginEvent
						, funcReadInputPlugin *pfuncReadInputPlugin
						, funcCleanInputPluginContext *pfuncCleanInputPluginContext , funcUnloadInputPluginConfig *pfuncUnloadInputPluginConfig
						, int fd , void *context )
{
	struct LogpipeInputPlugin	*p_logpipe_input_plugin = NULL ;
	struct epoll_event		event ;
	
	int				nret = 0 ;
	
	p_logpipe_input_plugin = (struct LogpipeInputPlugin *)malloc( sizeof(struct LogpipeInputPlugin) ) ;
	if( p_logpipe_input_plugin == NULL )
		return NULL;
	memset( p_logpipe_input_plugin , 0x00 , sizeof(struct LogpipeInputPlugin) );
	
	INIT_LIST_HEAD( & (p_logpipe_input_plugin->plugin_config_items.this_node) );
	
	if( so_filename )
	{
		strncpy( p_logpipe_input_plugin->so_filename , so_filename , sizeof(p_logpipe_input_plugin->so_filename)-1 );
	}
	
	p_logpipe_input_plugin->pfuncOnInputPluginEvent = pfuncOnInputPluginEvent ;
	p_logpipe_input_plugin->pfuncReadInputPlugin = pfuncReadInputPlugin ;
	p_logpipe_input_plugin->pfuncCleanInputPluginContext = pfuncCleanInputPluginContext ;
	p_logpipe_input_plugin->pfuncUnloadInputPluginConfig = pfuncUnloadInputPluginConfig ;
	
	p_logpipe_input_plugin->fd = fd ;
	p_logpipe_input_plugin->context = context ;
	
	if( p_logpipe_input_plugin->fd >= 0 )
	{
		memset( & event , 0x00 , sizeof(struct epoll_event) );
		event.events = EPOLLIN | EPOLLERR ;
		event.data.ptr = p_logpipe_input_plugin ;
		nret = epoll_ctl( p_env->epoll_fd , EPOLL_CTL_ADD , p_logpipe_input_plugin->fd , & event ) ;
		if( nret == -1 )
		{
			ERRORLOG( "epoll_ctl[%d] add input plugin fd[%d] failed , errno[%d]" , p_env->epoll_fd , p_logpipe_input_plugin->fd , errno );
			free( p_logpipe_input_plugin );
			return NULL;
		}
		else
		{
			INFOLOG( "epoll_ctl[%d] add input plugin fd[%d] ok" , p_env->epoll_fd , p_logpipe_input_plugin->fd );
		}
	}
	
	list_add( & (p_logpipe_input_plugin->this_node) , & (p_env->logpipe_input_plugins_list.this_node) );
	
	return p_logpipe_input_plugin;
}

void RemoveInputPluginSession( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin )
{
	int		nret = 0 ;
	
	if( p_env->epoll_fd >= 0 )
	{
		if( p_logpipe_input_plugin->fd >= 0 )
		{
			INFOLOG( "epoll_ctl[%d] del input plugin fd[%d]" , p_env->epoll_fd , p_logpipe_input_plugin->fd );
			epoll_ctl( p_env->epoll_fd , EPOLL_CTL_DEL , p_logpipe_input_plugin->fd , NULL );
		}
	}
	
	nret = p_logpipe_input_plugin->pfuncCleanInputPluginContext( p_env , p_logpipe_input_plugin , p_logpipe_input_plugin->context ) ;
	if( nret )
	{
		ERRORLOG( "[%s]->pfuncCleanInputPluginContext failed , errno[%d]" , p_logpipe_input_plugin->so_filename , errno );
	}
	else
	{
		DEBUGLOG( "[%s]->pfuncCleanInputPluginContext ok" , p_logpipe_input_plugin->so_filename );
	}
	
	UnloadInputPluginSession( p_env , p_logpipe_input_plugin );
	
	return;
}

