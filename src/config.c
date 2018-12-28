/*
 * logpipe - Distribute log collector
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "logpipe_in.h"

#define LOGPIPE_CONFIG_LOG_LOGFILE	"/log/log_file"
#define LOGPIPE_CONFIG_LOG_LOGLEVEL	"/log/log_level"
#define LOGPIPE_CONFIG_INPUTS		"/inputs"
#define LOGPIPE_CONFIG_INPUTS_		"/inputs/"
#define LOGPIPE_CONFIG_FILTERS		"/filters"
#define LOGPIPE_CONFIG_FILTERS_		"/filters/"
#define LOGPIPE_CONFIG_OUTPUTS		"/outputs"
#define LOGPIPE_CONFIG_OUTPUTS_		"/outputs/"

/* 日志等级 字符串 转成 整型宏 */
static int ConvLogLevelStr( char *log_level_str , int log_level_str_len )
{
	if( log_level_str_len == 5 && STRNCMP( log_level_str , == , "DEBUG" , log_level_str_len ) )
		return LOGCLEVEL_DEBUG;
	else if( log_level_str_len == 4 && STRNCMP( log_level_str , == , "INFO" , log_level_str_len ) )
		return LOGCLEVEL_INFO ;
	else if( log_level_str_len == 6 && STRNCMP( log_level_str , == , "NOTICE" , log_level_str_len ) )
		return LOGCLEVEL_NOTICE ;
	else if( log_level_str_len == 4 && STRNCMP( log_level_str , == , "WARN" , log_level_str_len ) )
		return LOGCLEVEL_WARN ;
	else if( log_level_str_len == 5 && STRNCMP( log_level_str , == , "ERROR" , log_level_str_len ) )
		return LOGCLEVEL_ERROR ;
	else if( log_level_str_len == 5 && STRNCMP( log_level_str , == , "FATAL" , log_level_str_len ) )
		return LOGCLEVEL_FATAL ;
	else
		return -1;
}

/* 装载插入插件 */
static int LoadLogpipeInputPlugin( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin )
{
	char		*p = NULL ;
	
	/* 查询输入插件文件名 */
	p = QueryPluginConfigItem( & (p_logpipe_input_plugin->plugin_config_items) , "plugin" ) ;
	if( p == NULL )
	{
		ERRORLOGC( "expect 'plugin' in 'inputs'" )
		return -1;
	}
	
	strncpy( p_logpipe_input_plugin->so_filename , p , sizeof(p_logpipe_input_plugin->so_filename)-1 );
	if( p_logpipe_input_plugin->so_filename[0] == '/' )
	{
		strcpy( p_logpipe_input_plugin->so_path_filename , p_logpipe_input_plugin->so_filename );
	}
	else
	{
		snprintf( p_logpipe_input_plugin->so_path_filename , sizeof(p_logpipe_input_plugin->so_path_filename)-1 , "%s/%s" , getenv("HOME") , p_logpipe_input_plugin->so_filename );
	}
	
	/* 打开输入插件 */
	p_logpipe_input_plugin->so_handler = dlopen( p_logpipe_input_plugin->so_path_filename , RTLD_LAZY ) ;
	if( p_logpipe_input_plugin->so_handler == NULL )
	{
		ERRORLOGC( "dlopen[%s] failed , dlerror[%s]" , p_logpipe_input_plugin->so_path_filename , dlerror() )
		return -1;
	}
	
	/* 查询所有回调函数指针 */
	p_logpipe_input_plugin->pfuncLoadInputPluginConfig = (funcLoadInputPluginConfig *)dlsym( p_logpipe_input_plugin->so_handler , "LoadInputPluginConfig" ) ;
	if( p_logpipe_input_plugin->pfuncLoadInputPluginConfig == NULL )
	{
		ERRORLOGC( "dlsym[%s][LoadInputPluginConfig] failed , dlerror[%s]" , p_logpipe_input_plugin->so_path_filename , dlerror() )
		return -1;
	}
	
	p_logpipe_input_plugin->pfuncInitInputPluginContext = (funcInitInputPluginContext *)dlsym( p_logpipe_input_plugin->so_handler , "InitInputPluginContext" ) ;
	if( p_logpipe_input_plugin->pfuncInitInputPluginContext == NULL )
	{
		ERRORLOGC( "dlsym[%s][LoadInputPluginConfig] failed , dlerror[%s]" , p_logpipe_input_plugin->so_path_filename , dlerror() )
		return -1;
	}
	
	p_logpipe_input_plugin->pfuncOnInputPluginIdle = (funcOnInputPluginEvent *)dlsym( p_logpipe_input_plugin->so_handler , "OnInputPluginIdle" ) ;
	if( p_logpipe_input_plugin->pfuncOnInputPluginIdle )
		p_env->idle_processing_flag = 1 ;
	
	p_logpipe_input_plugin->pfuncOnInputPluginEvent = (funcOnInputPluginEvent *)dlsym( p_logpipe_input_plugin->so_handler , "OnInputPluginEvent" ) ;
	if( p_logpipe_input_plugin->pfuncOnInputPluginEvent == NULL )
	{
		ERRORLOGC( "dlsym[%s][OnInputPluginEvent] failed , dlerror[%s]" , p_logpipe_input_plugin->so_path_filename , dlerror() )
		return -1;
	}
	
	p_logpipe_input_plugin->pfuncBeforeReadInputPlugin = (funcBeforeReadInputPlugin *)dlsym( p_logpipe_input_plugin->so_handler , "BeforeReadInputPlugin" ) ;
	
	p_logpipe_input_plugin->pfuncReadInputPlugin = (funcReadInputPlugin *)dlsym( p_logpipe_input_plugin->so_handler , "ReadInputPlugin" ) ;
	if( p_logpipe_input_plugin->pfuncOnInputPluginEvent == NULL )
	{
		ERRORLOGC( "dlsym[%s][ReadInputPlugin] failed , dlerror[%s]" , p_logpipe_input_plugin->so_path_filename , dlerror() )
		return -1;
	}
	
	p_logpipe_input_plugin->pfuncAfterReadInputPlugin = (funcAfterReadInputPlugin *)dlsym( p_logpipe_input_plugin->so_handler , "AfterReadInputPlugin" ) ;
	
	p_logpipe_input_plugin->pfuncCleanInputPluginContext = (funcCleanInputPluginContext *)dlsym( p_logpipe_input_plugin->so_handler , "CleanInputPluginContext" ) ;
	if( p_logpipe_input_plugin->pfuncCleanInputPluginContext == NULL )
	{
		ERRORLOGC( "dlsym[%s][CleanInputPluginContext] failed , dlerror[%s]" , p_logpipe_input_plugin->so_path_filename , dlerror() )
		return -1;
	}
	
	p_logpipe_input_plugin->pfuncUnloadInputPluginConfig = (funcUnloadInputPluginConfig *)dlsym( p_logpipe_input_plugin->so_handler , "UnloadInputPluginConfig" ) ;
	if( p_logpipe_input_plugin->pfuncUnloadInputPluginConfig == NULL )
	{
		ERRORLOGC( "dlsym[%s][UnloadInputPluginConfig] failed , dlerror[%s]" , p_logpipe_input_plugin->so_path_filename , dlerror() )
		return -1;
	}
	
	/* 加入到输入插件链表 */
	list_add_tail( & (p_logpipe_input_plugin->this_node) , & (p_env->logpipe_input_plugins_list.this_node) );
	
	return 0;
}

/* 装载插出插件 */
static int LoadLogpipeFilterPlugin( struct LogpipeEnv *p_env , struct LogpipeFilterPlugin *p_logpipe_filter_plugin )
{
	char		*p = NULL ;
	
	/* 查询输出插件文件名 */
	p = QueryPluginConfigItem( & (p_logpipe_filter_plugin->plugin_config_items) , "plugin" ) ;
	if( p == NULL )
	{
		ERRORLOGC( "expect 'plugin' in 'filters'" )
		return -1;
	}
	
	strncpy( p_logpipe_filter_plugin->so_filename , p , sizeof(p_logpipe_filter_plugin->so_filename)-1 );
	if( p_logpipe_filter_plugin->so_filename[0] == '/' )
	{
		strcpy( p_logpipe_filter_plugin->so_path_filename , p_logpipe_filter_plugin->so_filename );
	}
	else
	{
		snprintf( p_logpipe_filter_plugin->so_path_filename , sizeof(p_logpipe_filter_plugin->so_path_filename)-1 , "%s/%s" , getenv("HOME") , p_logpipe_filter_plugin->so_filename );
	}
	
	/* 打开输出插件 */
	p_logpipe_filter_plugin->so_handler = dlopen( p_logpipe_filter_plugin->so_path_filename , RTLD_LAZY ) ;
	if( p_logpipe_filter_plugin->so_handler == NULL )
	{
		ERRORLOGC( "dlopen[%s] failed , dlerror[%s]" , p_logpipe_filter_plugin->so_path_filename , dlerror() )
		return -1;
	}
	
	/* 查询所有回调函数指针 */
	p_logpipe_filter_plugin->pfuncLoadFilterPluginConfig = (funcLoadFilterPluginConfig *)dlsym( p_logpipe_filter_plugin->so_handler , "LoadFilterPluginConfig" ) ;
	if( p_logpipe_filter_plugin->pfuncLoadFilterPluginConfig == NULL )
	{
		ERRORLOGC( "dlsym[%s][InitLogpipeFilterPlugin] failed , dlerror[%s]" , p_logpipe_filter_plugin->so_path_filename , dlerror() )
		return -1;
	}
	
	p_logpipe_filter_plugin->pfuncInitFilterPluginContext = (funcInitFilterPluginContext *)dlsym( p_logpipe_filter_plugin->so_handler , "InitFilterPluginContext" ) ;
	if( p_logpipe_filter_plugin->pfuncInitFilterPluginContext == NULL )
	{
		ERRORLOGC( "dlsym[%s][InitLogpipeFilterPlugin] failed , dlerror[%s]" , p_logpipe_filter_plugin->so_path_filename , dlerror() )
		return -1;
	}
	
	p_logpipe_filter_plugin->pfuncBeforeProcessFilterPlugin = (funcBeforeProcessFilterPlugin *)dlsym( p_logpipe_filter_plugin->so_handler , "BeforeProcessFilterPlugin" ) ;
	
	p_logpipe_filter_plugin->pfuncProcessFilterPlugin = (funcProcessFilterPlugin *)dlsym( p_logpipe_filter_plugin->so_handler , "ProcessFilterPlugin" ) ;
	if( p_logpipe_filter_plugin->pfuncProcessFilterPlugin == NULL )
	{
		ERRORLOGC( "dlsym[%s][ProcessFilterPlugin] failed , dlerror[%s]" , p_logpipe_filter_plugin->so_path_filename , dlerror() )
		return -1;
	}
	
	p_logpipe_filter_plugin->pfuncAfterProcessFilterPlugin = (funcAfterProcessFilterPlugin *)dlsym( p_logpipe_filter_plugin->so_handler , "AfterProcessFilterPlugin" ) ;
	
	p_logpipe_filter_plugin->pfuncCleanFilterPluginContext = (funcCleanFilterPluginContext *)dlsym( p_logpipe_filter_plugin->so_handler , "CleanFilterPluginContext" ) ;
	if( p_logpipe_filter_plugin->pfuncCleanFilterPluginContext == NULL )
	{
		ERRORLOGC( "dlsym[%s][CleanFilterPluginContext] failed , dlerror[%s]" , p_logpipe_filter_plugin->so_path_filename , dlerror() )
		return -1;
	}
	
	p_logpipe_filter_plugin->pfuncUnloadFilterPluginConfig = (funcUnloadFilterPluginConfig *)dlsym( p_logpipe_filter_plugin->so_handler , "UnloadFilterPluginConfig" ) ;
	if( p_logpipe_filter_plugin->pfuncUnloadFilterPluginConfig == NULL )
	{
		ERRORLOGC( "dlsym[%s][UnloadFilterPluginConfig] failed , dlerror[%s]" , p_logpipe_filter_plugin->so_path_filename , dlerror() )
		return -1;
	}
	
	/* 加入到输出插件链表 */
	list_add_tail( & (p_logpipe_filter_plugin->this_node) , & (p_env->logpipe_filter_plugins_list.this_node) );
	
	return 0;
}

/* 装载插出插件 */
static int LoadLogpipeOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin )
{
	char		*p = NULL ;
	
	/* 查询输出插件文件名 */
	p = QueryPluginConfigItem( & (p_logpipe_output_plugin->plugin_config_items) , "plugin" ) ;
	if( p == NULL )
	{
		ERRORLOGC( "expect 'plugin' in 'outputs'" )
		return -1;
	}
	
	strncpy( p_logpipe_output_plugin->so_filename , p , sizeof(p_logpipe_output_plugin->so_filename)-1 );
	if( p_logpipe_output_plugin->so_filename[0] == '/' )
	{
		strcpy( p_logpipe_output_plugin->so_path_filename , p_logpipe_output_plugin->so_filename );
	}
	else
	{
		snprintf( p_logpipe_output_plugin->so_path_filename , sizeof(p_logpipe_output_plugin->so_path_filename)-1 , "%s/%s" , getenv("HOME") , p_logpipe_output_plugin->so_filename );
	}
	
	/* 打开输出插件 */
	p_logpipe_output_plugin->so_handler = dlopen( p_logpipe_output_plugin->so_path_filename , RTLD_LAZY ) ;
	if( p_logpipe_output_plugin->so_handler == NULL )
	{
		ERRORLOGC( "dlopen[%s] failed , dlerror[%s]" , p_logpipe_output_plugin->so_path_filename , dlerror() )
		return -1;
	}
	
	/* 查询所有回调函数指针 */
	p_logpipe_output_plugin->pfuncLoadOutputPluginConfig = (funcLoadOutputPluginConfig *)dlsym( p_logpipe_output_plugin->so_handler , "LoadOutputPluginConfig" ) ;
	if( p_logpipe_output_plugin->pfuncLoadOutputPluginConfig == NULL )
	{
		ERRORLOGC( "dlsym[%s][InitLogpipeOutputPlugin] failed , dlerror[%s]" , p_logpipe_output_plugin->so_path_filename , dlerror() )
		return -1;
	}
	
	p_logpipe_output_plugin->pfuncInitOutputPluginContext = (funcInitOutputPluginContext *)dlsym( p_logpipe_output_plugin->so_handler , "InitOutputPluginContext" ) ;
	if( p_logpipe_output_plugin->pfuncInitOutputPluginContext == NULL )
	{
		ERRORLOGC( "dlsym[%s][InitLogpipeOutputPlugin] failed , dlerror[%s]" , p_logpipe_output_plugin->so_path_filename , dlerror() )
		return -1;
	}
	
	p_logpipe_output_plugin->pfuncOnOutputPluginIdle = (funcOnOutputPluginEvent *)dlsym( p_logpipe_output_plugin->so_handler , "OnOutputPluginIdle" ) ;
	if( p_logpipe_output_plugin->pfuncOnOutputPluginIdle )
		p_env->idle_processing_flag = 1 ;
	
	p_logpipe_output_plugin->pfuncOnOutputPluginEvent = (funcOnOutputPluginEvent *)dlsym( p_logpipe_output_plugin->so_handler , "OnOutputPluginEvent" ) ;
	if( p_logpipe_output_plugin->pfuncOnOutputPluginEvent == NULL )
	{
		ERRORLOGC( "dlsym[%s][OnOutputPluginEvent] failed , dlerror[%s]" , p_logpipe_output_plugin->so_path_filename , dlerror() )
		return -1;
	}
	
	p_logpipe_output_plugin->pfuncBeforeWriteOutputPlugin = (funcBeforeWriteOutputPlugin *)dlsym( p_logpipe_output_plugin->so_handler , "BeforeWriteOutputPlugin" ) ;
	
	p_logpipe_output_plugin->pfuncWriteOutputPlugin = (funcWriteOutputPlugin *)dlsym( p_logpipe_output_plugin->so_handler , "WriteOutputPlugin" ) ;
	if( p_logpipe_output_plugin->pfuncWriteOutputPlugin == NULL )
	{
		ERRORLOGC( "dlsym[%s][WriteOutputPlugin] failed , dlerror[%s]" , p_logpipe_output_plugin->so_path_filename , dlerror() )
		return -1;
	}
	
	p_logpipe_output_plugin->pfuncAfterWriteOutputPlugin = (funcAfterWriteOutputPlugin *)dlsym( p_logpipe_output_plugin->so_handler , "AfterWriteOutputPlugin" ) ;
	
	p_logpipe_output_plugin->pfuncCleanOutputPluginContext = (funcCleanOutputPluginContext *)dlsym( p_logpipe_output_plugin->so_handler , "CleanOutputPluginContext" ) ;
	if( p_logpipe_output_plugin->pfuncCleanOutputPluginContext == NULL )
	{
		ERRORLOGC( "dlsym[%s][CleanOutputPluginContext] failed , dlerror[%s]" , p_logpipe_output_plugin->so_path_filename , dlerror() )
		return -1;
	}
	
	p_logpipe_output_plugin->pfuncUnloadOutputPluginConfig = (funcUnloadOutputPluginConfig *)dlsym( p_logpipe_output_plugin->so_handler , "UnloadOutputPluginConfig" ) ;
	if( p_logpipe_output_plugin->pfuncUnloadOutputPluginConfig == NULL )
	{
		ERRORLOGC( "dlsym[%s][UnloadOutputPluginConfig] failed , dlerror[%s]" , p_logpipe_output_plugin->so_path_filename , dlerror() )
		return -1;
	}
	
	/* 加入到输出插件链表 */
	list_add_tail( & (p_logpipe_output_plugin->this_node) , & (p_env->logpipe_output_plugins_list.this_node) );
	
	return 0;
}

/* 解析配置文件的JSON节点回调函数 */
int CallbackOnJsonNode( int type , char *jpath , int jpath_len , int jpath_size , char *node , int node_len , char *content , int content_len , void *p )
{
	struct LogpipeEnv			*p_env = (struct LogpipeEnv *)p ;
	static struct LogpipeInputPlugin	*p_logpipe_input_plugin = NULL ;
	static struct LogpipeFilterPlugin	*p_logpipe_filter_plugin = NULL ;
	static struct LogpipeOutputPlugin	*p_logpipe_output_plugin = NULL ;
	
	int					nret = 0 ;
	
	/* 当进入配置树枝 */
	if( (type&FASTERJSON_NODE_ENTER) && (type&FASTERJSON_NODE_BRANCH) )
	{
		if( jpath_len == sizeof(LOGPIPE_CONFIG_INPUTS)-1 && STRNCMP( jpath , == , LOGPIPE_CONFIG_INPUTS , jpath_len ) )
		{
			p_logpipe_input_plugin = (struct LogpipeInputPlugin *)malloc( sizeof(struct LogpipeInputPlugin) ) ;
			if( p_logpipe_input_plugin == NULL )
				return -1;
			memset( p_logpipe_input_plugin , 0x00 , sizeof(struct LogpipeInputPlugin) );
			
			p_logpipe_input_plugin->type = LOGPIPE_PLUGIN_TYPE_INPUT ;
			
			INIT_LIST_HEAD( & (p_logpipe_input_plugin->plugin_config_items.this_node) );
		}
		else if( jpath_len == sizeof(LOGPIPE_CONFIG_FILTERS)-1 && STRNCMP( jpath , == , LOGPIPE_CONFIG_FILTERS , jpath_len ) )
		{
			p_logpipe_filter_plugin = (struct LogpipeFilterPlugin *)malloc( sizeof(struct LogpipeFilterPlugin) ) ;
			if( p_logpipe_filter_plugin == NULL )
				return -1;
			memset( p_logpipe_filter_plugin , 0x00 , sizeof(struct LogpipeFilterPlugin) );
			
			p_logpipe_filter_plugin->type = LOGPIPE_PLUGIN_TYPE_FILTER ;
			
			INIT_LIST_HEAD( & (p_logpipe_filter_plugin->plugin_config_items.this_node) );
		}
		else if( jpath_len == sizeof(LOGPIPE_CONFIG_OUTPUTS)-1 && STRNCMP( jpath , == , LOGPIPE_CONFIG_OUTPUTS , jpath_len ) )
		{
			p_logpipe_output_plugin = (struct LogpipeOutputPlugin *)malloc( sizeof(struct LogpipeOutputPlugin) ) ;
			if( p_logpipe_output_plugin == NULL )
				return -1;
			memset( p_logpipe_output_plugin , 0x00 , sizeof(struct LogpipeOutputPlugin) );
			
			p_logpipe_output_plugin->type = LOGPIPE_PLUGIN_TYPE_OUTPUT ;
			
			INIT_LIST_HEAD( & (p_logpipe_output_plugin->plugin_config_items.this_node) );
		}
	}
	/* 当离开配置树枝 */
	else if( (type&FASTERJSON_NODE_LEAVE) && (type&FASTERJSON_NODE_BRANCH) )
	{
		if( jpath_len == sizeof(LOGPIPE_CONFIG_INPUTS)-1 && STRNCMP( jpath , == , LOGPIPE_CONFIG_INPUTS , jpath_len ) )
		{
			nret = LoadLogpipeInputPlugin( p_env , p_logpipe_input_plugin ) ;
			if( nret )
				return nret;
		}
		else if( jpath_len == sizeof(LOGPIPE_CONFIG_FILTERS)-1 && STRNCMP( jpath , == , LOGPIPE_CONFIG_FILTERS , jpath_len ) )
		{
			nret = LoadLogpipeFilterPlugin( p_env , p_logpipe_filter_plugin ) ;
			if( nret )
				return nret;
		}
		else if( jpath_len == sizeof(LOGPIPE_CONFIG_OUTPUTS)-1 && STRNCMP( jpath , == , LOGPIPE_CONFIG_OUTPUTS , jpath_len ) )
		{
			nret = LoadLogpipeOutputPlugin( p_env , p_logpipe_output_plugin ) ;
			if( nret )
				return nret;
		}
	}
	/* 当配置树叶 */
	else if( (type&FASTERJSON_NODE_LEAF) )
	{
		if( jpath_len == sizeof(LOGPIPE_CONFIG_LOG_LOGFILE)-1 && STRNCMP( jpath , == , LOGPIPE_CONFIG_LOG_LOGFILE , jpath_len ) )
		{
			strncpy( p_env->log_file , content , content_len );
		}
		else if( jpath_len == sizeof(LOGPIPE_CONFIG_LOG_LOGLEVEL)-1 && STRNCMP( jpath , == , LOGPIPE_CONFIG_LOG_LOGLEVEL , jpath_len ) )
		{
			p_env->log_level = ConvLogLevelStr( content , content_len ) ;
			if( p_env->log_level == -1 )
			{
				ERRORLOGC( "log_level[%.*s] invalid" , content_len , content )
				return -1;
			}
		}
		else if( MEMCMP( jpath , == , LOGPIPE_CONFIG_INPUTS_ , sizeof(LOGPIPE_CONFIG_INPUTS_)-1 ) )
		{
			nret = AddPluginConfigItem( & (p_logpipe_input_plugin->plugin_config_items) , node , node_len , content , content_len ) ;
			if( nret )
			{
				ERRORLOGC( "AddPluginConfigItem [%.*s][%.*s] failed" , node_len , node , content_len , content )
				return -1;
			}
		}
		else if( MEMCMP( jpath , == , LOGPIPE_CONFIG_FILTERS_ , sizeof(LOGPIPE_CONFIG_FILTERS_)-1 ) )
		{
			nret = AddPluginConfigItem( & (p_logpipe_filter_plugin->plugin_config_items) , node , node_len , content , content_len ) ;
			if( nret )
			{
				ERRORLOGC( "AddPluginConfigItem [%.*s][%.*s] failed" , node_len , node , content_len , content )
				return -1;
			}
		}
		else if( MEMCMP( jpath , == , LOGPIPE_CONFIG_OUTPUTS_ , sizeof(LOGPIPE_CONFIG_OUTPUTS_)-1 ) )
		{
			nret = AddPluginConfigItem( & (p_logpipe_output_plugin->plugin_config_items) , node , node_len , content , content_len ) ;
			if( nret )
			{
				ERRORLOGC( "AddPluginConfigItem [%.*s][%.*s] failed" , node_len , node , content_len , content )
				return -1;
			}
		}
	}
	
	return 0;
}

/* 解析配置文件，构建插件链表 */
int LoadConfig( struct LogpipeEnv *p_env )
{
	char				*file_content = NULL ;
	char				jpath[ 1024 + 1 ] ;
	
	struct LogpipeInputPlugin	*p_logpipe_input_plugin = NULL ;
	struct LogpipeFilterPlugin	*p_logpipe_filter_plugin = NULL ;
	struct LogpipeOutputPlugin	*p_logpipe_output_plugin = NULL ;
	
	int				nret = 0 ;
	
	file_content = StrdupEntireFile( p_env->config_path_filename , NULL ) ;
	if( file_content == NULL )
	{
		ERRORLOGC( "open file[%s] failed , errno[%d]" , p_env->config_path_filename , errno )
		return -1;
	}
	
	g_fasterjson_encoding = FASTERJSON_ENCODING_GB18030 ;
	
	memset( jpath , 0x00 , sizeof(jpath) );
	nret = TravelJsonBuffer( file_content , jpath , sizeof(jpath) , & CallbackOnJsonNode , p_env ) ;
	if( nret )
	{
		ERRORLOGC( "parse config[%s] failed[%d]" , p_env->config_path_filename , nret )
		free( file_content );
		return -1;
	}
	
	free( file_content );
	
	/* 执行所有输出端初始化函数 */
	list_for_each_entry( p_logpipe_output_plugin , & (p_env->logpipe_output_plugins_list.this_node) , struct LogpipeOutputPlugin , this_node )
	{
		p_logpipe_output_plugin->fd = -1 ;
		p_logpipe_output_plugin->context = NULL ;
		nret = p_logpipe_output_plugin->pfuncLoadOutputPluginConfig( p_env , p_logpipe_output_plugin , & (p_logpipe_output_plugin->plugin_config_items) , & (p_logpipe_output_plugin->context) ) ;
		if( nret )
		{
			ERRORLOGC( "[%s]->pfuncLoadOutputPluginConfig failed , errno[%d]" , p_logpipe_output_plugin->so_filename , errno )
			return -1;
		}
		else
		{
			DEBUGLOGC( "[%s]->pfuncLoadOutputPluginConfig ok" , p_logpipe_output_plugin->so_filename )
		}
	}
	
	/* 执行所有过滤端初始化函数 */
	list_for_each_entry( p_logpipe_filter_plugin , & (p_env->logpipe_filter_plugins_list.this_node) , struct LogpipeFilterPlugin , this_node )
	{
		p_logpipe_filter_plugin->context = NULL ;
		nret = p_logpipe_filter_plugin->pfuncLoadFilterPluginConfig( p_env , p_logpipe_filter_plugin , & (p_logpipe_filter_plugin->plugin_config_items) , & (p_logpipe_filter_plugin->context) ) ;
		if( nret )
		{
			ERRORLOGC( "[%s]->pfuncLoadFilterPluginConfig failed , errno[%d]" , p_logpipe_filter_plugin->so_filename , errno )
			return -1;
		}
		else
		{
			DEBUGLOGC( "[%s]->pfuncLoadFilterPluginConfig ok" , p_logpipe_filter_plugin->so_filename )
		}
	}
	
	/* 执行所有输入端初始化函数 */
	list_for_each_entry( p_logpipe_input_plugin , & (p_env->logpipe_input_plugins_list.this_node) , struct LogpipeInputPlugin , this_node )
	{
		p_logpipe_input_plugin->fd = -1 ;
		p_logpipe_input_plugin->context = NULL ;
		nret = p_logpipe_input_plugin->pfuncLoadInputPluginConfig( p_env , p_logpipe_input_plugin , & (p_logpipe_input_plugin->plugin_config_items) , & (p_logpipe_input_plugin->context) ) ;
		if( nret )
		{
			ERRORLOGC( "[%s]->pfuncLoadInputPluginConfig failed , errno[%d]" , p_logpipe_input_plugin->so_filename , errno )
			return -1;
		}
		else
		{
			DEBUGLOGC( "[%s]->pfuncLoadInputPluginConfig ok" , p_logpipe_input_plugin->so_filename )
		}
	}
	
	return 0;
}

/* 释放插件链表，卸载配置 */
void UnloadConfig( struct LogpipeEnv *p_env )
{
	struct LogpipeInputPlugin	*p_logpipe_input_plugin = NULL ;
	struct LogpipeInputPlugin	*p_next_logpipe_input_plugin = NULL ;
	struct LogpipeFilterPlugin	*p_logpipe_filter_plugin = NULL ;
	struct LogpipeFilterPlugin	*p_next_logpipe_filter_plugin = NULL ;
	struct LogpipeOutputPlugin	*p_logpipe_output_plugin = NULL ;
	struct LogpipeOutputPlugin	*p_next_logpipe_output_plugin = NULL ;
	
	/* 执行所有输入端初始化函数 */
	list_for_each_entry_safe( p_logpipe_input_plugin , p_next_logpipe_input_plugin , & (p_env->logpipe_input_plugins_list.this_node) , struct LogpipeInputPlugin , this_node )
	{
		UnloadInputPluginSession( p_env , p_logpipe_input_plugin );
	}
	
	/* 执行所有过滤端初始化函数 */
	list_for_each_entry_safe( p_logpipe_filter_plugin , p_next_logpipe_filter_plugin , & (p_env->logpipe_filter_plugins_list.this_node) , struct LogpipeFilterPlugin , this_node )
	{
		UnloadFilterPluginSession( p_env , p_logpipe_filter_plugin );
	}
	
	/* 执行所有输出端初始化函数 */
	list_for_each_entry_safe( p_logpipe_output_plugin , p_next_logpipe_output_plugin , & (p_env->logpipe_output_plugins_list.this_node) , struct LogpipeOutputPlugin , this_node )
	{
		UnloadOutputPluginSession( p_env , p_logpipe_output_plugin );
	}
	
	/* 释放执行单次配置 */
	RemoveAllPluginConfigItems( & (p_env->start_once_for_plugin_config_items) );
	
	return;
}

/* 卸载输入插件实例 */
void UnloadInputPluginSession( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin )
{
	RemoveAllPluginConfigItems( & (p_logpipe_input_plugin->plugin_config_items) );
	
	p_logpipe_input_plugin->pfuncUnloadInputPluginConfig( p_env , p_logpipe_input_plugin , & (p_logpipe_input_plugin->context) );
	
	if( p_logpipe_input_plugin->so_handler )
	{
		dlclose( p_logpipe_input_plugin->so_handler );
	}
	
	list_del( & (p_logpipe_input_plugin->this_node) );
	
	free( p_logpipe_input_plugin );
	
	return;
}

/* 卸载过滤插件实例 */
void UnloadFilterPluginSession( struct LogpipeEnv *p_env , struct LogpipeFilterPlugin *p_logpipe_filter_plugin )
{
	RemoveAllPluginConfigItems( & (p_logpipe_filter_plugin->plugin_config_items) );
	
	p_logpipe_filter_plugin->pfuncUnloadFilterPluginConfig( p_env , p_logpipe_filter_plugin , & (p_logpipe_filter_plugin->context) );
	
	if( p_logpipe_filter_plugin->so_handler )
	{
		dlclose( p_logpipe_filter_plugin->so_handler );
	}
	
	list_del( & (p_logpipe_filter_plugin->this_node) );
	
	free( p_logpipe_filter_plugin );
	
	return;
}

/* 卸载输出插件实例 */
void UnloadOutputPluginSession( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin )
{
	RemoveAllPluginConfigItems( & (p_logpipe_output_plugin->plugin_config_items) );
	
	p_logpipe_output_plugin->pfuncUnloadOutputPluginConfig( p_env , p_logpipe_output_plugin , & (p_logpipe_output_plugin->context) );
	
	if( p_logpipe_output_plugin->so_handler )
	{
		dlclose( p_logpipe_output_plugin->so_handler );
	}
	
	list_del( & (p_logpipe_output_plugin->this_node) );
	
	free( p_logpipe_output_plugin );
	
	return;
}

