#ifndef _H_LOGPIPE_API_
#define _H_LOGPIPE_API_

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
#include <sys/ioctl.h>
#include <dlfcn.h>
#include <time.h>

char *strndup(const char *s, size_t n);
int asprintf(char **strp, const char *fmt, ...);

#include "rbtree_tpl.h"

#include "LOGC.h"

ssize_t writen(int fd, const void *vptr, size_t n);
ssize_t readn(int fd, void *vptr, size_t n);

#define LOGPIPE_BLOCK_BUFSIZE			100*1024
#define LOGPIPE_UNCOMPRESS_BLOCK_BUFSIZE	100*1023

#define LOGPIPE_COMM_HEAD_MAGIC			'@'

struct LogpipeEnv ;
struct LogpipePluginConfigItem ;
struct LogpipeInputPlugin ;
struct LogpipeOutputPlugin ;

/* 插件配置函数 */
int AddPluginConfigItem( struct LogpipePluginConfigItem *config , char *key , int key_len , char *value , int value_len );
char *QueryPluginConfigItem( struct LogpipePluginConfigItem *config , char *key );
void RemoveAllPluginConfigItem( struct LogpipePluginConfigItem *config );

/* 输入插件回调函数原型 */
typedef int funcInitLogpipeInputPlugin( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , struct LogpipePluginConfigItem *p_plugin_config_items , void **pp_context , int *p_fd );
typedef int funcOnLogpipeInputEvent( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context );
typedef int funcBeforeReadLogpipeInput( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context );
typedef int funcReadLogpipeInput( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context , uint32_t *p_block_len , char *block_buf , int block_bufsize );
typedef int funcAfterReadLogpipeInput( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context );
typedef int funcCleanLogpipeInputPlugin( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context );

/* 输出插件回调函数原型 */
typedef int funcInitLogpipeOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , struct LogpipePluginConfigItem *p_plugin_config_items , void **pp_context );
typedef int funcBeforeWriteLogpipeOutput( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint16_t filename_len , char *filename );
typedef int funcWriteLogpipeOutput( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint32_t block_len , char *block_buf );
typedef int funcAfterWriteLogpipeOutput( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context );
typedef int funcCleanLogpipeOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context );

/* 快速创建输入插件环境结构 */
struct LogpipeInputPlugin *AddLogpipeInputPlugin( struct LogpipeEnv *p_env
						, char *so_filename
						, funcOnLogpipeInputEvent *pfuncOnLogpipeInputEvent
						, funcBeforeReadLogpipeInput *pfuncBeforeReadLogpipeInput , funcReadLogpipeInput *pfuncReadLogpipeInput , funcAfterReadLogpipeInput *pfuncAfterReadLogpipeInput
						, funcCleanLogpipeInputPlugin *pfuncCleanLogpipeInputPlugin
						, int fd , void *context );

/* 导出所有输出端 */
int WriteAllOutputPlugins( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , uint16_t filename_len , char *filename );

#ifdef __cplusplus
extern }
#endif

#endif
