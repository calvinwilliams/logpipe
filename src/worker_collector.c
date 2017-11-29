#include "logpipe_in.h"

static int AddFileWatcher( struct LogPipeEnv *p_env , char *filename )
{
	struct TraceFile	*p_trace_file = NULL ;
	int			fd ;
	static uint32_t		inotify_mask = IN_CLOSE_WRITE|IN_DELETE_SELF|IN_MOVE_SELF|IN_IGNORED ;
	
	int			nret = 0 ;
	
	if( filename[0] == '.' )
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
	
	nret = asprintf( & (p_trace_file->path_filename) , "%s/%s" , p_env->role_context.collector.monitor_path , filename );
	if( nret == -1 )
	{
		ERRORLOG( "asprintf failed , errno[%d]" , errno )
		free( p_trace_file );
		return -1;
	}
	fd = open( p_trace_file->path_filename , O_RDONLY ) ;
	if( fd == -1 )
	{
		ERRORLOG( "open[%s] failed , errno[%d]" , p_trace_file->path_filename , errno )
		free( p_trace_file->path_filename );
		free( p_trace_file );
		return -1;
	}
	p_trace_file->trace_offset = lseek( fd , 0 , SEEK_END ) ;
	close( fd );
	
	p_trace_file->inotify_file_wd = inotify_add_watch( p_env->role_context.collector.inotify_fd , p_trace_file->path_filename , inotify_mask ) ;
	if( p_trace_file->inotify_file_wd == -1 )
	{
		ERRORLOG( "inotify_add_watch[%s] failed , errno[%d]" , p_trace_file->path_filename , errno )
		free( p_trace_file->path_filename );
		free( p_trace_file );
		return -1;
	}
	else
	{
		INFOLOG( "inotify_add_watch[%s] ok , inotify_fd[%d] inotify_wd[%d]" , p_trace_file->path_filename , p_env->role_context.collector.inotify_fd , p_trace_file->inotify_file_wd )
	}
	
	nret = LinkTraceFileWdTreeNode( p_env , p_trace_file ) ;
	if( nret )
	{
		ERRORLOG( "LinkTraceFileWdTreeNode[%s] failed , errno[%d]" , p_trace_file->path_filename , errno )
		INFOLOG( "inotify_rm_watch[%s] ok , inotify_fd[%d] inotify_wd[%d]" , p_trace_file->path_filename , p_env->role_context.collector.inotify_fd , p_trace_file->inotify_file_wd )
		inotify_rm_watch( p_env->role_context.collector.inotify_fd , p_trace_file->inotify_file_wd );
		free( p_trace_file->path_filename );
		free( p_trace_file );
		return -1;
	}
	
	return 0;
}

static int RemoveFileWatcher( struct LogPipeEnv *p_env , struct TraceFile *p_trace_file )
{
	UnlinkTraceFileWdTreeNode( p_env , p_trace_file );
	
	INFOLOG( "inotify_rm_watch[%s] ok , inotify_fd[%d] inotify_wd[%d]" , p_trace_file->path_filename , p_env->role_context.collector.inotify_fd , p_trace_file->inotify_file_wd )
	inotify_rm_watch( p_env->role_context.collector.inotify_fd , p_trace_file->inotify_file_wd );
	free( p_trace_file->path_filename );
	free( p_trace_file );
	
	return 0;
}

static int ReadFilesToInotifyWdTree( struct LogPipeEnv *p_env )
{
	DIR			*dir = NULL ;
	struct dirent		*ent = NULL ;
	
	int			nret = 0 ;
	
	dir = opendir( p_env->role_context.collector.monitor_path ) ;
	if( dir == NULL )
	{
		ERRORLOG( "opendir[%s] failed , errno[%d]" , p_env->role_context.collector.monitor_path , errno )
		return -1;
	}
	
	while(1)
	{
		ent = readdir( dir ) ;
		if( ent == NULL )
			break;
		
		if( ent->d_type & DT_REG )
		{
			nret = AddFileWatcher( p_env , ent->d_name ) ;
			if( nret )
			{
				ERRORLOG( "AddFileWatcher[%s] failed , errno[%d]" , ent->d_name , errno )
				return -1;
			}
		}
	}
	
	closedir( dir );
	
	return 0;
}

static int OutputFileAppender( struct LogPipeEnv *p_env , struct TraceFile *p_trace_file )
{
	int		fd ;
	struct stat	file_stat ;
	int		appender_len ;
	static char	*buffer = NULL ;
	static int	buf_size = 0 ;
	int		buf_len ;
	
	int		nret = 0 ;
	
	DEBUGLOG( "catch file[%s] appender" , p_trace_file->path_filename )
	
	fd = open( p_trace_file->path_filename , O_RDONLY ) ;
	if( fd == -1 )
	{
		ERRORLOG( "open[%s] failed , errno[%d]" , p_trace_file->path_filename , errno )
		return -1;
	}
	
	memset( & file_stat , 0x00 , sizeof(struct stat) );
	nret = fstat( fd , & file_stat ) ;
	if( nret == -1 )
	{
		ERRORLOG( "fstat[%s] failed , errno[%d]" , p_trace_file->path_filename , errno )
		close( fd );
		return RemoveFileWatcher( p_env , p_trace_file );
	}
	
	DEBUGLOG( "file_size[%d] trace_offset[%d]" , file_stat.st_size , p_trace_file->trace_offset )
	if( file_stat.st_size < p_trace_file->trace_offset )
	{
		p_trace_file->trace_offset = file_stat.st_size ;
	}
	else if( file_stat.st_size > p_trace_file->trace_offset )
	{
		appender_len = file_stat.st_size - p_trace_file->trace_offset ;
		if( buffer == NULL )
		{
			buf_size = appender_len + 1 ;
			buffer = (char*)malloc( buf_size ) ;
			if( buffer == NULL )
			{
				ERRORLOG( "malloc failed , errno[%d]" , errno )
				close( fd );
				return -1;
			}
			else
			{
				DEBUGLOG( "malloc buffer ok, [%d]bytes" , buf_size )
			}
		}
		else if( buf_size-1 < appender_len )
		{
			int		new_buf_size ;
			char		*new_buffer = NULL ;
			
			new_buf_size = appender_len + 1 ;
			new_buffer = (char*)realloc( buffer , new_buf_size ) ;
			if( new_buffer == NULL )
			{
				ERRORLOG( "realloc failed , errno[%d]" , errno )
				close( fd );
				return -1;
			}
			else
			{
				DEBUGLOG( "realloc buffer ok, [%d]bytes" , buf_size )
			}
			buffer = new_buffer ;
			buf_size = new_buf_size ;
		}
		memset( buffer , 0x00 , buf_size );
		
		buf_len = read( fd , buffer , appender_len ) ;
		if( buf_len != appender_len )
		{
			ERRORLOG( "read[%s] failed , errno[%d]" , p_trace_file->path_filename , errno )
			close( fd );
			return -1;
		}
		else
		{
			DEBUGHEXLOG( buffer , appender_len , "output file appender , [%d]bytes" , appender_len )
		}
	}
	
	close( fd );
	
	return 0;
}

int worker_collector( struct LogPipeEnv *p_env )
{
	long			read_len ;
	struct inotify_event	*p_inotify_event = NULL ;
	struct inotify_event	*p_overflow_inotify_event = NULL ;
	struct TraceFile	trace_file ;
	struct TraceFile	*p_trace_file = NULL ;
	
	int			nret = 0 ;
	
	SetLogFile( "%s" , p_env->log_pathfilename );
	INFOLOG( "monitor_path[%s] inotify_fd[%d] inotify_path_wd[%d]" , p_env->role_context.collector.monitor_path , p_env->role_context.collector.inotify_fd , p_env->role_context.collector.inotify_path_wd )
	
	nret = ReadFilesToInotifyWdTree( p_env ) ;
	if( nret )
	{
		ERRORLOG( "ReadFilesToInotifyWdTree failed[%d]" , nret )
		return nret;
	}
	
	while(1)
	{
		DEBUGLOG( "read inotify ..." )
		read_len = read( p_env->role_context.collector.inotify_fd , p_env->role_context.collector.inotify_read_buffer , sizeof(p_env->role_context.collector.inotify_read_buffer)-1 ) ;
		if( read_len == -1 )
		{
			ERRORLOG( "read failed , errno[%d]" , errno )
			return -1;
		}
		else
		{
			INFOLOG( "read inotify ok , [%d]bytes" , read_len )
		}
		
		p_inotify_event = (struct inotify_event *)(p_env->role_context.collector.inotify_read_buffer) ;
		p_overflow_inotify_event = (struct inotify_event *)(p_env->role_context.collector.inotify_read_buffer+read_len) ;
		while( p_inotify_event < p_overflow_inotify_event )
		{
			DEBUGLOG( "inotify event wd[%d] mask[0x%X] cookie[%d] len[%d] name[%.*s]" , p_inotify_event->wd , p_inotify_event->mask , p_inotify_event->cookie , p_inotify_event->len , p_inotify_event->len , p_inotify_event->name )
			
			if( p_inotify_event->mask & IN_IGNORED )
			{
				;
			}
			else if( p_inotify_event->wd == p_env->role_context.collector.inotify_path_wd )
			{
				if( ( p_inotify_event->mask & IN_CREATE ) || ( p_inotify_event->mask & IN_MOVED_TO ) )
				{
					nret = AddFileWatcher( p_env , p_inotify_event->name ) ;
					if( nret )
					{
						ERRORLOG( "AddFileWatcher[%s] failed , errno[%d]" , p_inotify_event->name , errno )
						return -1;
					}
				}
				else if( ( p_inotify_event->mask & IN_DELETE_SELF ) || ( p_inotify_event->mask & IN_MOVE_SELF ) )
				{
					INFOLOG( "[%s] had deleted" , p_env->role_context.collector.monitor_path )
					INFOLOG( "inotify_rm_watch[%s] ok , inotify_fd[%d] inotify_wd[%d]" , p_env->role_context.collector.monitor_path , p_env->role_context.collector.inotify_fd , p_env->role_context.collector.inotify_path_wd )
					inotify_rm_watch( p_env->role_context.collector.inotify_fd , p_env->role_context.collector.inotify_path_wd );
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
				p_trace_file = QueryTraceFileWdTreeNode( p_env , & trace_file ) ;
				if( p_trace_file == NULL )
				{
					ERRORLOG( "wd[%d] not found" , trace_file.inotify_file_wd )
				}
				else
				{
					if( p_inotify_event->mask & IN_CLOSE_WRITE )
					{
						nret = OutputFileAppender( p_env , p_trace_file ) ;
						if( nret )
						{
							ERRORLOG( "OutputFileAppender failed , errno[%d]" , errno )
							return -1;
						}
					}
					else if( ( p_inotify_event->mask & IN_DELETE_SELF ) || ( p_inotify_event->mask & IN_MOVE_SELF ) )
					{
						nret = RemoveFileWatcher( p_env , p_trace_file ) ;
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
	}
	
	return 0;
}

