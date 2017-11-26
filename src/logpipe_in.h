#ifndef _H_LOGPIPE_
#define _H_LOGPIPE_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <limits.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/inotify.h>

#include "rbtree.h"
#include "LOGC.h"

struct TraceFile
{
	char				filename[ PATH_MAX + 1 ] ;
	int				fd ;
	off_t				trace_offset ;
	
	int				inotify_file_wd ;
	struct rb_node			inotify_file_wd_rbnode ;
} ;

#define LOGPIPE_ROLE_COLLECTOR		'C'
#define LOGPIPE_ROLE_PIPER		'P'
#define LOGPIPE_ROLE_DUMPSERVER		'S'

#define LOGPIPE_FILE_TRACE_SPACE_DEFAULT_SIZE		1000
#define LOGPIPE_FILE_TRACE_SPACE_EXPANSION_FACTOR	4

struct LogPipeEnv
{
	char					role ;
	union
	{
		struct LogCollector
		{
			char			monitor_path[ PATH_MAX + 1 ] ;
			int			trace_file_space_size ;
			int			trace_file_space_total_size ;
			struct TraceFile	*trace_file_space ;
			int			inotify_fd ;
			int			inotify_path_wd ;
			struct rb_root		inotify_wd_rbtree ;
		} collector ;
	} role_context ;
	char					log_pathfilename[ PATH_MAX + 1 ] ;
	int					log_level ;
	int					no_daemon ;
	
	
} ;

int LinkTraceFileWdTreeNode( struct LogPipeEnv *p_env , struct TraceFile *p_trace_file );
struct TraceFile *QueryTraceFileWdTreeNode( struct LogPipeEnv *p_env , struct TraceFile *p_trace_file );
void UnlinkTraceFileWdTreeNode( struct LogPipeEnv *p_env , struct TraceFile *p_trace_file );
void DestroyTraceFileTree( struct LogPipeEnv *p_env );

int BindDaemonServer( int (* ServerMain)( void *pv ) , void *pv , int close_flag );

int InitEnvironment( struct LogPipeEnv *p_env );
int CleanEnvironment( struct LogPipeEnv *p_env );

int monitor( struct LogPipeEnv *p_env );
int _monitor( void *pv );

int worker( struct LogPipeEnv *p_env );

#ifdef __cplusplus
}
#endif

#endif

