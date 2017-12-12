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
		ERRORLOG( "dlopen[%s] failed , errno[%d]" , p_logpipe_input_plugin->so_path_filename , errno );
		return -1;
	}
	
	p_logpipe_input_plugin->pfuncInitLogpipeInputPlugin = (funcInitLogpipeInputPlugin *)dlsym( p_logpipe_input_plugin->so_handler , "InitLogpipeInputPlugin" ) ;
	if( p_logpipe_input_plugin->pfuncInitLogpipeInputPlugin == NULL )
	{
		ERRORLOG( "dlsym[%s][InitLogpipeInputPlugin] failed , errno[%d]" , p_logpipe_input_plugin->so_path_filename , errno );
		return -1;
	}
	
	p_logpipe_input_plugin->pfuncBeforeReadLogpipeInput = (funcBeforeReadLogpipeInput *)dlsym( p_logpipe_input_plugin->so_handler , "BeforeReadLogpipeInput" ) ;
	if( p_logpipe_input_plugin->pfuncBeforeReadLogpipeInput == NULL )
	{
		ERRORLOG( "dlsym[%s][BeforeReadLogpipeInput] failed , errno[%d]" , p_logpipe_input_plugin->so_path_filename , errno );
		return -1;
	}
	
	p_logpipe_input_plugin->pfuncOnLogpipeInputEvent = (funcOnLogpipeInputEvent *)dlsym( p_logpipe_input_plugin->so_handler , "OnLogpipeInputEvent" ) ;
	if( p_logpipe_input_plugin->pfuncOnLogpipeInputEvent == NULL )
	{
		ERRORLOG( "dlsym[%s][OnLogpipeInputEvent] failed , errno[%d]" , p_logpipe_input_plugin->so_path_filename , errno );
		return -1;
	}
	
	p_logpipe_input_plugin->pfuncReadLogpipeInput = (funcReadLogpipeInput *)dlsym( p_logpipe_input_plugin->so_handler , "ReadLogpipeInput" ) ;
	if( p_logpipe_input_plugin->pfuncReadLogpipeInput == NULL )
	{
		ERRORLOG( "dlsym[%s][ReadLogpipeInput] failed , errno[%d]" , p_logpipe_input_plugin->so_path_filename , errno );
		return -1;
	}
	
	p_logpipe_input_plugin->pfuncAfterReadLogpipeInput = (funcAfterReadLogpipeInput *)dlsym( p_logpipe_input_plugin->so_handler , "AfterReadLogpipeInput" ) ;
	if( p_logpipe_input_plugin->pfuncAfterReadLogpipeInput == NULL )
	{
		ERRORLOG( "dlsym[%s][AfterReadLogpipeInput] failed , errno[%d]" , p_logpipe_input_plugin->so_path_filename , errno );
		return -1;
	}
	
	p_logpipe_input_plugin->pfuncCleanLogpipeInputPlugin = (funcCleanLogpipeInputPlugin *)dlsym( p_logpipe_input_plugin->so_handler , "CleanLogpipeInputPlugin" ) ;
	if( p_logpipe_input_plugin->pfuncCleanLogpipeInputPlugin == NULL )
	{
		ERRORLOG( "dlsym[%s][CleanLogpipeInputPlugin] failed , errno[%d]" , p_logpipe_input_plugin->so_path_filename , errno );
		return -1;
	}
	
	list_add_tail( & (p_logpipe_input_plugin->this_node) , & (p_env->logpipe_inputs_plugin_list.this_node) );
	
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
		ERRORLOG( "dlopen[%s] failed , errno[%d]" , p_logpipe_output_plugin->so_path_filename , errno );
		return -1;
	}
	
	p_logpipe_output_plugin->pfuncInitLogpipeOutputPlugin = (funcInitLogpipeOutputPlugin *)dlsym( p_logpipe_output_plugin->so_handler , "InitLogpipeOutputPlugin" ) ;
	if( p_logpipe_output_plugin->pfuncInitLogpipeOutputPlugin == NULL )
	{
		ERRORLOG( "dlsym[%s][InitLogpipeOutputPlugin] failed , errno[%d]" , p_logpipe_output_plugin->so_path_filename , errno );
		return -1;
	}
	
	p_logpipe_output_plugin->pfuncBeforeWriteLogpipeOutput = (funcBeforeWriteLogpipeOutput *)dlsym( p_logpipe_output_plugin->so_handler , "BeforeWriteLogpipeOutput" ) ;
	if( p_logpipe_output_plugin->pfuncBeforeWriteLogpipeOutput == NULL )
	{
		ERRORLOG( "dlsym[%s][BeforeWriteLogpipeOutput] failed , errno[%d]" , p_logpipe_output_plugin->so_path_filename , errno );
		return -1;
	}
	
	p_logpipe_output_plugin->pfuncWriteLogpipeOutput = (funcWriteLogpipeOutput *)dlsym( p_logpipe_output_plugin->so_handler , "WriteLogpipeOutput" ) ;
	if( p_logpipe_output_plugin->pfuncWriteLogpipeOutput == NULL )
	{
		ERRORLOG( "dlsym[%s][WriteLogpipeOutput] failed , errno[%d]" , p_logpipe_output_plugin->so_path_filename , errno );
		return -1;
	}
	
	p_logpipe_output_plugin->pfuncAfterWriteLogpipeOutput = (funcAfterWriteLogpipeOutput *)dlsym( p_logpipe_output_plugin->so_handler , "AfterWriteLogpipeOutput" ) ;
	if( p_logpipe_output_plugin->pfuncAfterWriteLogpipeOutput == NULL )
	{
		ERRORLOG( "dlsym[%s][AfterWriteLogpipeOutput] failed , errno[%d]" , p_logpipe_output_plugin->so_path_filename , errno );
		return -1;
	}
	
	p_logpipe_output_plugin->pfuncCleanLogpipeOutputPlugin = (funcCleanLogpipeOutputPlugin *)dlsym( p_logpipe_output_plugin->so_handler , "CleanLogpipeOutputPlugin" ) ;
	if( p_logpipe_output_plugin->pfuncCleanLogpipeOutputPlugin == NULL )
	{
		ERRORLOG( "dlsym[%s][CleanLogpipeOutputPlugin] failed , errno[%d]" , p_logpipe_output_plugin->so_path_filename , errno );
		return -1;
	}
	
	list_add_tail( & (p_logpipe_output_plugin->this_node) , & (p_env->logpipe_outputs_plugin_list.this_node) );
	
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
			
			INIT_LIST_HEAD( & (p_logpipe_input_plugin->plugin_config_items.this_node) );
		}
		else if( jpath_len == sizeof(LOGPIPE_CONFIG_OUTPUTS)-1 && STRNCMP( jpath , == , LOGPIPE_CONFIG_OUTPUTS , jpath_len ) )
		{
			p_logpipe_output_plugin = (struct LogpipeOutputPlugin *)malloc( sizeof(struct LogpipeOutputPlugin) ) ;
			if( p_logpipe_output_plugin == NULL )
				return -1;
			memset( p_logpipe_output_plugin , 0x00 , sizeof(struct LogpipeOutputPlugin) );
			
			INIT_LIST_HEAD( & (p_logpipe_output_plugin->plugin_config_items.this_node) );
		}
	}
	else if( (type&FASTERJSON_NODE_LEAVE) && (type&FASTERJSON_NODE_BRANCH) )
	{
		if( jpath_len == sizeof(LOGPIPE_CONFIG_INPUTS_)-1 && STRNCMP( jpath , == , LOGPIPE_CONFIG_INPUTS_ , jpath_len ) )
		{
			nret = LoadLogpipeInputPlugin( p_env , p_logpipe_input_plugin ) ;
			if( nret )
				return nret;
		}
		else if( jpath_len == sizeof(LOGPIPE_CONFIG_OUTPUTS_)-1 && STRNCMP( jpath , == , LOGPIPE_CONFIG_OUTPUTS_ , jpath_len ) )
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
		else if( jpath_len == sizeof(LOGPIPE_CONFIG_INPUTS_)-1 && STRNCMP( jpath , == , LOGPIPE_CONFIG_INPUTS_ , jpath_len ) )
		{
			nret = AddPluginConfigItem( & (p_logpipe_input_plugin->plugin_config_items) , node , node_len , content , content_len ) ;
			if( nret )
			{
				ERRORLOG( "AddPluginConfigItem [%.*s][%.*s] failed" , node_len , node , content_len , content );
				return -1;
			}
		}
		else if( jpath_len == sizeof(LOGPIPE_CONFIG_OUTPUTS_)-1 && STRNCMP( jpath , == , LOGPIPE_CONFIG_OUTPUTS_ , jpath_len ) )
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
	char		*file_content = NULL ;
	
	char		jpath[ 1024 + 1 ] ;
	
	int		nret = 0 ;
	
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
		ERRORLOG( "parse config[%s] failed" , p_env->config_path_filename );
		free( file_content );
		return -1;
	}
	
	free( file_content );
	
	return 0;
}

