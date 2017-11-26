#include "logpipe_in.h"

int InitEnvironment( struct LogPipeEnv *p_env )
{
	if( p_env->role == LOGPIPE_ROLE_COLLECTOR )
	{
		p_env->role_context.collector.trace_file_space = (struct TraceFile *)mmap( NULL , sizeof(struct TraceFile) * p_env->role_context.collector.trace_file_space_total_size , PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS , -1 , 0 ) ;
		if( p_env->role_context.collector.trace_file_space == NULL )
		{
			printf( "*** ERROR : mmap failed , errno[%d]\n" , errno );
			return -1;
		}
		memset( p_env->role_context.collector.trace_file_space , 0x00 , sizeof(struct TraceFile) * p_env->role_context.collector.trace_file_space_total_size );
		
		p_env->role_context.collector.inotify_fd = inotify_init() ;
		if( p_env->role_context.collector.inotify_fd == -1 )
		{
			printf( "*** ERROR : inotify_init failed , errno[%d]\n" , errno );
			return -1;
		}
		
		p_env->role_context.collector.inotify_path_wd = inotify_add_watch( p_env->role_context.collector.inotify_fd , p_env->role_context.collector.monitor_path , IN_CREATE|IN_DELETE|IN_MOVED_FROM|IN_MOVED_TO ) ;
		if( p_env->role_context.collector.inotify_path_wd == -1 )
		{
			printf( "*** ERROR : inotify_add_watch[%s] failed , errno[%d]\n" , p_env->role_context.collector.monitor_path , errno );
			return -1;
		}
	}
	
	
	
	return 0;
}

int CleanEnvironment( struct LogPipeEnv *p_env )
{
	munmap( p_env->role_context.collector.trace_file_space , sizeof(struct TraceFile) * p_env->role_context.collector.trace_file_space_total_size );
	
	return 0;
}

