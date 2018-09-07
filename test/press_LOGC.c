#include "LOGC.h"

static void usage()
{
	printf( "USAGE : press_LOGC log_pathfilename count\n" );
	return;
}

int main( int argc , char *argv[] )
{
	char		*log_pathfilename = NULL ;
	int		count ;
	int		i ;
	
	if( argc == 1 )
	{
		usage();
		return 0;
	}
	else if( argc != 1 + 2 )
	{
		usage();
		return 7;
	}
	
	log_pathfilename = argv[1] ;
	count = atoi(argv[2]) ;
	
	SetLogcFile( log_pathfilename );
	SetLogcLevel( LOGCLEVEL_DEBUG );
	
	for( i = 0 ; i < count ; i++ )
	{
		DEBUGLOGC( "debug" )
	}
	
	return 0;
}

