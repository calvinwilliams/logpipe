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
	struct LogpipeFilterPlugin	*p_travel_logpipe_filter_plugin = NULL ;
	struct LogpipeOutputPlugin	*p_travel_logpipe_output_plugin = NULL ;
	
	uint64_t			file_offset ;
	uint64_t			file_line ;
	char				block_buf[ LOGPIPE_OUTPUT_BUFSIZE + 1 ] ;
	uint64_t			block_len ;
	struct timeval			tv_begin ;
	struct timeval			tv_end ;
	struct timeval			tv_diff ;
	
	int				nret = 0 ;
	int				nret2 = 0 ;
	
	file_offset = 0 ;
	file_line = 0 ;
	
	/* 执行输入端读前函数 */
	INFOLOGC( "[%s]->pfuncBeforeReadInputPlugin ..." , p_logpipe_input_plugin->so_filename )
	if( p_logpipe_input_plugin->pfuncBeforeReadInputPlugin )
	{
		gettimeofday( & tv_begin , NULL );
		nret = p_logpipe_input_plugin->pfuncBeforeReadInputPlugin( p_env , p_logpipe_input_plugin , p_logpipe_input_plugin->context , & file_offset , & file_line ) ;
		gettimeofday( & tv_end , NULL );
		DiffTimeval( & tv_begin , & tv_end , & tv_diff );
		if( nret < 0 )
		{
			ERRORLOGC( "[%s]->pfuncBeforeReadInputPlugin failed , errno[%d]" , p_logpipe_input_plugin->so_filename , errno )
			return -1;
		}
		else if( nret > 0 )
		{
			WARNLOGC( "[%s]->pfuncBeforeReadInputPlugin failed , errno[%d]" , p_logpipe_input_plugin->so_filename , errno )
			goto _GOTO_AFTER_OUTPUT;
		}
		else
		{
			INFOLOGC( "[%s]->pfuncBeforeReadInputPlugin ok , ELAPSE[%ld.%06ld]" , p_logpipe_input_plugin->so_filename , tv_diff.tv_sec , tv_diff.tv_usec )
		}
	}
	
	/* 执行所有过滤端过滤前函数 */
	list_for_each_entry( p_travel_logpipe_filter_plugin , & (p_env->logpipe_filter_plugins_list.this_node) , struct LogpipeFilterPlugin , this_node )
	{
		INFOLOGC( "[%s]->pfuncBeforeProcessFilterPlugin ..." , p_travel_logpipe_filter_plugin->so_filename )
		if( p_travel_logpipe_filter_plugin->pfuncBeforeProcessFilterPlugin )
		{
			gettimeofday( & tv_begin , NULL );
			nret = p_travel_logpipe_filter_plugin->pfuncBeforeProcessFilterPlugin( p_env , p_travel_logpipe_filter_plugin , p_travel_logpipe_filter_plugin->context , filename_len , filename ) ;
			gettimeofday( & tv_end , NULL );
			DiffTimeval( & tv_begin , & tv_end , & tv_diff );
			if( nret < 0 )
			{
				ERRORLOGC( "[%s]->pfuncBeforeProcessFilterPlugin failed , errno[%d]" , p_travel_logpipe_filter_plugin->so_filename , errno )
				return -1;
			}
			else if( nret > 0 )
			{
				WARNLOGC( "[%s]->pfuncBeforeProcessFilterPlugin failed , errno[%d]" , p_travel_logpipe_filter_plugin->so_filename , errno )
				goto _GOTO_AFTER_OUTPUT;
			}
			else
			{
				INFOLOGC( "[%s]->pfuncBeforeProcessFilterPlugin ok , ELAPSE[%ld.%06ld]" , p_travel_logpipe_filter_plugin->so_filename , tv_diff.tv_sec , tv_diff.tv_usec )
			}
		}
	}
	
	/* 执行所有输出端写前函数 */
	list_for_each_entry( p_travel_logpipe_output_plugin , & (p_env->logpipe_output_plugins_list.this_node) , struct LogpipeOutputPlugin , this_node )
	{
		INFOLOGC( "[%s]->pfuncBeforeWriteOutputPlugin ..." , p_travel_logpipe_output_plugin->so_filename )
		if( p_travel_logpipe_output_plugin->pfuncBeforeWriteOutputPlugin )
		{
			gettimeofday( & tv_begin , NULL );
			nret = p_travel_logpipe_output_plugin->pfuncBeforeWriteOutputPlugin( p_env , p_travel_logpipe_output_plugin , p_travel_logpipe_output_plugin->context , filename_len , filename ) ;
			gettimeofday( & tv_end , NULL );
			DiffTimeval( & tv_begin , & tv_end , & tv_diff );
			if( nret < 0 )
			{
				ERRORLOGC( "[%s]->pfuncBeforeWriteOutputPlugin failed , errno[%d]" , p_travel_logpipe_output_plugin->so_filename , errno )
				return -1;
			}
			else if( nret > 0 )
			{
				WARNLOGC( "[%s]->pfuncBeforeWriteOutputPlugin failed , errno[%d]" , p_travel_logpipe_output_plugin->so_filename , errno )
				goto _GOTO_AFTER_OUTPUT;
			}
			else
			{
				INFOLOGC( "[%s]->pfuncBeforeWriteOutputPlugin ok , ELAPSE[%ld.%06ld]" , p_travel_logpipe_output_plugin->so_filename , tv_diff.tv_sec , tv_diff.tv_usec )
			}
		}
	}
	
	while(1)
	{
		/* 执行输入端读函数 */
		INFOLOGC( "[%s]->pfuncReadInputPlugin ..." , p_logpipe_input_plugin->so_filename )
		memset( block_buf , 0x00 , sizeof(block_buf) );
		gettimeofday( & tv_begin , NULL );
		nret = p_logpipe_input_plugin->pfuncReadInputPlugin( p_env , p_logpipe_input_plugin , p_logpipe_input_plugin->context , & file_offset , & file_line , & block_len , block_buf , LOGPIPE_INPUT_BUFSIZE ) ;
		gettimeofday( & tv_end , NULL );
		DiffTimeval( & tv_begin , & tv_end , & tv_diff );
		if( nret == LOGPIPE_READ_END_FROM_INPUT )
		{
			INFOLOGC( "[%s]->pfuncReadInputPlugin return LOGPIPE_READ_END_FROM_INPUT" , p_logpipe_input_plugin->so_filename )
			nret = 0 ;
			break;
		}
		else if( nret < 0 )
		{
			ERRORLOGC( "[%s]->pfuncReadInputPlugin failed[%d]" , p_logpipe_input_plugin->so_filename , nret )
			return -1;
		}
		else if( nret > 0 )
		{
			WARNLOGC( "[%s]->pfuncReadInputPlugin return[%d]" , p_logpipe_input_plugin->so_filename , nret )
			goto _GOTO_AFTER_OUTPUT;
		}
		else
		{
			INFOLOGC( "[%s]->pfuncReadInputPlugin ok , ELAPSE[%ld.%06ld]" , p_logpipe_input_plugin->so_filename , tv_diff.tv_sec , tv_diff.tv_usec )
		}
		
		/* 执行所有过滤端函数 */
		list_for_each_entry( p_travel_logpipe_filter_plugin , & (p_env->logpipe_filter_plugins_list.this_node) , struct LogpipeFilterPlugin , this_node )
		{
			INFOLOGC( "[%s]->pfuncProcessFilterPlugin ..." , p_travel_logpipe_filter_plugin->so_filename )
			gettimeofday( & tv_begin , NULL );
			nret = p_travel_logpipe_filter_plugin->pfuncProcessFilterPlugin( p_env , p_travel_logpipe_filter_plugin , p_travel_logpipe_filter_plugin->context , file_offset , file_line , & block_len , block_buf , LOGPIPE_OUTPUT_BUFSIZE ) ;
			gettimeofday( & tv_end , NULL );
			DiffTimeval( & tv_begin , & tv_end , & tv_diff );
			if( nret < 0 )
			{
				ERRORLOGC( "[%s]->pfuncProcessFilterPlugin failed[%d]" , p_travel_logpipe_filter_plugin->so_filename , nret )
				return -1;
			}
			else if( nret > 0 )
			{
				WARNLOGC( "[%s]->pfuncProcessFilterPlugin return[%d]" , p_travel_logpipe_filter_plugin->so_filename , nret )
				goto _GOTO_AFTER_OUTPUT;
			}
			else
			{
				INFOLOGC( "[%s]->pfuncProcessFilterPlugin ok , ELAPSE[%ld.%06ld]" , p_travel_logpipe_filter_plugin->so_filename , tv_diff.tv_sec , tv_diff.tv_usec )
			}
		}
		
		/* 执行所有输出端写函数 */
		list_for_each_entry( p_travel_logpipe_output_plugin , & (p_env->logpipe_output_plugins_list.this_node) , struct LogpipeOutputPlugin , this_node )
		{
			INFOLOGC( "[%s]->pfuncWriteOutputPlugin ..." , p_travel_logpipe_output_plugin->so_filename )
			gettimeofday( & tv_begin , NULL );
			nret = p_travel_logpipe_output_plugin->pfuncWriteOutputPlugin( p_env , p_travel_logpipe_output_plugin , p_travel_logpipe_output_plugin->context , file_offset , file_line , block_len , block_buf , LOGPIPE_OUTPUT_BUFSIZE ) ;
			gettimeofday( & tv_end , NULL );
			DiffTimeval( & tv_begin , & tv_end , & tv_diff );
			if( nret < 0 )
			{
				ERRORLOGC( "[%s]->pfuncWriteOutputPlugin failed[%d]" , p_travel_logpipe_output_plugin->so_filename , nret )
				return -1;
			}
			else if( nret > 0 )
			{
				WARNLOGC( "[%s]->pfuncWriteOutputPlugin return[%d]" , p_travel_logpipe_output_plugin->so_filename , nret )
				goto _GOTO_AFTER_OUTPUT;
			}
			else
			{
				INFOLOGC( "[%s]->pfuncWriteOutputPlugin ok , ELAPSE[%ld.%06ld]" , p_travel_logpipe_output_plugin->so_filename , tv_diff.tv_sec , tv_diff.tv_usec )
			}
		}
	}
	
_GOTO_AFTER_OUTPUT :
	
	/* 执行输入端读后函数 */
	INFOLOGC( "[%s]->pfuncAfterReadInputPlugin ..." , p_logpipe_input_plugin->so_filename )
	if( p_logpipe_input_plugin->pfuncAfterReadInputPlugin )
	{
		gettimeofday( & tv_begin , NULL );
		nret2 = p_logpipe_input_plugin->pfuncAfterReadInputPlugin( p_env , p_logpipe_input_plugin , p_logpipe_input_plugin->context , & file_offset , & file_line ) ;
		gettimeofday( & tv_end , NULL );
		DiffTimeval( & tv_begin , & tv_end , & tv_diff );
		if( nret2 < 0 )
		{
			ERRORLOGC( "[%s]->pfuncAfterReadInputPlugin failed[%d]" , p_logpipe_input_plugin->so_filename , nret2 )
		}
		else if( nret2 > 0 )
		{
			WARNLOGC( "[%s]->pfuncAfterReadInputPlugin return[%d]" , p_logpipe_input_plugin->so_filename , nret2 )
		}
		else
		{
			INFOLOGC( "[%s]->pfuncAfterReadInputPlugin ok , ELAPSE[%ld.%06ld]" , p_logpipe_input_plugin->so_filename , tv_diff.tv_sec , tv_diff.tv_usec )
		}
	}
	
	/* 执行所有过滤端过滤后函数 */
	list_for_each_entry( p_travel_logpipe_filter_plugin , & (p_env->logpipe_filter_plugins_list.this_node) , struct LogpipeFilterPlugin , this_node )
	{
		INFOLOGC( "[%s]->pfuncAfterProcessFilterPlugin ..." , p_travel_logpipe_filter_plugin->so_filename )
		if( p_travel_logpipe_filter_plugin->pfuncAfterProcessFilterPlugin )
		{
			gettimeofday( & tv_begin , NULL );
			nret2 = p_travel_logpipe_filter_plugin->pfuncAfterProcessFilterPlugin( p_env , p_travel_logpipe_filter_plugin , p_travel_logpipe_filter_plugin->context , filename_len , filename ) ;
			gettimeofday( & tv_end , NULL );
			DiffTimeval( & tv_begin , & tv_end , & tv_diff );
			if( nret2 < 0 )
			{
				ERRORLOGC( "[%s]->pfuncAfterProcessFilterPlugin failed[%d]" , p_travel_logpipe_filter_plugin->so_filename , nret2 )
			}
			else if( nret2 > 0 )
			{
				WARNLOGC( "[%s]->pfuncAfterProcessFilterPlugin return[%d]" , p_travel_logpipe_filter_plugin->so_filename , nret2 )
			}
			else
			{
				INFOLOGC( "[%s]->pfuncAfterProcessFilterPlugin ok , ELAPSE[%ld.%06ld]" , p_travel_logpipe_filter_plugin->so_filename , tv_diff.tv_sec , tv_diff.tv_usec )
			}
		}
	}
	
	/* 执行所有输出端写后函数 */
	list_for_each_entry( p_travel_logpipe_output_plugin , & (p_env->logpipe_output_plugins_list.this_node) , struct LogpipeOutputPlugin , this_node )
	{
		INFOLOGC( "[%s]->pfuncAfterWriteOutputPlugin ..." , p_travel_logpipe_output_plugin->so_filename )
		if( p_travel_logpipe_output_plugin->pfuncAfterWriteOutputPlugin )
		{
			gettimeofday( & tv_begin , NULL );
			nret2 = p_travel_logpipe_output_plugin->pfuncAfterWriteOutputPlugin( p_env , p_travel_logpipe_output_plugin , p_travel_logpipe_output_plugin->context , filename_len , filename ) ;
			gettimeofday( & tv_end , NULL );
			DiffTimeval( & tv_begin , & tv_end , & tv_diff );
			if( nret2 < 0 )
			{
				ERRORLOGC( "[%s]->pfuncAfterWriteOutputPlugin failed[%d]" , p_travel_logpipe_output_plugin->so_filename , nret2 )
			}
			else if( nret2 > 0 )
			{
				WARNLOGC( "[%s]->pfuncAfterWriteOutputPlugin return[%d]" , p_travel_logpipe_output_plugin->so_filename , nret2 )
			}
			else
			{
				INFOLOGC( "[%s]->pfuncAfterWriteOutputPlugin ok , ELAPSE[%ld.%06ld]" , p_travel_logpipe_output_plugin->so_filename , tv_diff.tv_sec , tv_diff.tv_usec )
			}
		}
	}
	
	return nret;
}

