#include "logpipe_in.h"

char	__LOGPIPE_VERSION_0_2_0[] = "0.2.0" ;
char	*__LOGPIPE_VERSION = __LOGPIPE_VERSION_0_2_0 ;

static void version()
{
	printf( "logpipe v%s build %s %s\n" , __LOGPIPE_VERSION , __DATE__ , __TIME__ );
	return;
}

static void usage()
{
	printf( "USAGE : logpipe -v\n" );
	printf( "        logpipe -i\n" );
	printf( "        logpipe -f (config_file) [ --no-daemon ]\n" );
	return;
}

static int ParseCommandParameter( struct LogPipeEnv *p_env , int argc , char *argv[] )
{
	int		c ;
	
	SetLogLevel( LOGLEVEL_WARN );
	
	for( c = 1 ; c < argc ; c++ )
	{
		if( STRCMP( argv[c] , == , "-v" ) )
		{
			version();
			exit(0);
		}
		else if( STRCMP( argv[c] , == , "-i" ) )
		{
			InitConfig();
			exit(0);
		}
		else if( STRCMP( argv[c] , == , "-f" ) && c + 1 < argc )
		{
			strncpy( p_env->config_path_filename , argv[c+1] , sizeof(p_env->config_path_filename)-1 );
			c++;
		}
		else if( STRCMP( argv[c] , == , "--no-daemon" ) )
		{
			p_env->no_daemon = 1 ;
		}
		else
		{
			printf( "*** ERROR : invalid command parameter '%s'\n" , argv[c] );
			usage();
			exit(1);
		}
	}
	
	return 0;
}

int main( int argc , char *argv[] )
{
	struct LogPipeEnv	*p_env = NULL ;
	
	int			nret = 0 ;
	
	setbuf( stdout , NULL );
	
	if( argc == 1 )
	{
		usage();
		exit(0);
	}
	
	p_env = (struct LogPipeEnv *)malloc( sizeof(struct LogPipeEnv) ) ;
	if( p_env == NULL )
	{
		printf( "*** ERROR : malloc failed , errno[%d]\n" , errno );
		return 1;
	}
	memset( p_env , 0x00 , sizeof(struct LogPipeEnv) );
	
	nret = ParseCommandParameter( p_env , argc , argv ) ;
	if( nret )
		return -nret;
	
	nret = LoadConfig( p_env ) ;
	if( nret )
		return -nret;
	
	nret = InitEnvironment( p_env ) ;
	if( nret )
		return -nret;
	
	if( p_env->no_daemon )
	{
		umask( 0 ) ;
		chdir( "/tmp" );
		nret = monitor( p_env ) ;
	}
	else
	{
		nret = BindDaemonServer( & _monitor , (void*)p_env , 1 ) ;
	}
	
	CleanEnvironment( p_env );
	free( p_env );
	
	return -nret;
}

