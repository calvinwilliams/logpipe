#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "logpipe_api.h"

int main( int argc , char *argv[] )
{
	char		*filename = NULL ;
	int		count ;
	char		*ptr = NULL ;
	int		i ;
	int		fd ;
	
	if( argc != 1 + 2 )
	{
		printf( "USAGE : press_logpipe filename count\n" );
		exit(7);
	}
	
	filename = argv[1] ;
	count = atoi(argv[2]) ;
	
	ptr = (char*)malloc( LOGPIPE_BLOCK_BUFSIZE+1 ) ;
	if( ptr == NULL )
	{
		printf( "malloc failed , errno[%d]\n" , errno );
		return 1;
	}
	memset( ptr , 'X' , LOGPIPE_BLOCK_BUFSIZE );
	ptr[LOGPIPE_BLOCK_BUFSIZE] = '\0' ;
	
	if( count == -1 )
		count = LOGPIPE_BLOCK_BUFSIZE ;
	for( i = 1 ; i <= count ; i++ )
	{
		fd = open( filename , O_CREAT|O_APPEND|O_WRONLY , 00777 ) ;
		if( fd == -1 )
		{
			printf( "can't open file[%s]\n" , filename );
		}
		
		write( fd , ptr , i );
		write( fd , "\n" , 1 );
		
		close( fd );
	}
	
	free( ptr );
	
	printf( "well done\n" );
	
	return 0;
}

