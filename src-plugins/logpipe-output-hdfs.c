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

/* add to $HADOOP/etc/hadoop/hdfs-site.xml
<property>
    <name>dfs.support.append</name>
    <value>true</value>
</property>
*/

char	*__LOGPIPE_OUTPUT_FILE_VERSION = "0.1.0" ;

struct OutputPluginContext
{
	char		*name_node ;
	int		port ;
	char		*user ;
	char		*path ;
	char		*uncompress_algorithm ;
	
	hdfsFS		fs ;
	char		path_filename[ PATH_MAX + 1 ] ;
	hdfsFile	file ;
	uint16_t	filename_len ;
	char		*filename ;
} ;

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
	
	p_plugin_ctx->name_node = QueryPluginConfigItem( p_plugin_config_items , "name_node" ) ;
	INFOLOG( "name_node[%s]" , p_plugin_ctx->name_node )
	if( p_plugin_ctx->name_node == NULL || p_plugin_ctx->name_node[0] == '\0' )
	{
		ERRORLOG( "expect config for 'name_node'" );
		return -1;
	}
	
	p = QueryPluginConfigItem( p_plugin_config_items , "port" ) ;
	if( p == NULL || p[0] == '\0' )
	{
		ERRORLOG( "expect config for 'port'" );
		return -1;
	}
	p_plugin_ctx->port = atoi(p) ;
	INFOLOG( "port[%d]" , p_plugin_ctx->port )
	if( p_plugin_ctx->port < 0 )
	{
		ERRORLOG( "port[%s] invalid" , p );
		return -1;
	}
	
	p_plugin_ctx->user = QueryPluginConfigItem( p_plugin_config_items , "user" ) ;
	INFOLOG( "user[%s]" , p_plugin_ctx->user )
	
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
	
	/* 设置插件环境上下文 */
	(*pp_context) = p_plugin_ctx ;
	
	return 0;
}

funcInitOutputPluginContext InitOutputPluginContext ;
int InitOutputPluginContext( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	if( p_plugin_ctx->fs == NULL )
	{
		if( p_plugin_ctx->user && p_plugin_ctx->user[0] )
		{
			INFOLOG( "hdfsConnectAsUser ... name_node[%s] port[%d] user[%s]" , p_plugin_ctx->name_node , p_plugin_ctx->port , p_plugin_ctx->user )
			p_plugin_ctx->fs = hdfsConnectAsUser( p_plugin_ctx->name_node , p_plugin_ctx->port , p_plugin_ctx->user ) ;
			if( p_plugin_ctx->fs == NULL )
			{
				ERRORLOG( "hdfsConnectAsUser failed , name_node[%s] port[%d] user[%s]" , p_plugin_ctx->name_node , p_plugin_ctx->port , p_plugin_ctx->user )
				return -1;
			}
			else
			{
				INFOLOG( "hdfsConnectAsUser ok , name_node[%s] port[%d] user[%s]" , p_plugin_ctx->name_node , p_plugin_ctx->port , p_plugin_ctx->user )
			}
		}
		else
		{
			INFOLOG( "hdfsConnect ... name_node[%s] port[%d]" , p_plugin_ctx->name_node , p_plugin_ctx->port )
			p_plugin_ctx->fs = hdfsConnect( p_plugin_ctx->name_node , p_plugin_ctx->port ) ;
			if( p_plugin_ctx->fs == NULL )
			{
				ERRORLOG( "hdfsConnect failed , name_node[%s] port[%d]" , p_plugin_ctx->name_node , p_plugin_ctx->port )
				return -1;
			}
			else
			{
				INFOLOG( "hdfsConnect ok , name_node[%s] port[%d]" , p_plugin_ctx->name_node , p_plugin_ctx->port )
			}
		}
	}
	
	return 0;
}

funcOnOutputPluginEvent OnOutputPluginEvent;
int OnOutputPluginEvent( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context )
{
	return 0;
}

static int ReMkdirPath( hdfsFS fs , char *path )
{
	hdfsFileInfo		*file_info = NULL ;
	char			up_path[ PATH_MAX + 1 ] ;
	char			*p = NULL ;
	
	int			nret = 0 ;
	
	if( path[0] == '\0' )
		return 0;
	
	file_info = hdfsGetPathInfo( fs , path ) ;
	if( file_info )
	{
		DEBUGLOG( "hdfs path[%s] exist" , path )
		if( file_info->mKind != kObjectKindDirectory )
		{
			ERRORLOG( "hdfs path[%s] is't a directory" , path )
			hdfsFreeFileInfo( file_info , 1 );
			return -1;
		}
		hdfsFreeFileInfo( file_info , 1 );
		
		return 0;
	}
	else
	{
		WARNLOG( "hdfs path[%s] not exist" , path )
	}
	
	strcpy( up_path , path );
	p = strrchr( up_path , '/' ) ;
	if( p == NULL )
	{
		ERRORLOG( "hdfs path[%s] invalid" , up_path )
		return -2;
	}
	(*p) = '\0' ;
	nret = ReMkdirPath( fs , up_path ) ;
	if( nret )
		return nret;
	
	nret = hdfsCreateDirectory( fs , path ) ;
	if( nret )
	{
		ERRORLOG( "mkdir hdfs path[%s] failed[%d]" , path , nret )
		return nret;
	}
	else
	{
		DEBUGLOG( "mkdir hdfs path[%s] ok" , path )
	}
	
	return 0;
}

funcBeforeWriteOutputPlugin BeforeWriteOutputPlugin ;
int BeforeWriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint16_t filename_len , char *filename )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	char				path_expand[ PATH_MAX + 1 ] ;
	hdfsFileInfo			*file_info = NULL ;
	
	int				nret = 0 ;
	
	p_plugin_ctx->filename_len = filename_len ;
	p_plugin_ctx->filename = filename ;
	
	memset( path_expand , 0x00 , sizeof(path_expand) );
	strncpy( path_expand , p_plugin_ctx->path , sizeof(path_expand)-1 );
	nret = ExpandStringBuffer( path_expand , sizeof(path_expand) ) ;
	if( nret )
	{
		ERRORLOG( "ExpandStringBuffer[%s] failed[%d]" , p_plugin_ctx->path , nret )
		return 1;
	}
	else
	{
		DEBUGLOG( "ExpandStringBuffer[%s] ok , path_expand[%s]" , p_plugin_ctx->path , path_expand )
	}
	nret = ReMkdirPath( p_plugin_ctx->fs , path_expand );
	if( nret )
	{
		ERRORLOG( "ReMkdirPath[%s] failed[%d]" , path_expand , nret )
	}
	else
	{
		DEBUGLOG( "ReMkdirPath[%s] ok" , path_expand )
	}
	
	memset( p_plugin_ctx->path_filename , 0x00 , sizeof(p_plugin_ctx->path_filename) );
	snprintf( p_plugin_ctx->path_filename , sizeof(p_plugin_ctx->path_filename)-1 , "%s/%.*s" , path_expand , filename_len , filename );
	
	file_info = hdfsGetPathInfo( p_plugin_ctx->fs , p_plugin_ctx->path_filename ) ;
	if( file_info == NULL )
	{
		DEBUGLOG( "file[%s] not exist" , p_plugin_ctx->path_filename )
	}
	else
	{
		DEBUGLOG( "file[%s] exist" , p_plugin_ctx->path_filename )
	}
	
	p_plugin_ctx->file = hdfsOpenFile( p_plugin_ctx->fs , p_plugin_ctx->path_filename , O_CREAT|O_WRONLY , 0 , 0 , 0 ) ;
	// p_plugin_ctx->file = hdfsOpenFile( p_plugin_ctx->fs , p_plugin_ctx->path_filename , O_WRONLY|O_APPEND , 0 , 0 , 0 ) ;
	// p_plugin_ctx->file = hdfsOpenFile( p_plugin_ctx->fs , p_plugin_ctx->path_filename , O_CREAT|O_APPEND , 0 , 0 , 0 ) ;
	if( p_plugin_ctx->file == NULL )
	{
		ERRORLOG( "hdfsOpenFile[%s] failed , errno[%d]" , p_plugin_ctx->path_filename , errno )
		if( file_info )
		{
			hdfsFreeFileInfo( file_info , 1 );
		}
		return 1;
	}
	else
	{
		DEBUGLOG( "hdfsOpenFile[%s] ok" , p_plugin_ctx->path_filename )
	}
	
	/*
	if( file_info && file_info->mSize > 0 )
	{
		nret = hdfsSeek( p_plugin_ctx->fs , p_plugin_ctx->file , file_info->mSize ) ;
		if( nret == -1 )
		{
			ERRORLOG( "hdfsSeek[%s][%d] failed" , p_plugin_ctx->path_filename , file_info->mSize )
			hdfsCloseFile( p_plugin_ctx->fs , p_plugin_ctx->file ); p_plugin_ctx->file = NULL ;
			hdfsFreeFileInfo( file_info , 1 );
			return 1;
		}
		else
		{
			DEBUGLOG( "hdfsSeek[%s][%d] ok" , p_plugin_ctx->path_filename , file_info->mSize )
		}
	}
	*/
	
	if( file_info )
	{
		hdfsFreeFileInfo( file_info , 1 );
	}
	
	return 0;
}

funcWriteOutputPlugin WriteOutputPlugin ;
int WriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint32_t block_len , char *block_buf )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	int				len ;
	
	int				nret = 0 ;
	
	if( p_plugin_ctx->uncompress_algorithm == NULL )
	{
		if( p_plugin_ctx->fs )
		{
			nret = InitOutputPluginContext( p_env , p_logpipe_output_plugin , p_context ) ;
			if( nret )
				return nret;
		}
		
		if( p_plugin_ctx->file )
		{
			nret = BeforeWriteOutputPlugin( p_env , p_logpipe_output_plugin , p_context , p_plugin_ctx->filename_len , p_plugin_ctx->filename ) ;
			if( nret )
				return nret;
		}
		
		len = hdfsWrite( p_plugin_ctx->fs , p_plugin_ctx->file , block_buf , block_len ) ;
		if( len == -1 )
		{
			ERRORLOG( "hdfsWrite data to hdfs failed , errno[%d]" , errno )
			return 1;
		}
		else
		{
			INFOLOG( "hdfsWrite data to hdfs ok , [%d]bytes" , block_len )
			DEBUGHEXLOG( block_buf , len , NULL )
		}
	}
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
				return -1;
			}
			else
			{
				DEBUGLOG( "UncompressInputPluginData ok" )
			}
			
			len = hdfsWrite( p_plugin_ctx->fs , p_plugin_ctx->file , block_out_buf , block_out_len ) ;
			if( len == -1 )
			{
				ERRORLOG( "hdfsWrite data to hdfs failed , errno[%d]" , errno )
				return 1;
			}
			else
			{
				INFOLOG( "hdfsWrite data to hdfs ok , [%d]bytes" , block_len )
				DEBUGHEXLOG( block_out_buf , len , NULL )
			}
		}
		else
		{
			ERRORLOG( "uncompress_algorithm[%s] invalid" , p_plugin_ctx->uncompress_algorithm );
			return -1;
		}
	}
	
	return 0;
}

funcAfterWriteOutputPlugin AfterWriteOutputPlugin ;
int AfterWriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint16_t filename_len , char *filename )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	if( p_plugin_ctx->file )
	{
		INFOLOG( "hdfsCloseFile[%s]" , p_plugin_ctx->path_filename )
		hdfsCloseFile( p_plugin_ctx->fs , p_plugin_ctx->file ); p_plugin_ctx->file = NULL ;
	}
	
	return 0;
}

funcCleanOutputPluginContext CleanOutputPluginContext ;
int CleanOutputPluginContext( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	if( p_plugin_ctx->fs )
	{
		INFOLOG( "hdfsDisconnect" )
		hdfsDisconnect( p_plugin_ctx->fs ); p_plugin_ctx->fs = NULL ;
	}
	
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

