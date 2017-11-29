#include "logpipe_in.h"

char	__LOGPIPE_VERSION_0_1_0[] = "0.1.0" ;
char	*__LOGPIPE_VERSION = __LOGPIPE_VERSION_0_1_0 ;

static void version()
{
	printf( "logpipe v%s build %s %s\n" , __LOGPIPE_VERSION , __DATE__ , __TIME__ );
	return;
}

static void usage()
{
	printf( "USAGE : logpipe -v\n" );
	printf( "        logpipe --role C --monitor-path (dir_path) [ --trace-file-space-size (max_file_count) ]\n" );
	printf( "                --role S --listen-ip (ip) --listen-port (port)\n" );
	printf( "                        --log-file (log_file) --log-level (DEBUG|INFO|WARN|ERROR|FATAL)\n" );
	printf( "                        --no-daemon\n" );
	return;
}

static int ParseCommandParameter( struct LogPipeEnv *p_env , int argc , char *argv[] )
{
	int		c ;
	
	int		nret = 0 ;
	
	SetLogLevel( LOGLEVEL_WARN );
	
	for( c = 1 ; c < argc ; c++ )
	{
		if( STRCMP( argv[c] , == , "-v" ) )
		{
			version();
			exit(0);
		}
		else if( STRCMP( argv[c] , == , "--role" ) && c + 1 < argc )
		{
			if( STRCMP(argv[c+1],==,"C") )
			{
				p_env->role = LOGPIPE_ROLE_COLLECTOR ;
			}
			else if( STRCMP(argv[c+1],==,"P") )
			{
				p_env->role = LOGPIPE_ROLE_PIPER ;
			}
			else if( STRCMP(argv[c+1],==,"S") )
			{
				p_env->role = LOGPIPE_ROLE_DUMPSERVER ;
			}
			else
			{
				printf( "*** ERROR : parse invalid value '%s' of command parameter '--role'\n" , argv[c+1] );
				return -1;
			}
			c++;
		}
		else if( STRCMP( argv[c] , == , "--monitor-path" ) && c + 1 < argc )
		{
			strncpy( p_env->role_context.collector.monitor_path , argv[c+1] , sizeof(p_env->role_context.collector.monitor_path)-1 );
			c++;
		}
		else if( STRCMP( argv[c] , == , "--listen-ip" ) && c + 1 < argc )
		{
			strncpy( p_env->role_context.dumpserver.listen_ip , argv[c+1] , sizeof(p_env->role_context.dumpserver.listen_ip)-1 );
			c++;
		}
		else if( STRCMP( argv[c] , == , "--listen-port" ) && c + 1 < argc )
		{
			p_env->role_context.dumpserver.listen_port = atoi(argv[c+1]) ;
			c++;
		}
		else if( STRCMP( argv[c] , == , "--log-file" ) && c + 1 < argc )
		{
			strncpy( p_env->log_pathfilename , argv[c+1] , sizeof(p_env->log_pathfilename)-1 );
			c++;
		}
		else if( STRCMP( argv[c] , == , "--log-level" ) && c + 1 < argc )
		{
			if( STRCMP(argv[c+1],==,"DEBUG") )
			{
				SetLogLevel( LOGLEVEL_DEBUG );
			}
			else if( STRCMP(argv[c+1],==,"INFO") )
			{
				SetLogLevel( LOGLEVEL_INFO );
			}
			else if( STRCMP(argv[c+1],==,"WARN") )
			{
				SetLogLevel( LOGLEVEL_WARN );
			}
			else if( STRCMP(argv[c+1],==,"ERROR") )
			{
				SetLogLevel( LOGLEVEL_ERROR );
			}
			else if( STRCMP(argv[c+1],==,"FATAL") )
			{
				SetLogLevel( LOGLEVEL_FATAL );
			}
			else
			{
				printf( "*** ERROR : parse invalid value '%s' of command parameter '--log-level'\n" , argv[c+1] );
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
			printf( "*** ERROR : invalid command parameter '%s'\n" , argv[c] );
			usage();
			exit(1);
		}
	}
	
	if( p_env->role == LOGPIPE_ROLE_COLLECTOR )
	{
		struct stat	dir_stat ;
		
		if( p_env->role_context.collector.monitor_path[0] == '\0' )
		{
			printf( "*** ERROR : need parameter '--monitor-path' for --role C\n" );
			return -1;
		}
		
		memset( & dir_stat , 0x00 , sizeof(struct stat) );
		nret = stat( p_env->role_context.collector.monitor_path , & dir_stat ) ;
		if( nret == -1 )
		{
			printf( "*** ERROR : path '%s' invalid\n" , p_env->role_context.collector.monitor_path );
			return -1;
		}
		
		if( ! S_ISDIR(dir_stat.st_mode) )
		{
			printf( "*** ERROR : path '%s' isn't a directory\n" , p_env->role_context.collector.monitor_path );
			return -1;
		}
		
		printf( "role : LOGPIPE_ROLE_COLLECTOR\n" );
		printf( "monitor_path : %s\n" , p_env->role_context.collector.monitor_path );
	}
	else if( p_env->role == LOGPIPE_ROLE_DUMPSERVER )
	{
		if( p_env->role_context.dumpserver.listen_ip[0] == '\0' )
		{
			printf( "*** ERROR : need parameter '--listen-ip' for --role S\n" );
			return -1;
		}
		
		if( p_env->role_context.dumpserver.listen_port <= 0 )
		{
			printf( "*** ERROR : need parameter '--listen-port' for --role S\n" );
			return -1;
		}
		
		printf( "role : LOGPIPE_ROLE_DUMPSERVER\n" );
		printf( "listen_ip : %s\n" , p_env->role_context.dumpserver.listen_ip );
		printf( "listen_port : %d\n" , p_env->role_context.dumpserver.listen_port );
	}
	else
	{
		printf( "*** ERROR : role '%c' invalid\n" , p_env->role );
		return -1;
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
	
	nret = InitEnvironment( p_env ) ;
	if( nret )
		return -nret;
	
	if( p_env->no_daemon )
	{
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

