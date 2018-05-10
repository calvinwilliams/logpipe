#include "logpipe_api.h"

/* cmd for testing
logpipe -f logpipe_case1_collector.conf --start-once-for-env "start_once_for_full_dose 1"
*/

char	*__LOGPIPE_INPUT_FILE_VERSION = "0.1.0" ;

/* 跟踪文件信息结构 */
struct TraceFile
{
	char			*path_filename ;
	uint16_t		path_filename_len ;
	char			*pathname ;
	char			*filename ;
	uint16_t		filename_len ;
	off_t			trace_offset ;
	
	int			inotify_file_wd ;
	struct rb_node		inotify_file_wd_rbnode ;
} ;

/* 插件环境结构 */
#define INOTIFY_READ_BUFSIZE	1024*1024

struct InputPluginContext
{
	char			*path ;
	char			*files ;
	char			*files2 ;
	char			*files3 ;
	char			*exclude_files ;
	char			*exclude_files2 ;
	char			*exclude_files3 ;
	char			exec_before_rotating_buffer[ PATH_MAX * 3 ] ;
	char			*exec_before_rotating ;
	int			rotate_size ;
	char			exec_after_rotating_buffer[ PATH_MAX * 3 ] ;
	char			*exec_after_rotating ;
	char			*compress_algorithm ;
	int			max_append_count ;
	int			start_once_for_full_dose ;
	
	int			inotify_fd ;
	int			inotify_path_wd ;
	struct rb_root		inotify_wd_rbtree ;
	
	char			*inotify_read_buffer ;
	int			inotify_read_bufsize ;
	
	int			append_count ;
	
	struct TraceFile	*p_trace_file ;
	int			fd ;
	int			remain_len ;
	int			read_len ;
} ;

LINK_RBTREENODE_INT( LinkTraceFileWdTreeNode , struct InputPluginContext , inotify_wd_rbtree , struct TraceFile , inotify_file_wd_rbnode , inotify_file_wd )
QUERY_RBTREENODE_INT( QueryTraceFileWdTreeNode , struct InputPluginContext , inotify_wd_rbtree , struct TraceFile , inotify_file_wd_rbnode , inotify_file_wd )
UNLINK_RBTREENODE( UnlinkTraceFileWdTreeNode , struct InputPluginContext , inotify_wd_rbtree , struct TraceFile , inotify_file_wd_rbnode )

void FreeTraceFile( void *pv )
{
	struct TraceFile      *p_trace_file = (struct TraceFile *) pv ;
	
	if( p_trace_file )
	{
		free( p_trace_file->path_filename );
		free( p_trace_file->filename );
		free( p_trace_file );
		p_trace_file = NULL ;
	}
	
	return;
}

DESTROY_RBTREE( DestroyTraceFileTree , struct InputPluginContext , inotify_wd_rbtree , struct TraceFile , inotify_file_wd_rbnode , FreeTraceFile )

/* 日志文件大小转档 */
static int RotatingFile( struct InputPluginContext *p_plugin_ctx , char *pathname , char *filename , int filename_len )
{
	struct timeval	tv ;
	struct tm	tm ;
	char		old_filename[ PATH_MAX + 1 ] ;
	char		new_filename[ PATH_MAX + 1 ] ;
	
	int		nret = 0 ;
	
	/* 组装转档后文件名 */
	snprintf( old_filename , sizeof(old_filename)-1 , "%s/%.*s" , pathname , filename_len , filename );
	gettimeofday( & tv , NULL );
	memset( & tm , 0x00 , sizeof(struct tm) );
	localtime_r( & (tv.tv_sec) , & tm );
	memset( new_filename , 0x00 , sizeof(new_filename) );
	snprintf( new_filename , sizeof(new_filename)-1 , "%s/_%.*s-%04d%02d%02d_%02d%02d%02d_%06ld" , pathname , filename_len , filename , tm.tm_year+1900 , tm.tm_mon+1 , tm.tm_mday , tm.tm_hour , tm.tm_min , tm.tm_sec , tv.tv_usec );
	
	/* 设置环境变量 */
	setenv( "LOGPIPE_ROTATING_PATHNAME" , pathname , 1 );
	setenv( "LOGPIPE_ROTATING_OLD_FILENAME" , old_filename , 1 );
	setenv( "LOGPIPE_ROTATING_NEW_FILENAME" , new_filename , 1 );
	
	/* 执行转档前命令 */
	if( p_plugin_ctx->exec_before_rotating && p_plugin_ctx->exec_before_rotating[0] )
	{
		system( p_plugin_ctx->exec_before_rotating );
	}
	
	/* 文件改名转档 */
	nret = rename( old_filename , new_filename ) ;
	
	/* 执行转档后命令 */
	if( p_plugin_ctx->exec_after_rotating && p_plugin_ctx->exec_after_rotating[0] )
	{
		system( p_plugin_ctx->exec_after_rotating );
	}
	
	/* 判断改名是否成功 */
	if( nret )
	{
		FATALLOG( "rename [%s] to [%s] failed , errno[%d]" , old_filename , new_filename , errno )
		return -1;
	}
	else
	{
		INFOLOG( "rename [%s] to [%s] ok" , old_filename , new_filename )
	}
	
	return 0;
}

/* 清除文件变化监视器 */
static int RemoveFileWatcher( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , struct InputPluginContext *p_plugin_ctx , struct TraceFile *p_trace_file )
{
	UnlinkTraceFileWdTreeNode( p_plugin_ctx , p_trace_file );
	
	INFOLOG( "inotify_rm_watch[%s] ok , inotify_fd[%d] inotify_wd[%d]" , p_trace_file->path_filename , p_plugin_ctx->inotify_fd , p_trace_file->inotify_file_wd )
	inotify_rm_watch( p_plugin_ctx->inotify_fd , p_trace_file->inotify_file_wd );
	free( p_trace_file->path_filename );
	free( p_trace_file );
	
	return 0;
}

/* 检查文件最后偏移量 */
static int CheckFileOffset( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , struct InputPluginContext *p_plugin_ctx , struct TraceFile *p_trace_file , int remove_watcher_flag )
{
	int			fd ;
	struct stat		file_stat ;
	
	int			nret = 0 ;
	
	DEBUGLOG( "catch file[%s] append" , p_trace_file->path_filename )
	
	/* 打开文件 */
	fd = open( p_trace_file->path_filename , O_RDONLY ) ;
	if( fd == -1 )
	{
		WARNLOG( "open[%s] failed , errno[%d]" , p_trace_file->path_filename , errno )
		if( remove_watcher_flag )
			return RemoveFileWatcher( p_env , p_logpipe_input_plugin , p_plugin_ctx , p_trace_file );
		else
			return 0;
	}
	
	/* 获得文件大小 */
	memset( & file_stat , 0x00 , sizeof(struct stat) );
	nret = fstat( fd , & file_stat ) ;
	if( nret == -1 )
	{
		ERRORLOG( "fstat[%s] failed , errno[%d]" , p_trace_file->path_filename , errno )
		close( fd );
		return RemoveFileWatcher( p_env , p_logpipe_input_plugin , p_plugin_ctx , p_trace_file );
	}
	
	/* 如果启用大小转档，且文件有追加内容 */
	if( p_plugin_ctx->rotate_size > 0 && file_stat.st_size >= p_plugin_ctx->rotate_size )
	{
		/* 大小转档文件 */
		INFOLOG( "file_stat.st_size[%d] > p_plugin_ctx->rotate_size[%d]" , file_stat.st_size , p_plugin_ctx->rotate_size )
		RotatingFile( p_plugin_ctx , p_trace_file->pathname , p_trace_file->filename , p_trace_file->filename_len );
		p_plugin_ctx->append_count = -1 ;
		remove_watcher_flag = 1 ;
		
		/* 再次获得文件大小 */
		memset( & file_stat , 0x00 , sizeof(struct stat) );
		nret = fstat( fd , & file_stat ) ;
		if( nret == -1 )
		{
			ERRORLOG( "fstat[%s] failed , errno[%d]" , p_trace_file->path_filename , errno )
			close( fd );
			return RemoveFileWatcher( p_env , p_logpipe_input_plugin , p_plugin_ctx , p_trace_file );
		}
	}
	
_GOTO_WRITEALLOUTPUTPLUGINS :
	
	/* 如果文件大小 小于 跟踪的文件大小 */
	DEBUGLOG( "compare size - file_size[%d] trace_offset[%d]" , file_stat.st_size , p_trace_file->trace_offset )
	if( file_stat.st_size < p_trace_file->trace_offset )
	{
		p_trace_file->trace_offset = file_stat.st_size ;
	}
	/* 如果文件大小 大于 跟踪的文件大小 */
	else if( file_stat.st_size > p_trace_file->trace_offset )
	{
		lseek( fd , p_trace_file->trace_offset , SEEK_SET );
		
		/* 激活一轮从输入插件读，写到所有输出插件流程处理 */
		p_plugin_ctx->p_trace_file = p_trace_file ;
		p_plugin_ctx->fd = fd ;
		p_plugin_ctx->remain_len = file_stat.st_size - p_trace_file->trace_offset ;
		nret = WriteAllOutputPlugins( p_env , p_logpipe_input_plugin , p_trace_file->filename_len , p_trace_file->filename ) ;
		if( nret )
		{
			ERRORLOG( "WriteAllOutputPlugins failed[%d]" , nret )
			close( fd );
			return nret;
		}
		
		/* p_trace_file->trace_offset = file_stat.st_size ; */
		
		if( p_plugin_ctx->rotate_size > 0 && file_stat.st_size >= p_plugin_ctx->rotate_size )
		{
			/* 再次获得文件大小 */
			memset( & file_stat , 0x00 , sizeof(struct stat) );
			nret = fstat( fd , & file_stat ) ;
			if( nret == -1 )
			{
				ERRORLOG( "fstat[%s] failed , errno[%d]" , p_trace_file->path_filename , errno )
				close( fd );
				return RemoveFileWatcher( p_env , p_logpipe_input_plugin , p_plugin_ctx , p_trace_file );
			}
			
			/* 如果启用文件追读，再处理一个数据块  */
			if( p_plugin_ctx->append_count >= 0 )
			{
				p_plugin_ctx->append_count++;
				if( p_plugin_ctx->append_count <= p_plugin_ctx->max_append_count )
					goto _GOTO_WRITEALLOUTPUTPLUGINS;
			}
			else if( p_plugin_ctx->append_count == -1 )
			{
				goto _GOTO_WRITEALLOUTPUTPLUGINS;
			}
		}
	}
	
	/* 关闭文件 */
	close( fd );
	
	/* 清除文件变化监视器 */
	if( p_plugin_ctx->rotate_size > 0 && file_stat.st_size >= p_plugin_ctx->rotate_size )
	{
		if( remove_watcher_flag )
		{
			RemoveFileWatcher( p_env , p_logpipe_input_plugin , p_plugin_ctx , p_trace_file );
		}
	}
	
	return 0;
}

/* 识别通配表达式 */
static int IsMatchString(char *pcMatchString, char *pcObjectString, char cMatchMuchCharacters, char cMatchOneCharacters)
{
	int el=strlen(pcMatchString);
	int sl=strlen(pcObjectString);
	char cs,ce;

	int is,ie;
	int last_xing_pos=-1;

	for(is=0,ie=0;is<sl && ie<el;){
		cs=pcObjectString[is];
		ce=pcMatchString[ie];

		if(cs!=ce){
			if(ce==cMatchMuchCharacters){
				last_xing_pos=ie;
				ie++;
			}else if(ce==cMatchOneCharacters){
				is++;
				ie++;
			}else if(last_xing_pos>=0){
				while(ie>last_xing_pos){
					ce=pcMatchString[ie];
					if(ce==cs)
						break;
					ie--;
				}

				if(ie==last_xing_pos)
					is++;
			}else
				return -1;
		}else{
			is++;
			ie++;
		}
	}

	if(pcObjectString[is]==0 && pcMatchString[ie]==0)
		return 0;

	if(pcMatchString[ie]==0)
		ie--;

	if(ie>=0){
		while(pcMatchString[ie])
			if(pcMatchString[ie++]!=cMatchMuchCharacters)
				return -2;
	} 

	return 0;
}

/* 添加文件变化监视器 */
static int AddFileWatcher( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , struct InputPluginContext *p_plugin_ctx , char *filename , int check_flag_offset_flag , int remove_watcher_flag )
{
	struct TraceFile	*p_trace_file = NULL ;
	struct TraceFile	*p_trace_file_not_exist = NULL ;
	struct stat		file_stat ;
	int			len ;
	int			survive_flag ;
	
	int			nret = 0 ;
	
	/* 不处理文件名以'.'或'_'开头的文件 */
	if( filename[0] == '.' || filename[0] == '_' )
	{
		DEBUGLOG( "filename[%s] ignored" , filename )
		return 0;
	}
	
	/* 不处理文件名白名单以外的文件 */
	survive_flag = 1 ;
	
	if( p_plugin_ctx->files && p_plugin_ctx->files[0] )
	{
		if( IsMatchString( p_plugin_ctx->files , filename , '*' , '?' ) != 0 )
		{
			DEBUGLOG( "filename[%s] not match files[%s]" , filename , p_plugin_ctx->files )
			survive_flag = 0 ;
		}
	}
	
	if( p_plugin_ctx->files2 && p_plugin_ctx->files2[0] )
	{
		if( IsMatchString( p_plugin_ctx->files2 , filename , '*' , '?' ) != 0 )
		{
			DEBUGLOG( "filename[%s] not match files[%s]" , filename , p_plugin_ctx->files2 )
			survive_flag = 0 ;
		}
	}
	
	if( p_plugin_ctx->files3 && p_plugin_ctx->files3[0] )
	{
		if( IsMatchString( p_plugin_ctx->files3 , filename , '*' , '?' ) != 0 )
		{
			DEBUGLOG( "filename[%s] not match files[%s]" , filename , p_plugin_ctx->files3 )
			survive_flag = 0 ;
		}
	}
	
	if( survive_flag == 0 )
		return 0;
	
	/* 不处理文件名黑名单以内的文件 */
	if( p_plugin_ctx->exclude_files && p_plugin_ctx->exclude_files[0] )
	{
		if( IsMatchString( p_plugin_ctx->exclude_files , filename , '*' , '?' ) == 0 )
		{
			DEBUGLOG( "filename[%s] match exclude_files[%s]" , filename , p_plugin_ctx->exclude_files )
			return 0;
		}
	}
	
	if( p_plugin_ctx->exclude_files2 && p_plugin_ctx->exclude_files2[0] )
	{
		if( IsMatchString( p_plugin_ctx->exclude_files2 , filename , '*' , '?' ) == 0 )
		{
			DEBUGLOG( "filename[%s] match exclude_files[%s]" , filename , p_plugin_ctx->exclude_files2 )
			return 0;
		}
	}
	
	if( p_plugin_ctx->exclude_files3 && p_plugin_ctx->exclude_files3[0] )
	{
		if( IsMatchString( p_plugin_ctx->exclude_files3 , filename , '*' , '?' ) == 0 )
		{
			DEBUGLOG( "filename[%s] match exclude_files[%s]" , filename , p_plugin_ctx->exclude_files3 )
			return 0;
		}
	}
	
	/* 分配内存以构建文件跟踪结构 */
	p_trace_file = (struct TraceFile *)malloc( sizeof(struct TraceFile) ) ;
	if( p_trace_file == NULL )
	{
		ERRORLOG( "malloc failed , errno[%d]" , errno )
		return -1;
	}
	memset( p_trace_file , 0x00 , sizeof(struct TraceFile) );
	
	/* 填充文件跟踪结构 */
	len = asprintf( & (p_trace_file->path_filename) , "%s/%s" , p_plugin_ctx->path , filename ) ;
	if( len == -1 )
	{
		ERRORLOG( "asprintf failed , errno[%d]" , errno )
		free( p_trace_file );
		return -1;
	}
	p_trace_file->path_filename_len = len ;
	
	nret = stat( p_trace_file->path_filename , & file_stat ) ;
	if( nret == -1 )
	{
		WARNLOG( "file[%s] not found" , p_trace_file->path_filename )
		free( p_trace_file->path_filename );
		free( p_trace_file );
		return 0;
	}
	
	if( ! S_ISREG(file_stat.st_mode) )
	{
		INFOLOG( "[%s] is not a file" , p_trace_file->path_filename )
		free( p_trace_file->path_filename );
		free( p_trace_file );
		return 0;
	}
	
	p_trace_file->pathname = p_plugin_ctx->path ;
	
	len = asprintf( & p_trace_file->filename , "%s" , filename );
	if( len == -1 )
	{
		ERRORLOG( "asprintf failed , errno[%d]" , errno )
		free( p_trace_file->path_filename );
		free( p_trace_file );
		return -1;
	}
	p_trace_file->filename_len = len ;
	
	if( check_flag_offset_flag )
		p_trace_file->trace_offset = 0 ;
	else
		p_trace_file->trace_offset = file_stat.st_size ;
	
	/* 添加文件变化监视器 */
	p_trace_file->inotify_file_wd = inotify_add_watch( p_plugin_ctx->inotify_fd , p_trace_file->path_filename , (uint32_t)(IN_MODIFY|IN_CLOSE_WRITE|IN_DELETE_SELF|IN_MOVE_SELF|IN_IGNORED) ) ;
	if( p_trace_file->inotify_file_wd == -1 )
	{
		ERRORLOG( "inotify_add_watch[%s] failed , errno[%d]" , p_trace_file->path_filename , errno )
		free( p_trace_file->filename );
		free( p_trace_file->path_filename );
		free( p_trace_file );
		return -1;
	}
	else
	{
		INFOLOG( "inotify_add_watch[%s] ok , inotify_fd[%d] inotify_wd[%d] trace_offset[%d]" , p_trace_file->path_filename , p_plugin_ctx->inotify_fd , p_trace_file->inotify_file_wd , p_trace_file->trace_offset )
	}
	
	p_trace_file_not_exist = QueryTraceFileWdTreeNode( p_plugin_ctx , p_trace_file ) ;
	if( p_trace_file_not_exist == NULL )
	{
		nret = LinkTraceFileWdTreeNode( p_plugin_ctx , p_trace_file ) ;
		if( nret )
		{
			ERRORLOG( "LinkTraceFileWdTreeNode[%s] failed , errno[%d]" , p_trace_file->path_filename , errno )
			INFOLOG( "inotify_rm_watch[%s] , inotify_fd[%d] inotify_wd[%d]" , p_trace_file->path_filename , p_plugin_ctx->inotify_fd , p_trace_file->inotify_file_wd )
			inotify_rm_watch( p_plugin_ctx->inotify_fd , p_trace_file->inotify_file_wd );
			free( p_trace_file->filename );
			free( p_trace_file->path_filename );
			free( p_trace_file );
			return -1;
		}
	}
	
	/* 如果要马上检查文件变化 */
	if( check_flag_offset_flag )
	{
		p_plugin_ctx->append_count = -1 ;
		return CheckFileOffset( p_env , p_logpipe_input_plugin, p_plugin_ctx , p_trace_file , remove_watcher_flag );
	}
	else
	{
		return 0;
	}
}

/* 启动时把现存文件都设置变化监视器 */
static int ReadFilesToinotifyWdTree( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , struct InputPluginContext *p_plugin_ctx )
{
	DIR			*dir = NULL ;
	struct dirent		*ent = NULL ;
	
	int			nret = 0 ;
	
	/* 打开目录 */
	dir = opendir( p_plugin_ctx->path ) ;
	if( dir == NULL )
	{
		ERRORLOG( "opendir[%s] failed , errno[%d]\n" , p_plugin_ctx->path , errno );
		return -1;
	}
	
	while(1)
	{
		/* 遍历目录中的所有文件 */
		ent = readdir( dir ) ;
		if( ent == NULL )
			break;
		
		if( ent->d_type & DT_REG )
		{
			nret = AddFileWatcher( p_env , p_logpipe_input_plugin , p_plugin_ctx , ent->d_name , p_plugin_ctx->start_once_for_full_dose , 1 ) ;
			if( nret )
			{
				ERRORLOG( "AddFileWatcher[%s] failed[%d]" , ent->d_name , nret );
				closedir( dir );
				return -1;
			}
		}
	}
	
	/* 打开目录 */
	closedir( dir );
	
	return 0;
}

funcLoadInputPluginConfig LoadInputPluginConfig ;
int LoadInputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , struct LogpipePluginConfigItem *p_plugin_config_items , void **pp_context )
{
	struct InputPluginContext	*p_plugin_ctx = NULL ;
	char				*p = NULL ;
	
	/* 申请内存以存放插件上下文 */
	p_plugin_ctx = (struct InputPluginContext *)malloc( sizeof(struct InputPluginContext) ) ;
	if( p_plugin_ctx == NULL )
	{
		ERRORLOG( "malloc failed , errno[%d]" , errno );
		return -1;
	}
	memset( p_plugin_ctx , 0x00 , sizeof(struct InputPluginContext) );
	
	/* 解析插件配置 */
	p_plugin_ctx->path = QueryPluginConfigItem( p_plugin_config_items , "path" ) ;
	INFOLOG( "path[%s]" , p_plugin_ctx->path )
	if( p_plugin_ctx->path == NULL || p_plugin_ctx->path[0] == '\0' )
	{
		ERRORLOG( "expect config for 'path'" );
		return -1;
	}
	
	p_plugin_ctx->files = QueryPluginConfigItem( p_plugin_config_items , "files" ) ;
	INFOLOG( "files[%s]" , p_plugin_ctx->files )
	
	p_plugin_ctx->files2 = QueryPluginConfigItem( p_plugin_config_items , "files2" ) ;
	INFOLOG( "files2[%s]" , p_plugin_ctx->files2 )
	
	p_plugin_ctx->files3 = QueryPluginConfigItem( p_plugin_config_items , "files3" ) ;
	INFOLOG( "files3[%s]" , p_plugin_ctx->files3 )
	
	p_plugin_ctx->exclude_files = QueryPluginConfigItem( p_plugin_config_items , "exclude_files" ) ;
	INFOLOG( "exclude_files[%s]" , p_plugin_ctx->exclude_files )
	
	p_plugin_ctx->exclude_files2 = QueryPluginConfigItem( p_plugin_config_items , "exclude_files2" ) ;
	INFOLOG( "exclude_files2[%s]" , p_plugin_ctx->exclude_files2 )
	
	p_plugin_ctx->exclude_files3 = QueryPluginConfigItem( p_plugin_config_items , "exclude_files3" ) ;
	INFOLOG( "exclude_files3[%s]" , p_plugin_ctx->exclude_files3 )
	
	p = QueryPluginConfigItem( p_plugin_config_items , "exec_before_rotating" ) ;
	if( p )
	{
		int		buffer_len = 0 ;
		int		remain_len = sizeof(p_plugin_ctx->exec_before_rotating_buffer)-1 ;
		
		memset( p_plugin_ctx->exec_before_rotating_buffer , 0x00 , sizeof(p_plugin_ctx->exec_before_rotating_buffer) );
		JSONUNESCAPE_FOLD( p , strlen(p) , p_plugin_ctx->exec_before_rotating_buffer , buffer_len , remain_len )
		if( buffer_len == -1 )
		{
			ERRORLOG( "exec_before_rotating[%s] invalid" , p );
			return -1;
		}
		
		p_plugin_ctx->exec_before_rotating = p_plugin_ctx->exec_before_rotating_buffer ;
	}
	INFOLOG( "exec_before_rotating[%s]" , p_plugin_ctx->exec_before_rotating )
	
	p = QueryPluginConfigItem( p_plugin_config_items , "rotate_size" ) ;
	if( p )
		p_plugin_ctx->rotate_size = atoi(p) ;
	else
		p_plugin_ctx->rotate_size = 0 ;
	INFOLOG( "rotate_size[%d]" , p_plugin_ctx->rotate_size )
	
	p = QueryPluginConfigItem( p_plugin_config_items , "exec_after_rotating" ) ;
	if( p )
	{
		int		buffer_len = 0 ;
		int		remain_len = sizeof(p_plugin_ctx->exec_after_rotating_buffer)-1 ;
		
		memset( p_plugin_ctx->exec_after_rotating_buffer , 0x00 , sizeof(p_plugin_ctx->exec_after_rotating_buffer) );
		JSONUNESCAPE_FOLD( p , strlen(p) , p_plugin_ctx->exec_after_rotating_buffer , buffer_len , remain_len )
		if( buffer_len == -1 )
		{
			ERRORLOG( "exec_after_rotating[%s] invalid" , p );
			return -1;
		}
		
		p_plugin_ctx->exec_after_rotating = p_plugin_ctx->exec_after_rotating_buffer ;
	}
	INFOLOG( "exec_after_rotating[%s]" , p_plugin_ctx->exec_after_rotating )
	
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
	
	p = QueryPluginConfigItem( p_plugin_config_items , "max_append_count" ) ;
	if( p )
		p_plugin_ctx->max_append_count = atoi(p) ;
	else
		p_plugin_ctx->max_append_count = 0 ;
	INFOLOG( "max_append_count[%d]" , p_plugin_ctx->max_append_count )
	
	/* 设置插件环境上下文 */
	(*pp_context) = p_plugin_ctx ;
	
	return 0;
}

funcInitInputPluginContext InitInputPluginContext ;
int InitInputPluginContext( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	struct InputPluginContext	*p_plugin_ctx = (struct InputPluginContext *)(p_context) ;
	char				*p = NULL ;
	
	int				nret = 0 ;
	
	/* 补充从环境变量中解析配置 */
	p = getenv( "start_once_for_full_dose" ) ;
	if( p )
		p_plugin_ctx->start_once_for_full_dose = 1 ;
	else
		p_plugin_ctx->start_once_for_full_dose = 0 ;
	INFOLOG( "start_once_for_full_dose[%d]" , p_plugin_ctx->start_once_for_full_dose )
	
	/* 初始化插件环境内部数据 */
	p_plugin_ctx->inotify_fd = inotify_init() ;
	if( p_plugin_ctx->inotify_fd == -1 )
	{
		ERRORLOG( "inotify_init failed , errno[%d]" , errno );
		return -1;
	}
	
	p_plugin_ctx->inotify_path_wd = inotify_add_watch( p_plugin_ctx->inotify_fd , p_plugin_ctx->path , (uint32_t)(IN_CREATE|IN_MOVED_TO|IN_DELETE_SELF|IN_MOVE_SELF) );
	if( p_plugin_ctx->inotify_path_wd == -1 )
	{
		ERRORLOG( "inotify_add_watch[%s] failed , errno[%d]" , p_plugin_ctx->path , errno );
		return -1;
	}
	else
	{
		INFOLOG( "inotify_add_watch[%s] ok , inotify_fd[%d] inotify_wd[%d]" , p_plugin_ctx->path , p_plugin_ctx->inotify_fd , p_plugin_ctx->inotify_path_wd )
	}
	
	p_plugin_ctx->inotify_read_bufsize = INOTIFY_READ_BUFSIZE ;
	p_plugin_ctx->inotify_read_buffer = (char*)malloc( p_plugin_ctx->inotify_read_bufsize ) ;
	if( p_plugin_ctx->inotify_read_buffer == NULL )
	{
		ERRORLOG( "malloc failed , errno[%d]" , errno );
		return -1;
	}
	memset( p_plugin_ctx->inotify_read_buffer , 0x00 , p_plugin_ctx->inotify_read_bufsize );
	
	/* 装载现存文件 */
	nret = ReadFilesToinotifyWdTree( p_env , p_logpipe_input_plugin , p_plugin_ctx ) ;
	if( nret )
	{
		return nret;
	}
	
	/* 设置输入描述字 */
	AddInputPluginEvent( p_env , p_logpipe_input_plugin , p_plugin_ctx->inotify_fd );
	
	return 0;
}

funcOnInputPluginEvent OnInputPluginEvent ;
int OnInputPluginEvent( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	struct InputPluginContext	*p_plugin_ctx = (struct InputPluginContext *)p_context ;
	
	int				inotify_read_nbytes ;
	long				len ;
	struct inotify_event		*p_inotify_event = NULL ;
	struct inotify_event		*p_overflow_inotify_event = NULL ;
	struct TraceFile		trace_file ;
	struct TraceFile		*p_trace_file = NULL ;
	
	int				nret = 0 ;
	
	/* 如果累积事件数量 大于 默认事件缓冲区大小，临时扩大事件缓冲区大小 */
	nret = ioctl( p_plugin_ctx->inotify_fd , FIONREAD , & inotify_read_nbytes );
	if( nret )
	{
		FATALLOG( "ioctl failed , errno[%d]" , errno )
		return -1;
	}
	
	if( inotify_read_nbytes+1 > p_plugin_ctx->inotify_read_bufsize )
	{
		char	*tmp = NULL ;
		
		WARNLOG( "inotify read buffer resize [%d]bytes to [%d]bytes" , p_plugin_ctx->inotify_read_bufsize , inotify_read_nbytes+1 )
		p_plugin_ctx->inotify_read_bufsize = inotify_read_nbytes+1 ;
		tmp = (char*)realloc( p_plugin_ctx->inotify_read_buffer , p_plugin_ctx->inotify_read_bufsize ) ;
		if( tmp == NULL )
		{
			FATALLOG( "realloc failed , errno[%d]" , errno )
			return -1;
		}
		p_plugin_ctx->inotify_read_buffer = tmp ;
	}
	memset( p_plugin_ctx->inotify_read_buffer , 0x00 , p_plugin_ctx->inotify_read_bufsize );
	
	/* 读文件变化事件 */
	DEBUGLOG( "read inotify[%d] ..." , p_plugin_ctx->inotify_fd )
	len = read( p_plugin_ctx->inotify_fd , p_plugin_ctx->inotify_read_buffer , p_plugin_ctx->inotify_read_bufsize-1 ) ;
	if( len == -1 )
	{
		FATALLOG( "read inotify[%d] failed , errno[%d]" , p_plugin_ctx->inotify_fd , errno )
		return -1;
	}
	else
	{
		INFOLOG( "read inotify[%d] ok , [%d]bytes" , p_plugin_ctx->inotify_fd , len )
	}
	
	p_inotify_event = (struct inotify_event *)(p_plugin_ctx->inotify_read_buffer) ;
	p_overflow_inotify_event = (struct inotify_event *)(p_plugin_ctx->inotify_read_buffer+len) ;
	while( p_inotify_event < p_overflow_inotify_event )
	{
		DEBUGLOG( "inotify event wd[%d] mask[0x%X] cookie[%d] len[%d] name[%.*s]" , p_inotify_event->wd , p_inotify_event->mask , p_inotify_event->cookie , p_inotify_event->len , p_inotify_event->len , p_inotify_event->name )
		
		/* 如果发生UNMOUNT事件，logpipe退出 */
		if( p_inotify_event->mask & IN_UNMOUNT )
		{
			FATALLOG( "something unmounted" )
			return -1;
		}
		else if( p_inotify_event->mask & IN_IGNORED )
		{
			;
		}
		else if( p_inotify_event->wd == p_plugin_ctx->inotify_path_wd )
		{
			/* 如果发生 创建文件 或 移入文件 事件 */
			if( ( p_inotify_event->mask & IN_CREATE ) || ( p_inotify_event->mask & IN_MOVED_TO ) )
			{
				nret = AddFileWatcher( p_env , p_logpipe_input_plugin , p_plugin_ctx , p_inotify_event->name , (p_inotify_event->mask&IN_CREATE) , 0 ) ;
				if( nret )
				{
					ERRORLOG( "AddFileWatcher[%s] failed , errno[%d]" , p_inotify_event->name , errno )
					return -1;
				}
			}
			/* 如果发生 删除监控目录 或 移动监控目录 事件 */
			else if( ( p_inotify_event->mask & IN_DELETE_SELF ) || ( p_inotify_event->mask & IN_MOVE_SELF ) )
			{
				ERRORLOG( "[%s] had deleted" , p_plugin_ctx->path )
				ERRORLOG( "inotify_rm_watch[%s] ok , inotify_fd[%d] inotify_wd[%d]" , p_plugin_ctx->path , p_plugin_ctx->inotify_fd , p_plugin_ctx->inotify_path_wd )
				inotify_rm_watch( p_plugin_ctx->inotify_fd , p_plugin_ctx->inotify_path_wd );
				return -1;
			}
			else
			{
				ERRORLOG( "unknow dir inotify event mask[0x%X]" , p_inotify_event->mask )
			}
		}
		else
		{
			/* 查询文件跟踪结构树 */
			trace_file.inotify_file_wd = p_inotify_event->wd ;
			p_trace_file = QueryTraceFileWdTreeNode( p_plugin_ctx , & trace_file ) ;
			if( p_trace_file == NULL )
			{
				WARNLOG( "wd[%d] not found" , trace_file.inotify_file_wd )
			}
			else
			{
				/* 如果发生 文件变化 或 写完关闭 事件 */
				if( ( p_inotify_event->mask & IN_MODIFY ) || ( p_inotify_event->mask & IN_CLOSE_WRITE ) )
				{
					p_plugin_ctx->append_count = 0 ;
					nret = CheckFileOffset( p_env , p_logpipe_input_plugin , p_plugin_ctx , p_trace_file , (p_inotify_event->mask&IN_CLOSE_WRITE) ) ;
					if( nret )
					{
						ERRORLOG( "CheckFileOffset failed[%d] , errno[%d]" , nret , errno )
						return -1;
					}
				}
				/* 如果发生 删除文件 或 移动文件 事件 */
				else if( ( p_inotify_event->mask & IN_DELETE_SELF ) || ( p_inotify_event->mask & IN_MOVE_SELF ) )
				{
					nret = RemoveFileWatcher( p_env , p_logpipe_input_plugin , p_plugin_ctx , p_trace_file ) ;
					if( nret )
					{
						ERRORLOG( "RemoveFileWatcher failed , errno[%d]" , errno )
						return -1;
					}
				}
				else
				{
					ERRORLOG( "unknow file inotify event mask[0x%X]" , p_inotify_event->mask )
				}
			}
		}
		
		p_inotify_event = (struct inotify_event *)( (char*)p_inotify_event + sizeof(struct inotify_event) + p_inotify_event->len ) ;
	}
	
	/* 恢复临时调整事件缓冲区 */
	if( p_plugin_ctx->inotify_read_bufsize != INOTIFY_READ_BUFSIZE )
	{
		char	*tmp = NULL ;
		
		WARNLOG( "inotify read buffer resize [%d]bytes to [%d]bytes" , p_plugin_ctx->inotify_read_bufsize , INOTIFY_READ_BUFSIZE )
		p_plugin_ctx->inotify_read_bufsize = INOTIFY_READ_BUFSIZE ;
		tmp = (char*)realloc( p_plugin_ctx->inotify_read_buffer , p_plugin_ctx->inotify_read_bufsize ) ;
		if( tmp == NULL )
		{
			FATALLOG( "realloc failed , errno[%d]" , errno )
			return -1;
		}
		p_plugin_ctx->inotify_read_buffer = tmp ;
	}
	
	return 0;
}

funcReadInputPlugin ReadInputPlugin ;
int ReadInputPlugin( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context , uint32_t *p_file_offset , uint32_t *p_block_len , char *block_buf , int block_bufsize )
{
	struct InputPluginContext	*p_plugin_ctx = (struct InputPluginContext *)p_context ;
	
	int				len ;
	
	int				nret = 0 ;
	
	if( p_plugin_ctx->remain_len == 0 )
		return LOGPIPE_READ_END_OF_INPUT;
	
	/* 如果未启用压缩 */
	if( p_plugin_ctx->compress_algorithm == NULL )
	{
		if( p_plugin_ctx->remain_len > block_bufsize - 1 )
			len = block_bufsize - 1 ;
		else
			len = p_plugin_ctx->remain_len ;
		memset( block_buf , 0x00 , len+1 );
		p_plugin_ctx->read_len = read( p_plugin_ctx->fd , block_buf , len ) ;
		if( p_plugin_ctx->read_len == -1 )
		{
			ERRORLOG( "read file failed , errno[%d]" , errno )
			return -1;
		}
		else if( p_plugin_ctx->read_len == 0 )
		{
			WARNLOG( "read eof of file" )
			return LOGPIPE_READ_END_OF_INPUT;
		}
		else
		{
			INFOLOG( "read file ok , [%d]bytes" , p_plugin_ctx->read_len )
			DEBUGHEXLOG( block_buf , p_plugin_ctx->read_len , NULL )
		}
		
		(*p_block_len) = p_plugin_ctx->read_len ;
	}
	/* 如果启用了压缩 */
	else
	{
		char			block_in_buf[ LOGPIPE_UNCOMPRESS_BLOCK_BUFSIZE + 1 ] ;
		uint32_t		block_in_len ;
		
		if( p_plugin_ctx->remain_len > sizeof(block_in_buf) - 1 )
			block_in_len = sizeof(block_in_buf) - 1 ;
		else
			block_in_len = p_plugin_ctx->remain_len ;
		
		memset( block_in_buf , 0x00 , block_in_len+1 );
		p_plugin_ctx->read_len = read( p_plugin_ctx->fd , block_in_buf , block_in_len ) ;
		if( p_plugin_ctx->read_len == -1 )
		{
			ERRORLOG( "read file failed , errno[%d]" , errno )
			return -1;
		}
		else
		{
			INFOLOG( "read file ok , [%d]bytes" , p_plugin_ctx->read_len )
			DEBUGHEXLOG( block_in_buf , p_plugin_ctx->read_len , NULL )
		}
		
		memset( block_buf , 0x00 , block_bufsize );
		nret = CompressInputPluginData( p_plugin_ctx->compress_algorithm , block_in_buf , p_plugin_ctx->read_len , block_buf , p_block_len ) ;
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
	
	p_plugin_ctx->remain_len -= p_plugin_ctx->read_len ;
	(*p_file_offset) = p_plugin_ctx->p_trace_file->trace_offset ;
	p_plugin_ctx->p_trace_file->trace_offset += p_plugin_ctx->read_len ;
	
	return 0;
}

funcCleanInputPluginContext CleanInputPluginContext ;
int CleanInputPluginContext( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	struct InputPluginContext	*p_plugin_ctx = (struct InputPluginContext *)p_context ;
	
	inotify_rm_watch( p_plugin_ctx->inotify_fd , p_plugin_ctx->inotify_path_wd );
	close( p_plugin_ctx->inotify_fd );
	DestroyTraceFileTree( p_plugin_ctx );
	
	free( p_plugin_ctx->inotify_read_buffer );
	
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

