/*
 * logpipe - Distribute log collector
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "logpipe_in.h"

/* 空闲时调用所有已注册空闲事件函数 */
int ProcessOnIdle( struct LogpipeEnv *p_env )
{
	struct LogpipeInputPlugin	*p_logpipe_input_plugin = NULL ;
	struct LogpipeOutputPlugin	*p_logpipe_output_plugin = NULL ;
	
	int				nret = 0 ;
	
	/* 执行所有输入端空闲函数 */
	list_for_each_entry( p_logpipe_input_plugin , & (p_env->logpipe_input_plugins_list.this_node) , struct LogpipeInputPlugin , this_node )
	{
		if( p_logpipe_input_plugin->pfuncOnInputPluginIdle == NULL )
			continue;
		
		DEBUGLOGC( "[%s]->pfuncOnInputPluginIdle ..." , p_logpipe_input_plugin->so_filename )
		nret = p_logpipe_input_plugin->pfuncOnInputPluginIdle( p_env , p_logpipe_input_plugin , p_logpipe_input_plugin->context ) ;
		if( nret < 0 )
		{
			ERRORLOGC( "[%s]->pfuncOnInputPluginIdle failed , errno[%d]" , p_logpipe_input_plugin->so_filename , errno )
			return 1;
		}
		else if( nret > 0 )
		{
			WARNLOGC( "[%s]->pfuncOnInputPluginIdle failed , errno[%d]" , p_logpipe_input_plugin->so_filename , errno )
			return 0;
		}
		else
		{
			DEBUGLOGC( "[%s]->pfuncOnInputPluginIdle ok" , p_logpipe_input_plugin->so_filename )
		}
	}
	
	/* 执行所有输出端空闲函数 */
	list_for_each_entry( p_logpipe_output_plugin , & (p_env->logpipe_output_plugins_list.this_node) , struct LogpipeOutputPlugin , this_node )
	{
		if( p_logpipe_output_plugin->pfuncOnOutputPluginIdle == NULL )
			continue;
		
		DEBUGLOGC( "[%s]->pfuncOnOutputPluginIdle ..." , p_logpipe_output_plugin->so_filename )
		nret = p_logpipe_output_plugin->pfuncOnOutputPluginIdle( p_env , p_logpipe_output_plugin , p_logpipe_output_plugin->context ) ;
		if( nret < 0 )
		{
			ERRORLOGC( "[%s]->pfuncOnOutputPluginIdle failed , errno[%d]" , p_logpipe_output_plugin->so_filename , errno )
			return 1;
		}
		else if( nret > 0 )
		{
			WARNLOGC( "[%s]->pfuncOnOutputPluginIdle failed , errno[%d]" , p_logpipe_output_plugin->so_filename , errno )
			return 0;
		}
		else
		{
			DEBUGLOGC( "[%s]->pfuncOnOutputPluginIdle ok" , p_logpipe_output_plugin->so_filename )
		}
	}
	
	return 0;
}
