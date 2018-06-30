#include "logpipe_in.h"

#include "zlib.h"

/* 压缩输入插件数据块 */
int CompressInputPluginData( char *compress_algorithm , char *block_in_buf , uint64_t block_in_len , char *block_out_buf , uint64_t *p_block_out_len )
{
	int		nret = 0 ;
	
	if( STRCMP( compress_algorithm , == , "deflate" ) )
	{
		z_stream		deflate_strm ;
		
		memset( & deflate_strm , 0x00 , sizeof(z_stream) );
		nret = deflateInit( & deflate_strm , Z_DEFAULT_COMPRESSION ) ;
		if( nret != Z_OK )
		{
			FATALLOGC( "deflateInit failed[%d]" , nret )
			return -1;
		}
		
		deflate_strm.avail_in = block_in_len ;
		deflate_strm.next_in = (Bytef*)block_in_buf ;
		deflate_strm.avail_out = LOGPIPE_BLOCK_BUFSIZE ;
		deflate_strm.next_out = (Bytef*)block_out_buf ;
		nret = deflate( & deflate_strm , Z_FINISH ) ;
		if( nret == Z_STREAM_ERROR )
		{
			FATALLOGC( "deflate return Z_STREAM_ERROR" )
			deflateEnd( & deflate_strm );
			return -1;
		}
		if( deflate_strm.avail_out == 0 )
		{
			FATALLOGC( "deflate remain data [%d]bytes" , deflate_strm.avail_out )
			deflateEnd( & deflate_strm );
			return -1;
		}
		(*p_block_out_len) = LOGPIPE_BLOCK_BUFSIZE-1 - deflate_strm.avail_out ;
		
		deflateEnd( & deflate_strm );
	}
	else
	{
		ERRORLOGC( "compress_algorithm[%s] invalid" , compress_algorithm )
		return -1;
	}
	
	return 0;
}

/* 解压输出插件数据块 */
int UncompressInputPluginData( char *uncompress_algorithm , char *block_in_buf , uint64_t block_in_len , char *block_out_buf , uint64_t *p_block_out_len )
{
	int		nret = 0 ;
	
	if( STRCMP( uncompress_algorithm , == , "deflate" ) )
	{
		z_stream		inflate_strm ;
		
		memset( & inflate_strm , 0x00 , sizeof(z_stream) );
		nret = inflateInit( & inflate_strm ) ;
		if( nret != Z_OK )
		{
			FATALLOGC( "inflateInit failed[%d]" , nret )
			return -1;
		}
		
		inflate_strm.avail_in = block_in_len ;
		inflate_strm.next_in = (Bytef*)block_in_buf ;
		inflate_strm.avail_out = LOGPIPE_BLOCK_BUFSIZE ;
		inflate_strm.next_out = (Bytef*)block_out_buf ;
		nret = inflate( & inflate_strm , Z_NO_FLUSH ) ;
		if( nret == Z_STREAM_ERROR )
		{
			FATALLOGC( "inflate return Z_STREAM_ERROR" )
			inflateEnd( & inflate_strm );
			return 1;
		}
		else if( nret == Z_NEED_DICT || nret == Z_DATA_ERROR || nret == Z_MEM_ERROR )
		{
			FATALLOGC( "inflate return[%d]" , nret )
			inflateEnd( & inflate_strm );
			return 1;
		}
		if( inflate_strm.avail_out == 0 )
		{
			FATALLOGC( "unexpect inflate avail_out[%d] unexpect" , inflate_strm.avail_out )
			inflateEnd( & inflate_strm );
			return 1;
		}
		(*p_block_out_len) = LOGPIPE_BLOCK_BUFSIZE - inflate_strm.avail_out ;
		
		inflateEnd( & inflate_strm );
	}
	else
	{
		ERRORLOGC( "uncompress_algorithm[%s] invalid" , uncompress_algorithm )
		return -1;
	}
	
	return 0;
}

