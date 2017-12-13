#include "logpipe_api.h"

#include "zlib.h"

char	*__LOGPIPE_OUTPUT_FILE_VERSION = "0.1.0" ;

struct LogpipeOutputPluginContext
{
	char				*path ;
	char				*uncompress_algorithm ;
	
	int				fd ;
} ;

funcInitLogpipeOutputPlugin InitLogpipeOutputPlugin ;
int InitLogpipeOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , struct LogpipePluginConfigItem *p_plugin_config_items , void **pp_context )
{
	struct LogpipeOutputPluginContext	*p_plugin_ctx = NULL ;
	
	p_plugin_ctx = (struct LogpipeOutputPluginContext *)malloc( sizeof(struct LogpipeOutputPluginContext) ) ;
	if( p_plugin_ctx == NULL )
	{
		ERRORLOG( "malloc failed , errno[%d]" , errno );
		return -1;
	}
	memset( p_plugin_ctx , 0x00 , sizeof(struct LogpipeOutputPluginContext) );
	
	p_plugin_ctx->path = QueryPluginConfigItem( p_plugin_config_items , "path" ) ;
	INFOLOG( "path[%s]" , p_plugin_ctx->path )
	
	p_plugin_ctx->uncompress_algorithm = QueryPluginConfigItem( p_plugin_config_items , "uncompress_algorithm" ) ;
	if( p_plugin_ctx->uncompress_algorithm )
	{
		if( STRCMP( p_plugin_ctx->uncompress_algorithm , == , "deflate" ) )
		{
			;
		}
		else
		{
			ERRORLOG( "uncompress_algorithm[%s] invalid" , p_plugin_ctx->uncompress_algorithm );
			return -1;
		}
	}
	INFOLOG( "uncompress_algorithm[%s]" , p_plugin_ctx->uncompress_algorithm )
	
	p_plugin_ctx->fd = -1 ;
	
	/* ���ò������������ */
	(*pp_context) = p_plugin_ctx ;
	
	return 0;
}

funcInitLogpipeOutputPlugin2 InitLogpipeOutputPlugin2 ;
int InitLogpipeOutputPlugin2( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , struct LogpipePluginConfigItem *p_plugin_config_items , void **pp_context )
{
	return 0;
}

funcBeforeWriteLogpipeOutput BeforeWriteLogpipeOutput ;
int BeforeWriteLogpipeOutput( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint16_t filename_len , char *filename )
{
	struct LogpipeOutputPluginContext	*p_plugin_ctx = (struct LogpipeOutputPluginContext *)p_context ;
	
	char				path_filename[ PATH_MAX + 1 ] ;
	
	memset( path_filename , 0x00 , sizeof(path_filename) );
	snprintf( path_filename , sizeof(path_filename)-1 , "%s/%.*s" , p_plugin_ctx->path , filename_len , filename );
	p_plugin_ctx->fd = open( path_filename , O_CREAT|O_WRONLY|O_APPEND , 00777 ) ;
	if( p_plugin_ctx->fd == -1 )
	{
		ERRORLOG( "open file[%s] failed , errno[%d]" , path_filename , errno )
		return 1;
	}
	else
	{
		DEBUGLOG( "open file[%s] ok" , path_filename )
	}
	
	return 0;
}

funcWriteLogpipeOutput WriteLogpipeOutput ;
int WriteLogpipeOutput( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint32_t block_len , char *block_buf )
{
	struct LogpipeOutputPluginContext	*p_plugin_ctx = (struct LogpipeOutputPluginContext *)p_context ;
	
	int				len ;
	
	int				nret = 0 ;
	
	if( p_plugin_ctx->uncompress_algorithm == NULL )
	{
		len = writen( p_plugin_ctx->fd , block_buf , block_len ) ;
		if( len == -1 )
		{
			ERRORLOG( "write block data to file failed , errno[%d]" , errno )
			return 1;
		}
		else
		{
			INFOLOG( "write block data to file ok , [%d]bytes" , block_len )
			DEBUGHEXLOG( block_buf , len , NULL )
		}
	}
	else
	{
		if( STRCMP( p_plugin_ctx->uncompress_algorithm , == , "deflate" ) )
		{
			z_stream		inflate_strm ;
			
			char			block_out_buf[ LOGPIPE_BLOCK_BUFSIZE + 1 ] ;
			uint32_t		block_out_len ;
			
			memset( & inflate_strm , 0x00 , sizeof(z_stream) );
			nret = inflateInit( & inflate_strm ) ;
			if( nret != Z_OK )
			{
				FATALLOG( "inflateInit failed[%d]" , nret );
				return -1;
			}
			
			inflate_strm.avail_in = block_len ;
			inflate_strm.next_in = (Bytef*)block_buf ;
			
			do
			{
				inflate_strm.avail_out = sizeof(block_out_buf)-1 ;
				inflate_strm.next_out = (Bytef*)block_out_buf ;
				nret = inflate( & inflate_strm , Z_NO_FLUSH ) ;
				if( nret == Z_STREAM_ERROR )
				{
					FATALLOG( "inflate return Z_STREAM_ERROR" )
					inflateEnd( & inflate_strm );
					return 1;
				}
				else if( nret == Z_NEED_DICT || nret == Z_DATA_ERROR || nret == Z_MEM_ERROR )
				{
					FATALLOG( "inflate return[%d]" , nret )
					inflateEnd( & inflate_strm );
					return 1;
				}
				block_out_len = sizeof(block_out_buf)-1 - inflate_strm.avail_out ;
				
				len = writen( p_plugin_ctx->fd , block_out_buf , block_out_len ) ;
				if( len == -1 )
				{
					ERRORLOG( "write uncompress block data to file failed , errno[%d]" , errno )
					inflateEnd( & inflate_strm );
					return 1;
				}
				else
				{
					INFOLOG( "write uncompress block data to file ok , [%d]bytes" , block_out_len )
					DEBUGHEXLOG( block_out_buf , len , NULL )
				}
			}
			while( inflate_strm.avail_out == 0 );
			
			inflateEnd( & inflate_strm );
		}
		else
		{
			ERRORLOG( "uncompress_algorithm[%s] invalid" , p_plugin_ctx->uncompress_algorithm );
			return -1;
		}
	}
	
	return 0;
}

funcAfterWriteLogpipeOutput AfterWriteLogpipeOutput ;
int AfterWriteLogpipeOutput( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context )
{
	struct LogpipeOutputPluginContext	*p_plugin_ctx = (struct LogpipeOutputPluginContext *)p_context ;
	
	if( p_plugin_ctx->fd >= 0 )
	{
		INFOLOG( "close file" )
		close( p_plugin_ctx->fd ); p_plugin_ctx->fd = -1 ;
	}
	
	return 0;
}

funcCleanLogpipeOutputPlugin CleanLogpipeOutputPlugin ;
int CleanLogpipeOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context )
{
	struct LogpipeOutputPluginContext	*p_plugin_ctx = (struct LogpipeOutputPluginContext *)p_context ;
	
	INFOLOG( "free p_plugin_ctx" )
	free( p_plugin_ctx );
	
	return 0;
}
