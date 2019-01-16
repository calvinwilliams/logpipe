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
#include <sys/stat.h>
#include <math.h>
#include <inttypes.h>
#include <poll.h>

char *strndup(const char *s, size_t n);
int asprintf(char **strp, const char *fmt, ...);

#include "iconv.h"

#include "list.h"
#include "rbtree_tpl.h"

#include "LOGC.h"
#include "fasterjson.h"

#ifndef MAX
#define MAX(_a_,_b_) ( (_a_)>(_b_)?(_a_):(_b_) )
#endif

#ifndef MIN
#define MIN(_a_,_b_) ( (_a_)<(_b_)?(_a_):(_b_) )
#endif

#ifndef SNPRINTF_OVERFLOW
#define SNPRINTF_OVERFLOW(_ret_,_sizeof_)		( (_ret_) == -1 || (_ret_) >= (_sizeof_) )
#endif

#define VAL_TIMEVAL(_timeval_,_tv_sec_,_tv_usec_) \
	{ \
		(_timeval_).tv_sec = (_tv_sec_) ; \
		(_timeval_).tv_usec = (_tv_usec_) ; \
	} \

#define DIFF_TIMEVAL(_tv_diff_,_tv1_,_tv2_) \
	(_tv_diff_).tv_sec = (_tv2_).tv_sec - (_tv1_).tv_sec ; \
	(_tv_diff_).tv_usec = (_tv2_).tv_usec - (_tv1_).tv_usec ; \
	while( (_tv_diff_).tv_usec < 0 ) \
	{ \
		(_tv_diff_).tv_usec += 1000000 ; \
		(_tv_diff_).tv_sec--; \
	} \

#define INCREASE_TIMEVAL(_timeval1_,_timeval2_) \
	{ \
		(_timeval1_).tv_sec += (_timeval2_).tv_sec ; \
		(_timeval1_).tv_usec += (_timeval2_).tv_usec ; \
		while( (_timeval1_).tv_usec > 1000000 ) \
		{ \
			(_timeval1_).tv_usec -= 1000000 ; \
			(_timeval1_).tv_sec++; \
		} \
	} \

#define DECREASE_TIMEVAL(_timeval1_,_timeval2_) \
	{ \
		(_timeval1_).tv_sec -= (_timeval2_).tv_sec ; \
		(_timeval1_).tv_usec -= (_timeval2_).tv_usec ; \
		while( (_timeval1_).tv_usec < 0 ) \
		{ \
			(_timeval1_).tv_usec += 1000000 ; \
			(_timeval1_).tv_sec--; \
		} \
	} \

ssize_t writen(int fd, const void *vptr, size_t n);
ssize_t writev3( int fd , struct iovec **pp_iov, int *p_iovcnt , int iov_total_len , int *p_timeout , int *p_elapse );
ssize_t readn(int fd, void *vptr, size_t n);
char *ConvertContentEncodingEx( char *encFrom , char *encTo , char *inptr , int *inptrlen , char *outptr , int *outptrlen );
char *ConvertContentEncoding( char *encFrom , char *encTo , char *inptr );

#define LOGPIPE_INPUT_BEFORE_COMPRESSBUFSIZE	99*1024
#define LOGPIPE_INPUT_BUFSIZE			100*1024
#define LOGPIPE_OUTPUT_BUFSIZE			200*1024

#define LOGPIPE_COMM_HEAD_MAGIC			'@'

#define LOGPIPE_READ_END_FROM_INPUT		4

struct LogpipeEnv ;

struct LogpipePluginConfigItem ;

struct LogpipeInputPlugin ;
struct LogpipeFilterPlugin ;
struct LogpipeOutputPlugin ;

/* 插件配置函数 */
char *QueryPluginConfigItem( struct LogpipePluginConfigItem *config , char *key_format , ... );

/* 输入插件回调函数原型 */
typedef int funcLoadInputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , struct LogpipePluginConfigItem *p_plugin_config_items , void **pp_context );
typedef int funcInitInputPluginContext( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context );
typedef int funcOnInputPluginIdle( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context );
typedef int funcOnInputPluginEvent( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context );
typedef int funcBeforeReadInputPlugin( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context , uint64_t *p_file_offset , uint64_t *p_file_line );
typedef int funcReadInputPlugin( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context , uint64_t *p_file_offset , uint64_t *p_file_line , uint64_t *p_block_len , char *block_buf , uint64_t block_buf_size );
typedef int funcAfterReadInputPlugin( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context , uint64_t *p_file_offset , uint64_t *p_file_line );
typedef int funcCleanInputPluginContext( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context );
typedef int funcUnloadInputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void **pp_context );

/* 过滤插件回调函数原型 */
typedef int funcLoadFilterPluginConfig( struct LogpipeEnv *p_env , struct LogpipeFilterPlugin *p_logpipe_filter_plugin , struct LogpipePluginConfigItem *p_plugin_config_items , void **pp_context );
typedef int funcInitFilterPluginContext( struct LogpipeEnv *p_env , struct LogpipeFilterPlugin *p_logpipe_filter_plugin , void *p_context );
typedef int funcBeforeProcessFilterPlugin( struct LogpipeEnv *p_env , struct LogpipeFilterPlugin *p_logpipe_filter_plugin , void *p_context , uint16_t filename_len , char *filename );
typedef int funcProcessFilterPlugin( struct LogpipeEnv *p_env , struct LogpipeFilterPlugin *p_logpipe_filter_plugin , void *p_context , uint64_t file_offset , uint64_t file_line , uint64_t *p_block_len , char *block_buf , uint64_t block_buf_size );
typedef int funcAfterProcessFilterPlugin( struct LogpipeEnv *p_env , struct LogpipeFilterPlugin *p_logpipe_filter_plugin , void *p_context , uint16_t filename_len , char *filename );
typedef int funcCleanFilterPluginContext( struct LogpipeEnv *p_env , struct LogpipeFilterPlugin *p_logpipe_filter_plugin , void *p_context );
typedef int funcUnloadFilterPluginConfig( struct LogpipeEnv *p_env , struct LogpipeFilterPlugin *p_logpipe_filter_plugin , void **pp_context );

/* 输出插件回调函数原型 */
typedef int funcLoadOutputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , struct LogpipePluginConfigItem *p_plugin_config_items , void **pp_context );
typedef int funcInitOutputPluginContext( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context );
typedef int funcOnOutputPluginIdle( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context );
typedef int funcOnOutputPluginEvent( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context );
typedef int funcBeforeWriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint16_t filename_len , char *filename );
typedef int funcWriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint64_t file_offset , uint64_t file_line , uint64_t block_len , char *block_buf , uint64_t block_buf_size );
typedef int funcAfterWriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint16_t filename_len , char *filename );
typedef int funcCleanOutputPluginContext( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context );
typedef int funcUnloadOutputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void **pp_context );

/* 设置描述字到事件总线 */
void AddInputPluginEvent( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , int fd );
void AddOutputPluginEvent( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , int fd );
void DeleteInputPluginEvent( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , int fd );
void DeleteOutputPluginEvent( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , int fd );

/* 创建输入插件子会话 */
struct LogpipeInputPlugin *AddInputPluginSession( struct LogpipeEnv *p_env , char *so_filename
						, funcOnInputPluginEvent *pfuncOnInputPluginEvent
						, funcBeforeReadInputPlugin *pfuncBeforeReadInputPlugin
						, funcReadInputPlugin *pfuncReadInputPlugin
						, funcAfterReadInputPlugin *pfuncAfterReadInputPlugin
						, funcCleanInputPluginContext *pfuncCleanInputPluginContext , funcUnloadInputPluginConfig *pfuncUnloadInputPluginConfig
						, int fd , void *context );
void RemoveInputPluginSession( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin );

/* 激活一轮从输入插件读，写到所有输出插件流程处理 */
int WriteAllOutputPlugins( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , uint16_t filename_len , char *filename );

/* 存取环境结构成员 */
char *GetInputPluginFilenamePtr( struct LogpipeEnv *p_env , uint16_t *p_filename_len );
void SetInputPluginFilename( struct LogpipeEnv *p_env , uint16_t filename_len , char *filename );
uint64_t GetInputPluginFileOffset( struct LogpipeEnv *p_env );
void SetInputPluginFileOffset( struct LogpipeEnv *p_env , uint64_t file_offset );
uint64_t GetInputPluginFileLine( struct LogpipeEnv *p_env );
void SetInputPluginFileLine( struct LogpipeEnv *p_env , uint64_t file_line );
char *GetInputPluginBlockPtr( struct LogpipeEnv *p_env , uint64_t *p_filename_len );
void SetInputPluginBlock( struct LogpipeEnv *p_env , uint64_t block_len , char *block );
char *GetInputPluginTagPtr( struct LogpipeEnv *p_env , int index , uint16_t *p_tag_len );
void SetInputPluginTag( struct LogpipeEnv *p_env , int index , uint16_t tag_len , char *tag );

/* 加密输入插件读取的块数据 */
int CompressInputPluginData( char *compress_algorithm , char *block_in_buf , uint64_t block_in_len , char *block_out_buf , uint64_t *p_block_out_len , uint64_t block_out_buf_size );
/* 解密输出插件读取的块数据 */
int UncompressInputPluginData( char *uncompress_algorithm , char *block_in_buf , uint64_t block_in_len , char *block_out_buf , uint64_t *p_block_out_len , uint64_t block_out_buf_size );

/* 字符串展开 */        
int ExpandStringBuffer( char *base , int buf_size );
/* 大小字符串按单位转换为数字 */
uint64_t size64_atou64( char *str );
/* 微秒字符串按单位转换为数字 */
uint64_t usleep_atou64( char *str );

/* 时间 */
void DiffTimeval( struct timeval *p_tv1 , struct timeval *p_tv2 , struct timeval *p_diff );

/* 拆分行工具库 */
struct SplitLineBuffer ;

#define LOGPIPE_NO_LINE		6

struct SplitLineBuffer *AllocSplitLineCache();
char *GetSplitLineBufferPtr( struct SplitLineBuffer *split_line_buf , uint64_t *p_split_line_buflen );
uint64_t GetSplitLineBufferLength( struct SplitLineBuffer *split_line_buf );
int FetchSplitLineBuffer( struct SplitLineBuffer *split_line_buf , uint64_t *p_block_len , char *block_buf );
void FreeSplitLineBuffer( struct SplitLineBuffer *split_line_buf );

#ifdef __cplusplus
extern }
#endif

#endif
