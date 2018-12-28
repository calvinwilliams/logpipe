/*
 * logpipe - Distribute log collector
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#ifndef _H_LOGPIPE_IN_
#define _H_LOGPIPE_IN_

#ifdef __cplusplus
extern "C" {
#endif

#include "logpipe_api.h"

#include "list.h"
#include "rbtree.h"

#include "LOGC.h"

extern char *__LOGPIPE_VERSION ;

/* 插件配置结构 */
struct LogpipePluginConfigItem
{
	char			*key ;
	char			*value ;
	
	struct list_head	this_node ;
} ;

/* 插件类型 */
#define LOGPIPE_PLUGIN_TYPE_INPUT	'I'
#define LOGPIPE_PLUGIN_TYPE_FILTER	'F'
#define LOGPIPE_PLUGIN_TYPE_OUTPUT	'O'

/* 插件环境结构，用于先查询插件类型时使用 */
struct LogpipePlugin
{
	unsigned char			type ; /* 插件类型 */
} ;

/* 输入插件环境结构 */
struct LogpipeInputPlugin
{
	unsigned char			type ; /* 插件类型 */
	
	struct LogpipePluginConfigItem	plugin_config_items ; /* 自定义配置参数 */
	
	char				so_filename[ PATH_MAX + 1 ] ; /* 插件文件名 */
	char				so_path_filename[ PATH_MAX + 1 ] ; /* 插件路径文件名 */
	void				*so_handler ; /* 插件打开句柄 */
	funcLoadInputPluginConfig	*pfuncLoadInputPluginConfig ; /* 解析配置文件时装载插件 */
	funcInitInputPluginContext	*pfuncInitInputPluginContext ; /* 工作主循环前初始化插件 */
	funcOnInputPluginIdle		*pfuncOnInputPluginIdle ; /* 空闲时的调用 */
	funcOnInputPluginEvent		*pfuncOnInputPluginEvent ; /* 发生输入事件时的调用 */
	funcBeforeReadInputPlugin	*pfuncBeforeReadInputPlugin ; /* 读取一个数据块前 */
	funcReadInputPlugin		*pfuncReadInputPlugin ; /* 读取一个数据块 */
	funcAfterReadInputPlugin	*pfuncAfterReadInputPlugin ; /* 读取一个数据块后 */
	funcCleanInputPluginContext	*pfuncCleanInputPluginContext ; /* 工作主循环后清理插件 */
	funcUnloadInputPluginConfig	*pfuncUnloadInputPluginConfig ; /* 退出前卸载插件 */
	int				fd ;
	void				*context ; /* 插件实例上下文 */
	
	struct list_head		this_node ;
} ;

/* 过滤插件环境结构 */
struct LogpipeFilterPlugin
{
	unsigned char			type ; /* 插件类型 */
	
	struct LogpipePluginConfigItem	plugin_config_items ; /* 自定义配置参数 */
	
	char				so_filename[ PATH_MAX + 1 ] ; /* 插件文件名 */
	char				so_path_filename[ PATH_MAX + 1 ] ; /* 插件路径文件名 */
	void				*so_handler ; /* 插件打开句柄 */
	funcLoadFilterPluginConfig	*pfuncLoadFilterPluginConfig ; /* 解析配置文件时装载插件 */
	funcInitFilterPluginContext	*pfuncInitFilterPluginContext ; /* 工作主循环前初始化插件 */
	funcBeforeProcessFilterPlugin	*pfuncBeforeProcessFilterPlugin ; /* 在过滤一个数据块前 */
	funcProcessFilterPlugin		*pfuncProcessFilterPlugin ; /* 过滤一个数据块 */
	funcAfterProcessFilterPlugin	*pfuncAfterProcessFilterPlugin ; /* 在过滤一个数据块后 */
	funcCleanFilterPluginContext	*pfuncCleanFilterPluginContext ; /* 工作主循环后清理插件 */
	funcUnloadFilterPluginConfig	*pfuncUnloadFilterPluginConfig ; /* 退出前卸载插件 */
	void				*context ; /* 插件实例上下文 */
	
	struct list_head		this_node ;
} ;

/* 输出插件环境结构 */
struct LogpipeOutputPlugin
{
	unsigned char			type ; /* 插件类型 */
	
	struct LogpipePluginConfigItem	plugin_config_items ; /* 自定义配置参数 */
	
	char				so_filename[ PATH_MAX + 1 ] ; /* 插件文件名 */
	char				so_path_filename[ PATH_MAX + 1 ] ; /* 插件路径文件名 */
	void				*so_handler ; /* 插件打开句柄 */
	funcLoadOutputPluginConfig	*pfuncLoadOutputPluginConfig ; /* 解析配置文件时装载插件 */
	funcInitOutputPluginContext	*pfuncInitOutputPluginContext ; /* 工作主循环前初始化插件 */
	funcOnOutputPluginIdle		*pfuncOnOutputPluginIdle ; /* 空闲时的调用 */
	funcOnOutputPluginEvent		*pfuncOnOutputPluginEvent ; /* 输出句柄发生事件时调用 */
	funcBeforeWriteOutputPlugin	*pfuncBeforeWriteOutputPlugin ; /* 在写出一个数据块前 */
	funcWriteOutputPlugin		*pfuncWriteOutputPlugin ; /* 写出一个数据块 */
	funcAfterWriteOutputPlugin	*pfuncAfterWriteOutputPlugin ; /* 在写出一个数据块后 */
	funcCleanOutputPluginContext	*pfuncCleanOutputPluginContext ; /* 工作主循环后清理插件 */
	funcUnloadOutputPluginConfig	*pfuncUnloadOutputPluginConfig ; /* 退出前卸载插件 */
	int				fd ;
	void				*context ; /* 插件实例上下文 */
	
	struct list_head		this_node ;
} ;

/* 环境结构 */
struct LogpipeEnv
{
	char				config_path_filename[ PATH_MAX + 1 ] ; /* 配置文件名 */
	int				no_daemon ; /* 是否转化为守护进程运行 */
	
	char				log_file[ PATH_MAX + 1 ] ; /* logpipe日志文件名 */
	int				log_level ; /* logpipe日志等级 */
	struct LogpipePluginConfigItem	start_once_for_plugin_config_items ; /* 启动时只起作用一次的配置，如启动时发送所有存量日志 */
	
	char				hostname[ HOST_NAME_MAX + 1 ] ;
	struct passwd			*pwd ;
	
	int				epoll_fd ; /* epoll事件总线 */
	
	struct LogpipeInputPlugin	logpipe_input_plugins_list ; /* 输入插件链表 */
	struct LogpipeFilterPlugin	logpipe_filter_plugins_list ; /* 过滤插件链表 */
	struct LogpipeOutputPlugin	logpipe_output_plugins_list ; /* 输出插件链表 */
	struct LogpipeInputPlugin	*p_block_input_plugin ; /* 禁用epoll事件总线而改用某一输入插件的堵塞模式，指向该输入插件链表节点 */
	unsigned char			idle_processing_flag ;
	
	int				quit_pipe[2] ; /* 父子进程命令管道 */
} ;

/* 公共函数 */
int WriteEntireFile( char *pathfilename , char *file_content , int file_len );
char *StrdupEntireFile( char *pathfilename , int *p_file_len );
int BindDaemonServer( int (* ServerMain)( void *pv ) , void *pv , int close_flag );

/* 插件配置函数 */
int AddPluginConfigItem( struct LogpipePluginConfigItem *config , char *key , int key_len , char *value , int value_len );
int DuplicatePluginConfigItems( struct LogpipePluginConfigItem *dst , struct LogpipePluginConfigItem *src );
void RemovePluginConfigItemsFrom( struct LogpipePluginConfigItem *config , struct LogpipePluginConfigItem *from );
void RemoveAllPluginConfigItems( struct LogpipePluginConfigItem *config );

/* 配置 */
int LoadConfig( struct LogpipeEnv *p_env );
void UnloadConfig( struct LogpipeEnv *p_env );
void UnloadInputPluginSession( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin );
void UnloadFilterPluginSession( struct LogpipeEnv *p_env , struct LogpipeFilterPlugin *p_logpipe_filter_plugin );
void UnloadOutputPluginSession( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin );

/* 环境 */
int InitEnvironment( struct LogpipeEnv *p_env );
void CleanEnvironment( struct LogpipeEnv *p_env );

/* 内部状态输出管道 */
int CreateLogpipeFifo( struct LogpipeEnv *p_env );
int ProcessLogpipeFifoEvents( struct LogpipeEnv *p_env );

/* 管理进程 */
int monitor( struct LogpipeEnv *p_env );
int _monitor( void *pv );

/* 工作进程 */
int worker( struct LogpipeEnv *p_env );

/* 空闲事件函数 */
int ProcessOnIdle( struct LogpipeEnv *p_env );

/* 拆分行工具库 */
struct SplitLineBuffer
{
	char			split_line_buffer[ LOGPIPE_BLOCK_BUFSIZE + 1 ] ; /* 拆分缓冲区 */
	uint64_t		split_line_buflen ; /* 拆分缓冲区数据长度 */
} ;

#ifdef __cplusplus
}
#endif

#endif

