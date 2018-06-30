#include "logpipe_api.h"

#include "hdfs.h"

/* command for compile && install
make logpipe-output-hdfs.so && cp logpipe-output-hdfs.so ~/so/
*/

/* add to ~/.profile
# for hadoop
export HADOOP_HOME=/home/hdfs/expack/hadoop
export PATH=$HADOOP_HOME/bin:$PATH
export HADOOP_CLASSPATH=`hadoop classpath --glob`
export CLASSPATH=$HADOOP_CLASSPATH:$CLASSPATH
export LD_LIBRARY_PATH=$HADOOP_HOME/lib/native:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$JAVA_HOME/jre/lib/amd64/server:$LD_LIBRARY_PATH
*/

/* 写目录规则
由于HDFS C API不支持追加写，只能保持文件打开句柄，数据落地目录规则为：
* 每次新启动 创建并写入 "配置目录/YYYYMMDD_hhmmss/"
* 午夜零时 创建并写入 "配置目录/YYYYMMDD/"
*/

char	*__LOGPIPE_OUTPUT_HDFS_VERSION = "0.1.0" ;

/* 跟踪文件信息结构 */
struct TraceFile
{
	char			path_filename[ PATH_MAX + 1 ] ;
	
	hdfsFS			fs ;
	hdfsFile		file ;
	
	struct rb_node		filename_rbnode ;
} ;

/* 插件环境结构 */
struct OutputPluginContext
{
	char			*name_node ;
	int			port ;
	char			*user ;
	char			*path ;
	char			*uncompress_algorithm ;
	
	hdfsFS			fs ;
	
	char			pathname[ PATH_MAX + 1 ] ;
	struct tm		stime ;
	
	struct TraceFile	*p_trace_file ;
	
	struct rb_root		filename_rbtree ;
} ;

LINK_RBTREENODE_STRING( LinkTraceFilenameTreeNode , struct OutputPluginContext , filename_rbtree , struct TraceFile , filename_rbnode , path_filename )
QUERY_RBTREENODE_STRING( QueryTraceFilenameTreeNode , struct OutputPluginContext , filename_rbtree , struct TraceFile , filename_rbnode , path_filename )
UNLINK_RBTREENODE( UnlinkTraceFileWdTreeNode , struct OutputPluginContext , filename_rbtree , struct TraceFile , filename_rbnode )

void FreeTraceFile( void *pv )
{
	struct TraceFile      *p_trace_file = (struct TraceFile *) pv ;
	
	if( p_trace_file )
	{
		INFOLOGC( "hdfsCloseFile[%s]" , p_trace_file->path_filename )
		hdfsCloseFile( p_trace_file->fs , p_trace_file->file );
		
		free( p_trace_file );
		p_trace_file = NULL ;
	}
	
	return;
}

DESTROY_RBTREE( DestroyTraceFilenameTree , struct OutputPluginContext , filename_rbtree , struct TraceFile , filename_rbnode , FreeTraceFile )

funcLoadOutputPluginConfig LoadOutputPluginConfig ;
int LoadOutputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , struct LogpipePluginConfigItem *p_plugin_config_items , void **pp_context )
{
	struct OutputPluginContext	*p_plugin_ctx = NULL ;
	char				*p = NULL ;
	
	/* 申请内存以存放插件上下文 */
	p_plugin_ctx = (struct OutputPluginContext *)malloc( sizeof(struct OutputPluginContext) ) ;
	if( p_plugin_ctx == NULL )
	{
		ERRORLOGC( "malloc failed , errno[%d]" , errno )
		return -1;
	}
	memset( p_plugin_ctx , 0x00 , sizeof(struct OutputPluginContext) );
	
	p_plugin_ctx->name_node = QueryPluginConfigItem( p_plugin_config_items , "name_node" ) ;
	INFOLOGC( "name_node[%s]" , p_plugin_ctx->name_node )
	if( p_plugin_ctx->name_node == NULL || p_plugin_ctx->name_node[0] == '\0' )
	{
		ERRORLOGC( "expect config for 'name_node'" )
		return -1;
	}
	
	p = QueryPluginConfigItem( p_plugin_config_items , "port" ) ;
	if( p == NULL || p[0] == '\0' )
	{
		ERRORLOGC( "expect config for 'port'" )
		return -1;
	}
	p_plugin_ctx->port = atoi(p) ;
	INFOLOGC( "port[%d]" , p_plugin_ctx->port )
	if( p_plugin_ctx->port < 0 )
	{
		ERRORLOGC( "port[%s] invalid" , p )
		return -1;
	}
	
	p_plugin_ctx->user = QueryPluginConfigItem( p_plugin_config_items , "user" ) ;
	INFOLOGC( "user[%s]" , p_plugin_ctx->user )
	
	p_plugin_ctx->path = QueryPluginConfigItem( p_plugin_config_items , "path" ) ;
	INFOLOGC( "path[%s]" , p_plugin_ctx->path )
	if( p_plugin_ctx->path == NULL || p_plugin_ctx->path[0] == '\0' )
	{
		ERRORLOGC( "expect config for 'path'" )
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
			ERRORLOGC( "uncompress_algorithm[%s] invalid" , p_plugin_ctx->uncompress_algorithm )
			return -1;
		}
	}
	INFOLOGC( "uncompress_algorithm[%s]" , p_plugin_ctx->uncompress_algorithm )
	
	/* 设置插件环境上下文 */
	(*pp_context) = p_plugin_ctx ;
	
	return 0;
}

funcInitOutputPluginContext InitOutputPluginContext ;
int InitOutputPluginContext( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	time_t				tt ;
	
	int				nret = 0 ;
	
	/* 连接HDFS */
	if( p_plugin_ctx->user && p_plugin_ctx->user[0] )
	{
		INFOLOGC( "hdfsConnectAsUser ... name_node[%s] port[%d] user[%s]" , p_plugin_ctx->name_node , p_plugin_ctx->port , p_plugin_ctx->user )
		p_plugin_ctx->fs = hdfsConnectAsUser( p_plugin_ctx->name_node , p_plugin_ctx->port , p_plugin_ctx->user ) ;
		if( p_plugin_ctx->fs == NULL )
		{
			ERRORLOGC( "hdfsConnectAsUser failed , name_node[%s] port[%d] user[%s]" , p_plugin_ctx->name_node , p_plugin_ctx->port , p_plugin_ctx->user )
			return -1;
		}
		else
		{
			INFOLOGC( "hdfsConnectAsUser ok , name_node[%s] port[%d] user[%s]" , p_plugin_ctx->name_node , p_plugin_ctx->port , p_plugin_ctx->user )
		}
	}
	else
	{
		INFOLOGC( "hdfsConnect ... name_node[%s] port[%d]" , p_plugin_ctx->name_node , p_plugin_ctx->port )
		p_plugin_ctx->fs = hdfsConnect( p_plugin_ctx->name_node , p_plugin_ctx->port ) ;
		if( p_plugin_ctx->fs == NULL )
		{
			ERRORLOGC( "hdfsConnect failed , name_node[%s] port[%d]" , p_plugin_ctx->name_node , p_plugin_ctx->port )
			return -1;
		}
		else
		{
			INFOLOGC( "hdfsConnect ok , name_node[%s] port[%d]" , p_plugin_ctx->name_node , p_plugin_ctx->port )
		}
	}
	
	/* 创建新启动写目录 */
	time( & tt );
	localtime_r( & tt , & (p_plugin_ctx->stime) );
	memset( p_plugin_ctx->pathname , 0x00 , sizeof(p_plugin_ctx->pathname) );
	snprintf( p_plugin_ctx->pathname , sizeof(p_plugin_ctx->pathname) , "%s/%04d%02d%02d_%02d%02d%02d" , p_plugin_ctx->path , p_plugin_ctx->stime.tm_year+1900 , p_plugin_ctx->stime.tm_mon , p_plugin_ctx->stime.tm_mday , p_plugin_ctx->stime.tm_hour , p_plugin_ctx->stime.tm_min , p_plugin_ctx->stime.tm_sec );
	nret = hdfsCreateDirectory( p_plugin_ctx->fs , p_plugin_ctx->pathname ) ;
	if( nret )
	{
		ERRORLOGC( "hdfsCreateDirectory[%s] failed[%d]" , p_plugin_ctx->pathname , nret )
		return -1;
	}
	else
	{
		DEBUGLOGC( "hdfsCreateDirectory[%s] ok" , p_plugin_ctx->pathname )
	}
	
	return 0;
}

funcOnOutputPluginEvent OnOutputPluginEvent;
int OnOutputPluginEvent( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context )
{
	return 0;
}

funcBeforeWriteOutputPlugin BeforeWriteOutputPlugin ;
int BeforeWriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint16_t filename_len , char *filename )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	time_t				tt ;
	struct tm			stime ;
	struct TraceFile		trace_file ;
	
	int				nret = 0 ;
	
	/* 午夜零时，创建新一天的写目录 */
	time( & tt );
	localtime_r( & tt , & stime );
	if( stime.tm_year != p_plugin_ctx->stime.tm_year || stime.tm_mon != p_plugin_ctx->stime.tm_mon || stime.tm_mday != p_plugin_ctx->stime.tm_mday )
	{
		INFOLOGC( "DestroyTraceFilenameTree" )
		DestroyTraceFilenameTree( p_plugin_ctx );
		
		memset( p_plugin_ctx->pathname , 0x00 , sizeof(p_plugin_ctx->pathname) );
		snprintf( p_plugin_ctx->pathname , sizeof(p_plugin_ctx->pathname) , "%s/%04d%02d%02d" , p_plugin_ctx->path , p_plugin_ctx->stime.tm_year+1900 , p_plugin_ctx->stime.tm_mon , p_plugin_ctx->stime.tm_mday );
		nret = hdfsCreateDirectory( p_plugin_ctx->fs , p_plugin_ctx->pathname ) ;
		if( nret )
		{
			ERRORLOGC( "hdfsCreateDirectory[%s] failed[%d]" , p_plugin_ctx->pathname , nret )
			return -1;
		}
		else
		{
			DEBUGLOGC( "hdfsCreateDirectory[%s] ok" , p_plugin_ctx->pathname )
		}
		
		INFOLOGC( "DestroyTraceFilenameTree" )
		DestroyTraceFilenameTree( p_plugin_ctx );
	}
	
	/* 查询文件跟踪结构 */
	memset( & trace_file , 0x00 , sizeof(struct TraceFile) );
	snprintf( trace_file.path_filename , sizeof(trace_file.path_filename)-1 , "%s/%.*s" , p_plugin_ctx->pathname , filename_len , filename );
	p_plugin_ctx->p_trace_file = QueryTraceFilenameTreeNode( p_plugin_ctx , & trace_file ) ;
	if( p_plugin_ctx->p_trace_file == NULL )
	{
		p_plugin_ctx->p_trace_file = (struct TraceFile *)malloc( sizeof(struct TraceFile) ) ;
		if( p_plugin_ctx->p_trace_file == NULL )
		{
			ERRORLOGC( "malloc failed , errno[%d]" , errno )
			return -1;
		}
		memset( p_plugin_ctx->p_trace_file , 0x00 , sizeof(struct TraceFile) );
		
		strcpy( p_plugin_ctx->p_trace_file->path_filename , trace_file.path_filename );
		
		p_plugin_ctx->p_trace_file->fs = p_plugin_ctx->fs ;
		
		/* 打开HDFS文件 */
		p_plugin_ctx->p_trace_file->file = hdfsOpenFile( p_plugin_ctx->fs , p_plugin_ctx->p_trace_file->path_filename , O_CREAT|O_WRONLY , 0 , 0 , 0 ) ;
		if( p_plugin_ctx->p_trace_file->file == NULL )
		{
			ERRORLOGC( "hdfsOpenFile[%s] failed , errno[%d]" , p_plugin_ctx->p_trace_file->path_filename , errno )
			free( p_plugin_ctx->p_trace_file );
			return -1;
		}
		else
		{
			DEBUGLOGC( "hdfsOpenFile[%s] ok" , p_plugin_ctx->p_trace_file->path_filename )
		}
		
		/* 挂接文件跟踪结构树 */
		nret = LinkTraceFilenameTreeNode( p_plugin_ctx , p_plugin_ctx->p_trace_file ) ;
		if( nret )
		{
			INFOLOGC( "hdfsCloseFile[%s]" , p_plugin_ctx->p_trace_file->path_filename )
			hdfsCloseFile( p_plugin_ctx->p_trace_file->fs , p_plugin_ctx->p_trace_file->file );
			free( p_plugin_ctx->p_trace_file );
			ERRORLOGC( "LinkTraceFilenameTreeNode failed , errno[%d]" , errno )
			return 1;
		}
	}
	
	return 0;
}

funcWriteOutputPlugin WriteOutputPlugin ;
int WriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint64_t file_offset , uint64_t file_line , uint64_t block_len , char *block_buf )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	int				len ;
	
	int				nret = 0 ;
	
	/* 如果未启用解压 */
	if( p_plugin_ctx->uncompress_algorithm == NULL )
	{
		len = hdfsWrite( p_plugin_ctx->fs , p_plugin_ctx->p_trace_file->file , block_buf , block_len ) ;
		if( len == -1 )
		{
			ERRORLOGC( "hdfsWrite data to hdfs failed , errno[%d]" , errno )
			return -1;
		}
		else
		{
			INFOLOGC( "hdfsWrite data to hdfs ok , [%d]bytes" , block_len )
			DEBUGHEXLOGC( block_buf , len , NULL )
		}
		
		nret = hdfsHFlush( p_plugin_ctx->fs , p_plugin_ctx->p_trace_file->file ) ;
		if( nret )
		{
			ERRORLOGC( "hdfsHFlush data to hdfs failed , errno[%d]" , errno )
			return -1;
		}
		else
		{
			INFOLOGC( "hdfsHFlush data to hdfs ok , [%d]bytes" , block_len )
		}
	}
	/* 如果启用了解压 */
	else
	{
		if( STRCMP( p_plugin_ctx->uncompress_algorithm , == , "deflate" ) )
		{
			char			block_out_buf[ LOGPIPE_BLOCK_BUFSIZE + 1 ] ;
			uint64_t		block_out_len ;
			
			memset( block_out_buf , 0x00 , sizeof(block_out_buf) );
			nret = UncompressInputPluginData( p_plugin_ctx->uncompress_algorithm , block_buf , block_len , block_out_buf , & block_out_len ) ;
			if( nret )
			{
				ERRORLOGC( "UncompressInputPluginData failed[%d]" , nret )
				return -1;
			}
			else
			{
				DEBUGLOGC( "UncompressInputPluginData ok" )
			}
			
			len = hdfsWrite( p_plugin_ctx->fs , p_plugin_ctx->p_trace_file->file , block_out_buf , block_out_len ) ;
			if( len == -1 )
			{
				ERRORLOGC( "hdfsWrite data to hdfs failed , errno[%d]" , errno )
				return -1;
			}
			else
			{
				INFOLOGC( "hdfsWrite data to hdfs ok , [%d]bytes" , block_len )
				DEBUGHEXLOGC( block_out_buf , len , NULL )
			}
			
			nret = hdfsHFlush( p_plugin_ctx->fs , p_plugin_ctx->p_trace_file->file ) ;
			if( nret )
			{
				ERRORLOGC( "dfsWrite data to hdfs failed , errno[%d]" , errno )
				return -1;
			}
			else
			{
				INFOLOGC( "dfsWrite data to hdfs ok , [%d]bytes" , block_len )
			}
		}
		else
		{
			ERRORLOGC( "uncompress_algorithm[%s] invalid" , p_plugin_ctx->uncompress_algorithm )
			return -1;
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
	
	INFOLOGC( "DestroyTraceFilenameTree" )
	DestroyTraceFilenameTree( p_plugin_ctx );
	
	INFOLOGC( "hdfsDisconnect" )
	hdfsDisconnect( p_plugin_ctx->fs ); p_plugin_ctx->fs = NULL ;
	
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

