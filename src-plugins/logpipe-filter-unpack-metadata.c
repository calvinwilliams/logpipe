#include "logpipe_api.h"

int	__LOGPIPE_FILTER_UNPACK_METADATA_0_1_0 = 1 ;

/*
[[system=...][server=...][filename=...][offset=...][line=...]]log\n
*/

#define UNPACK_METADATA_SYSTEM_INDEX	1
#define UNPACK_METADATA_SERVER_INDEX	2

struct FilterPluginContext
{
	char			*system ;
	char			*server ;
} ;

/* 插件环境结构 */
funcLoadFilterPluginConfig LoadFilterPluginConfig ;
int LoadFilterPluginConfig( struct LogpipeEnv *p_env , struct LogpipeFilterPlugin *p_logpipe_filter_plugin , struct LogpipePluginConfigItem *p_plugin_config_items , void **pp_context )
{
	struct FilterPluginContext	*p_plugin_ctx = NULL ;
	
	/* 申请内存以存放插件上下文 */
	p_plugin_ctx = (struct FilterPluginContext *)malloc( sizeof(struct FilterPluginContext) ) ;
	if( p_plugin_ctx == NULL )
	{
		ERRORLOGC( "malloc failed , errno[%d]" , errno )
		return -1;
	}
	memset( p_plugin_ctx , 0x00 , sizeof(struct FilterPluginContext) );
	
	p_plugin_ctx->system = QueryPluginConfigItem( p_plugin_config_items , "system" ) ;
	INFOLOGC( "system[%s]" , p_plugin_ctx->system )
	
	p_plugin_ctx->server = QueryPluginConfigItem( p_plugin_config_items , "server" ) ;
	INFOLOGC( "server[%s]" , p_plugin_ctx->server )
	
	/* 设置插件环境上下文 */
	(*pp_context) = p_plugin_ctx ;
	
	return 0;
}

funcInitFilterPluginContext InitFilterPluginContext ;
int InitFilterPluginContext( struct LogpipeEnv *p_env , struct LogpipeFilterPlugin *p_logpipe_filter_plugin , void *p_context )
{
	return 0;
}

funcBeforeProcessFilterPlugin BeforeProcessFilterPlugin ;
int BeforeProcessFilterPlugin( struct LogpipeEnv *p_env , struct LogpipeFilterPlugin *p_logpipe_filter_plugin , void *p_context , uint16_t filename_len , char *filename )
{
	return 0;
}

funcProcessFilterPlugin ProcessFilterPlugin ;
int ProcessFilterPlugin( struct LogpipeEnv *p_env , struct LogpipeFilterPlugin *p_logpipe_filter_plugin , void *p_context , uint64_t file_offset , uint64_t file_line , uint64_t *p_block_len , char *block_buf , uint64_t block_buf_size )
{
	struct FilterPluginContext	*p_plugin_ctx = (struct FilterPluginContext *)p_context ;
	char				*p_begin_bracket = NULL ;
	char				*p_left_bracket = NULL ;
	char				*p_equal_sign = NULL ;
	char				*p_right_bracket = NULL ;
	char				*p_end_bracket = NULL ;
	
	SetInputPluginFilename( p_env , 0 , "" );
	SetInputPluginFileOffset( p_env , 0 );
	SetInputPluginFileLine( p_env , 0 );
	if( p_plugin_ctx->system )
		SetInputPluginTag( p_env , UNPACK_METADATA_SYSTEM_INDEX , strlen(p_plugin_ctx->system) , p_plugin_ctx->system );
	else
		SetInputPluginTag( p_env , UNPACK_METADATA_SYSTEM_INDEX , 0 , "" );
	if( p_plugin_ctx->server )
		SetInputPluginTag( p_env , UNPACK_METADATA_SERVER_INDEX , strlen(p_plugin_ctx->server) , p_plugin_ctx->server );
	else
		SetInputPluginTag( p_env , UNPACK_METADATA_SERVER_INDEX , 0 , "" );
	
	p_begin_bracket = block_buf ;
	if( (*p_begin_bracket) != '[' )
	{
		ERRORLOGC( "expect begin bracket in block_buf[%.*s]" , (int)(*p_block_len),block_buf )
		return 1;
	}
	
	p_left_bracket = p_begin_bracket+1 ;
	while(1)
	{
		if( (*p_left_bracket) == ']' )
		{
			break;
		}
		else if( (*p_left_bracket) == '[' )
		{
		}
		else
		{
			ERRORLOGC( "bracket specifications invalid in block_buf[%.*s]" , (int)(*p_block_len),block_buf )
			return 1;
		}
		
		p_equal_sign = strchr( p_left_bracket+1 , '=' ) ;
		if( p_equal_sign == NULL )
		{
			ERRORLOGC( "bracket specifications invalid in block_buf[%.*s]" , (int)(*p_block_len),block_buf )
			return 1;
		}
		
		p_right_bracket = strchr( p_equal_sign+1 , ']' ) ;
		if( p_right_bracket == NULL )
		{
			ERRORLOGC( "bracket specifications invalid in block_buf[%.*s]" , (int)(*p_block_len),block_buf )
			return 1;
		}
		
		if( p_equal_sign-p_left_bracket-1 == 6 && STRNCMP( p_left_bracket+1 , == , "system" , 6 ) )
		{
			if( p_plugin_ctx->system == NULL )
				SetInputPluginTag( p_env , UNPACK_METADATA_SYSTEM_INDEX , p_right_bracket-p_equal_sign-1 , p_equal_sign+1 );
		}
		else if( p_equal_sign-p_left_bracket-1 == 6 && STRNCMP( p_left_bracket+1 , == , "server" , 6 ) )
		{
			if( p_plugin_ctx->server == NULL )
				SetInputPluginTag( p_env , UNPACK_METADATA_SERVER_INDEX , p_right_bracket-p_equal_sign-1 , p_equal_sign+1 );
		}
		else if( p_equal_sign-p_left_bracket-1 == 6 && STRNCMP( p_left_bracket+1 , == , "filename" , 6 ) )
		{
			SetInputPluginFilename( p_env , p_right_bracket-p_equal_sign-1 , p_equal_sign+1 );
		}
		else if( p_equal_sign-p_left_bracket-1 == 6 && STRNCMP( p_left_bracket+1 , == , "offset" , 6 ) )
		{
			SetInputPluginFileOffset( p_env , strnlen(p_equal_sign+1,p_right_bracket-p_equal_sign-1) );
		}
		else if( p_equal_sign-p_left_bracket-1 == 6 && STRNCMP( p_left_bracket+1 , == , "line" , 6 ) )
		{
			SetInputPluginFileOffset( p_env , strnlen(p_equal_sign+1,p_right_bracket-p_equal_sign-1) );
		}
		
		p_left_bracket = p_right_bracket+1 ;
	}
	
	p_end_bracket = p_begin_bracket ;
	(*p_block_len) -= p_end_bracket-p_begin_bracket-1 ;
	memmove( block_buf , p_end_bracket+1 , (*p_block_len) );
	
	return 0;
}

funcAfterProcessFilterPlugin AfterProcessFilterPlugin ;
int AfterProcessFilterPlugin( struct LogpipeEnv *p_env , struct LogpipeFilterPlugin *p_logpipe_filter_plugin , void *p_context , uint16_t filename_len , char *filename )
{
	return 0;
}

funcCleanFilterPluginContext CleanFilterPluginContext ;
int CleanFilterPluginContext( struct LogpipeEnv *p_env , struct LogpipeFilterPlugin *p_logpipe_filter_plugin , void *p_context )
{
	return 0;
}

funcUnloadFilterPluginConfig UnloadFilterPluginConfig ;
int UnloadFilterPluginConfig( struct LogpipeEnv *p_env , struct LogpipeFilterPlugin *p_logpipe_filter_plugin , void **pp_context )
{
	struct OutputPluginContext	**pp_plugin_ctx = (struct OutputPluginContext **)pp_context ;
	
	/* 释放内存以存放插件上下文 */
	free( (*pp_plugin_ctx) ); (*pp_plugin_ctx) = NULL ;
	
	return 0;
}

