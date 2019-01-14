#include "logpipe_api.h"

int	__LOGPIPE_FILTER_PACK_METADATA_0_1_0 = 1 ;

struct FilterPluginContext
{
	char			*system ;
	char			*server ;
	char			metadata[ 1024 ] ;
	uint16_t		metadata_head_len ;
	uint16_t		metadata_head2_len ;
	uint16_t		metadata_len ;
	
	uint16_t		*filename_len ;
	char			*filename ;
} ;

/* 插件环境结构 */
funcLoadFilterPluginConfig LoadFilterPluginConfig ;
int LoadFilterPluginConfig( struct LogpipeEnv *p_env , struct LogpipeFilterPlugin *p_logpipe_filter_plugin , struct LogpipePluginConfigItem *p_plugin_config_items , void **pp_context )
{
	struct FilterPluginContext	*p_plugin_ctx = NULL ;
	
	// uint16_t			len ;
	
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
	struct FilterPluginContext	*p_plugin_ctx = (struct FilterPluginContext *)p_context ;
	
	uint16_t			len ;
	
	memset( p_plugin_ctx->metadata , 0x00 , sizeof(p_plugin_ctx->metadata) );
	
	if( p_plugin_ctx->system )
	{
		len = snprintf( p_plugin_ctx->metadata+p_plugin_ctx->metadata_len , sizeof(p_plugin_ctx->metadata)-1-p_plugin_ctx->metadata_len , "[system=%s]" , p_plugin_ctx->system ) ;
		if( len > 0 )
			p_plugin_ctx->metadata_head_len += len ;
		else
		{
			ERRORLOGC( "system[%s] too long" , p_plugin_ctx->system )
			return -1;
		}
	}
	
	if( p_plugin_ctx->server )
	{
		len = snprintf( p_plugin_ctx->metadata+p_plugin_ctx->metadata_len , sizeof(p_plugin_ctx->metadata)-1-p_plugin_ctx->metadata_len , "[server=%s]" , p_plugin_ctx->server ) ;
		if( len > 0 )
			p_plugin_ctx->metadata_head_len += len ;
		else
		{
			ERRORLOGC( "server[%s] too long" , p_plugin_ctx->server )
			return -1;
		}
	}
	
	return 0;
}

funcBeforeProcessFilterPlugin BeforeProcessFilterPlugin ;
int BeforeProcessFilterPlugin( struct LogpipeEnv *p_env , struct LogpipeFilterPlugin *p_logpipe_filter_plugin , void *p_context , uint16_t filename_len , char *filename )
{
	struct FilterPluginContext	*p_plugin_ctx = (struct FilterPluginContext *)p_context ;
	
	uint16_t			len ;
	
	len = snprintf( p_plugin_ctx->metadata+p_plugin_ctx->metadata_head_len , sizeof(p_plugin_ctx->metadata)-1-p_plugin_ctx->metadata_head_len , "[filename=%.*s]" , filename_len,filename ) ;
	if( len > 0 )
		p_plugin_ctx->metadata_head2_len = p_plugin_ctx->metadata_head_len + len ;
	else
	{
		ERRORLOGC( "filename[%.*s] too long" , filename_len,filename )
		return 1;
	}
	
	return 0;
}

funcProcessFilterPlugin ProcessFilterPlugin ;
int ProcessFilterPlugin( struct LogpipeEnv *p_env , struct LogpipeFilterPlugin *p_logpipe_filter_plugin , void *p_context , uint64_t file_offset , uint64_t file_line , uint64_t *p_block_len , char *block_buf , uint64_t block_buf_size )
{
	struct FilterPluginContext	*p_plugin_ctx = (struct FilterPluginContext *)p_context ;
	
	uint16_t			len ;
	
	len = snprintf( p_plugin_ctx->metadata+p_plugin_ctx->metadata_head2_len , sizeof(p_plugin_ctx->metadata)-1-p_plugin_ctx->metadata_head2_len , "[offset=%d][line=%d]" , (int)file_offset , (int)file_line ) ;
	if( len > 0 )
		p_plugin_ctx->metadata_len = p_plugin_ctx->metadata_head2_len + len ;
	else
	{
		ERRORLOGC( "offset[%d] or line[%d] too long" , (int)file_offset , (int)file_line )
		return 1;
	}
	
	if( (*p_block_len) + p_plugin_ctx->metadata_len > block_buf_size-1 )
	{
		ERRORLOGC( "output buffer overflow" )
		return 1;
	}
	
	memmove( block_buf+p_plugin_ctx->metadata_len , block_buf , (*p_block_len) );
	memcpy( block_buf , p_plugin_ctx->metadata , p_plugin_ctx->metadata_len );
	(*p_block_len) += p_plugin_ctx->metadata_len ;
	
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

