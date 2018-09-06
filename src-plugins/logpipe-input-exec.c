#include "logpipe_api.h"

int	__LOGPIPE_INPUT_EXEC_VERSION_0_1_0 = 1 ;

#define CMD_BUFSIZE		1024*1024

struct InputPluginContext
{
	char		cmd_buffer[ CMD_BUFSIZE + 1 ] ;
	char		*cmd ;
	char		*compress_algorithm ;
	char		*output_filename ;
	
	FILE		*pp ;
} ;

funcLoadInputPluginConfig LoadInputPluginConfig ;
int LoadInputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , struct LogpipePluginConfigItem *p_plugin_config_items , void **pp_context )
{
	struct InputPluginContext	*p_plugin_ctx = NULL ;
	
	/* 申请内存以存放插件上下文 */
	p_plugin_ctx = (struct InputPluginContext *)malloc( sizeof(struct InputPluginContext) ) ;
	if( p_plugin_ctx == NULL )
	{
		ERRORLOGC( "malloc failed , errno[%d]" , errno )
		return -1;
	}
	memset( p_plugin_ctx , 0x00 , sizeof(struct InputPluginContext) );
	
	/* 解析插件配置 */
	p_plugin_ctx->cmd = QueryPluginConfigItem( p_plugin_config_items , "cmd" ) ;
	INFOLOGC( "cmd[%s]" , p_plugin_ctx->cmd )
	if( p_plugin_ctx->cmd == NULL || p_plugin_ctx->cmd[0] == '\0' )
	{
		ERRORLOGC( "expect config for 'cmd'" )
		return -1;
	}
	
	{
		int		buffer_len = 0 ;
		int		remain_len = sizeof(p_plugin_ctx->cmd_buffer)-1 ;
		
		memset( p_plugin_ctx->cmd_buffer , 0x00 , sizeof(p_plugin_ctx->cmd_buffer) );
		JSONUNESCAPE_FOLD( p_plugin_ctx->cmd , strlen(p_plugin_ctx->cmd) , p_plugin_ctx->cmd_buffer , buffer_len , remain_len )
		if( buffer_len == -1 )
		{
			ERRORLOGC( "output_tempalte[%s] invalid" , p_plugin_ctx->cmd )
			return -1;
		}
		
		p_plugin_ctx->cmd = p_plugin_ctx->cmd_buffer ;
	}
	INFOLOGC( "cmd[%s]" , p_plugin_ctx->cmd )
	
	p_plugin_ctx->compress_algorithm = QueryPluginConfigItem( p_plugin_config_items , "compress_algorithm" ) ;
	if( p_plugin_ctx->compress_algorithm )
	{
		if( STRCMP( p_plugin_ctx->compress_algorithm , == , "deflate" ) )
		{
			;
		}
		else
		{
			ERRORLOGC( "compress_algorithm[%s] invalid" , p_plugin_ctx->compress_algorithm )
			return -1;
		}
	}
	INFOLOGC( "compress_algorithm[%s]" , p_plugin_ctx->compress_algorithm )
	
	p_plugin_ctx->output_filename = QueryPluginConfigItem( p_plugin_config_items , "output_filename" ) ;
	INFOLOGC( "output_filename[%s]" , p_plugin_ctx->output_filename )
	if( p_plugin_ctx->output_filename == NULL || p_plugin_ctx->output_filename[0] == '\0' )
	{
		ERRORLOGC( "expect config for 'output_filename'" )
		return -1;
	}
	
	/* 设置插件环境上下文 */
	(*pp_context) = p_plugin_ctx ;
	
	return 0;
}

funcInitInputPluginContext InitInputPluginContext ;
int InitInputPluginContext( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	struct InputPluginContext	*p_plugin_ctx = (struct InputPluginContext *)(p_context) ;
	
_GOTO_POPEN :
	
	/* 执行命令 */
	INFOLOGC( "popen[%s] ..." , p_plugin_ctx->cmd )
	p_plugin_ctx->pp = popen( p_plugin_ctx->cmd , "r" ) ;
	if( p_plugin_ctx->pp == NULL )
	{
		ERRORLOGC( "popen[%s] failed , errno[%d]" , p_plugin_ctx->cmd , errno )
		sleep(2);
		goto _GOTO_POPEN;
	}
	else
	{
		INFOLOGC( "popen[%s] ok" , p_plugin_ctx->cmd )
	}
	
	/* 设置为非堵塞 */
	{
		int     opts ;
		opts = fcntl( fileno(p_plugin_ctx->pp) , F_GETFL ) ;
		opts |= O_NONBLOCK ;
		fcntl( fileno(p_plugin_ctx->pp) , F_SETFL , opts ) ;
	}
	
	/* 设置输入描述字 */
	AddInputPluginEvent( p_env , p_logpipe_input_plugin , fileno(p_plugin_ctx->pp) );
	
	return 0;
}

funcOnInputPluginEvent OnInputPluginEvent ;
int OnInputPluginEvent( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	struct InputPluginContext	*p_plugin_ctx = (struct InputPluginContext *)p_context ;
	
	if( p_plugin_ctx->pp == NULL )
	{
		OnInputPluginEvent( p_env , p_logpipe_input_plugin , p_context );
	}
	
	/* 激活一轮从输入插件读，写到所有输出插件流程处理 */
	WriteAllOutputPlugins( p_env , p_logpipe_input_plugin , strlen(p_plugin_ctx->output_filename) , p_plugin_ctx->output_filename );
	
	return 0;
}

funcReadInputPlugin ReadInputPlugin ;
int ReadInputPlugin( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context , uint64_t *p_file_offset , uint64_t *p_file_line , uint64_t *p_block_len , char *block_buf , uint64_t block_bufsize )
{
	struct InputPluginContext	*p_plugin_ctx = (struct InputPluginContext *)p_context ;
	
	int				nret = 0 ;
	
	/* 如果未启用压缩 */
	if( p_plugin_ctx->compress_algorithm == NULL )
	{
		int			len ;
		
		INFOLOGC( "fread popen ..." )
		memset( block_buf , 0x00 , block_bufsize );
		len = fread( block_buf , 1 , block_bufsize-1 , p_plugin_ctx->pp ) ;
		if( len == -1 )
		{
			if( errno == EAGAIN )
			{
				INFOLOGC( "fread none" )
				return LOGPIPE_READ_END_OF_INPUT;
			}
			else
			{
				ERRORLOGC( "fread popen failed , errno[%d]" , errno )
				DeleteInputPluginEvent( p_env , p_logpipe_input_plugin , fileno(p_plugin_ctx->pp) );
				pclose( p_plugin_ctx->pp ); p_plugin_ctx->pp = NULL ;
				return -1;
			}
		}
		else if( len == 0 )
		{
			INFOLOGC( "fread none" )
			return LOGPIPE_READ_END_OF_INPUT;
		}
		else
		{
			INFOLOGC( "fread popen ok , [%d]bytes" , len )
			DEBUGHEXLOGC( block_buf , len , NULL )
		}
		
		(*p_block_len) = len ;
	}
	/* 如果启用了压缩 */
	else
	{
		char			block_in_buf[ LOGPIPE_UNCOMPRESS_BLOCK_BUFSIZE + 1 ] ;
		int			len ;
		
		INFOLOGC( "fread popen ..." )
		memset( block_in_buf , 0x00 , sizeof(block_in_buf) );
		len = fread( block_in_buf , 1 , LOGPIPE_UNCOMPRESS_BLOCK_BUFSIZE , p_plugin_ctx->pp ) ;
		if( len == -1 )
		{
			if( errno == EAGAIN )
			{
				INFOLOGC( "fread none" )
				return LOGPIPE_READ_END_OF_INPUT;
			}
			else
			{
				ERRORLOGC( "fread popen failed , errno[%d]" , errno )
				DeleteInputPluginEvent( p_env , p_logpipe_input_plugin , fileno(p_plugin_ctx->pp) );
				pclose( p_plugin_ctx->pp ); p_plugin_ctx->pp = NULL ;
				return -1;
			}
		}
		else if( len == 0 )
		{
			INFOLOGC( "fread none" )
			return LOGPIPE_READ_END_OF_INPUT;
		}
		else
		{
			INFOLOGC( "fread popen ok , [%d]bytes" , len )
			DEBUGHEXLOGC( block_in_buf , len , NULL )
		}
		
		memset( block_buf , 0x00 , block_bufsize );
		nret = CompressInputPluginData( p_plugin_ctx->compress_algorithm , block_in_buf , len , block_buf , p_block_len ) ;
		if( nret )
		{
			ERRORLOGC( "CompressInputPluginData failed[%d]" , nret )
			return -1;
		}
		else
		{
			DEBUGLOGC( "CompressInputPluginData ok" )
		}
	}
	
	return 0;
}

funcCleanInputPluginContext CleanInputPluginContext ;
int CleanInputPluginContext( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	struct InputPluginContext	*p_plugin_ctx = (struct InputPluginContext *)p_context ;
	
	if( p_plugin_ctx->pp == NULL )
	{
		OnInputPluginEvent( p_env , p_logpipe_input_plugin , p_context );
	}
	
	return 0;
}

funcUnloadInputPluginConfig UnloadInputPluginConfig ;
int UnloadInputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void **pp_context )
{
	struct InputPluginContext	**pp_plugin_ctx = (struct InputPluginContext **)pp_context ;
	
	/* 释放内存以存放插件上下文 */
	free( (*pp_plugin_ctx) ); (*pp_plugin_ctx) = NULL ;
	
	return 0;
}

