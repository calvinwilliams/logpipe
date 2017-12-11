#include "logpipe_api.h"

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
#define INOTIFY_READ_BUFSIZE	16*1024*1024

struct LogpipeInputPlugin_file
{
	struct LogpipeEnv		*p_env ;
	struct LogpipeInputPlugin	*p_logpipe_input_plugin ;
	
	char				*path = NULL ;
	char				*file = NULL ;
	int				rotate_max_size ;
	char				*exec_after_rotate ;
	char				*compress_algorithm = NULL ;
	
	int				inotify_fd ;
	int				inotify_path_wd ;
	struct rb_root			inotify_wd_rbtree ;
	
	char				*inotify_read_buffer ;
	int				inotify_read_bufsize ;
	
	struct TraceFile		*p_trace_file ;
	int				fd ;
	int				append_len ;
	int				remain_len ;
} ;

LINK_RBTREENODE_INT( LinkTraceFileWdTreeNode , struct LogpipeInputPlugin_file , inotify_wd_rbtree , struct TraceFile , inotify_file_wd_rbnode , inotify_file_wd )
QUERY_RBTREENODE_INT( QueryTraceFileWdTreeNode , struct LogpipeInputPlugin_file , inotify_wd_rbtree , struct TraceFile , inotify_file_wd_rbnode , inotify_file_wd )
UNLINK_RBTREENODE( UnlinkTraceFileWdTreeNode , struct LogpipeInputPlugin_file , inotify_wd_rbtree , struct TraceFile , inotify_file_wd_rbnode )

DESTROY_RBTREE( DestroyTraceFileTree , struct InotifySession , inotify_wd_rbtree , struct TraceFile , inotify_file_wd_rbnode , FREE_RBTREENODEENTRY_DIRECTLY )

static int RoratingFile( char *pathname , char *filename , int filename_len )
{
	time_t		tt ;
	struct tm	tm ;
	char		old_filename[ PATH_MAX + 1 ] ;
	char		new_filename[ PATH_MAX + 1 ] ;
	
	int		nret = 0 ;
	
	snprintf( old_filename , sizeof(old_filename)-1 , "%s/%.*s" , pathname , filename_len , filename );
	
	time( & tt );
	memset( & tm , 0x00 , sizeof(struct tm) );
	localtime_r( & tt , & tm );
	memset( new_filename , 0x00 , sizeof(new_filename) );
	snprintf( new_filename , sizeof(new_filename)-1 , "%s/_%.*s-%04d%02d%02d_%02d%02d%02d" , pathname , filename_len , filename , tm.tm_year+1900 , tm.tm_mon+1 , tm.tm_mday , tm.tm_hour , tm.tm_min , tm.tm_sec );
	nret = rename( old_filename , new_filename ) ;
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

static int CheckFileOffset( struct LogpipeInputPlugin_file *p_plugin_env , struct TraceFile *p_trace_file )
{
	int			fd ;
	struct stat		file_stat ;
	int			append_len ;
	
	int			nret = 0 ;
	
	DEBUGLOG( "catch file[%s] append" , p_trace_file->path_filename )
	
	fd = open( p_trace_file->path_filename , O_RDONLY ) ;
	if( fd == -1 )
	{
		ERRORLOG( "open[%s] failed , errno[%d]" , p_trace_file->path_filename , errno )
		return RemoveFileWatcher( p_plugin_env , p_trace_file );
	}
	
	memset( & file_stat , 0x00 , sizeof(struct stat) );
	nret = fstat( fd , & file_stat ) ;
	if( nret == -1 )
	{
		ERRORLOG( "fstat[%s] failed , errno[%d]" , p_trace_file->path_filename , errno )
		close( fd );
		return RemoveFileWatcher( p_plugin_env , p_trace_file );
	}
	
	if( p_plugin_env->rotate_max_size > 0 && file_stat.st_size >= p_plugin_env->rotate_max_size )
	{
		INFOLOG( "file_stat.st_size[%d] > p_env->conf.rotate.file_rotate_max_size[%d]" , file_stat.st_size , p_plugin_env->rotate_max_size )
		RoratingFile( p_trace_file->pathname , p_trace_file->filename , p_trace_file->filename_len );
		
		memset( & file_stat , 0x00 , sizeof(struct stat) );
		nret = fstat( fd , & file_stat ) ;
		if( nret == -1 )
		{
			ERRORLOG( "fstat[%s] failed , errno[%d]" , p_trace_file->path_filename , errno )
			close( fd );
			return RemoveFileWatcher( p_plugin_env , p_trace_file );
		}
	}
	
	DEBUGLOG( "file_size[%d] trace_offset[%d]" , file_stat.st_size , p_trace_file->trace_offset )
	if( file_stat.st_size < p_trace_file->trace_offset )
	{
		p_trace_file->trace_offset = file_stat.st_size ;
	}
	else if( file_stat.st_size > p_trace_file->trace_offset )
	{
		append_len = file_stat.st_size - p_trace_file->trace_offset ;
		
		lseek( fd , p_trace_file->trace_offset , SEEK_SET );
		
		/* 导出所有输出端 */
		p_plugin_env->p_trace_file = p_trace_file ;
		p_plugin_env->fd = fd ;
		p_plugin_env->append_len = append_len ;
		// nret = ToOutputs( p_env , NULL , -1 , p_trace_file->filename , p_trace_file->filename_len , fd , append_len , p_env->compress_algorithm ) ;
		nret = WriteAllOutputPlugins( p_plugin_env->p_env , p_plugin_env->p_logpipe_input_plugin , p_trace_file->filename_len , p_trace_file->filename ) ;
		if( nret )
		{
			ERRORLOG( "WriteAllOutputPlugins failed[%d]" , nret )
			close( fd );
			return 0;
		}
		
		p_trace_file->trace_offset = file_stat.st_size ;
	}
	
	close( fd );
	
	if( p_plugin_env->rotate_max_size > 0 && file_stat.st_size >= p_plugin_env->rotate_max_size )
	{
		RemoveFileWatcher( p_plugin_env , p_trace_file );
	}
	
	return 0;
}

static int AddFileWatcher( struct LogpipeInputPlugin_file *p_plugin_env , char *filename )
{
	struct TraceFile	*p_trace_file = NULL ;
	struct TraceFile	*p_trace_file_not_exist = NULL ;
	struct stat		file_stat ;
	int			len ;
	
	int			nret = 0 ;
	
	if( filename[0] == '.' || filename[0] == '_' )
	{
		INFOLOG( "file[%s] has ignored" , filename )
		return 0;
	}
	
	p_trace_file = (struct TraceFile *)malloc( sizeof(struct TraceFile) ) ;
	if( p_trace_file == NULL )
	{
		ERRORLOG( "malloc failed , errno[%d]" , errno )
		return -1;
	}
	memset( p_trace_file , 0x00 , sizeof(struct TraceFile) );
	
	len = asprintf( & (p_trace_file->path_filename) , "%s/%s" , p_plugin_env->path , filename ) ;
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
	
	p_trace_file->pathname = p_plugin_env->path ;
	
	len = asprintf( & p_trace_file->filename , "%s" , filename );
	if( len == -1 )
	{
		ERRORLOG( "asprintf failed , errno[%d]" , errno )
		free( p_trace_file->path_filename );
		free( p_trace_file );
		return -1;
	}
	p_trace_file->filename_len = len ;
	
	p_trace_file->inotify_file_wd = inotify_add_watch( p_plugin_env->inotify_fd , p_trace_file->path_filename , (uint32_t)(IN_CLOSE_WRITE|IN_DELETE_SELF|IN_MOVE_SELF|IN_IGNORED) ) ;
	if( p_trace_file->inotify_file_wd == -1 )
	{
		ERRORLOG( "inotify_add_watch[%s] failed , errno[%d]" , p_trace_file->path_filename , errno )
		free( p_trace_file->path_filename );
		free( p_trace_file );
		return -1;
	}
	else
	{
		INFOLOG( "inotify_add_watch[%s] ok , inotify_fd[%d] inotify_wd[%d]" , p_trace_file->path_filename , p_inotify_session->inotify_fd , p_trace_file->inotify_file_wd )
	}
	
	p_trace_file_not_exist = QueryTraceFileWdTreeNode( p_plugin_env , p_trace_file ) ;
	if( p_trace_file_not_exist == NULL )
	{
		nret = LinkTraceFileWdTreeNode( p_plugin_env , p_trace_file ) ;
		if( nret )
		{
			ERRORLOG( "LinkTraceFileWdTreeNode[%s] failed , errno[%d]" , p_trace_file->path_filename , errno )
			INFOLOG( "inotify_rm_watch[%s] ok , inotify_fd[%d] inotify_wd[%d]" , p_trace_file->path_filename , p_plugin_env->inotify_fd , p_trace_file->inotify_file_wd )
			inotify_rm_watch( p_plugin_env->inotify_fd , p_trace_file->inotify_file_wd );
			free( p_trace_file->path_filename );
			free( p_trace_file );
			return -1;
		}
	}
	
	return CheckFileOffset( p_env , p_plugin_env , p_trace_file );
}

static int RemoveFileWatcher( struct LogpipeInputPlugin_file *p_plugin_env , struct TraceFile *p_trace_file )
{
	UnlinkTraceFileWdTreeNode( p_plugin_env , p_trace_file );
	
	INFOLOG( "inotify_rm_watch[%s] ok , inotify_fd[%d] inotify_wd[%d]" , p_trace_file->path_filename , p_plugin_env->inotify_fd , p_trace_file->inotify_file_wd )
	inotify_rm_watch( p_plugin_env->inotify_fd , p_trace_file->inotify_file_wd );
	free( p_trace_file->path_filename );
	free( p_trace_file );
	
	return 0;
}

static int ReadFilesToinotifyWdTree( struct LogpipeInputPlugin_file *p_plugin_env )
{
	DIR			*dir = NULL ;
	struct dirent		*ent = NULL ;
	
	int			nret = 0 ;
	
	dir = opendir( p_plugin_env->path ) ;
	if( dir == NULL )
	{
		printf( "ERROR : opendir[%s] failed , errno[%d]\n" , p_plugin_env->path , errno );
		return -1;
	}
	
	while(1)
	{
		ent = readdir( dir ) ;
		if( ent == NULL )
			break;
		
		if( ent->d_type & DT_REG )
		{
			nret = AddFileWatcher( p_plugin_env , ent->d_name ) ;
			if( nret )
			{
				printf( "ERROR : AddFileWatcher[%s] failed , errno[%d]\n" , ent->d_name , errno );
				closedir( dir );
				return -1;
			}
		}
	}
	
	closedir( dir );
	
	return 0;
}

funcInitLogpipeInputPlugin InitLogpipeInputPlugin ;
int InitLogpipeInputPlugin( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , struct LogpipePluginConfigItem *p_plugin_config_items , void **pp_context , int *p_fd )
{
	struct LogpipeInputPlugin_file	*p_plugin_env = NULL ;
	char				*p = NULL ;
	
	p_plugin_env = (struct LogpipeInputPlugin_file *)malloc( sizeof(struct LogpipeInputPlugin_file) ) ;
	if( p_plugin_env == NULL )
	{
		ERRORLOG( "malloc failed , errno[%d]" , errno );
		return -1;
	}
	memset( p_plugin_env , 0x00 , sizeof(struct LogpipeInputPlugin_file) );
	
	/* 解析插件配置 */
	p_plugin_env->p_env = p_env ;
	p_plugin_env->p_logpipe_input_plugin = p_logpipe_input_plugin ;
	
	p_plugin_env->path = QueryPluginConfigItem( p_plugin_config_items , "path" ) ;
	
	p_plugin_env->file = QueryPluginConfigItem( p_plugin_config_items , "file" ) ;
	
	p = QueryPluginConfigItem( p_plugin_config_items , "file" ) ;
	if( p )
		p_plugin_env->rotate_max_size = atoi(p) ;
	else
		p_plugin_env->rotate_max_size = 0 ;
	
	p_plugin_env->exec_after_rotate = QueryPluginConfigItem( p_plugin_config_items , "exec_after_rotate" ) ;
	
	p_plugin_env->compress_algorithm = QueryPluginConfigItem( p_plugin_config_items , "compress_algorithm" ) ;
	if( p_plugin_env->compress_algorithm )
	{
		if( STRCMP( p_plugin_env->compress_algorithm , == , "deflate" ) )
		{
			;
		}
		else
		{
			ERRORLOG( "compress_algorithm[%s] invalid" , p_plugin_env->compress_algorithm );
			return -1;
		}
	}
	
	/* 初始化插件环境内部数据 */
	p_plugin_env->inotify_fd = inotify_init() ;
	if( p_plugin_env->inotify_fd == -1 )
	{
		ERRORLOG( "inotify_init failed , errno[%d]" , errno );
		return -1;
	}
	
	p_plugin_env->inotify_path_wd = inotify_add_watch( p_plugin_env->inotify_fd , p_plugin_env->path , (uint32_t)(IN_CREATE|IN_MOVED_TO|IN_DELETE_SELF|IN_MOVE_SELF) );
	if( p_plugin_env->inotify_path_wd == -1 )
	{
		ERRORLOG( "inotify_add_watch[%s] failed , errno[%d]" , p_inotify_session->inotify_path , errno );
		return -1;
	}
	
	p_env->inotify_read_bufsize = INOTIFY_READ_BUFSIZE ;
	p_env->inotify_read_buffer = (char*)malloc( p_env->inotify_read_bufsize ) ;
	if( p_env->inotify_read_buffer == NULL )
	{
		ERRORLOG( "malloc failed , errno[%d]" , errno );
		return -1;
	}
	memset( p_env->inotify_read_buffer , 0x00 , p_env->inotify_read_bufsize );
	
	nret = ReadFilesToinotifyWdTree( p_plugin_env ) ;
	if( nret )
	{
		return nret;
	}
	
	/* 设置插件环境上下文 */
	(*pp_context) = p_plugin_env ;
	(*p_fd) = p_plugin_env->inotify_fd ;
	
	return 0;
}

funcOnLogpipeInputEvent OnLogpipeInputEvent ;
int OnLogpipeInputEvent( struct LogpipeEnv *p_env , void *p_context )
{
	struct LogpipeInputPlugin_file	*p_plugin_env = (struct LogpipeInputPlugin_file *)p_context ;
	
	int				inotify_read_nbytes ;
	
	long				len ;
	struct inotify_event		*p_inotify_event = NULL ;
	struct inotify_event		*p_overflow_inotify_event = NULL ;
	struct TraceFile		trace_file ;
	struct TraceFile		*p_trace_file = NULL ;
	
	int				nret = 0 ;
	
	nret = ioctl( p_plugin_env->inotify_fd , FIONREAD , & inotify_read_nbytes );
	if( nret )
	{
		FATALLOG( "ioctl failed , errno[%d]" , errno )
		return -1;
	}
	
	if( inotify_read_nbytes+1 > p_plugin_env->inotify_read_bufsize )
	{
		char	*tmp = NULL ;
		
		WARNLOG( "inotify read buffer resize [%d]bytes to [%d]bytes" , p_plugin_env->inotify_read_bufsize , inotify_read_nbytes+1 )
		p_plugin_env->inotify_read_bufsize = inotify_read_nbytes+1 ;
		tmp = (char*)realloc( p_env->inotify_read_buffer , p_plugin_env->inotify_read_bufsize ) ;
		if( tmp == NULL )
		{
			FATALLOG( "realloc failed , errno[%d]" , errno )
			return -1;
		}
		p_plugin_env->inotify_read_buffer = tmp ;
	}
	memset( p_plugin_env->inotify_read_buffer , 0x00 , p_plugin_env->inotify_read_bufsize );
	
	DEBUGLOG( "read inotify[%d] ..." , p_plugin_env->inotify_fd )
	len = read( p_plugin_env->inotify_fd , p_plugin_env->inotify_read_buffer , LOGPIPE_INOTIFY_READ_BUFSIZE-1 ) ;
	if( len == -1 )
	{
		ERRORLOG( "read inotify[%d] failed , errno[%d]" , p_plugin_env->inotify_fd , errno )
		return -1;
	}
	else
	{
		INFOLOG( "read inotify[%d] ok , [%d]bytes" , p_plugin_env->inotify_fd , len )
	}
	
	p_inotify_event = (struct inotify_event *)(p_plugin_env->inotify_read_buffer) ;
	p_overflow_inotify_event = (struct inotify_event *)(p_plugin_env->inotify_read_buffer+len) ;
	while( p_inotify_event < p_overflow_inotify_event )
	{
		DEBUGLOG( "inotify event wd[%d] mask[0x%X] cookie[%d] len[%d] name[%.*s]" , p_inotify_event->wd , p_inotify_event->mask , p_inotify_event->cookie , p_inotify_event->len , p_inotify_event->len , p_inotify_event->name )
		
		if( p_inotify_event->mask & IN_IGNORED )
		{
			;
		}
		else if( p_inotify_event->wd == p_plugin_env->inotify_path_wd )
		{
			if( ( p_inotify_event->mask & IN_CREATE ) || ( p_inotify_event->mask & IN_MOVED_TO ) )
			{
				nret = AddFileWatcher( p_plugin_env , p_inotify_event->name ) ;
				if( nret )
				{
					ERRORLOG( "AddFileWatcher[%s] failed , errno[%d]" , p_inotify_event->name , errno )
					return -1;
				}
			}
			else if( ( p_inotify_event->mask & IN_DELETE_SELF ) || ( p_inotify_event->mask & IN_MOVE_SELF ) )
			{
				INFOLOG( "[%s] had deleted" , p_plugin_env->inotify_path )
				INFOLOG( "inotify_rm_watch[%s] ok , inotify_fd[%d] inotify_wd[%d]" , p_plugin_env->inotify_path , p_plugin_env->inotify_fd , p_plugin_env->inotify_path_wd )
				inotify_rm_watch( p_plugin_env->inotify_fd , p_plugin_env->inotify_path_wd );
				break;
			}
			else
			{
				ERRORLOG( "unknow dir inotify event mask[0x%X]" , p_inotify_event->mask )
			}
		}
		else
		{
			trace_file.inotify_file_wd = p_inotify_event->wd ;
			p_trace_file = QueryTraceFileWdTreeNode( p_plugin_env , & trace_file ) ;
			if( p_trace_file == NULL )
			{
				ERRORLOG( "wd[%d] not found" , trace_file.inotify_file_wd )
			}
			else
			{
				if( p_inotify_event->mask & IN_CLOSE_WRITE )
				{
					nret = CheckFileOffset( p_plugin_env , p_trace_file ) ;
					if( nret )
					{
						ERRORLOG( "CheckFileOffset failed[%d] , errno[%d]" , nret , errno )
						return -1;
					}
				}
				else if( ( p_inotify_event->mask & IN_DELETE_SELF ) || ( p_inotify_event->mask & IN_MOVE_SELF ) )
				{
					nret = RemoveFileWatcher( p_plugin_env , p_trace_file ) ;
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
	
	if( p_plugin_env->inotify_read_bufsize != INOTIFY_READ_BUFSIZE )
	{
		char	*tmp = NULL ;
		
		WARNLOG( "inotify read buffer resize [%d]bytes to [%d]bytes" , p_plugin_env->inotify_read_bufsize , LOGPIPE_INOTIFY_READ_BUFSIZE )
		p_plugin_env->inotify_read_bufsize = INOTIFY_READ_BUFSIZE ;
		tmp = (char*)realloc( p_env->inotify_read_buffer , p_plugin_env->inotify_read_bufsize ) ;
		if( tmp == NULL )
		{
			FATALLOG( "realloc failed , errno[%d]" , errno )
			return -1;
		}
		p_plugin_env->inotify_read_buffer = tmp ;
	}
	
	return 0;
}

funcBeforeReadLogpipeInput BeforeReadLogpipeInput ;
int BeforeReadLogpipeInput( struct LogpipeEnv *p_env , void *p_context )
{
}

funcReadLogpipeInput ReadLogpipeInput ;
int ReadLogpipeInput( struct LogpipeEnv *p_env , void *p_context , uint32_t *p_block_len , char *block_buf , int block_bufsize )
{
	struct LogpipeInputPlugin_file	*p_plugin_env = (struct LogpipeInputPlugin_file *)p_context ;
	
	int				len ;
	
	if( p_plugin_env->compress_algorithm == NULL )
	{
		if( p_plugin_env->remain_len > block_bufsize - 1 )
			(*p_block_len) = block_bufsize - 1 ;
		else
			(*p_block_len) = p_plugin_env->remain_len ;
		
		if( (*p_block_len) > 0 )
		{
			len = readn( p_plugin_env->fd , block_buf , (*p_block_len) ) ;
			if( len == -1 )
			{
				ERRORLOG( "read file failed , errno[%d]" , errno )
				return -1;
			}
			else
			{
				INFOLOG( "read file ok , [%d]bytes" , (*p_block_len) )
				DEBUGHEXLOG( block_buf , len , NULL )
			}
		}
	}
	else
	{
		if( STRCMP( p_plugin_env->compress_algorithm , == , "deflate" ) )
		{
			z_stream		deflate_strm ;
			
			char			block_in_buf[ 102357 + 1 ] ;
			uint32_t		block_in_len ;
			uint32_t		block_in_len_htonl ;
			int			len ;
			
			if( p_plugin_env->remain_len > sizeof(block_in_buf) - 1 )
				block_in_len = sizeof(block_in_buf) - 1 ;
			else
				block_in_len = p_plugin_env->remain_len ;
			
			if( block_in_len > 0 )
			{
				len = readn( p_plugin_env->fd , block_in_buf , block_in_len ) ;
				if( len == -1 )
				{
					ERRORLOG( "read file failed , errno[%d]" , errno )
					return -1;
				}
				else
				{
					INFOLOG( "read file ok , [%d]bytes" , block_in_len )
					DEBUGHEXLOG( block_in_buf , len , NULL )
				}
				
				memset( & deflate_strm , 0x00 , sizeof(z_stream) );
				nret = deflateInit( & deflate_strm , Z_DEFAULT_COMPRESSION ) ;
				if( nret != Z_OK )
				{
					FATALLOG( "deflateInit failed[%d]" , nret );
					CLOSE_ALL_DUMP_FILE_AND_FORWARD_SOCKET
					return -1;
				}
				
				deflate_strm.avail_in = block_in_len ;
				deflate_strm.next_in = (Bytef*)block_in_buf ;
				
				deflate_strm.avail_out = block_bufsize-1 ;
				deflate_strm.next_out = (Bytef*)block_buf ;
				nret = deflate( & deflate_strm , Z_FINISH ) ;
				if( nret == Z_STREAM_ERROR )
				{
					FATALLOG( "deflate return Z_STREAM_ERROR" )
					deflateEnd( & deflate_strm );
					return -1;
				}
				if( deflate_strm.avail_out > 0 )
				{
					FATALLOG( "deflate remain data" )
					deflateEnd( & deflate_strm );
					return -1;
				}
				(*p_block_len) = block_bufsize-1 - deflate_strm.avail_out ;
				
				// block_out_len_htonl = htonl( block_out_len ) ;
				
				deflateEnd( & deflate_strm );
			}
		}
		else
		{
			ERRORLOG( "compress_algorithm[%s] invalid" , p_plugin_env->compress_algorithm );
			return -1;
		}
	}
	
	// block_in_len_htonl = htonl( block_in_len ) ;
	
	p_plugin_env->remain_len -= (*p_block_len) ;
	
	return 0;
}

func AfterReadLogpipeInput AfterReadLogpipeInput ;
int AfterReadLogpipeInput( struct LogpipeEnv *p_env , void *p_context )
{
	
	
}

funcCleanLogpipeInputPlugin CleanLogpipeInputPlugin ;
int CleanLogpipeInputPlugin( struct LogpipeEnv *p_env , void *p_context )
{
	struct LogpipeInputPlugin_file	*p_plugin_env = (struct LogpipeInputPlugin_file *)p_context ;
	
	inotify_rm_watch( p_plugin_env->inotify_fd , p_plugin_env->inotify_path_wd );
	close( p_plugin_env->inotify_fd );
	
	free( p_plugin_env->inotify_read_buffer );
	
	INFOLOG( "free p_plugin_env" )
	free( p_plugin_env );
	
	return 0;
}

