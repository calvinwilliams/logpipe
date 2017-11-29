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
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>

int asprintf(char **strp, const char *fmt, ...);

#include "list.h"
#include "rbtree.h"
#include "LOGC.h"

struct TraceFile
{
	char				*path_filename ;
	off_t				trace_offset ;
	
	int				inotify_file_wd ;
	struct rb_node			inotify_file_wd_rbnode ;
} ;

#define LOGPIPE_ROLE_COLLECTOR		'C'
#define LOGPIPE_ROLE_PIPER		'P'
#define LOGPIPE_ROLE_DUMPSERVER		'S'

#define LOGPIPE_INOTIFY_READ_BUFSIZE	16*1024*1024

#define COMMBUFFER_INIT_SIZE		40960
#define COMMBUFFER_INCREASE_SIZE	40960

/* 客户端连接会话结构 */
struct AcceptedSession
{
	struct sockaddr_in      client_addr ;
	int			client_sock ;
	
	char			*comm_buf ; /* 通讯接收缓冲区 */
	int			comm_buf_size ; /* 缓冲区总大小 */
	int			comm_data_len ; /* 缓冲区已接收到数据长度 */
	int			comm_body_len ; /* 通讯头的值，也即通讯体的长度 */
	
	struct list_head	this_node ; /* 客户端已连接会话链表节点 */
} ;

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
		struct LogDumpServer
		{
			char			listen_ip[ 30 + 1 ] ;
			int			listen_port ;
			struct sockaddr_in      listen_addr ;
			int			listen_sock ;
			int			epoll_fd ;
			struct AcceptedSession	accepted_session_list ; /* 客户端已连接会话链表 */
		} dumpserver ;
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

int worker_collector( struct LogPipeEnv *p_env );
int worker_dumpserver( struct LogPipeEnv *p_env );

#ifdef __cplusplus
}
#endif

#endif

