#include "logpipe_in.h"

static int AddFileWatcher( struct LogPipeEnv *p_env , char *filename )
{
	struct TraceFile	*p_trace_file = NULL ;
	
	int			nret = 0 ;
	
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
		return -1;
	}
	p_trace_file->fd = open( p_trace_file->path_filename , O_RDONLY ) ;
	if( p_trace_file->fd == -1 )
	{
		ERRORLOG( "open[%s] failed , errno[%d]" , p_trace_file->path_filename , errno )
		return -1;
	}
	p_trace_file->trace_offset = lseek( p_trace_file->fd , 0 , SEEK_END ) ;
	p_trace_file->inotify_file_wd = inotify_add_watch( p_env->role_context.collector.inotify_fd , p_trace_file->path_filename , IN_CLOSE_WRITE|IN_DELETE_SELF|IN_MOVED_FROM ) ;
	if( p_trace_file->inotify_file_wd == -1 )
	{
		ERRORLOG( "inotify_add_watch[%s] failed , errno[%d]" , p_trace_file->path_filename , errno )
		return -1;
	}
	
	LinkTraceFileWdTreeNode( p_env , p_trace_file );
	
	return 0;
}

static int RemoveFileWatcher( struct LogPipeEnv *p_env , struct TraceFile *p_trace_file )
{
	UnlinkTraceFileWdTreeNode( p_env , p_trace_file );
	free( p_trace_file->path_filename );
	free( p_trace_file );
	
	return 0;
}

static int OutputFileAppender( struct LogPipeEnv *p_env , struct TraceFile *p_trace_file )
{
	struct stat	file_stat ;
	int		appender_len ;
	static char	*buffer = NULL ;
	static int	buf_size = 0 ;
	int		buf_len ;
	
	int		nret = 0 ;
	
	memset( & file_stat , 0x00 , sizeof(struct stat) );
	nret = fstat( p_trace_file->fd , & file_stat ) ;
	if( nret == -1 )
	{
		ERRORLOG( "fstat[%s] failed , errno[%d]" , p_trace_file->path_filename , errno )
		return RemoveFileWatcher( p_env , p_trace_file );
	}
	
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
		
		buf_len = read( p_trace_file->fd , buffer , appender_len ) ;
		if( buf_len != appender_len )
		{
			ERRORLOG( "read[%s] failed , errno[%d]" , p_trace_file->path_filename , errno )
			return -1;
		}
		else
		{
			DEBUGHEXLOG( buffer , appender_len , "output file appender , [%d]bytes" , appender_len )
		}
	}
	
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

int worker( struct LogPipeEnv *p_env )
{
	long			read_len ;
	long			parse_len ;
	struct inotify_event	*p_inotify_event = NULL ;
	struct TraceFile	trace_file ;
	struct TraceFile	*p_trace_file = NULL ;
	
	int			nret = 0 ;
	
	nret = ReadFilesToInotifyWdTree( p_env ) ;
	if( nret )
	{
		ERRORLOG( "ReadFilesToInotifyWdTree failed[%d]" , nret )
		return nret;
	}
	
	while(1)
	{
		read_len = read( p_env->role_context.collector.inotify_fd , p_env->role_context.collector.inotify_read_buffer , sizeof(p_env->role_context.collector.inotify_read_buffer)-1 ) ;
		if( read_len == -1 )
		{
			ERRORLOG( "read failed , errno[%d]" , errno )
			return -1;
		}
		
		parse_len = 0 ;
		while( parse_len >= read_len )
		{
			p_inotify_event = (struct inotify_event *)(p_env->role_context.collector.inotify_read_buffer+parse_len) ;
			
			if( p_inotify_event->wd == p_env->role_context.collector.inotify_path_wd )
			{
				if( p_inotify_event->mask & IN_CREATE )
				{
					nret = AddFileWatcher( p_env , p_inotify_event->name ) ;
					if( nret )
					{
						ERRORLOG( "AddFileWatcher[%s] failed , errno[%d]" , p_inotify_event->name , errno )
						return -1;
					}
				}
				else if( p_inotify_event->mask & IN_MOVED_FROM )
				{
					trace_file.inotify_file_wd = p_inotify_event->wd ;
					p_trace_file = QueryTraceFileWdTreeNode( p_env , & trace_file ) ;
					if( p_trace_file == NULL )
					{
						ERRORLOG( "wd[%d] not found" , trace_file.inotify_file_wd )
					}
					else
					{
						nret = RemoveFileWatcher( p_env , p_trace_file ) ;
						if( nret )
						{
							ERRORLOG( "RemoveFileWatcher failed , errno[%d]" , errno )
							return -1;
						}
					}
				}
				else
				{
					ERRORLOG( "unknow dir inotify event mask[%d]" , p_inotify_event->mask )
					return -1;
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
					else if( ( p_inotify_event->mask & IN_DELETE_SELF ) || ( p_inotify_event->mask & IN_MOVED_FROM ) )
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
						ERRORLOG( "unknow file inotify event mask[%d]" , p_inotify_event->mask )
						return -1;
					}
				}
			}
			
			parse_len += sizeof(struct inotify_event) + p_inotify_event->len ;
		}
	}
	
	return 0;
}

