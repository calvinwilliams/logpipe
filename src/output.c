#include "logpipe_in.h"

int WriteAllOutputPlugins( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , uint16_t filename_len , char *filename )
{
	struct LogpipeOutputPlugin	*p_logpipe_output_plugin = NULL ;
	
	char				block_buf[ LOGPIPE_BLOCK_BUFSIZE + 1 ] ;
	uint32_t			block_len ;
	
	int				nret = 0 ;
	
	/* 执行所有输出端写前函数 */
	list_for_each_entry( p_logpipe_output_plugin , & (p_env->logpipe_output_plugins_list.this_node) , struct LogpipeOutputPlugin , this_node )
	{
		nret = p_logpipe_output_plugin->pfuncBeforeWriteLogpipeOutput( p_env , p_logpipe_output_plugin , p_logpipe_output_plugin->context , filename_len , filename ) ;
		if( nret < 0 )
		{
			ERRORLOG( "[%s]->pfuncBeforeWriteLogpipeOutput failed , errno[%d]" , p_logpipe_output_plugin->so_filename , errno );
			return -1;
		}
		else if( nret > 0 )
		{
			ERRORLOG( "[%s]->pfuncBeforeWriteLogpipeOutput failed , errno[%d]" , p_logpipe_output_plugin->so_filename , errno );
			list_for_each_entry( p_logpipe_output_plugin , & (p_env->logpipe_output_plugins_list.this_node) , struct LogpipeOutputPlugin , this_node )
			{
				nret = p_logpipe_output_plugin->pfuncAfterWriteLogpipeOutput( p_env , p_logpipe_output_plugin , p_logpipe_output_plugin->context ) ;
				INFOLOG( "[%s]->pfuncAfterWriteLogpipeOutput return[%d]" , p_logpipe_output_plugin->so_filename , nret );
			}
			return 0;
		}
		else
		{
			INFOLOG( "[%s]->pfuncBeforeWriteLogpipeOutput ok" , p_logpipe_output_plugin->so_filename );
		}
	}
	
	while(1)
	{
		/* 执行输入端读函数 */
		nret = p_logpipe_input_plugin->pfuncReadLogpipeInput( p_env , p_logpipe_input_plugin , p_logpipe_input_plugin->context , & block_len , block_buf , sizeof(block_buf) ) ;
		if( nret == LOGPIPE_READ_END_OF_INPUT )
		{
			INFOLOG( "[%s]->pfuncReadLogpipeInput done" , p_logpipe_input_plugin->so_filename );
			break;
		}
		else if( nret < 0 )
		{
			ERRORLOG( "[%s]->pfuncReadLogpipeInput failed[%d]" , p_logpipe_input_plugin->so_filename , nret );
			return -1;
		}
		else if( nret > 0 )
		{
			INFOLOG( "[%s]->pfuncReadLogpipeInput return[%d]" , p_logpipe_input_plugin->so_filename , nret );
			list_for_each_entry( p_logpipe_output_plugin , & (p_env->logpipe_output_plugins_list.this_node) , struct LogpipeOutputPlugin , this_node )
			{
				nret = p_logpipe_output_plugin->pfuncAfterWriteLogpipeOutput( p_env , p_logpipe_output_plugin , p_logpipe_output_plugin->context ) ;
				INFOLOG( "[%s]->pfuncAfterWriteLogpipeOutput return[%d]" , p_logpipe_output_plugin->so_filename , nret );
			}
			return 0;
		}
		else
		{
			INFOLOG( "[%s]->pfuncReadLogpipeInput ok" , p_logpipe_input_plugin->so_filename );
		}
		
		/* 执行所有输出端写函数 */
		list_for_each_entry( p_logpipe_output_plugin , & (p_env->logpipe_output_plugins_list.this_node) , struct LogpipeOutputPlugin , this_node )
		{
			nret = p_logpipe_output_plugin->pfuncWriteLogpipeOutput( p_env , p_logpipe_output_plugin , p_logpipe_output_plugin->context , block_len , block_buf ) ;
			if( nret < 0 )
			{
				ERRORLOG( "[%s]->pfuncWriteLogpipeOutput failed[%d]" , p_logpipe_output_plugin->so_filename , nret );
				return -1;
			}
			else if( nret > 0 )
			{
				INFOLOG( "[%s]->pfuncWriteLogpipeOutput return[%d]" , p_logpipe_output_plugin->so_filename , nret );
				list_for_each_entry( p_logpipe_output_plugin , & (p_env->logpipe_output_plugins_list.this_node) , struct LogpipeOutputPlugin , this_node )
				{
					nret = p_logpipe_output_plugin->pfuncAfterWriteLogpipeOutput( p_env , p_logpipe_output_plugin , p_logpipe_output_plugin->context ) ;
					INFOLOG( "[%s]->pfuncAfterWriteLogpipeOutput return[%d]" , p_logpipe_output_plugin->so_filename , nret );
				}
				return 0;
			}
			else
			{
				INFOLOG( "[%s]->pfuncWriteLogpipeOutput ok" , p_logpipe_output_plugin->so_filename );
			}
		}
	}
	
	/* 执行所有输出端写后函数 */
	list_for_each_entry( p_logpipe_output_plugin , & (p_env->logpipe_output_plugins_list.this_node) , struct LogpipeOutputPlugin , this_node )
	{
		nret = p_logpipe_output_plugin->pfuncAfterWriteLogpipeOutput( p_env , p_logpipe_output_plugin , p_logpipe_output_plugin->context ) ;
		if( nret < 0 )
		{
			ERRORLOG( "[%s]->pfuncAfterWriteLogpipeOutput failed[%d]" , p_logpipe_output_plugin->so_filename , nret );
			return -1;
		}
		else if( nret > 0 )
		{
			INFOLOG( "[%s]->pfuncAfterWriteLogpipeOutput return[%d]" , p_logpipe_output_plugin->so_filename , nret );
		}
		else
		{
			INFOLOG( "[%s]->pfuncAfterWriteLogpipeOutput ok" , p_logpipe_output_plugin->so_filename );
		}
	}
	
	return 0;
}
