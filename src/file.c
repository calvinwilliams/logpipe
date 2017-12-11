#include "logpipe_in.h"

int AddFileWatcher( struct LogPipeEnv *p_env , struct InotifySession *p_inotify_session , char *filename )
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
	
	len = asprintf( & (p_trace_file->path_filename) , "%s/%s" , p_inotify_session->inotify_path , filename ) ;
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
	
	p_trace_file->pathname = p_inotify_session->inotify_path ;
	
	len = asprintf( & p_trace_file->filename , "%s" , filename );
	if( len == -1 )
	{
		ERRORLOG( "asprintf failed , errno[%d]" , errno )
		free( p_trace_file->path_filename );
		free( p_trace_file );
		return -1;
	}
	p_trace_file->filename_len = len ;
	
	p_trace_file->inotify_file_wd = inotify_add_watch( p_inotify_session->inotify_fd , p_trace_file->path_filename , (uint32_t)(IN_CLOSE_WRITE|IN_DELETE_SELF|IN_MOVE_SELF|IN_IGNORED) ) ;
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
	
	p_trace_file_not_exist = QueryTraceFileWdTreeNode( p_inotify_session , p_trace_file ) ;
	if( p_trace_file_not_exist == NULL )
	{
		nret = LinkTraceFileWdTreeNode( p_inotify_session , p_trace_file ) ;
		if( nret )
		{
			ERRORLOG( "LinkTraceFileWdTreeNode[%s] failed , errno[%d]" , p_trace_file->path_filename , errno )
			INFOLOG( "inotify_rm_watch[%s] ok , inotify_fd[%d] inotify_wd[%d]" , p_trace_file->path_filename , p_inotify_session->inotify_fd , p_trace_file->inotify_file_wd )
			inotify_rm_watch( p_inotify_session->inotify_fd , p_trace_file->inotify_file_wd );
			free( p_trace_file->path_filename );
			free( p_trace_file );
			return -1;
		}
	}
	
	return OnReadingFile( p_env , p_inotify_session , p_trace_file );
}

int RemoveFileWatcher( struct LogPipeEnv *p_env , struct InotifySession *p_inotify_session , struct TraceFile *p_trace_file )
{
	UnlinkTraceFileWdTreeNode( p_inotify_session , p_trace_file );
	
	INFOLOG( "inotify_rm_watch[%s] ok , inotify_fd[%d] inotify_wd[%d]" , p_trace_file->path_filename , p_inotify_session->inotify_fd , p_trace_file->inotify_file_wd )
	inotify_rm_watch( p_inotify_session->inotify_fd , p_trace_file->inotify_file_wd );
	free( p_trace_file->path_filename );
	free( p_trace_file );
	
	return 0;
}

int RoratingFile( char *pathname , char *filename , int filename_len )
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

int OnReadingFile( struct LogPipeEnv *p_env , struct InotifySession *p_inotify_session , struct TraceFile *p_trace_file )
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
		return RemoveFileWatcher( p_env , p_inotify_session , p_trace_file );
	}
	
	memset( & file_stat , 0x00 , sizeof(struct stat) );
	nret = fstat( fd , & file_stat ) ;
	if( nret == -1 )
	{
		ERRORLOG( "fstat[%s] failed , errno[%d]" , p_trace_file->path_filename , errno )
		close( fd );
		return RemoveFileWatcher( p_env , p_inotify_session , p_trace_file );
	}
	
	if( p_env->conf.rotate.file_rotate_max_size > 0 && file_stat.st_size >= p_env->conf.rotate.file_rotate_max_size )
	{
		INFOLOG( "file_stat.st_size[%d] > p_env->conf.rotate.file_rotate_max_size[%d]" , file_stat.st_size , p_env->conf.rotate.file_rotate_max_size )
		RoratingFile( p_trace_file->pathname , p_trace_file->filename , p_trace_file->filename_len );
		
		memset( & file_stat , 0x00 , sizeof(struct stat) );
		nret = fstat( fd , & file_stat ) ;
		if( nret == -1 )
		{
			ERRORLOG( "fstat[%s] failed , errno[%d]" , p_trace_file->path_filename , errno )
			close( fd );
			return RemoveFileWatcher( p_env , p_inotify_session , p_trace_file );
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
		nret = ToOutputs( p_env , NULL , -1 , p_trace_file->filename , p_trace_file->filename_len , fd , append_len , p_env->compress_algorithm ) ;
		if( nret )
		{
			ERRORLOG( "ToOutputs failed[%d]" , nret )
			close( fd );
			return 0;
		}
		
		p_trace_file->trace_offset = file_stat.st_size ;
	}
	
	close( fd );
	
	if( p_env->conf.rotate.file_rotate_max_size > 0 && file_stat.st_size >= p_env->conf.rotate.file_rotate_max_size )
	{
		RemoveFileWatcher( p_env , p_inotify_session , p_trace_file );
	}
	
	return 0;
}

int OnInotifyHandler( struct LogPipeEnv *p_env , struct InotifySession *p_inotify_session )
{
	int			inotify_read_nbytes ;
	
	long			len ;
	struct inotify_event	*p_inotify_event = NULL ;
	struct inotify_event	*p_overflow_inotify_event = NULL ;
	struct TraceFile	trace_file ;
	struct TraceFile	*p_trace_file = NULL ;
	
	int			nret = 0 ;
	
	nret = ioctl( p_inotify_session->inotify_fd , FIONREAD , & inotify_read_nbytes );
	if( nret )
	{
		FATALLOG( "ioctl failed , errno[%d]" , errno )
		return -1;
	}
	
	if( inotify_read_nbytes+1 > p_env->inotify_read_bufsize )
	{
		char	*tmp = NULL ;
		
		WARNLOG( "inotify read buffer resize [%d]bytes to [%d]bytes" , p_env->inotify_read_bufsize , inotify_read_nbytes+1 )
		p_env->inotify_read_bufsize = inotify_read_nbytes+1 ;
		tmp = (char*)realloc( p_env->inotify_read_buffer , p_env->inotify_read_bufsize ) ;
		if( tmp == NULL )
		{
			FATALLOG( "realloc failed , errno[%d]" , errno )
			return -1;
		}
		p_env->inotify_read_buffer = tmp ;
	}
	memset( p_env->inotify_read_buffer , 0x00 , p_env->inotify_read_bufsize );
	
	DEBUGLOG( "read inotify[%d] ..." , p_inotify_session->inotify_fd )
	len = read( p_inotify_session->inotify_fd , p_env->inotify_read_buffer , LOGPIPE_INOTIFY_READ_BUFSIZE-1 ) ;
	if( len == -1 )
	{
		ERRORLOG( "read inotify[%d] failed , errno[%d]" , p_inotify_session->inotify_fd , errno )
		return -1;
	}
	else
	{
		INFOLOG( "read inotify[%d] ok , [%d]bytes" , p_inotify_session->inotify_fd , len )
	}
	
	p_inotify_event = (struct inotify_event *)(p_env->inotify_read_buffer) ;
	p_overflow_inotify_event = (struct inotify_event *)(p_env->inotify_read_buffer+len) ;
	while( p_inotify_event < p_overflow_inotify_event )
	{
		DEBUGLOG( "inotify event wd[%d] mask[0x%X] cookie[%d] len[%d] name[%.*s]" , p_inotify_event->wd , p_inotify_event->mask , p_inotify_event->cookie , p_inotify_event->len , p_inotify_event->len , p_inotify_event->name )
		
		if( p_inotify_event->mask & IN_IGNORED )
		{
			;
		}
		else if( p_inotify_event->wd == p_inotify_session->inotify_path_wd )
		{
			if( ( p_inotify_event->mask & IN_CREATE ) || ( p_inotify_event->mask & IN_MOVED_TO ) )
			{
				nret = AddFileWatcher( p_env , p_inotify_session , p_inotify_event->name ) ;
				if( nret )
				{
					ERRORLOG( "AddFileWatcher[%s] failed , errno[%d]" , p_inotify_event->name , errno )
					return -1;
				}
			}
			else if( ( p_inotify_event->mask & IN_DELETE_SELF ) || ( p_inotify_event->mask & IN_MOVE_SELF ) )
			{
				INFOLOG( "[%s] had deleted" , p_inotify_session->inotify_path )
				INFOLOG( "inotify_rm_watch[%s] ok , inotify_fd[%d] inotify_wd[%d]" , p_inotify_session->inotify_path , p_inotify_session->inotify_fd , p_inotify_session->inotify_path_wd )
				inotify_rm_watch( p_inotify_session->inotify_fd , p_inotify_session->inotify_path_wd );
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
			p_trace_file = QueryTraceFileWdTreeNode( p_inotify_session , & trace_file ) ;
			if( p_trace_file == NULL )
			{
				ERRORLOG( "wd[%d] not found" , trace_file.inotify_file_wd )
			}
			else
			{
				if( p_inotify_event->mask & IN_CLOSE_WRITE )
				{
					nret = OnReadingFile( p_env , p_inotify_session , p_trace_file ) ;
					if( nret )
					{
						ERRORLOG( "OnReadingFile failed[%d] , errno[%d]" , nret , errno )
						return -1;
					}
				}
				else if( ( p_inotify_event->mask & IN_DELETE_SELF ) || ( p_inotify_event->mask & IN_MOVE_SELF ) )
				{
					nret = RemoveFileWatcher( p_env , p_inotify_session , p_trace_file ) ;
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
	
	if( p_env->inotify_read_bufsize != LOGPIPE_INOTIFY_READ_BUFSIZE )
	{
		char	*tmp = NULL ;
		
		WARNLOG( "inotify read buffer resize [%d]bytes to [%d]bytes" , p_env->inotify_read_bufsize , LOGPIPE_INOTIFY_READ_BUFSIZE )
		p_env->inotify_read_bufsize = LOGPIPE_INOTIFY_READ_BUFSIZE ;
		tmp = (char*)realloc( p_env->inotify_read_buffer , p_env->inotify_read_bufsize ) ;
		if( tmp == NULL )
		{
			FATALLOG( "realloc failed , errno[%d]" , errno )
			return -1;
		}
		p_env->inotify_read_buffer = tmp ;
	}
	
	return 0;
}

