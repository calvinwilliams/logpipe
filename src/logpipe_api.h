/*
 * logpipe - Distribute log collector
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

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
#include "fasterjson.h"

ssize_t writen(int fd, const void *vptr, size_t n);
ssize_t readn(int fd, void *vptr, size_t n);

#define LOGPIPE_BLOCK_BUFSIZE			102400
#define LOGPIPE_UNCOMPRESS_BLOCK_BUFSIZE	102357

#define LOGPIPE_COMM_HEAD_MAGIC			'@'

#define LOGPIPE_READ_END_OF_INPUT		4

struct LogpipeEnv ;
struct LogpipePluginConfigItem ;
struct LogpipeInputPlugin ;
struct LogpipeOutputPlugin ;

/* 插件配置函数 */
char *QueryPluginConfigItem( struct LogpipePluginConfigItem *config , char *key );

/* 输入插件回调函数原型 */
typedef int funcLoadInputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , struct LogpipePluginConfigItem *p_plugin_config_items , void **pp_context , int *p_fd );
typedef int funcInitInputPluginContext( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context , int *p_fd );
typedef int funcOnInputPluginEvent( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context );
typedef int funcReadInputPlugin( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context , uint32_t *p_block_len , char *block_buf , int block_bufsize );
typedef int funcCleanInputPluginContext( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context );
typedef int funcUnloadInputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void **pp_context );

/* 输出插件回调函数原型 */
typedef int funcLoadOutputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , struct LogpipePluginConfigItem *p_plugin_config_items , void **pp_context );
typedef int funcInitOutputPluginContext( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context );
typedef int funcBeforeWriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint16_t filename_len , char *filename );
typedef int funcWriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint32_t block_len , char *block_buf );
typedef int funcAfterWriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context );
typedef int funcCleanOutputPluginContext( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context );
typedef int funcUnloadOutputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void **pp_context );

/* 快速创建输入插件环境结构 */
struct LogpipeInputPlugin *AddInputPluginSession( struct LogpipeEnv *p_env , char *so_filename
						, funcOnInputPluginEvent *pfuncOnInputPluginEvent
						, funcReadInputPlugin *pfuncReadInputPlugin
						, funcCleanInputPluginContext *pfuncCleanInputPluginContext , funcUnloadInputPluginConfig *pfuncUnloadInputPluginConfig
						, int fd , void *context );
void RemoveInputPluginSession( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin );

/* 导出所有输出端 */
int WriteAllOutputPlugins( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , uint16_t filename_len , char *filename );

#ifdef __cplusplus
extern }
#endif

#endif
