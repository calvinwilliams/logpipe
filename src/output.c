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
		nret = p_logpipe_output_plugin->pfuncBeforeWriteLogpipeOutput( p_env , p_logpipe_output_plugin , & (p_logpipe_output_plugin->context) , filename_len , filename ) ;
		if( nret )
		{
			ERRORLOG( "[%s]p_logpipe_output_plugin->pfuncBeforeWriteLogpipeOutput failed , errno[%d]" , p_logpipe_output_plugin->so_path_filename , errno );
			return -1;
		}
		else
		{
			INFOLOG( "[%s]p_logpipe_output_plugin->pfuncBeforeWriteLogpipeOutput ok" , p_logpipe_output_plugin->so_path_filename );
		}
	}
	
	while(1)
	{
		/* 执行输入端读函数 */
		nret = p_logpipe_input_plugin->pfuncReadLogpipeInput( p_env , p_logpipe_input_plugin , & (p_logpipe_input_plugin->context) , & block_len , block_buf , sizeof(block_buf) ) ;
		if( nret > 0 )
		{
			INFOLOG( "[%s]p_logpipe_input_plugin->pfuncReadLogpipeInput done" , p_logpipe_input_plugin->so_path_filename );
		}
		else if( nret < 0 )
		{
			ERRORLOG( "[%s]p_logpipe_input_plugin->pfuncReadLogpipeInput failed , errno[%d]" , p_logpipe_input_plugin->so_path_filename , errno );
			return -1;
		}
		else
		{
			INFOLOG( "[%s]p_logpipe_input_plugin->pfuncReadLogpipeInput ok" , p_logpipe_input_plugin->so_path_filename );
		}
		
		/* 执行所有输出端写函数 */
		list_for_each_entry( p_logpipe_output_plugin , & (p_env->logpipe_output_plugins_list.this_node) , struct LogpipeOutputPlugin , this_node )
		{
			nret = p_logpipe_output_plugin->pfuncWriteLogpipeOutput( p_env , p_logpipe_output_plugin , & (p_logpipe_output_plugin->context) , block_len , block_buf ) ;
			if( nret )
			{
				ERRORLOG( "[%s]p_logpipe_output_plugin->pfuncWriteLogpipeOutput failed , errno[%d]" , p_logpipe_output_plugin->so_path_filename , errno );
				return -1;
			}
			else
			{
				INFOLOG( "[%s]p_logpipe_output_plugin->pfuncWriteLogpipeOutput ok" , p_logpipe_output_plugin->so_path_filename );
			}
		}
	}
	
	/* 执行所有输出端写后函数 */
	list_for_each_entry( p_logpipe_output_plugin , & (p_env->logpipe_output_plugins_list.this_node) , struct LogpipeOutputPlugin , this_node )
	{
		nret = p_logpipe_output_plugin->pfuncAfterWriteLogpipeOutput( p_env , p_logpipe_output_plugin , & (p_logpipe_output_plugin->context) ) ;
		if( nret )
		{
			ERRORLOG( "[%s]p_logpipe_output_plugin->pfuncAfterWriteLogpipeOutput failed , errno[%d]" , p_logpipe_output_plugin->so_path_filename , errno );
			return -1;
		}
		else
		{
			INFOLOG( "[%s]p_logpipe_output_plugin->pfuncAfterWriteLogpipeOutput ok" , p_logpipe_output_plugin->so_path_filename );
		}
	}
	
	return 0;
}
