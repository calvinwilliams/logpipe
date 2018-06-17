#include "logpipe_api.h"

char	*__LOGPIPE_OUTPUT_FILE_VERSION = "0.2.0" ;

/* 跟踪文件信息结构 */
struct TraceFile
{
	char		filename[ PATH_MAX + 1 ] ;
	char		path_filename[ PATH_MAX + 1 ] ;
	int		fd ;
	
	struct rb_node	path_filename_rbnode ;
} ;

struct OutputPluginContext
{
	char			*path ;
	char			*uncompress_algorithm ;
	long			rotate_size ;
	char			exec_after_rotating_buffer[ PATH_MAX * 3 ] ;
	char			*exec_after_rotating ;
	
	struct rb_root		path_filename_rbtree ;
	struct TraceFile	*p_trace_file ;
} ;

LINK_RBTREENODE_STRING( LinkTraceFilePathFilenameTreeNode , struct OutputPluginContext , path_filename_rbtree , struct TraceFile , path_filename_rbnode , path_filename )
QUERY_RBTREENODE_STRING( QueryTraceFilePathFilenameTreeNode , struct OutputPluginContext , path_filename_rbtree , struct TraceFile , path_filename_rbnode , path_filename )
UNLINK_RBTREENODE( UnlinkTraceFilePathFilenameTreeNode , struct OutputPluginContext , path_filename_rbtree , struct TraceFile , path_filename_rbnode )

void FreeTraceFile( void *pv )
{
	struct TraceFile      *p_trace_file = (struct TraceFile *) pv ;
	
	if( p_trace_file )
	{
		free( p_trace_file ); p_trace_file = NULL ;
	}
	
	return;
}
DESTROY_RBTREE( DestroyTraceFileTree , struct OutputPluginContext , path_filename_rbtree , struct TraceFile , path_filename_rbnode , FreeTraceFile )

funcLoadOutputPluginConfig LoadOutputPluginConfig ;
int LoadOutputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , struct LogpipePluginConfigItem *p_plugin_config_items , void **pp_context )
{
	struct OutputPluginContext	*p_plugin_ctx = NULL ;
	char				*p = NULL ;
	
	/* 申请内存以存放插件上下文 */
	p_plugin_ctx = (struct OutputPluginContext *)malloc( sizeof(struct OutputPluginContext) ) ;
	if( p_plugin_ctx == NULL )
	{
		ERRORLOG( "malloc failed , errno[%d]" , errno );
		return -1;
	}
	memset( p_plugin_ctx , 0x00 , sizeof(struct OutputPluginContext) );
	
	p_plugin_ctx->path = QueryPluginConfigItem( p_plugin_config_items , "path" ) ;
	INFOLOG( "path[%s]" , p_plugin_ctx->path )
	if( p_plugin_ctx->path == NULL || p_plugin_ctx->path[0] == '\0' )
	{
		ERRORLOG( "expect config for 'path'" );
		return -1;
	}
	
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
	
	p = QueryPluginConfigItem( p_plugin_config_items , "rotate_size" ) ;
	if( p )
		p_plugin_ctx->rotate_size = size_atol(p) ;
	else
		p_plugin_ctx->rotate_size = 0 ;
	INFOLOG( "rotate_size[%ld]" , p_plugin_ctx->rotate_size )
	if( p_plugin_ctx->rotate_size < 0 )
	{
		ERRORLOG( "rotate_size[%ld] invalid" , p_plugin_ctx->rotate_size );
		return -1;
	}
	
	p = QueryPluginConfigItem( p_plugin_config_items , "exec_after_rotating" ) ;
	if( p )
	{
		int		buffer_len = 0 ;
		int		remain_len = sizeof(p_plugin_ctx->exec_after_rotating_buffer)-1 ;
		
		memset( p_plugin_ctx->exec_after_rotating_buffer , 0x00 , sizeof(p_plugin_ctx->exec_after_rotating_buffer) );
		JSONUNESCAPE_FOLD( p , strlen(p) , p_plugin_ctx->exec_after_rotating_buffer , buffer_len , remain_len )
		if( buffer_len == -1 )
		{
			ERRORLOG( "p[%s] invalid" , p );
			return -1;
		}
		
		p_plugin_ctx->exec_after_rotating = p_plugin_ctx->exec_after_rotating_buffer ;
	}
	INFOLOG( "exec_after_rotating[%s]" , p_plugin_ctx->exec_after_rotating )
	
	/* 设置插件环境上下文 */
	(*pp_context) = p_plugin_ctx ;
	
	return 0;
}

funcInitOutputPluginContext InitOutputPluginContext ;
int InitOutputPluginContext( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context )
{
	return 0;
}

funcOnOutputPluginEvent OnOutputPluginEvent;
int OnOutputPluginEvent( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context )
{
	return 0;
}

/* 打开文件 */
static int OpenFile( struct TraceFile *p_trace_file )
{
	p_trace_file->fd = open( p_trace_file->path_filename , O_CREAT|O_WRONLY|O_APPEND , 00777 ) ;
	if( p_trace_file->fd == -1 )
	{
		ERRORLOG( "open file[%s] failed , errno[%d]" , p_trace_file->path_filename , errno )
		return 1;
	}
	else
	{
		INFOLOG( "open file[%s] ok" , p_trace_file->path_filename )
	}
	
	return 0;
}

/* 关闭文件 */
static void CloseFile( struct TraceFile *p_trace_file )
{
	if( p_trace_file->fd >= 0 )
	{
		INFOLOG( "close file[%s]" , p_trace_file->path_filename )
		close( p_trace_file->fd ); p_trace_file->fd = -1 ;
	}
	
	return;
}

/* 文件大小转档处理 */
static int RotatingFile( struct OutputPluginContext *p_plugin_ctx )
{
	struct TraceFile	*p_trace_file = p_plugin_ctx->p_trace_file ;
	struct timeval		tv ;
	struct tm		tm ;
	char			old_filename[ PATH_MAX + 1 ] ;
	char			new_filename[ PATH_MAX + 1 ] ;
	
	int			nret = 0 ;
	
	/* 关闭文件 */
	CloseFile( p_trace_file );
	
	/* 改名 */
	snprintf( old_filename , sizeof(old_filename)-1 , "%s/%s" , p_plugin_ctx->path , p_trace_file->filename );
	gettimeofday( & tv , NULL );
	memset( & tm , 0x00 , sizeof(struct tm) );
	localtime_r( & (tv.tv_sec) , & tm );
	memset( new_filename , 0x00 , sizeof(new_filename) );
	snprintf( new_filename , sizeof(new_filename)-1 , "%s/_%s-%04d%02d%02d_%02d%02d%02d_%06ld" , p_plugin_ctx->path , p_trace_file->filename , tm.tm_year+1900 , tm.tm_mon+1 , tm.tm_mday , tm.tm_hour , tm.tm_min , tm.tm_sec , tv.tv_usec );
	
	setenv( "LOGPIPE_ROTATING_PATHNAME" , p_plugin_ctx->path , 1 );
	setenv( "LOGPIPE_ROTATING_NEW_FILENAME" , new_filename , 1 );
	
	nret = rename( old_filename , new_filename ) ;
	
	if( p_plugin_ctx->exec_after_rotating )
	{
		system( p_plugin_ctx->exec_after_rotating );
	}
	
	if( nret )
	{
		FATALLOG( "rename [%s] to [%s] failed , errno[%d]" , old_filename , new_filename , errno )
		return -1;
	}
	else
	{
		INFOLOG( "rename [%s] to [%s] ok" , old_filename , new_filename )
	}
	
	/* 打开文件 */
	OpenFile( p_trace_file );
	
	return 0;
}

funcBeforeWriteOutputPlugin BeforeWriteOutputPlugin ;
int BeforeWriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint16_t filename_len , char *filename )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	struct TraceFile		trace_file ;
	struct TraceFile		*p_trace_file = NULL ;
	int				len ;
	
	int				nret = 0 ;
	
	/* 查询文件跟踪信息缓存 */
	memset( & trace_file , 0x00 , sizeof(struct TraceFile) );
	snprintf( trace_file.path_filename , sizeof(trace_file.path_filename)-1 , "%s/%.*s" , p_plugin_ctx->path , filename_len , filename );
	p_trace_file = QueryTraceFilePathFilenameTreeNode( p_plugin_ctx , & trace_file ) ;
	if( p_trace_file )
	{
		/* 如果文件被删除了，清除文件跟踪结构缓存 */
		nret = access( p_trace_file->path_filename , F_OK ) ;
		if( nret == - 1 )
		{
			INFOLOG( "access[%.*s] failed , errno[%d]" , filename_len , filename , errno )
			UnlinkTraceFilePathFilenameTreeNode( p_plugin_ctx , p_trace_file );
			CloseFile( p_trace_file );
			FreeTraceFile( p_trace_file );
			
			p_trace_file = NULL ;
		}
		else
		{
			p_plugin_ctx->p_trace_file = p_trace_file ;
		}
	}
	if( p_trace_file == NULL )
	{
		/* 申请内存以存放文件跟踪结构 */
		p_trace_file = (struct TraceFile *)malloc( sizeof(struct TraceFile) ) ;
		if( p_trace_file == NULL )
		{
			ERRORLOG( "malloc failed , errno[%d]" , errno )
			return 1;
		}
		memset( p_trace_file , 0x00 , sizeof(struct TraceFile) );
		p_trace_file->fd = -1 ;
		
		/* 填充文件跟踪结构 */
		if( filename_len > sizeof(p_trace_file->filename)-1 )
		{
			ERRORLOG( "filename[%.*s] overflow" , filename_len , filename )
			FreeTraceFile( p_trace_file );
			return 1;
		}
		strncpy( p_trace_file->filename , filename , filename_len );
		len = snprintf( p_trace_file->path_filename , sizeof(p_trace_file->path_filename)-1 , "%s/%.*s" , p_plugin_ctx->path , filename_len , filename ) ;
		if( SNPRINTF_OVERFLOW(len,sizeof(p_trace_file->path_filename) ) )
		{
			ERRORLOG( "filename[%.*s] overflow" , filename_len , filename )
			FreeTraceFile( p_trace_file );
			return 1;
		}
		
		/* 打开文件 */
		nret = OpenFile( p_trace_file ) ;
		if( nret )
		{
			ERRORLOG( "open[%s] failed[%d]" , p_trace_file->path_filename , nret )
			FreeTraceFile( p_trace_file );
			return 1;
		}
		else
		{
			INFOLOG( "open[%s] ok" , p_trace_file->path_filename )
		}
		
		LinkTraceFilePathFilenameTreeNode( p_plugin_ctx , p_trace_file );
		
		p_plugin_ctx->p_trace_file = p_trace_file ;
	}
	
	return 0;
}

funcWriteOutputPlugin WriteOutputPlugin ;
int WriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint32_t file_offset , uint32_t file_line , uint32_t block_len , char *block_buf )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	struct TraceFile		*p_trace_file = p_plugin_ctx->p_trace_file ;
	
	int				len ;
	
	int				nret = 0 ;
	
	/* 如果未启用解压 */
	if( p_plugin_ctx->uncompress_algorithm == NULL )
	{
		len = writen( p_trace_file->fd , block_buf , block_len ) ;
		if( len == -1 )
		{
			ERRORLOG( "write block data to file[%d][%s] failed , errno[%d]" , p_trace_file->fd , p_trace_file->path_filename , errno )
			UnlinkTraceFilePathFilenameTreeNode( p_plugin_ctx , p_trace_file );
			CloseFile( p_trace_file );
			FreeTraceFile( p_trace_file );
			return 1;
		}
		else
		{
			INFOLOG( "write block data to file[%d][%s] ok , [%d]bytes" , p_trace_file->fd , p_trace_file->path_filename , block_len )
			DEBUGHEXLOG( block_buf , len , NULL )
		}
	}
	/* 如果启用了解压 */
	else
	{
		if( STRCMP( p_plugin_ctx->uncompress_algorithm , == , "deflate" ) )
		{
			char			block_out_buf[ LOGPIPE_BLOCK_BUFSIZE + 1 ] ;
			uint32_t		block_out_len ;
			
			memset( block_out_buf , 0x00 , sizeof(block_out_buf) );
			nret = UncompressInputPluginData( p_plugin_ctx->uncompress_algorithm , block_buf , block_len , block_out_buf , & block_out_len ) ;
			if( nret )
			{
				ERRORLOG( "UncompressInputPluginData failed[%d]" , nret )
				UnlinkTraceFilePathFilenameTreeNode( p_plugin_ctx , p_trace_file );
				CloseFile( p_trace_file );
				FreeTraceFile( p_trace_file );
				return -1;
			}
			else
			{
				DEBUGLOG( "UncompressInputPluginData ok" )
			}
			
			len = writen( p_trace_file->fd , block_out_buf , block_out_len ) ;
			if( len == -1 )
			{
				ERRORLOG( "write uncompress block data to file[%d][%s] failed , errno[%d]" , p_trace_file->fd , p_trace_file->path_filename , errno )
				UnlinkTraceFilePathFilenameTreeNode( p_plugin_ctx , p_trace_file );
				CloseFile( p_trace_file );
				FreeTraceFile( p_trace_file );
				return 1;
			}
			else
			{
				INFOLOG( "write uncompress block data to file[%d][%s] ok , [%d]bytes" , p_trace_file->fd , p_trace_file->path_filename , block_out_len )
				DEBUGHEXLOG( block_out_buf , len , NULL )
			}
		}
		else
		{
			ERRORLOG( "uncompress_algorithm[%s] invalid" , p_plugin_ctx->uncompress_algorithm );
			return -1;
		}
	}
	
	/* 如果启用文件大小转档 */
	if( p_plugin_ctx->rotate_size > 0 )
	{
		struct stat		file_stat ;
		
		memset( & file_stat , 0x00 , sizeof(struct stat) );
		nret = fstat( p_trace_file->fd , & file_stat ) ;
		if( nret == -1 )
		{
			ERRORLOG( "fstat[%s] failed[%d] , errno[%d]" , p_trace_file->path_filename , nret , errno )
			UnlinkTraceFilePathFilenameTreeNode( p_plugin_ctx , p_trace_file );
			CloseFile( p_trace_file );
			FreeTraceFile( p_trace_file );
		}
		else
		{
			/* 如果到达文件转档大小 */
			if( file_stat.st_size >= p_plugin_ctx->rotate_size )
			{
				INFOLOG( "file_stat.st_size[%d] > p_plugin_ctx->rotate_size[%d]" , file_stat.st_size , p_plugin_ctx->rotate_size )
				
				/* 文件大小转档处理 */
				nret = RotatingFile( p_plugin_ctx ) ;
				if( nret )
				{
					UnlinkTraceFilePathFilenameTreeNode( p_plugin_ctx , p_trace_file );
					CloseFile( p_trace_file );
					FreeTraceFile( p_trace_file );
				}
			}
		}
	}
	
	return 0;
}

funcAfterWriteOutputPlugin AfterWriteOutputPlugin ;
int AfterWriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint16_t filename_len , char *filename )
{
	return 0;
}

funcCleanOutputPluginContext CleanOutputPluginContext ;
int CleanOutputPluginContext( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	DestroyTraceFileTree( p_plugin_ctx );
	
	return 0;
}

funcUnloadOutputPluginConfig UnloadOutputPluginConfig ;
int UnloadOutputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void **pp_context )
{
	struct OutputPluginContext	**pp_plugin_ctx = (struct OutputPluginContext **)pp_context ;
	
	/* 释放内存以存放插件上下文 */
	free( (*pp_plugin_ctx) ); (*pp_plugin_ctx) = NULL ;
	
	return 0;
}

