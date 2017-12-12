#include "logpipe_in.h"

int InitEnvironment( struct LogpipeEnv *p_env )
{
	struct LogpipeInputPlugin	*p_logpipe_input_plugin = NULL ;
	struct LogpipeOutputPlugin	*p_logpipe_output_plugin = NULL ;
	struct epoll_event		event ;
	
	int				nret = 0 ;
	
	/* 创建epoll */
	p_env->epoll_fd = epoll_create( 1024 ) ;
	if( p_env->epoll_fd == -1 )
	{
		ERRORLOG( "epoll_create failed , errno[%d]" , errno );
		return -1;
	}
	
	/* 执行所有输入端初始化函数 */
	list_for_each_entry( p_logpipe_input_plugin , & (p_env->logpipe_inputs_plugin_list.this_node) , struct LogpipeInputPlugin , this_node )
	{
		p_logpipe_input_plugin->fd = -1 ;
		p_logpipe_input_plugin->context = NULL ;
		nret = p_logpipe_input_plugin->pfuncInitLogpipeInputPlugin( p_env , p_logpipe_input_plugin , & (p_logpipe_input_plugin->plugin_config_items) , & (p_logpipe_input_plugin->context) , & (p_logpipe_input_plugin->fd) ) ;
		if( nret )
		{
			ERRORLOG( "[%s]p_logpipe_input_plugin->pfuncInitLogpipeInputPlugin failed , errno[%d]" , p_logpipe_input_plugin->so_path_filename , errno );
			return -1;
		}
		else
		{
			INFOLOG( "[%s]p_logpipe_input_plugin->pfuncInitLogpipeInputPlugin ok" , p_logpipe_input_plugin->so_path_filename );
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
				ERRORLOG( "epoll_ctl[%d] add inotify_session[%s] failed , errno[%d]" , p_env->epoll_fd , p_logpipe_input_plugin->fd , errno );
				return -1;
			}
		}
		
		if( p_logpipe_input_plugin->fd < 0 )
			p_env->p_block_input_plugin = p_logpipe_input_plugin ;
	}
	
	/* 执行所有输出端初始化函数 */
	list_for_each_entry( p_logpipe_output_plugin , & (p_env->logpipe_outputs_plugin_list.this_node) , struct LogpipeOutputPlugin , this_node )
	{
		p_logpipe_output_plugin->context = NULL ;
		nret = p_logpipe_output_plugin->pfuncInitLogpipeOutputPlugin( p_env , p_logpipe_output_plugin , & (p_logpipe_output_plugin->plugin_config_items) , & (p_logpipe_output_plugin->context) ) ;
		if( nret )
		{
			ERRORLOG( "[%s]p_logpipe_output_plugin->pfuncInitLogpipeOutputPlugin failed , errno[%d]" , p_logpipe_output_plugin->so_path_filename , errno );
			return -1;
		}
		else
		{
			INFOLOG( "[%s]p_logpipe_output_plugin->pfuncInitLogpipeOutputPlugin ok" , p_logpipe_output_plugin->so_path_filename );
		}
	}
	
	return 0;
}

void CleanEnvironment( struct LogpipeEnv *p_env )
{
	struct LogpipeInputPlugin	*p_logpipe_input_plugin = NULL ;
	struct LogpipeOutputPlugin	*p_logpipe_output_plugin = NULL ;
	struct LogpipeInputPlugin	*p_next_logpipe_input_plugin = NULL ;
	struct LogpipeOutputPlugin	*p_next_logpipe_output_plugin = NULL ;
	
	int				nret = 0 ;
	
	/* 执行所有输入端初始化函数 */
	list_for_each_entry_safe( p_logpipe_input_plugin , p_next_logpipe_input_plugin , & (p_env->logpipe_inputs_plugin_list.this_node) , struct LogpipeInputPlugin , this_node )
	{
		nret = p_logpipe_input_plugin->pfuncCleanLogpipeInputPlugin( p_env , p_logpipe_input_plugin , & (p_logpipe_input_plugin->context) ) ;
		if( nret )
		{
			ERRORLOG( "[%s]p_logpipe_input_plugin->pfuncCleanLogpipeInputPlugin failed , errno[%d]" , p_logpipe_input_plugin->so_path_filename , errno );
			return;
		}
		
		RemoveAllPluginConfigItem( & (p_logpipe_input_plugin->plugin_config_items) );
	}
	
	/* 执行所有输出端初始化函数 */
	list_for_each_entry_safe( p_logpipe_output_plugin , p_next_logpipe_output_plugin , & (p_env->logpipe_outputs_plugin_list.this_node) , struct LogpipeOutputPlugin , this_node )
	{
		nret = p_logpipe_output_plugin->pfuncCleanLogpipeOutputPlugin( p_env , p_logpipe_output_plugin , & (p_logpipe_output_plugin->context) ) ;
		if( nret )
		{
			ERRORLOG( "[%s]p_logpipe_output_plugin->pfuncCleanLogpipeOutputPlugin failed , errno[%d]" , p_logpipe_output_plugin->so_path_filename , errno );
			return;
		}
		
		RemoveAllPluginConfigItem( & (p_logpipe_output_plugin->plugin_config_items) );
	}
	
	/* 销毁epoll */
	close( p_env->epoll_fd );
	
	return;
}

struct LogpipeInputPlugin *AddLogpipeInputPlugin( struct LogpipeEnv *p_env
						, char *so_filename
						, funcOnLogpipeInputEvent *pfuncOnLogpipeInputEvent
						, funcBeforeReadLogpipeInput *pfuncBeforeReadLogpipeInput , funcReadLogpipeInput *pfuncReadLogpipeInput , funcAfterReadLogpipeInput *pfuncAfterReadLogpipeInput
						, funcCleanLogpipeInputPlugin *pfuncCleanLogpipeInputPlugin
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
	strncpy( p_logpipe_input_plugin->so_filename , so_filename , sizeof(p_logpipe_input_plugin->so_filename)-1 );
	
	p_logpipe_input_plugin->pfuncOnLogpipeInputEvent = pfuncOnLogpipeInputEvent ;
	
	p_logpipe_input_plugin->pfuncBeforeReadLogpipeInput = pfuncBeforeReadLogpipeInput ;
	p_logpipe_input_plugin->pfuncReadLogpipeInput = pfuncReadLogpipeInput ;
	p_logpipe_input_plugin->pfuncAfterReadLogpipeInput = pfuncAfterReadLogpipeInput ;
	
	p_logpipe_input_plugin->pfuncCleanLogpipeInputPlugin = pfuncCleanLogpipeInputPlugin ;
	
	p_logpipe_input_plugin->fd = fd ;
	p_logpipe_input_plugin->context = context ;
	
	memset( & event , 0x00 , sizeof(struct epoll_event) );
	event.events = EPOLLIN | EPOLLERR ;
	event.data.ptr = p_logpipe_input_plugin ;
	nret = epoll_ctl( p_env->epoll_fd , EPOLL_CTL_ADD , p_logpipe_input_plugin->fd , & event ) ;
	if( nret == -1 )
	{
		ERRORLOG( "epoll_ctl[%d] add p_logpipe_input_plugin->fd[%s] failed , errno[%d]" , p_env->epoll_fd , p_logpipe_input_plugin->fd , errno );
		RemoveLogpipeInputPlugin( p_env , p_logpipe_input_plugin );
		return NULL;
	}
	else
	{
		INFOLOG( "epoll_ctl[%d] add p_logpipe_input_plugin->fd[%s] ok" , p_env->epoll_fd , p_logpipe_input_plugin->fd );
	}
	list_add_tail( & (p_logpipe_input_plugin->this_node) , & (p_env->logpipe_inputs_plugin_list.this_node) );
	
	return p_logpipe_input_plugin;
}

void RemoveLogpipeInputPlugin( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin )
{
	if( p_logpipe_input_plugin->fd >= 0 )
	{
		epoll_ctl( p_env->epoll_fd , EPOLL_CTL_DEL , p_logpipe_input_plugin->fd , NULL );
	}
	p_logpipe_input_plugin->pfuncCleanLogpipeInputPlugin( p_env , p_logpipe_input_plugin , p_logpipe_input_plugin->context );
	
	free( p_logpipe_input_plugin );
	
	return;
}
