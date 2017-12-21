#include "logpipe_api.h"

#include "zlib.h"

char	*__LOGPIPE_INPUT_EXEC_VERSION = "0.1.0" ;

struct InputPluginContext
{
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
		ERRORLOG( "malloc failed , errno[%d]" , errno );
		return -1;
	}
	memset( p_plugin_ctx , 0x00 , sizeof(struct InputPluginContext) );
	
	/* 解析插件配置 */
	p_plugin_ctx->cmd = QueryPluginConfigItem( p_plugin_config_items , "cmd" ) ;
	INFOLOG( "cmd[%s]" , p_plugin_ctx->cmd )
	if( p_plugin_ctx->cmd == NULL || p_plugin_ctx->cmd[0] == '\0' )
	{
		ERRORLOG( "expect config for 'cmd'" );
		return -1;
	}
	
	p_plugin_ctx->compress_algorithm = QueryPluginConfigItem( p_plugin_config_items , "compress_algorithm" ) ;
	if( p_plugin_ctx->compress_algorithm )
	{
		if( STRCMP( p_plugin_ctx->compress_algorithm , == , "deflate" ) )
		{
			;
		}
		else
		{
			ERRORLOG( "compress_algorithm[%s] invalid" , p_plugin_ctx->compress_algorithm );
			return -1;
		}
	}
	INFOLOG( "compress_algorithm[%s]" , p_plugin_ctx->compress_algorithm )
	
	p_plugin_ctx->output_filename = QueryPluginConfigItem( p_plugin_config_items , "output_filename" ) ;
	INFOLOG( "output_filename[%s]" , p_plugin_ctx->output_filename )
	if( p_plugin_ctx->output_filename == NULL || p_plugin_ctx->output_filename[0] == '\0' )
	{
		ERRORLOG( "expect config for 'output_filename'" );
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
	INFOLOG( "popen[%s] ..." , p_plugin_ctx->cmd );
	p_plugin_ctx->pp = popen( p_plugin_ctx->cmd , "r" ) ;
	if( p_plugin_ctx->pp == NULL )
	{
		ERRORLOG( "popen[%s] failed , errno[%d]" , p_plugin_ctx->cmd , errno );
		sleep(2);
		goto _GOTO_POPEN;
	}
	else
	{
		INFOLOG( "popen[%s] ok" , p_plugin_ctx->cmd );
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
	
	WriteAllOutputPlugins( p_env , p_logpipe_input_plugin , strlen(p_plugin_ctx->output_filename) , p_plugin_ctx->output_filename );
	
	return 0;
}

funcReadInputPlugin ReadInputPlugin ;
int ReadInputPlugin( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context , uint32_t *p_block_len , char *block_buf , int block_bufsize )
{
	struct InputPluginContext	*p_plugin_ctx = (struct InputPluginContext *)p_context ;
	
	int				nret = 0 ;
	
	if( p_plugin_ctx->compress_algorithm == NULL )
	{
		int			len ;
		
		INFOLOG( "fread popen ..." )
		len = fread( block_buf , 1 , block_bufsize-1 , p_plugin_ctx->pp ) ;
		if( len == -1 )
		{
			if( errno == EAGAIN )
			{
				INFOLOG( "fread none" )
				return LOGPIPE_READ_END_OF_INPUT;
			}
			else
			{
				ERRORLOG( "fread popen failed , errno[%d]" , errno )
				DeleteInputPluginEvent( p_env , p_logpipe_input_plugin , fileno(p_plugin_ctx->pp) );
				pclose( p_plugin_ctx->pp ); p_plugin_ctx->pp = NULL ;
				return -1;
			}
		}
		else if( len == 0 )
		{
			INFOLOG( "fread none" )
			return LOGPIPE_READ_END_OF_INPUT;
		}
		else
		{
			INFOLOG( "fread popen ok , [%d]bytes" , len )
			DEBUGHEXLOG( block_buf , len , NULL )
		}
		
		(*p_block_len) = len ;
	}
	else
	{
		char			block_in_buf[ LOGPIPE_UNCOMPRESS_BLOCK_BUFSIZE + 1 ] ;
		int			len ;
		
		INFOLOG( "fread popen ..." )
		len = fread( block_in_buf , 1 , LOGPIPE_UNCOMPRESS_BLOCK_BUFSIZE , p_plugin_ctx->pp ) ;
		if( len == -1 )
		{
			if( errno == EAGAIN )
			{
				INFOLOG( "fread none" )
				return LOGPIPE_READ_END_OF_INPUT;
			}
			else
			{
				ERRORLOG( "fread popen failed , errno[%d]" , errno )
				DeleteInputPluginEvent( p_env , p_logpipe_input_plugin , fileno(p_plugin_ctx->pp) );
				pclose( p_plugin_ctx->pp ); p_plugin_ctx->pp = NULL ;
				return -1;
			}
		}
		else if( len == 0 )
		{
			INFOLOG( "fread none" )
			return LOGPIPE_READ_END_OF_INPUT;
		}
		else
		{
			INFOLOG( "fread popen ok , [%d]bytes" , len )
			DEBUGHEXLOG( block_in_buf , len , NULL )
		}
		
		memset( block_buf , 0x00 , block_bufsize );
		nret = CompressInputPluginData( p_plugin_ctx->compress_algorithm , block_in_buf , len , block_buf , p_block_len ) ;
		if( nret )
		{
			ERRORLOG( "CompressInputPluginData failed[%d]" , nret )
			return -1;
		}
		else
		{
			DEBUGLOG( "CompressInputPluginData ok" )
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

