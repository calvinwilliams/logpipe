#include "logpipe_in.h"

/* cmd for testing
ps -ef | grep "logpipe -f" | awk '{if($3==1)print $2}' | xargs kill
*/

char	__LOGPIPE_VERSION_0_5_0[] = "0.5.0" ;
char	*__LOGPIPE_VERSION = __LOGPIPE_VERSION_0_5_0 ;

static void version()
{
	printf( "logpipe v%s build %s %s\n" , __LOGPIPE_VERSION , __DATE__ , __TIME__ );
	return;
}

static void usage()
{
	printf( "USAGE : logpipe -v\n" );
	printf( "        logpipe -i\n" );
	printf( "        logpipe -f (config_file) [ --no-daemon ] [ --public-plugin-config-item-(key) (value) ]\n" );
	return;
}

#define PUBLIC_PLUGIN_CONFIG_ITEM		"--public-plugin-config-item-"

static int ParseCommandParameters( struct LogpipeEnv *p_env , int argc , char *argv[] )
{
	int		c ;
	
	int		nret = 0 ;
	
	for( c = 1 ; c < argc ; c++ )
	{
		if( STRCMP( argv[c] , == , "-v" ) )
		{
			version();
			exit(0);
		}
		else if( STRCMP( argv[c] , == , "-f" ) && c + 1 < argc )
		{
			strncpy( p_env->config_path_filename , argv[c+1] , sizeof(p_env->config_path_filename)-1 );
			c++;
		}
		else if( STRNCMP( argv[c] , == , PUBLIC_PLUGIN_CONFIG_ITEM , sizeof(PUBLIC_PLUGIN_CONFIG_ITEM)-1 ) && c + 1 < argc )
		{
			char	*key = argv[c] + strlen(PUBLIC_PLUGIN_CONFIG_ITEM) ;
			char	*value = argv[c+1] ;
			nret = AddPluginConfigItem( & (p_env->public_plugin_config_items) , key , strlen(key) , value , strlen(value) ) ;
			if( nret )
			{
				ERRORLOG( "AddPluginConfigItem [%s][%s] failed" , key , value );
				return -1;
			}
			c++;
		}
		else if( STRCMP( argv[c] , == , "--no-daemon" ) )
		{
			p_env->no_daemon = 1 ;
		}
		else
		{
			printf( "ERROR : invalid command parameter '%s'\n" , argv[c] );
			usage();
			exit(1);
		}
	}
	
	return 0;
}

int main( int argc , char *argv[] )
{
	struct LogpipeEnv	*p_env = NULL ;
	
	int			nret = 0 ;
	
	setbuf( stdout , NULL );
	
	if( argc == 1 )
	{
		usage();
		exit(0);
	}
	
	SetLogFile( "#" );
	SetLogLevel( LOGLEVEL_DEBUG );
	
	p_env = (struct LogpipeEnv *)malloc( sizeof(struct LogpipeEnv) ) ;
	if( p_env == NULL )
	{
		ERRORLOG( "malloc failed , errno[%d]" , errno )
		return 1;
	}
	memset( p_env , 0x00 , sizeof(struct LogpipeEnv) );
	
	INIT_LIST_HEAD( & (p_env->public_plugin_config_items.this_node) );
	p_env->epoll_fd = -1 ;
	INIT_LIST_HEAD( & (p_env->logpipe_input_plugins_list.this_node) );
	INIT_LIST_HEAD( & (p_env->logpipe_output_plugins_list.this_node) );
	
	nret = ParseCommandParameters( p_env , argc , argv ) ;
	if( nret )
		return -nret;
	
	nret = LoadConfig( p_env ) ;
	if( nret )
		return -nret;
	
	if( list_empty( & (p_env->logpipe_input_plugins_list.this_node) ) )
	{
		ERRORLOG( "no inputs" )
		return 1;
	}
	if( list_empty( & (p_env->logpipe_output_plugins_list.this_node) ) )
	{
		ERRORLOG( "no outputs" )
		return 1;
	}
	
	if( p_env->no_daemon )
	{
		umask( 0 ) ;
		chdir( "/tmp" );
		nret = _monitor( (void*)p_env ) ;
	}
	else
	{
		nret = BindDaemonServer( & _monitor , (void*)p_env , 1 ) ;
	}
	
	UnloadConfig( p_env );
	
	free( p_env );
	
	return -nret;
}

