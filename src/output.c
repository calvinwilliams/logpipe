#include "logpipe_in.h"

#if 0

int ConnectForwardSocket( struct LogpipeEnv *p_env , struct ForwardSession *p_forward_session )
{
	int		nret = 0 ;
	
	/* 创建套接字 */
	p_forward_session->forward_sock = socket( AF_INET , SOCK_STREAM , IPPROTO_TCP ) ;
	if( p_forward_session->forward_sock == -1 )
	{
		ERRORLOG( "socket failed , errno[%d]" , errno );
		sleep(1);
		return -1;
	}
	
	/* 设置套接字选项 */
	{
		int	onoff = 1 ;
		setsockopt( p_forward_session->forward_sock , SOL_SOCKET , SO_REUSEADDR , (void *) & onoff , sizeof(int) );
	}
	
	{
		int	onoff = 1 ;
		setsockopt( p_forward_session->forward_sock , IPPROTO_TCP , TCP_NODELAY , (void*) & onoff , sizeof(int) );
	}
	
	/* 连接到服务端侦听端口 */
	nret = connect( p_forward_session->forward_sock , (struct sockaddr *) & (p_forward_session->forward_addr) , sizeof(struct sockaddr) ) ;
	if( nret == -1 )
	{
		ERRORLOG( "connect[%s:%d] failed , errno[%d]" , p_forward_session->forward_ip , p_forward_session->forward_port , errno );
		close( p_forward_session->forward_sock ); p_forward_session->forward_sock = -1 ;
		sleep(1);
		return 1;
	}
	else
	{
		INFOLOG( "connect[%s:%d] ok , sock[%d]" , p_forward_session->forward_ip , p_forward_session->forward_port , p_forward_session->forward_sock );
		return 0;
	}
}

#define CLOSE_ALL_DUMP_FILE_AND_FORWARD_SOCKET \
	list_for_each_entry( p_close_dump_session , & (p_env->dump_session_list.this_node) , struct DumpSession , this_node ) \
	{ \
		DEBUGLOG( "close dump fd[%d]" , p_close_dump_session->tmp_fd ) \
		close( p_close_dump_session->tmp_fd ); \
	} \
	list_for_each_entry( p_forward_session , & (p_env->forward_session_list.this_node) , struct ForwardSession , this_node ) \
	{ \
		DEBUGLOG( "close forward sock[%d]" , p_forward_session->forward_sock ) \
		close( p_forward_session->forward_sock ); p_forward_session->forward_sock = -1 ; \
	} \

int ToOutputs( struct LogpipeEnv *p_env , char *comm_buf , int comm_buf_len , char *filename , uint16_t filename_len , int in , int append_len , char compress_algorithm )
{
	struct DumpSession	*p_dump_session = NULL ;
	struct DumpSession	*p_close_dump_session = NULL ;
	struct ForwardSession	*p_forward_session = NULL ;
	
	int			remain_len ;
	char			block_in_buf[ LOGPIPE_COMM_FILE_BLOCK_BUFSIZE + 1 ] ;
	uint32_t		block_in_len ;
	uint32_t		block_in_len_htonl ;
	char			block_out_buf[ LOGPIPE_COMM_FILE_BLOCK_BUFSIZE + 1 ] ;
	uint32_t		block_out_len ;
	uint32_t		block_out_len_htonl ;
	int			len ;
	
	z_stream		deflate_strm ;
	z_stream		inflate_strm ;
	
	int			nret = 0 ;
	
	INFOLOG( "comm_buf[%p] comm_buf_len[%d] filename[%.*s] filename_len[%d] in[%d] append_len[%d]" , comm_buf , comm_buf_len , filename_len , filename , filename_len , in , append_len )
	
	list_for_each_entry( p_dump_session , & (p_env->dump_session_list.this_node) , struct DumpSession , this_node )
	{
		char			path_filename[ PATH_MAX + 1 ] ;
		
		memset( path_filename , 0x00 , sizeof(path_filename) );
		snprintf( path_filename , sizeof(path_filename)-1 , "%s/%.*s" , p_dump_session->dump_path , filename_len , filename );
		p_dump_session->tmp_fd = open( path_filename , O_CREAT|O_WRONLY|O_APPEND , 00777 ) ;
		if( p_dump_session->tmp_fd == -1 )
		{
			ERRORLOG( "open file[%s] failed , errno[%d]" , path_filename , errno )
			list_for_each_entry( p_close_dump_session , & (p_env->dump_session_list.this_node) , struct DumpSession , this_node )
			{
				if( p_close_dump_session == p_dump_session )
					break;
				close( p_close_dump_session->tmp_fd );
			}
			return -1;
		}
		else
		{
			DEBUGLOG( "open file[%s] ok" , path_filename )
		}
	}
	
	list_for_each_entry( p_forward_session , & (p_env->forward_session_list.this_node) , struct ForwardSession , this_node )
	{
_GOTO_RETRY_SEND :
		
		while( p_forward_session->forward_sock == -1 )
		{
			nret = ConnectForwardSocket( p_env , p_forward_session ) ;
			if( nret < 0 )
				return nret;
		}
		
		if( comm_buf == NULL )
		{
			uint16_t		*filename_len_htons = NULL ;
			char			comm_buf[ LOGPIPE_COMM_HEAD_LENGTH + sizeof(uint16_t) + PATH_MAX ] ;
			
			memset( comm_buf , 0x00 , sizeof(comm_buf) );
			comm_buf[LOGPIPE_COMM_HEAD_MAGIC_OFFSET] = LOGPIPE_COMM_HEAD_MAGIC ;
			comm_buf[LOGPIPE_COMM_HEAD_COMPRESS_OFFSET] = p_env->compress_algorithm ;
			
			if( filename_len > PATH_MAX )
			{
				ERRORLOG( "filename length[%d] too long" , filename_len )
				CLOSE_ALL_DUMP_FILE_AND_FORWARD_SOCKET
				return 0;
			}
			
			filename_len_htons = (uint16_t*)(comm_buf+LOGPIPE_COMM_HEAD_LENGTH) ;
			(*filename_len_htons) = htons(filename_len) ;
			
			strncpy( comm_buf+LOGPIPE_COMM_HEAD_LENGTH+sizeof(uint16_t) , filename , filename_len );
			
			/* 发送通讯头和文件名 */
			len = writen( p_forward_session->forward_sock , comm_buf , LOGPIPE_COMM_HEAD_LENGTH+sizeof(uint16_t)+filename_len ) ;
			if( len == -1 )
			{
				ERRORLOG( "send comm magic and filename failed , errno[%d]" , errno )
				close( p_forward_session->forward_sock ); p_forward_session->forward_sock = -1 ;
				sleep(1);
				goto _GOTO_RETRY_SEND;
			}
			else
			{
				INFOLOG( "send comm magic and filename ok , [%d]bytes" , LOGPIPE_COMM_HEAD_LENGTH+sizeof(uint16_t)+filename_len )
				DEBUGHEXLOG( comm_buf , len , NULL )
			}
		}
		else
		{
			/* 发送通讯头和文件名 */
			len = writen( p_forward_session->forward_sock , comm_buf , comm_buf_len ) ;
			if( len == -1 )
			{
				ERRORLOG( "send comm buffer failed , errno[%d]" , errno )
				close( p_forward_session->forward_sock ); p_forward_session->forward_sock = -1 ;
				sleep(1);
				goto _GOTO_RETRY_SEND;
			}
			else
			{
				INFOLOG( "send comm buffer ok , [%d]bytes" , comm_buf_len )
				DEBUGHEXLOG( comm_buf , len , NULL )
			}
		}
	}
	
	remain_len = append_len ;
	
	while(1)
	{
		if( comm_buf == NULL )
		{
			if( remain_len > sizeof(block_in_buf) - 1 )
				block_in_len = sizeof(block_in_buf) - 1 ;
			else
				block_in_len = remain_len ;
			
			if( block_in_len > 0 )
			{
				len = readn( in , block_in_buf , block_in_len ) ;
				if( len == -1 )
				{
					ERRORLOG( "read file failed , errno[%d]" , errno )
					CLOSE_ALL_DUMP_FILE_AND_FORWARD_SOCKET
					return -1;
				}
				else
				{
					INFOLOG( "read file ok , [%d]bytes" , block_in_len )
					DEBUGHEXLOG( block_in_buf , len , NULL )
				}
			}
			
			block_in_len_htonl = htonl( block_in_len ) ;
		}
		else
		{
			DEBUGLOG( "recv block len from socket[%d] ..." , in )
			len = readn( in , & block_in_len_htonl , sizeof(block_in_len_htonl) ) ;
			if( len == -1 )
			{
				ERRORLOG( "recv block len from socket failed , errno[%d]" , errno )
				CLOSE_ALL_DUMP_FILE_AND_FORWARD_SOCKET
				return -1;
			}
			else if( len == 0 )
			{
				WARNLOG( "remote socket closed on recv block len from socket" );
				return 1;
			}
			else
			{
				INFOLOG( "recv block len from socket ok , [%d]bytes" , sizeof(block_in_len_htonl) )
				DEBUGHEXLOG( (char*) & block_in_len_htonl , len , NULL )
			}
			
			block_in_len = ntohl( block_in_len_htonl ) ;
			if( block_in_len > 0 )
			{
				len = readn( in , block_in_buf , block_in_len ) ;
				if( len == -1 )
				{
					ERRORLOG( "recv block data from socket failed , errno[%d]" , errno )
					CLOSE_ALL_DUMP_FILE_AND_FORWARD_SOCKET
					return -1;
				}
				else if( len == 0 )
				{
					WARNLOG( "remote socket closed on recv block data from socket" );
					return 1;
				}
				else
				{
					INFOLOG( "recv block data from socket ok , [%d]bytes" , block_in_len )
					DEBUGHEXLOG( block_in_buf , len , NULL )
				}
			}
		}
		
		if( block_in_len > 0 )
		{
			if( comm_buf == NULL || compress_algorithm == 0 )
			{
				list_for_each_entry( p_dump_session , & (p_env->dump_session_list.this_node) , struct DumpSession , this_node )
				{
					len = writen( p_dump_session->tmp_fd , block_in_buf , block_in_len ) ;
					if( len == -1 )
					{
						ERRORLOG( "write block data to file failed , errno[%d]" , errno )
						CLOSE_ALL_DUMP_FILE_AND_FORWARD_SOCKET
						return -1;
					}
					else
					{
						INFOLOG( "write block data to file ok , [%d]bytes" , block_in_len )
						DEBUGHEXLOG( block_in_buf , len , NULL )
					}
				}
			}
			else
			{
				memset( & inflate_strm , 0x00 , sizeof(z_stream) );
				nret = inflateInit( & inflate_strm ) ;
				if( nret != Z_OK )
				{
					FATALLOG( "inflateInit failed[%d]" , nret );
					CLOSE_ALL_DUMP_FILE_AND_FORWARD_SOCKET
					return -1;
				}
				
				inflate_strm.avail_in = block_in_len ;
				inflate_strm.next_in = (Bytef*)block_in_buf ;
				
				do
				{
					inflate_strm.avail_out = sizeof(block_out_buf)-1 ;
					inflate_strm.next_out = (Bytef*)block_out_buf ;
					nret = inflate( & inflate_strm , Z_NO_FLUSH ) ;
					if( nret == Z_STREAM_ERROR )
					{
						FATALLOG( "inflate return Z_STREAM_ERROR" )
						CLOSE_ALL_DUMP_FILE_AND_FORWARD_SOCKET
						inflateEnd( & inflate_strm );
						return -1;
					}
					else if( nret == Z_NEED_DICT || nret == Z_DATA_ERROR || nret == Z_MEM_ERROR )
					{
						FATALLOG( "inflate return[%d]" , nret )
						CLOSE_ALL_DUMP_FILE_AND_FORWARD_SOCKET
						inflateEnd( & inflate_strm );
						return -1;
					}
					block_out_len = sizeof(block_out_buf)-1 - inflate_strm.avail_out ;
					
					list_for_each_entry( p_dump_session , & (p_env->dump_session_list.this_node) , struct DumpSession , this_node )
					{
						len = writen( p_dump_session->tmp_fd , block_out_buf , block_out_len ) ;
						if( len == -1 )
						{
							ERRORLOG( "write uncompress block data to file failed , errno[%d]" , errno )
							CLOSE_ALL_DUMP_FILE_AND_FORWARD_SOCKET
							inflateEnd( & inflate_strm );
							return -1;
						}
						else
						{
							INFOLOG( "write uncompress block data to file ok , [%d]bytes" , block_out_len )
							DEBUGHEXLOG( block_out_buf , len , NULL )
						}
					}
				}
				while( inflate_strm.avail_out == 0 );
				
				inflateEnd( & inflate_strm );
			}
		}
		
		if( comm_buf || compress_algorithm == 0 || block_in_len == 0 )
		{
			list_for_each_entry( p_forward_session , & (p_env->forward_session_list.this_node) , struct ForwardSession , this_node )
			{
				len = writen( p_forward_session->forward_sock , & block_in_len_htonl , sizeof(block_in_len_htonl) ) ;
				if( len == -1 )
				{
					ERRORLOG( "send block len to socket failed , errno[%d]" , errno )
					CLOSE_ALL_DUMP_FILE_AND_FORWARD_SOCKET
					return -1;
				}
				else
				{
					INFOLOG( "send block len to socket ok , [%d]bytes" , sizeof(block_in_len_htonl) )
					DEBUGHEXLOG( (char*) & block_in_len_htonl , len , NULL )
				}
				
				if( block_in_len > 0 )
				{
					len = writen( p_forward_session->forward_sock , block_in_buf , block_in_len ) ;
					if( len == -1 )
					{
						ERRORLOG( "send block data to socket failed , errno[%d]" , errno )
						CLOSE_ALL_DUMP_FILE_AND_FORWARD_SOCKET
						return -1;
					}
					else
					{
						INFOLOG( "send block data to socket ok , [%d]bytes" , block_in_len )
						DEBUGHEXLOG( block_in_buf , block_in_len , NULL )
					}
				}
			}
		}
		else
		{
			memset( & deflate_strm , 0x00 , sizeof(z_stream) );
			nret = deflateInit( & deflate_strm , Z_DEFAULT_COMPRESSION ) ;
			if( nret != Z_OK )
			{
				FATALLOG( "deflateInit failed[%d]" , nret );
				CLOSE_ALL_DUMP_FILE_AND_FORWARD_SOCKET
				return -1;
			}
			
			deflate_strm.avail_in = block_in_len ;
			deflate_strm.next_in = (Bytef*)block_in_buf ;
			
			do
			{
				deflate_strm.avail_out = sizeof(block_out_buf)-1 ;
				deflate_strm.next_out = (Bytef*)block_out_buf ;
				nret = deflate( & deflate_strm , Z_FINISH ) ;
				if( nret == Z_STREAM_ERROR )
				{
					FATALLOG( "deflate return Z_STREAM_ERROR" )
					CLOSE_ALL_DUMP_FILE_AND_FORWARD_SOCKET
					deflateEnd( & deflate_strm );
					return -1;
				}
				block_out_len = sizeof(block_out_buf)-1 - deflate_strm.avail_out ;
				
				block_out_len_htonl = htonl( block_out_len ) ;
				
				list_for_each_entry( p_forward_session , & (p_env->forward_session_list.this_node) , struct ForwardSession , this_node )
				{
					len = writen( p_forward_session->forward_sock , & block_out_len_htonl , sizeof(block_out_len_htonl) ) ;
					if( len == -1 )
					{
						ERRORLOG( "send block len to socket failed , errno[%d]" , errno )
						CLOSE_ALL_DUMP_FILE_AND_FORWARD_SOCKET
						deflateEnd( & deflate_strm );
						return -1;
					}
					else
					{
						INFOLOG( "send block len to socket ok , [%d]bytes" , sizeof(block_out_len_htonl) )
						DEBUGHEXLOG( (char*) & block_in_len_htonl , len , NULL )
					}
					
					len = writen( p_forward_session->forward_sock , block_out_buf , block_out_len ) ;
					if( len == -1 )
					{
						ERRORLOG( "send block data to socket failed , errno[%d]" , errno )
						CLOSE_ALL_DUMP_FILE_AND_FORWARD_SOCKET
						deflateEnd( & deflate_strm );
						return -1;
					}
					else
					{
						INFOLOG( "send block data to socket ok , [%d]bytes" , block_out_len )
						DEBUGHEXLOG( block_out_buf , len , NULL )
					}
				}
			}
			while( deflate_strm.avail_out == 0 );
			
			deflateEnd( & deflate_strm );
		}
		
		if( comm_buf == NULL )
		{
			if( remain_len == 0 )
				break;
		}
		else
		{
			if( block_in_len == 0 )
				break;
		}
		
		if( comm_buf == NULL )
		{
			remain_len -= block_in_len ;
		}
	}
	
	list_for_each_entry( p_dump_session , & (p_env->dump_session_list.this_node) , struct DumpSession , this_node )
	{
		struct stat		file_stat ;
		
		memset( & file_stat , 0x00 , sizeof(struct stat) );
		nret = fstat( p_dump_session->tmp_fd , & file_stat ) ;
		if( nret == -1 )
		{
			ERRORLOG( "fstat[%.*s] failed , errno[%d]" , filename_len , filename , errno )
		}
		
		if( p_env->conf.rotate.file_rotate_max_size > 0 && file_stat.st_size >= p_env->conf.rotate.file_rotate_max_size )
		{
			INFOLOG( "file_stat.st_size[%d] > p_env->conf.rotate.file_rotate_max_size[%d]" , file_stat.st_size , p_env->conf.rotate.file_rotate_max_size )
			RoratingFile( p_dump_session->dump_path , filename , filename_len );
		}
		
		close( p_dump_session->tmp_fd );
	}
	
	return 0;
}

#endif

#define BLOCK_BUFSIZE		100*1024

int WriteAllOutputPlugins( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , uint16_t filename_len , char *filename )
{
	struct LogpipeOutputPlugin	*p_logpipe_output_plugin = NULL ;
	
	char				block_buf[ BLOCK_BUFSIZE + 1 ] ;
	uint32_t			block_len ;
	uint32_t			block_len_htonl ;
	
	int				nret = 0 ;
	
	/* 执行所有输出端写前函数 */
	list_for_each_entry( p_logpipe_output_plugin , & (p_env->logpipe_outputs_plugin_list->this_node) , struct LogpipeOutputPlugin , this_node )
	{
		nret = p_logpipe_output_plugin->funcBeforeWriteLogpipeOutput( p_env , & (p_logpipe_output_plugin->context) ) ;
		if( nret )
		{
			ERRORLOG( "[%s]p_logpipe_output_plugin->funcBeforeWriteLogpipeOutput failed , errno[%d]" , p_logpipe_output_plugin->so_path_filename , errno );
			return -1;
		}
		else
		{
			INFOLOG( "[%s]p_logpipe_output_plugin->funcBeforeWriteLogpipeOutput ok" , p_logpipe_output_plugin->so_path_filename );
		}
	}
	
	while(1)
	{
		/* 执行输入端读函数 */
		nret = p_logpipe_input_plugin->funcReadLogpipeInput( p_env , & (p_logpipe_input_plugin->context) , & block_len , block_buf , sizeof(block_buf) ) ;
		if( nret > 0 )
		{
			INFOLOG( "[%s]p_logpipe_input_plugin->funcReadLogpipeInput done" , p_logpipe_input_plugin->so_path_filename );
		}
		else if( nret < 0 )
		{
			ERRORLOG( "[%s]p_logpipe_input_plugin->funcReadLogpipeInput failed , errno[%d]" , p_logpipe_input_plugin->so_path_filename , errno );
			return -1;
		}
		else
		{
			INFOLOG( "[%s]p_logpipe_input_plugin->funcReadLogpipeInput ok" , p_logpipe_input_plugin->so_path_filename );
		}
		
		/* 执行所有输出端写函数 */
		list_for_each_entry( p_logpipe_output_plugin , & (p_env->logpipe_outputs_plugin_list->this_node) , struct LogpipeOutputPlugin , this_node )
		{
			nret = p_logpipe_output_plugin->funcWriteLogpipeOutput( p_env , & (p_logpipe_output_plugin->context) ) ;
			if( nret )
			{
				ERRORLOG( "[%s]p_logpipe_output_plugin->funcWriteLogpipeOutput failed , errno[%d]" , p_logpipe_output_plugin->so_path_filename , errno );
				return -1;
			}
			else
			{
				INFOLOG( "[%s]p_logpipe_output_plugin->funcWriteLogpipeOutput ok" , p_logpipe_output_plugin->so_path_filename );
			}
		}
	}
	
	/* 执行所有输出端写后函数 */
	list_for_each_entry( p_logpipe_output_plugin , & (p_env->logpipe_outputs_plugin_list->this_node) , struct LogpipeOutputPlugin , this_node )
	{
		nret = p_logpipe_output_plugin->funcAfterWriteLogpipeOutput( p_env , & (p_logpipe_output_plugin->context) ) ;
		if( nret )
		{
			ERRORLOG( "[%s]p_logpipe_output_plugin->funcAfterWriteLogpipeOutput failed , errno[%d]" , p_logpipe_output_plugin->so_path_filename , errno );
			return -1;
		}
		else
		{
			INFOLOG( "[%s]p_logpipe_output_plugin->funcAfterWriteLogpipeOutput ok" , p_logpipe_output_plugin->so_path_filename );
		}
	}
	
	return 0;
}
