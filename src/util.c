/*
 * logpipe - Distribute log collector
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "logpipe_in.h"

/* 写数据覆盖到文件 */
int WriteEntireFile( char *pathfilename , char *file_content , int file_len )
{
	FILE		*fp = NULL ;
	size_t		size ;
	
	if( file_len == -1 )
		file_len = strlen(file_content) ;
	
	fp = fopen( pathfilename , "wb" ) ;
	if( fp == NULL )
	{
		return -1;
	}
	
	size = fwrite( file_content , 1 , file_len , fp ) ;
	if( size != file_len )
	{
		fclose( fp );
		return -2;
	}
	
	fclose( fp );
	
	return 0;
}

/* 读取文件，内容存放到动态申请内存块中 */
/* 注意：使用完后，应用负责释放内存 */
char *StrdupEntireFile( char *pathfilename , int *p_file_len )
{
	char		*file_content = NULL ;
	int		file_len ;
	FILE		*fp = NULL ;
	
	int		nret = 0 ;
	
	fp = fopen( pathfilename , "rb" ) ;
	if( fp == NULL )
	{
		return NULL;
	}
	
	fseek( fp , 0 , SEEK_END );
	file_len  = ftell( fp ) ;
	fseek( fp , 0 , SEEK_SET );
	
	file_content = (char*)malloc( file_len+1 ) ;
	if( file_content == NULL )
	{
		fclose( fp );
		return NULL;
	}
	memset( file_content , 0x00 , file_len+1 );
	
	nret = fread( file_content , 1 , file_len , fp ) ;
	if( nret != file_len )
	{
		fclose( fp );
		free( file_content );
		return NULL;
	}
	
	fclose( fp );
	
	if( p_file_len )
		(*p_file_len) = file_len ;
	return file_content;
}

/* 转换为守护进程 */
int BindDaemonServer( int (* ServerMain)( void *pv ) , void *pv , int close_flag )
{
	int	pid;
	
	pid = fork() ;
	switch( pid )
	{
		case -1:
			return -1;
		case 0:
			break;
		default		:
			return 0;
	}
	
	setsid();
	
	pid = fork() ;
	switch( pid )
	{
		case -1:
			return -2;
		case 0:
			break ;
		default:
			return 0;
	}
	
	if( close_flag )
	{
		close(0);
		close(1);
		close(2);
	}
	
	umask( 0 ) ;
	
	chdir( "/tmp" );
	
	ServerMain( pv );
	
	return 0;
}

/* 新增插件配置项 */
int AddPluginConfigItem( struct LogpipePluginConfigItem *config , char *key , int key_len , char *value , int value_len )
{
	struct LogpipePluginConfigItem	*item = NULL ;
	
	item = (struct LogpipePluginConfigItem *)malloc( sizeof(struct LogpipePluginConfigItem) ) ;
	if( item == NULL )
		return -1;
	memset( item , 0x00 , sizeof(struct LogpipePluginConfigItem) );
	
	item->key = strndup( key , key_len ) ;
	if( item->key == NULL )
	{
		free( item );
		return -12;
	}
	
	item->value = strndup( value , value_len ) ;
	if( item->value == NULL )
	{
		free( item->key );
		free( item );
		return -13;
	}
	
	list_add_tail( & (item->this_node) , & (config->this_node) );
	
	return 0;
}

/* 查询插件配置项 */
char *QueryPluginConfigItem( struct LogpipePluginConfigItem *config , char *key_format , ... )
{
	va_list		valist ;
	char		key[ 256 + 1 ] ;
	
	va_start( valist , key_format );
	memset( key , 0x00 , sizeof(key) );
	vsnprintf( key , sizeof(key)-1 , key_format , valist );
	va_end( valist );
	
	struct LogpipePluginConfigItem	*item = NULL ;
	
	list_for_each_entry( item , & (config->this_node) , struct LogpipePluginConfigItem , this_node )
	{
		if( STRCMP( item->key , == , key ) )
			return item->value;
	}
	
	return NULL;
}

/* 删除所有插件配置项 */
void RemoveAllPluginConfigItems( struct LogpipePluginConfigItem *config )
{
	struct LogpipePluginConfigItem	*item = NULL ;
	struct LogpipePluginConfigItem	*next_item = NULL ;
	
	list_for_each_entry_safe( item , next_item , & (config->this_node) , struct LogpipePluginConfigItem , this_node )
	{
		list_del( & (item->this_node) );
		free( item->key );
		free( item->value );
		free( item );
	}
	
	return;
}

/* 写定长字节块到描述字 */
ssize_t writen(int fd, const void *vptr, size_t n)
{
    size_t nleft;
    ssize_t nwriten;
    const char *ptr;
    ptr = vptr;
    nleft = n;
    
    while(nleft > 0)
    {
        if((nwriten = write(fd, ptr,nleft)) <= 0)
        {
            if(nwriten < 0 && errno == EINTR)
                nwriten = 0;
            else
                return -1;
        }
        
        nleft -= nwriten;
        ptr += nwriten;
    }
    
    return n;
}

ssize_t writev3( int fd , struct iovec **pp_iov, int *p_iovcnt , int iov_total_len , int *p_timeout , int *p_elapse )
{
	struct timeval		timestamp1 ;
	struct timeval		timestamp2 ;
	struct timeval		timestamp3 ;
	struct timeval		diff_time ;
	struct pollfd		fds ;
	ssize_t			len ;
	ssize_t			total_len ;
	
	int			nret = 0 ;
	
	if( p_elapse )
		(*p_elapse) = 0 ;
	
	total_len = 0 ;
	while( (*p_iovcnt) > 0 )
	{
		if( p_timeout )
			if( (*p_timeout) < 0 )
				(*p_timeout) = 0 ;
		
		gettimeofday( & timestamp1 , NULL );
		
		fds.fd = fd ;
		fds.events = POLLOUT ;
		fds.revents = 0 ;
		if( p_timeout )
			nret = poll( & fds , 1 , (*p_timeout)/1000 ) ;
		else
			nret = poll( & fds , 1 , -1 ) ;
		if( nret == -1 )
		{
			return -2;
		}
		else if( nret == 0 )
		{
			return total_len;
		}
		
		gettimeofday( & timestamp2 , NULL );
		DIFF_TIMEVAL( diff_time , timestamp1 , timestamp2 )
		if( p_timeout )
			(*p_timeout) -= diff_time.tv_sec*1000000 + diff_time.tv_usec ;
		if( p_elapse )
			(*p_elapse) += diff_time.tv_sec*1000000 + diff_time.tv_usec ;
		
		len = writev( fd , *pp_iov , *p_iovcnt ) ;
		if( len == -1 )
		{
			if( errno == EAGAIN || errno == EWOULDBLOCK )
				continue;
			
			return -1;
		}
		total_len += len ;
		
		if( iov_total_len >= 0 && total_len == iov_total_len )
		{
			len = 0 ;
			(*pp_iov) = NULL ;
			(*p_iovcnt) = 0 ;
		}
		else
		{
			while( len > 0 )
			{
				if( len >= (*pp_iov)->iov_len )
				{
					len -= (*pp_iov)->iov_len ;
					(*pp_iov)++;
					(*p_iovcnt)--;
					if( (*p_iovcnt) == 0 )
						(*pp_iov) = NULL ;
				}
				else
				{
					(*pp_iov)->iov_base += len ;
					(*pp_iov)->iov_len -= len ;
					len = 0 ;
				}
			}
		}
		
		gettimeofday( & timestamp3 , NULL );
		DIFF_TIMEVAL( diff_time , timestamp2 , timestamp3 )
		if( p_timeout )
			(*p_timeout) -= diff_time.tv_sec*1000000 + diff_time.tv_usec ;
		if( p_elapse )
			(*p_elapse) += diff_time.tv_sec*1000000 + diff_time.tv_usec ;
	}
	
	return total_len;
}

/* 从描述字读取定长字节块 */
ssize_t readn(int fd, void *vptr, size_t n)
{
    size_t nleft;
    ssize_t nread;
    char *ptr;
    ptr = vptr;
    nleft = n;
    
    while(nleft > 0)
    {
        if((nread = read(fd, ptr, nleft)) < 0)
        {
            if(errno == EINTR)
                nread = 0;
            else 
                return -1;
        }
        else if(nread == 0)
        {
            break;
    	}
        
        nleft -= nread;
        ptr += nread;
    }
    
    return (n-nleft);
}

/* 字符串展开 */
static struct timeval *GetTimeval( struct timeval *p_tv , struct timeval *tv )
{
	if( p_tv )
		return NULL;
	
	gettimeofday( tv , NULL );
	return tv;
}

static struct tm *GetTm( struct tm *p_stime , struct timeval *p_tv , struct tm *stime )
{
	if( p_stime )
		return p_stime;
	
	return localtime_r( & (p_tv->tv_sec) , stime );
}

int ExpandStringBuffer( char *base , int buf_size )
{
	struct timeval	tv , *p_tv = NULL ;
	struct tm	stime , *p_stime = NULL ;
	int		str_len ;
	char		*p1 = NULL , *p2 = NULL ;
	
	str_len = strlen(base) ;
	p1 = base ;
	while(*p1)
	{
		if( (*p1) == '%' )
		{
			p2 = p1 + 1 ;
			if( (*p2) == 'Y' )
			{
				char	buf[ 4 + 1 ] ;
				p_tv = GetTimeval( p_tv , & tv ) ;
				p_stime = GetTm( p_stime , p_tv , & stime ) ;
				snprintf( buf , sizeof(buf) , "%04d" , p_stime->tm_year+1900 );
				if( str_len + 4 > buf_size-1 )
					return -1;
				memmove( p2+1+2 , p2+1 , strlen(p2+1)+1 );
				memcpy( p1 , buf , 4 );
				p1 = p2 + 1 + 2 ;
				str_len += 2 ;
			}
			else if( (*p2) == 'M' )
			{
				char	buf[ 2 + 1 ] ;
				p_tv = GetTimeval( p_tv , & tv ) ;
				p_stime = GetTm( p_stime , p_tv , & stime ) ;
				snprintf( buf , sizeof(buf) , "%02d" , p_stime->tm_mon );
				memcpy( p1 , buf , 2 );
				p1 = p2 + 1 ;
			}
			else if( (*p2) == 'D' )
			{
				char	buf[ 2 + 1 ] ;
				p_tv = GetTimeval( p_tv , & tv ) ;
				p_stime = GetTm( p_stime , p_tv , & stime ) ;
				snprintf( buf , sizeof(buf) , "%02d" , p_stime->tm_mday );
				memcpy( p1 , buf , 2 );
				p1 = p2 + 1 ;
			}
			else if( (*p2) == 'h' )
			{
				char	buf[ 2 + 1 ] ;
				p_tv = GetTimeval( p_tv , & tv ) ;
				p_stime = GetTm( p_stime , p_tv , & stime ) ;
				snprintf( buf , sizeof(buf) , "%02d" , p_stime->tm_hour );
				memcpy( p1 , buf , 2 );
				p1 = p2 + 1 ;
			}
			else if( (*p2) == 'm' )
			{
				char	buf[ 2 + 1 ] ;
				p_tv = GetTimeval( p_tv , & tv ) ;
				p_stime = GetTm( p_stime , p_tv , & stime ) ;
				snprintf( buf , sizeof(buf) , "%02d" , p_stime->tm_min );
				memcpy( p1 , buf , 2 );
				p1 = p2 + 1 ;
			}
			else if( (*p2) == 's' )
			{
				char	buf[ 2 + 1 ] ;
				p_tv = GetTimeval( p_tv , & tv ) ;
				p_stime = GetTm( p_stime , p_tv , & stime ) ;
				snprintf( buf , sizeof(buf) , "%02d" , p_stime->tm_sec );
				memcpy( p1 , buf , 2 );
				p1 = p2 + 1 ;
			}
		}
		else
		{
			p1++;
		}
	}
	
	return 0;
}

/* 字符编码转换 */
#define MAXLEN_XMLCONTENT	100*1024

char *ConvertContentEncodingEx( char *encFrom , char *encTo , char *inptr , int *inptrlen , char *outptr , int *outptrlen )
{               
        iconv_t         ic ;
        
        int             ori_outptrlen = 0 ;
                
        static char     outbuf[ MAXLEN_XMLCONTENT + 1 ];
        size_t          outbuflen ;

        char            *pin = NULL ;
        size_t          inlen ;
        char            *pout = NULL ;
        size_t          *poutlen = NULL ;

        int             nret ;

        ic = iconv_open( encTo , encFrom ) ;
        if( ic == (iconv_t)-1 )
        {
                 return NULL;
        }
        nret = iconv( ic , NULL , NULL , NULL , NULL ) ;

        pin = inptr ;
        if( inptrlen )
        {
                inlen = (*inptrlen) ;
        }
        else 
        {
                inlen = strlen((char*)inptr) ;
        }
        if( outptr )
        {
                memset( outptr , 0x00 , (*outptrlen) );
                if( inptr == NULL )
                        return outptr; 

                pout = outptr ;
                poutlen = (size_t*)outptrlen ;

                ori_outptrlen = (*outptrlen) ;
        }
        else
        {
                memset( outbuf , 0x00 , sizeof(outbuf) );
                outbuflen = MAXLEN_XMLCONTENT ;
                if( inptr == NULL )
                        return outbuf;
                
                pout = outbuf ;
                poutlen = & outbuflen ;
        }

        nret = iconv( ic , (char **) & pin , & inlen , (char **) & pout , poutlen );
        iconv_close( ic );
        if( nret == -1 || inlen > 0 )
                return NULL;

        if( outptr )
        {
                (*outptrlen) = ori_outptrlen - (*poutlen) ;
                return pout - (*outptrlen) ;
        }
        else
        {
                return outbuf;
        }
}

char *ConvertContentEncoding( char *encFrom , char *encTo , char *inptr )
{
        return ConvertContentEncodingEx( encFrom , encTo , inptr , NULL , NULL , NULL );
}

/* 大小字符串按单位转换为数字 */
uint64_t size64_atou64( char *str )
{
	char	*endptr = NULL ;
	double	value ;
	
	value = strtod( str , & endptr ) ;
	if( ( value == HUGE_VALF || value == HUGE_VALL ) && errno == ERANGE )
		return -1;
	
	if( STRICMP( endptr , == , "gb" ) )
		return (uint64_t)(value*1024*1024*1024);
	else if( STRICMP( endptr , == , "mb" ) )
		return (uint64_t)(value*1024*1024);
	else if( STRICMP( endptr , == , "kb" ) )
		return (uint64_t)(value*1024);
	else if( STRICMP( endptr , == , "b" ) )
		return (uint64_t)value;
	else if( endptr[0] == 0 )
		return (uint64_t)value;
	else
		return UINT64_MAX;
}

/* 微秒字符串按单位转换为数字 */
uint64_t usleep_atou64( char *str )
{
	char	*endptr = NULL ;
	double	value ;
	
	value = strtod( str , & endptr ) ;
	if( ( value == HUGE_VALF || value == HUGE_VALL ) && errno == ERANGE )
		return -1;
	
	if( STRICMP( endptr , == , "s" ) )
		return (uint64_t)(value*1000000);
	else if( STRICMP( endptr , == , "ms" ) )
		return (uint64_t)(value*1000);
	else if( STRICMP( endptr , == , "us" ) )
		return (uint64_t)value;
	else if( endptr[0] == 0 )
		return (uint64_t)value;
	else
		return UINT64_MAX;
}

/* 计算两个秒戳结构之间的微秒差 */
void DiffTimeval( struct timeval *p_tv1 , struct timeval *p_tv2 , struct timeval *p_diff )
{
	p_diff->tv_sec = p_tv2->tv_sec - p_tv1->tv_sec ;
	p_diff->tv_usec = p_tv2->tv_usec - p_tv1->tv_usec ;
	
	while( p_diff->tv_usec < 0 )
	{
		p_diff->tv_usec += 1000000 ;
		p_diff->tv_sec--;
	}
	
	return;
}

struct SplitLineBuffer *AllocSplitLineCache()
{
	struct SplitLineBuffer	*split_line_buf = NULL ;
	
	split_line_buf = (struct SplitLineBuffer*)malloc( sizeof(struct SplitLineBuffer) ) ;
	if( split_line_buf == NULL )
		return NULL;
	memset( split_line_buf , 0x00 , sizeof(struct SplitLineBuffer) );
	
	return split_line_buf;
}

char *GetSplitLineBufferPtr( struct SplitLineBuffer *split_line_buf , uint64_t *p_split_line_buflen )
{
	if( p_split_line_buflen )
		(*p_split_line_buflen) = split_line_buf->split_line_buflen ;
	return split_line_buf->split_line_buffer;
}

uint64_t GetSplitLineBufferLength( struct SplitLineBuffer *split_line_buf )
{
	return split_line_buf->split_line_buflen;
}

int FetchSplitLineBuffer( struct SplitLineBuffer *split_line_buf , uint64_t *p_block_len , char *block_buf )
{
	char		*p_newline = NULL ;
	uint64_t	line_len ;
	
	if( (*p_block_len) )
	{
		/* 如果遗留数据+当前数据块能塞进拆分缓冲区 */
		if( split_line_buf->split_line_buflen + *(p_block_len) <= sizeof(split_line_buf->split_line_buffer)-1 )
		{
			memcpy( split_line_buf->split_line_buffer+split_line_buf->split_line_buflen , block_buf , *(p_block_len) );
			split_line_buf->split_line_buflen += *(p_block_len) ;
		}
		else
		{
			char		buf[ LOGPIPE_OUTPUT_BUFSIZE + 1 ] ;
			uint64_t	len ;
			
			/* 强制把缓存数据都处理掉 */
			memcpy( buf , block_buf , *(p_block_len) );
			len = *(p_block_len) ;
			
			memcpy( block_buf , split_line_buf->split_line_buffer , split_line_buf->split_line_buflen );
			*(p_block_len) = split_line_buf->split_line_buflen ;
			
			memcpy( split_line_buf->split_line_buffer , buf , len );
			split_line_buf->split_line_buflen = len ;
			
			return 0;
		}
	}
	
	/* 处理掉解析缓冲区中的一行有效行 */
	p_newline = memchr( split_line_buf->split_line_buffer , '\n' , split_line_buf->split_line_buflen ) ;
	if( p_newline == NULL )
		return LOGPIPE_NO_LINE;
	
	(*p_newline) = '\0' ;
	
	line_len = p_newline - split_line_buf->split_line_buffer ;
	memcpy( block_buf , split_line_buf->split_line_buffer , line_len );
	*(p_block_len) = line_len ;
	
	split_line_buf->split_line_buflen -= ( line_len + 1 ) ;
	memmove( split_line_buf->split_line_buffer , p_newline+1 , split_line_buf->split_line_buflen );
	
	return 0;
}

void FreeSplitLineBuffer( struct SplitLineBuffer *split_line_buf )
{
	if( split_line_buf )
	{
		free( split_line_buf );
	}
	
	return;
}
