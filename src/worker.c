#include "logpipe_in.h"

static int LoadAllTraceFileWd( struct LogPipeEnv *p_env )
{
	int			i ;
	struct TraceFile	*p_trace_file = NULL ;
	
	for( i = 0 , p_trace_file = p_env->role_context.collector.trace_file_space ; i < p_env->role_context.collector.trace_file_space_total_size ; i++ , p_trace_file++ )
	{
		if( p_trace_file->filename[0] )
		{
			LinkTraceFileWdTreeNode( p_env , p_trace_file );
		}
	}
	
	return 0;
}

int worker( struct LogPipeEnv *p_env )
{
	int		nret = 0 ;
	
	nret = LoadAllTraceFileWd( p_env ) ;
	if( nret )
	{
		ERRORLOG( "LoadAllTraceFileWd failed[%d]" , nret )
		return nret;
	}
	
	
	
	
	
	
	
	
	return 0;
}

