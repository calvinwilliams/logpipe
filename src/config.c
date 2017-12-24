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
#define LOGPIPE_CONFIG_OUTPUTS		"/outputs"
#define LOGPIPE_CONFIG_OUTPUTS_		"/outputs/"

static int ConvLogLevelStr( char *log_level_str , int log_level_str_len )
{
	if( log_level_str_len == 5 && STRNCMP( log_level_str , == , "DEBUG" , log_level_str_len ) )
		return LOGLEVEL_DEBUG;
	else if( log_level_str_len == 4 && STRNCMP( log_level_str , == , "INFO" , log_level_str_len ) )
		return LOGLEVEL_INFO ;
	else if( log_level_str_len == 4 && STRNCMP( log_level_str , == , "WARN" , log_level_str_len ) )
		return LOGLEVEL_WARN ;
	else if( log_level_str_len == 5 && STRNCMP( log_level_str , == , "ERROR" , log_level_str_len ) )
		return LOGLEVEL_ERROR ;
	else if( log_level_str_len == 5 && STRNCMP( log_level_str , == , "FATAL" , log_level_str_len ) )
		return LOGLEVEL_FATAL ;
	else
		return -1;
}

static int LoadLogpipeInputPlugin( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin )
{
	char		*p = NULL ;
	
	p = QueryPluginConfigItem( & (p_logpipe_input_plugin->plugin_config_items) , "plugin" ) ;
	if( p == NULL )
	{
		ERRORLOG( "expect 'plugin' in 'inputs'" );
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
	
	p_logpipe_input_plugin->so_handler = dlopen( p_logpipe_input_plugin->so_path_filename , RTLD_LAZY ) ;
	if( p_logpipe_input_plugin->so_handler == NULL )
	{
		ERRORLOG( "dlopen[%s] failed , dlerror[%s]" , p_logpipe_input_plugin->so_path_filename , dlerror() );
		return -1;
	}
	
	p_logpipe_input_plugin->pfuncLoadInputPluginConfig = (funcLoadInputPluginConfig *)dlsym( p_logpipe_input_plugin->so_handler , "LoadInputPluginConfig" ) ;
	if( p_logpipe_input_plugin->pfuncLoadInputPluginConfig == NULL )
	{
		ERRORLOG( "dlsym[%s][LoadInputPluginConfig] failed , dlerror[%s]" , p_logpipe_input_plugin->so_path_filename , dlerror() );
		return -1;
	}
	
	p_logpipe_input_plugin->pfuncInitInputPluginContext = (funcInitInputPluginContext *)dlsym( p_logpipe_input_plugin->so_handler , "InitInputPluginContext" ) ;
	if( p_logpipe_input_plugin->pfuncInitInputPluginContext == NULL )
	{
		ERRORLOG( "dlsym[%s][LoadInputPluginConfig] failed , dlerror[%s]" , p_logpipe_input_plugin->so_path_filename , dlerror() );
		return -1;
	}
	
	p_logpipe_input_plugin->pfuncOnInputPluginEvent = (funcOnInputPluginEvent *)dlsym( p_logpipe_input_plugin->so_handler , "OnInputPluginEvent" ) ;
	if( p_logpipe_input_plugin->pfuncOnInputPluginEvent == NULL )
	{
		ERRORLOG( "dlsym[%s][OnInputPluginEvent] failed , dlerror[%s]" , p_logpipe_input_plugin->so_path_filename , dlerror() );
		return -1;
	}
	
	p_logpipe_input_plugin->pfuncReadInputPlugin = (funcReadInputPlugin *)dlsym( p_logpipe_input_plugin->so_handler , "ReadInputPlugin" ) ;
	if( p_logpipe_input_plugin->pfuncOnInputPluginEvent == NULL )
	{
		ERRORLOG( "dlsym[%s][ReadInputPlugin] failed , dlerror[%s]" , p_logpipe_input_plugin->so_path_filename , dlerror() );
		return -1;
	}
	
	p_logpipe_input_plugin->pfuncCleanInputPluginContext = (funcCleanInputPluginContext *)dlsym( p_logpipe_input_plugin->so_handler , "CleanInputPluginContext" ) ;
	if( p_logpipe_input_plugin->pfuncCleanInputPluginContext == NULL )
	{
		ERRORLOG( "dlsym[%s][CleanInputPluginContext] failed , dlerror[%s]" , p_logpipe_input_plugin->so_path_filename , dlerror() );
		return -1;
	}
	
	p_logpipe_input_plugin->pfuncUnloadInputPluginConfig = (funcUnloadInputPluginConfig *)dlsym( p_logpipe_input_plugin->so_handler , "UnloadInputPluginConfig" ) ;
	if( p_logpipe_input_plugin->pfuncUnloadInputPluginConfig == NULL )
	{
		ERRORLOG( "dlsym[%s][UnloadInputPluginConfig] failed , dlerror[%s]" , p_logpipe_input_plugin->so_path_filename , dlerror() );
		return -1;
	}
	
	list_add_tail( & (p_logpipe_input_plugin->this_node) , & (p_env->logpipe_input_plugins_list.this_node) );
	
	return 0;
}

static int LoadLogpipeOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin )
{
	char		*p = NULL ;
	
	p = QueryPluginConfigItem( & (p_logpipe_output_plugin->plugin_config_items) , "plugin" ) ;
	if( p == NULL )
	{
		ERRORLOG( "expect 'plugin' in 'outputs'" );
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
	
	p_logpipe_output_plugin->so_handler = dlopen( p_logpipe_output_plugin->so_path_filename , RTLD_LAZY ) ;
	if( p_logpipe_output_plugin->so_handler == NULL )
	{
		ERRORLOG( "dlopen[%s] failed , dlerror[%s]" , p_logpipe_output_plugin->so_path_filename , dlerror() );
		return -1;
	}
	
	p_logpipe_output_plugin->pfuncLoadOutputPluginConfig = (funcLoadOutputPluginConfig *)dlsym( p_logpipe_output_plugin->so_handler , "LoadOutputPluginConfig" ) ;
	if( p_logpipe_output_plugin->pfuncLoadOutputPluginConfig == NULL )
	{
		ERRORLOG( "dlsym[%s][InitLogpipeOutputPlugin] failed , dlerror[%s]" , p_logpipe_output_plugin->so_path_filename , dlerror() );
		return -1;
	}
	
	p_logpipe_output_plugin->pfuncInitOutputPluginContext = (funcInitOutputPluginContext *)dlsym( p_logpipe_output_plugin->so_handler , "InitOutputPluginContext" ) ;
	if( p_logpipe_output_plugin->pfuncInitOutputPluginContext == NULL )
	{
		ERRORLOG( "dlsym[%s][InitLogpipeOutputPlugin] failed , dlerror[%s]" , p_logpipe_output_plugin->so_path_filename , dlerror() );
		return -1;
	}
	
	p_logpipe_output_plugin->pfuncOnOutputPluginEvent = (funcOnOutputPluginEvent *)dlsym( p_logpipe_output_plugin->so_handler , "OnOutputPluginEvent" ) ;
	if( p_logpipe_output_plugin->pfuncOnOutputPluginEvent == NULL )
	{
		ERRORLOG( "dlsym[%s][OnOutputPluginEvent] failed , dlerror[%s]" , p_logpipe_output_plugin->so_path_filename , dlerror() );
		return -1;
	}
	
	p_logpipe_output_plugin->pfuncBeforeWriteOutputPlugin = (funcBeforeWriteOutputPlugin *)dlsym( p_logpipe_output_plugin->so_handler , "BeforeWriteOutputPlugin" ) ;
	if( p_logpipe_output_plugin->pfuncBeforeWriteOutputPlugin == NULL )
	{
		ERRORLOG( "dlsym[%s][BeforeWriteOutputPlugin] failed , dlerror[%s]" , p_logpipe_output_plugin->so_path_filename , dlerror() );
		return -1;
	}
	
	p_logpipe_output_plugin->pfuncWriteOutputPlugin = (funcWriteOutputPlugin *)dlsym( p_logpipe_output_plugin->so_handler , "WriteOutputPlugin" ) ;
	if( p_logpipe_output_plugin->pfuncWriteOutputPlugin == NULL )
	{
		ERRORLOG( "dlsym[%s][WriteOutputPlugin] failed , dlerror[%s]" , p_logpipe_output_plugin->so_path_filename , dlerror() );
		return -1;
	}
	
	p_logpipe_output_plugin->pfuncAfterWriteOutputPlugin = (funcAfterWriteOutputPlugin *)dlsym( p_logpipe_output_plugin->so_handler , "AfterWriteOutputPlugin" ) ;
	if( p_logpipe_output_plugin->pfuncAfterWriteOutputPlugin == NULL )
	{
		ERRORLOG( "dlsym[%s][AfterWriteOutputPlugin] failed , dlerror[%s]" , p_logpipe_output_plugin->so_path_filename , dlerror() );
		return -1;
	}
	
	p_logpipe_output_plugin->pfuncCleanOutputPluginContext = (funcCleanOutputPluginContext *)dlsym( p_logpipe_output_plugin->so_handler , "CleanOutputPluginContext" ) ;
	if( p_logpipe_output_plugin->pfuncCleanOutputPluginContext == NULL )
	{
		ERRORLOG( "dlsym[%s][CleanOutputPluginContext] failed , dlerror[%s]" , p_logpipe_output_plugin->so_path_filename , dlerror() );
		return -1;
	}
	
	p_logpipe_output_plugin->pfuncUnloadOutputPluginConfig = (funcUnloadOutputPluginConfig *)dlsym( p_logpipe_output_plugin->so_handler , "UnloadOutputPluginConfig" ) ;
	if( p_logpipe_output_plugin->pfuncUnloadOutputPluginConfig == NULL )
	{
		ERRORLOG( "dlsym[%s][UnloadOutputPluginConfig] failed , dlerror[%s]" , p_logpipe_output_plugin->so_path_filename , dlerror() );
		return -1;
	}
	
	list_add_tail( & (p_logpipe_output_plugin->this_node) , & (p_env->logpipe_output_plugins_list.this_node) );
	
	return 0;
}

int CallbackOnJsonNode( int type , char *jpath , int jpath_len , int jpath_size , char *node , int node_len , char *content , int content_len , void *p )
{
	struct LogpipeEnv			*p_env = (struct LogpipeEnv *)p ;
	static struct LogpipeInputPlugin	*p_logpipe_input_plugin = NULL ;
	static struct LogpipeOutputPlugin	*p_logpipe_output_plugin = NULL ;
	
	int					nret = 0 ;
	
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
	else if( (type&FASTERJSON_NODE_LEAVE) && (type&FASTERJSON_NODE_BRANCH) )
	{
		if( jpath_len == sizeof(LOGPIPE_CONFIG_INPUTS)-1 && STRNCMP( jpath , == , LOGPIPE_CONFIG_INPUTS , jpath_len ) )
		{
			nret = LoadLogpipeInputPlugin( p_env , p_logpipe_input_plugin ) ;
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
				ERRORLOG( "log_level[%.*s] invalid" , content_len , content );
				return -1;
			}
		}
		else if( MEMCMP( jpath , == , LOGPIPE_CONFIG_INPUTS_ , sizeof(LOGPIPE_CONFIG_INPUTS_)-1 ) )
		{
			nret = AddPluginConfigItem( & (p_logpipe_input_plugin->plugin_config_items) , node , node_len , content , content_len ) ;
			if( nret )
			{
				ERRORLOG( "AddPluginConfigItem [%.*s][%.*s] failed" , node_len , node , content_len , content );
				return -1;
			}
		}
		else if( MEMCMP( jpath , == , LOGPIPE_CONFIG_OUTPUTS_ , sizeof(LOGPIPE_CONFIG_OUTPUTS_)-1 ) )
		{
			nret = AddPluginConfigItem( & (p_logpipe_output_plugin->plugin_config_items) , node , node_len , content , content_len ) ;
			if( nret )
			{
				ERRORLOG( "AddPluginConfigItem [%.*s][%.*s] failed" , node_len , node , content_len , content );
				return -1;
			}
		}
	}
	
	return 0;
}

int LoadConfig( struct LogpipeEnv *p_env )
{
	char				*file_content = NULL ;
	char				jpath[ 1024 + 1 ] ;
	
	struct LogpipeInputPlugin	*p_logpipe_input_plugin = NULL ;
	struct LogpipeOutputPlugin	*p_logpipe_output_plugin = NULL ;
	
	int				nret = 0 ;
	
	file_content = StrdupEntireFile( p_env->config_path_filename , NULL ) ;
	if( file_content == NULL )
	{
		ERRORLOG( "open file[%s] failed , errno[%d]" , p_env->config_path_filename , errno );
		return -1;
	}
	
	g_fasterjson_encoding = FASTERJSON_ENCODING_GB18030 ;
	
	memset( jpath , 0x00 , sizeof(jpath) );
	nret = TravelJsonBuffer( file_content , jpath , sizeof(jpath) , & CallbackOnJsonNode , p_env ) ;
	if( nret )
	{
		ERRORLOG( "parse config[%s] failed[%d]" , p_env->config_path_filename , nret );
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
			ERRORLOG( "[%s]->pfuncLoadOutputPluginConfig failed , errno[%d]" , p_logpipe_output_plugin->so_filename , errno );
			return -1;
		}
		else
		{
			DEBUGLOG( "[%s]->pfuncLoadOutputPluginConfig ok" , p_logpipe_output_plugin->so_filename );
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
			ERRORLOG( "[%s]->pfuncLoadInputPluginConfig failed , errno[%d]" , p_logpipe_input_plugin->so_filename , errno );
			return -1;
		}
		else
		{
			DEBUGLOG( "[%s]->pfuncLoadInputPluginConfig ok" , p_logpipe_input_plugin->so_filename );
		}
	}
	
	return 0;
}

void UnloadConfig( struct LogpipeEnv *p_env )
{
	struct LogpipeInputPlugin	*p_logpipe_input_plugin = NULL ;
	struct LogpipeOutputPlugin	*p_logpipe_output_plugin = NULL ;
	struct LogpipeInputPlugin	*p_next_logpipe_input_plugin = NULL ;
	struct LogpipeOutputPlugin	*p_next_logpipe_output_plugin = NULL ;
	
	/* 执行所有输入端初始化函数 */
	list_for_each_entry_safe( p_logpipe_input_plugin , p_next_logpipe_input_plugin , & (p_env->logpipe_input_plugins_list.this_node) , struct LogpipeInputPlugin , this_node )
	{
		UnloadInputPluginSession( p_env , p_logpipe_input_plugin );
	}
	
	/* 执行所有输出端初始化函数 */
	list_for_each_entry_safe( p_logpipe_output_plugin , p_next_logpipe_output_plugin , & (p_env->logpipe_output_plugins_list.this_node) , struct LogpipeOutputPlugin , this_node )
	{
		UnloadOutputPluginSession( p_env , p_logpipe_output_plugin );
	}
	
	RemoveAllPluginConfigItems( & (p_env->start_once_for_plugin_config_items) );
	
	return;
}

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

