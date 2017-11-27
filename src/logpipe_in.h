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
#include <dirent.h>

int asprintf(char **strp, const char *fmt, ...);

#include "rbtree.h"
#include "LOGC.h"

struct TraceFile
{
	char				*path_filename ;
	int				fd ;
	off_t				trace_offset ;
	
	int				inotify_file_wd ;
	struct rb_node			inotify_file_wd_rbnode ;
} ;

#define LOGPIPE_ROLE_COLLECTOR		'C'
#define LOGPIPE_ROLE_PIPER		'P'
#define LOGPIPE_ROLE_DUMPSERVER		'S'

#define LOGPIPE_INOTIFY_READ_BUFSIZE	16*1024*1024

struct LogPipeEnv
{
	char					role ;
	union
	{
		struct LogCollector
		{
			char			monitor_path[ PATH_MAX + 1 ] ;
			int			inotify_fd ;
			int			inotify_path_wd ;
			struct rb_root		inotify_wd_rbtree ;
			char			inotify_read_buffer[ LOGPIPE_INOTIFY_READ_BUFSIZE + 1 ] ;
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

