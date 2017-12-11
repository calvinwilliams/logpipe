#include "logpipe_api.h"

struct LogpipeInputPlugin_tcp
{
	char		*ip = NULL ;
	int		port ;
} ;

funcInitLogpipeInputPlugin InitLogpipeInputPlugin ;
int InitLogpipeInputPlugin( struct LogpipeEnv *p_env , struct LogpipePluginConfigItem *p_plugin_config_items , void **pp_context , int *p_fd )
{
	struct LogpipeInputPlugin_tcp	*p_plugin_env = NULL ;
	char				*p = NULL ;
	
	p_plugin_env = (struct LogpipeInputPlugin_tcp *)malloc( sizeof(struct LogpipeInputPlugin_tcp) ) ;
	if( p_plugin_env == NULL )
	{
		printf( "ERROR : malloc failed , errno[%d]\n" , errno );
		return -1;
	}
	memset( p_plugin_env , 0x00 , sizeof(struct LogpipeInputPlugin_tcp) );
	
	p_plugin_env->ip = QueryPluginConfigItem( p_plugin_config_items , "ip" ) ;
	
	p = QueryPluginConfigItem( p_plugin_config_items , "port" ) ;
	if( p )
		p_plugin_env->port = atoi(p) ;
	else
		p_plugin_env->port = 0 ;
	
	(*pp_context) = p_plugin_env ;
	
	return 0;
}

funcOnLogpipeInputEvent OnLogpipeInputEvent ;
int OnLogpipeInputEvent( struct LogpipeEnv *p_env , void *p_context )
{
}

funcBeforeReadLogpipeInput BeforeReadLogpipeInput ;
int BeforeReadLogpipeInput( struct LogpipeEnv *p_env , void *p_context )
{
}

funcReadLogpipeInput ReadLogpipeInput ;
int ReadLogpipeInput( struct LogpipeEnv *p_env , void *p_context , uint32_t *p_block_len , char *block_buf , int block_bufsize )
{
}

func AfterReadLogpipeInput AfterReadLogpipeInput ;
int AfterReadLogpipeInput( struct LogpipeEnv *p_env , void *p_context )
{
}

funcCleanLogpipeInputPlugin CleanLogpipeInputPlugin ;
int CleanLogpipeInputPlugin( struct LogpipeEnv *p_env , void *p_context )
{
	struct LogpipeInputPlugin_tcp	*p_plugin_env = (struct LogpipeInputPlugin_tcp *)p_context ;
	
	INFOLOG( "free p_plugin_env" )
	free( p_plugin_env );
	
	return 0;
}

