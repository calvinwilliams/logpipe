#ifndef _H_LOGPIPE_IN_
#define _H_LOGPIPE_IN_

#ifdef __cplusplus
extern "C" {
#endif

#include "logpipe_api.h"

#include "list.h"
#include "rbtree.h"

#include "fasterjson.h"

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
	funcInitLogpipeInputPlugin2	*pfuncInitLogpipeInputPlugin2 ;
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
	funcInitLogpipeOutputPlugin2	*pfuncInitLogpipeOutputPlugin2 ;
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
	
	int				epoll_fd ;
	
	struct LogpipeInputPlugin	logpipe_input_plugins_list ;
	struct LogpipeOutputPlugin	logpipe_output_plugins_list ;
	struct LogpipeInputPlugin	*p_block_input_plugin ;
	
	int				quit_pipe[2] ;
} ;

int WriteEntireFile( char *pathfilename , char *file_content , int file_len );
char *StrdupEntireFile( char *pathfilename , int *p_file_len );
int BindDaemonServer( int (* ServerMain)( void *pv ) , void *pv , int close_flag );

void InitConfig();
int LoadConfig( struct LogpipeEnv *p_env );

int InitEnvironment( struct LogpipeEnv *p_env );
int InitEnvironment2( struct LogpipeEnv *p_env );
void CleanEnvironment( struct LogpipeEnv *p_env );

int monitor( struct LogpipeEnv *p_env );
int _monitor( void *pv );

int worker( struct LogpipeEnv *p_env );

void RemoveLogpipeOutputSession( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin );

#ifdef __cplusplus
}
#endif

#endif

