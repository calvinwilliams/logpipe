#include "logpipe_in.h"

int InitEnvironment( struct LogPipeEnv *p_env )
{
	static uint32_t		inotify_mask = IN_CREATE|IN_MOVED_TO|IN_DELETE_SELF|IN_MOVE_SELF ;
	
	if( p_env->role == LOGPIPE_ROLE_COLLECTOR )
	{
		p_env->role_context.collector.inotify_fd = inotify_init() ;
		if( p_env->role_context.collector.inotify_fd == -1 )
		{
			printf( "*** ERROR : inotify_init failed , errno[%d]\n" , errno );
			return -1;
		}
		
		p_env->role_context.collector.inotify_path_wd = inotify_add_watch( p_env->role_context.collector.inotify_fd , p_env->role_context.collector.monitor_path , inotify_mask ) ;
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
	if( p_env->role == LOGPIPE_ROLE_COLLECTOR )
	{
		close( p_env->role_context.collector.inotify_fd );
	}
	
	return 0;
}

