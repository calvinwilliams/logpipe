#ifndef _H_LOGPIPE_IN_
#define _H_LOGPIPE_IN_

#ifdef __cplusplus
extern "C" {
#endif

#include "logpipe_api.h"

#include "zlib.h"

int asprintf(char **strp, const char *fmt, ...);

#include "list.h"
#include "rbtree.h"
#include "LOGC.h"


#define LOGPIPE_IO_TYPE_FILE		"file://"
#define LOGPIPE_IO_TYPE_TCP		"tcp://"

#define LOGPIPE_SESSION_TYPE_INOTIFY	'M'
#define LOGPIPE_SESSION_TYPE_LISTEN	'L'
#define LOGPIPE_SESSION_TYPE_ACCEPTED	'A'
#define LOGPIPE_SESSION_TYPE_DUMP	'D'
#define LOGPIPE_SESSION_TYPE_FORWARD	'F'

/* communication protocol :
	|'@'(1byte)|filename_len(2bytes)|file_name|file_block_len(2bytes)|file_block_data|...(other file blocks)...|\0\0\0\0|
*/

#define LOGPIPE_COMM_HEAD_LENGTH		6
#define LOGPIPE_COMM_HEAD_MAGIC_OFFSET		0
#define LOGPIPE_COMM_HEAD_MAGIC			'@'
#define LOGPIPE_COMM_HEAD_COMPRESS_OFFSET	1
#define LOGPIPE_COMM_HEAD_COMPRESS_ALGORITHM_S	"deflate"
#define LOGPIPE_COMM_HEAD_COMPRESS_ALGORITHM	'Z'

#define LOGPIPE_COMM_FILE_BLOCK_BUFSIZE		100*1024

/* 会话结构头 */
struct Session
{
	unsigned char		session_type ;
} ;

/* 目录监控端会话结构 */
struct InotifySession
{
	unsigned char		session_type ;
	
	char			inotify_path[ PATH_MAX + 1 ] ;
	int			inotify_fd ;
	int			inotify_path_wd ;
	struct rb_root		inotify_wd_rbtree ;
	
	struct list_head	this_node ;
} ;

/* 客户端会话结构 */
struct AcceptedSession
{
	unsigned char		session_type ;
	
	struct ListenSession	*p_listen_session ;
	
	struct sockaddr_in      accepted_addr ;
	int			accepted_sock ;
	
	struct list_head	this_node ;
} ;

/* 侦听端会话结构 */
struct ListenSession
{
	unsigned char		session_type ;
	
	char			listen_ip[ 20 + 1 ] ;
	int			listen_port ;
	
	struct sockaddr_in    	listen_addr ;
	int			listen_sock ;
	
	struct AcceptedSession	accepted_session_list ;
	
	struct list_head	this_node ;
} ;

/* 归集落地端会话结构 */
struct DumpSession
{
	unsigned char		session_type ;
	
	char			dump_path[ PATH_MAX + 1 ] ;
	
	int			tmp_fd ;
	
	struct list_head	this_node ;
} ;

/* 转发端会话结构 */
struct ForwardSession
{
	unsigned char		session_type ;
	
	char			forward_ip[ 20 + 1 ] ;
	int			forward_port ;
	
	struct sockaddr_in    	forward_addr ;
	int			forward_sock ;
	
	struct list_head	this_node ;
} ;







/* 插件配置结构 */
struct LogpipePluginConfigItem
{
	char			*key ;
	char			*value ;
	
	struct list_head	this_node ;
} ;

/* 输入插件环境结构 */
struct LogpipeInputPlugin
{
	struct LogpipePluginConfigItem	plugin_config_items ;
	
	char				so_filename[ PATH_MAX + 1 ] ;
	char				so_path_filename[ PATH_MAX + 1 ] ;
	void				*so_handler ;
	funcInitLogpipeInputPlugin	*pfuncInitLogpipeInputPlugin ;
	funcOnLogpipeInputEvent		*pfuncOnLogpipeInputEvent ;
	funcBeforeReadLogpipeInput	*pfuncBeforeReadLogpipeInput ;
	funcReadLogpipeInput		*pfuncReadLogpipeInput ;
	funcAfterReadLogpipeInput	*pfuncAfterReadLogpipeInput ;
	funcCleanLogpipeInputPlugin	*pfuncCleanLogpipeInputPlugin ;
	int				fd ;
	void				*context ;
	
	struct list_head		this_node ;
} ;

/* 输出插件环境结构 */
struct LogpipeOutputPlugin
{
	struct LogpipePluginConfigItem	plugin_config_items ;
	
	char				so_filename[ PATH_MAX + 1 ] ;
	char				so_path_filename[ PATH_MAX + 1 ] ;
	void				*so_handler ;
	funcInitLogpipeOutputPlugin	*pfuncInitLogpipeOutputPlugin ;
	funcBeforeWriteLogpipeOutput	*pfuncBeforeWriteLogpipeOutput ;
	funcWriteLogpipeOutput		*pfuncWriteLogpipeOutput ;
	funcAfterWriteLogpipeOutput	*pfuncAfterWriteLogpipeOutput ;
	funcCleanLogpipeOutputPlugin	*pfuncCleanLogpipeOutputPlugin ;
	void				*context ;
	
	struct list_head		this_node ;
} ;

/* 环境结构 */
struct LogpipeEnv
{
	char				config_path_filename[ PATH_MAX + 1 ] ;
	int				no_daemon ;
	
	char				log_file[ PATH_MAX + 1 ] ;
	int				log_level ;
	
	struct LogpipeInputPlugin	logpipe_inputs_plugin_list ;
	struct LogpipeOutputPlugin	logpipe_outputs_plugin_list ;
	
	int				epoll_fd ;
	
	int				is_monitor ;
	
	struct LogpipeInputPlugin	*p_block_input_plugin ;
	
	
	
	
	
	
	
	
#if 0
	logpipe_conf		conf ;
	int			log_level ;
	char			compress_algorithm ;
	
	struct InotifySession	inotify_session_list ;
	struct ListenSession	listen_session_list ;
	
	struct DumpSession	dump_session_list ;
	struct ForwardSession	forward_session_list ;
#endif
} ;

int LinkTraceFileWdTreeNode( struct InotifySession *p_inotify_session , struct TraceFile *p_trace_file );
struct TraceFile *QueryTraceFileWdTreeNode( struct InotifySession *p_inotify_session , struct TraceFile *p_trace_file );
void UnlinkTraceFileWdTreeNode( struct InotifySession *p_inotify_session , struct TraceFile *p_trace_file );
void DestroyTraceFileTree( struct InotifySession *p_inotify_session );

int WriteEntireFile( char *pathfilename , char *file_content , int file_len );
char *StrdupEntireFile( char *pathfilename , int *p_file_len );
int BindDaemonServer( int (* ServerMain)( void *pv ) , void *pv , int close_flag );
ssize_t writen(int fd, const void *vptr, size_t n);
ssize_t readn(int fd, void *vptr, size_t n);

void InitConfig();
int LoadConfig( struct LogpipeEnv *p_env );

int InitEnvironment( struct LogpipeEnv *p_env );
void CleanEnvironment( struct LogpipeEnv *p_env );
void LogEnvironment( struct LogpipeEnv *p_env );

int monitor( struct LogpipeEnv *p_env );
int _monitor( void *pv );

int worker( struct LogpipeEnv *p_env );

int AddFileWatcher( struct LogpipeEnv *p_env , struct InotifySession *p_inotify_session , char *filename );
int RemoveFileWatcher( struct LogpipeEnv *p_env , struct InotifySession *p_inotify_session , struct TraceFile *p_trace_file );
int RoratingFile( char *pathname , char *filename , int filename_len );
int OnReadingFile( struct LogpipeEnv *p_env , struct InotifySession *p_inotify_session , struct TraceFile *p_trace_file );
int OnInotifyHandler( struct LogpipeEnv *p_env , struct InotifySession *p_inotify_session );

int OnAcceptingSocket( struct LogpipeEnv *p_env , struct ListenSession *p_listen_session );
void OnClosingSocket( struct LogpipeEnv *p_env , struct AcceptedSession *p_accepted_session );
int OnReceivingSocket( struct LogpipeEnv *p_env , struct AcceptedSession *p_accepted_session );

int ConnectForwardSocket( struct LogpipeEnv *p_env , struct ForwardSession *p_forward_session );
int ToOutputs( struct LogpipeEnv *p_env , char *comm_buf , int comm_buf_len , char *filename , uint16_t filename_len , int in , int append_len , char compress_algorithm );

#ifdef __cplusplus
}
#endif

#endif

