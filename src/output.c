/*
 * logpipe - Distribute log collector
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "logpipe_in.h"

/* 激活一轮从输入插件读，写到所有输出插件流程处理 */
int WriteAllOutputPlugins( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , uint16_t filename_len , char *filename )
{
	struct LogpipeOutputPlugin	*p_logpipe_output_plugin = NULL ;
	
	char				block_buf[ LOGPIPE_BLOCK_BUFSIZE + 1 ] ;
	uint32_t			block_len ;
	
	int				nret = 0 ;
	
	/* 执行所有输出端写前函数 */
	list_for_each_entry( p_logpipe_output_plugin , & (p_env->logpipe_output_plugins_list.this_node) , struct LogpipeOutputPlugin , this_node )
	{
		DEBUGLOG( "[%s]->pfuncBeforeWriteOutputPlugin ..." , p_logpipe_output_plugin->so_filename )
		nret = p_logpipe_output_plugin->pfuncBeforeWriteOutputPlugin( p_env , p_logpipe_output_plugin , p_logpipe_output_plugin->context , filename_len , filename ) ;
		if( nret < 0 )
		{
			ERRORLOG( "[%s]->pfuncBeforeWriteOutputPlugin failed , errno[%d]" , p_logpipe_output_plugin->so_filename , errno )
			return 1;
		}
		else if( nret > 0 )
		{
			WARNLOG( "[%s]->pfuncBeforeWriteOutputPlugin failed , errno[%d]" , p_logpipe_output_plugin->so_filename , errno )
			list_for_each_entry( p_logpipe_output_plugin , & (p_env->logpipe_output_plugins_list.this_node) , struct LogpipeOutputPlugin , this_node )
			{
				nret = p_logpipe_output_plugin->pfuncAfterWriteOutputPlugin( p_env , p_logpipe_output_plugin , p_logpipe_output_plugin->context , filename_len , filename ) ;
				WARNLOG( "[%s]->pfuncAfterWriteOutputPlugin return[%d]" , p_logpipe_output_plugin->so_filename , nret )
			}
			return 0;
		}
		else
		{
			DEBUGLOG( "[%s]->pfuncBeforeWriteOutputPlugin ok" , p_logpipe_output_plugin->so_filename )
		}
	}
	
	while(1)
	{
		/* 执行输入端读函数 */
		DEBUGLOG( "[%s]->pfuncReadInputPlugin ..." , p_logpipe_input_plugin->so_filename );
		memset( block_buf , 0x00 , sizeof(block_buf) );
		nret = p_logpipe_input_plugin->pfuncReadInputPlugin( p_env , p_logpipe_input_plugin , p_logpipe_input_plugin->context , & block_len , block_buf , sizeof(block_buf) ) ;
		if( nret == LOGPIPE_READ_END_OF_INPUT )
		{
			INFOLOG( "[%s]->pfuncReadInputPlugin done" , p_logpipe_input_plugin->so_filename )
			break;
		}
		else if( nret < 0 )
		{
			ERRORLOG( "[%s]->pfuncReadInputPlugin failed[%d]" , p_logpipe_input_plugin->so_filename , nret )
			return 1;
		}
		else if( nret > 0 )
		{
			WARNLOG( "[%s]->pfuncReadInputPlugin return[%d]" , p_logpipe_input_plugin->so_filename , nret )
			list_for_each_entry( p_logpipe_output_plugin , & (p_env->logpipe_output_plugins_list.this_node) , struct LogpipeOutputPlugin , this_node )
			{
				nret = p_logpipe_output_plugin->pfuncAfterWriteOutputPlugin( p_env , p_logpipe_output_plugin , p_logpipe_output_plugin->context , filename_len , filename ) ;
				WARNLOG( "[%s]->pfuncAfterWriteOutputPlugin return[%d]" , p_logpipe_output_plugin->so_filename , nret )
			}
			return 0;
		}
		else
		{
			DEBUGLOG( "[%s]->pfuncReadInputPlugin ok" , p_logpipe_input_plugin->so_filename )
		}
		
		/* 执行所有输出端写函数 */
		list_for_each_entry( p_logpipe_output_plugin , & (p_env->logpipe_output_plugins_list.this_node) , struct LogpipeOutputPlugin , this_node )
		{
			DEBUGLOG( "[%s]->pfuncWriteOutputPlugin ..." , p_logpipe_output_plugin->so_filename )
			nret = p_logpipe_output_plugin->pfuncWriteOutputPlugin( p_env , p_logpipe_output_plugin , p_logpipe_output_plugin->context , block_len , block_buf ) ;
			if( nret < 0 )
			{
				ERRORLOG( "[%s]->pfuncWriteOutputPlugin failed[%d]" , p_logpipe_output_plugin->so_filename , nret )
				return -1;
			}
			else if( nret > 0 )
			{
				WARNLOG( "[%s]->pfuncWriteOutputPlugin return[%d]" , p_logpipe_output_plugin->so_filename , nret );
				list_for_each_entry( p_logpipe_output_plugin , & (p_env->logpipe_output_plugins_list.this_node) , struct LogpipeOutputPlugin , this_node )
				{
					nret = p_logpipe_output_plugin->pfuncAfterWriteOutputPlugin( p_env , p_logpipe_output_plugin , p_logpipe_output_plugin->context , filename_len , filename ) ;
					WARNLOG( "[%s]->pfuncAfterWriteOutputPlugin return[%d]" , p_logpipe_output_plugin->so_filename , nret )
				}
				return 0;
			}
			else
			{
				DEBUGLOG( "[%s]->pfuncWriteOutputPlugin ok" , p_logpipe_output_plugin->so_filename )
			}
		}
	}
	
	/* 执行所有输出端写后函数 */
	list_for_each_entry( p_logpipe_output_plugin , & (p_env->logpipe_output_plugins_list.this_node) , struct LogpipeOutputPlugin , this_node )
	{
		DEBUGLOG( "[%s]->pfuncAfterWriteOutputPlugin ..." , p_logpipe_output_plugin->so_filename )
		nret = p_logpipe_output_plugin->pfuncAfterWriteOutputPlugin( p_env , p_logpipe_output_plugin , p_logpipe_output_plugin->context , filename_len , filename ) ;
		if( nret < 0 )
		{
			ERRORLOG( "[%s]->pfuncAfterWriteOutputPlugin failed[%d]" , p_logpipe_output_plugin->so_filename , nret )
			return 1;
		}
		else if( nret > 0 )
		{
			WARNLOG( "[%s]->pfuncAfterWriteOutputPlugin return[%d]" , p_logpipe_output_plugin->so_filename , nret )
		}
		else
		{
			DEBUGLOG( "[%s]->pfuncAfterWriteOutputPlugin ok" , p_logpipe_output_plugin->so_filename )
		}
	}
	
	return 0;
}
