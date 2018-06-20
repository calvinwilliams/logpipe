#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main( int argc , char *argv[] )
{
	char		*filename = NULL ;
	int		i , count ;
	time_t		tt ;
	struct tm	tm ;
	struct timeval	tv ;
	char		buf[ 1024 + 1 ] ;
	int		len ;
	int		fd ;
	char		cbuf[] = "一二三四五六七八九" ;
	
	if( argc != 1 + 2 )
	{
		printf( "USAGE : press_logpipe filename count\n" );
		exit(7);
	}
	
	filename = argv[1] ;
	count = atoi(argv[2]) ;
	
	for( i = 1 ; i <= count ; i++ )
	{
		tt = time( NULL ) ;
		memset( & tm , 0x00 , sizeof(struct tm) );
		localtime_r( & tt , & tm );
		memset( buf , 0x00 , sizeof(buf) );
		len = strftime( buf , sizeof(buf) , "%Y-%m-%d %H:%M:%S." , & tm ) ;
		memset( & tv , 0x00 , sizeof(struct timeval) );
		gettimeofday( & tv , NULL );
		len += snprintf( buf+len , sizeof(buf)-len , "%ld | %s:%d | %d | %s\n" , tv.tv_usec , __FILE__ , __LINE__ , i , cbuf+((i-1)%10)*2 ) ;
		
		fd = open( filename , O_CREAT|O_APPEND|O_WRONLY , 00777 ) ;
		if( fd == -1 )
		{
			printf( "can't open file[%s]\n" , filename );
		}
		
		write( fd , buf , len );
		
		close( fd );
	}
	
	printf( "well done\n" );
	
	return 0;
}

