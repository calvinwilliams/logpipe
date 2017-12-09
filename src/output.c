#include "logpipe_in.h"

int ConnectForwardSocket( struct LogPipeEnv *p_env , struct ForwardSession *p_forward_session )
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
		close( p_close_dump_session->tmp_fd ); \
	} \
	list_for_each_entry( p_forward_session , & (p_env->forward_session_list.this_node) , struct ForwardSession , this_node ) \
	{ \
		close( p_forward_session->forward_sock ); p_forward_session->forward_sock = -1 ; \
	} \

int ToOutputs( struct LogPipeEnv *p_env , char *comm_buffer , int comm_buffer_len , char *filename , uint16_t filename_len , int in , int append_len )
{
	struct DumpSession	*p_dump_session = NULL ;
	struct DumpSession	*p_close_dump_session = NULL ;
	struct ForwardSession	*p_forward_session = NULL ;
	
	int			remain_len ;
	char			block_buf[ LOGPIPE_COMM_FILE_BLOCK_BUFSIZE + 1 ] ;
	uint32_t		block_len ;
	uint32_t		block_len_htonl ;
	int			len ;
	
	int			nret = 0 ;
	
	INFOLOG( "comm_buffer[%p] comm_buffer_len[%d] filename[%.*s] filename_len[%d] in[%d] append_len[%d]" , comm_buffer , comm_buffer_len , filename_len , filename , filename_len , in , append_len )
	
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
		
		if( comm_buffer == NULL )
		{
			char			*magic = NULL ;
			uint16_t		*filename_len_htons = NULL ;
			char			comm_buffer[ 1 + sizeof(uint16_t) + PATH_MAX ] ;
			
			magic = comm_buffer ;
			(*magic) = LOGPIPE_COMM_MAGIC ;
			
			if( filename_len > PATH_MAX )
			{
				ERRORLOG( "filename length[%d] too long" , filename_len )
				CLOSE_ALL_DUMP_FILE_AND_FORWARD_SOCKET
				return 0;
			}
			
			filename_len_htons = (uint16_t*)(comm_buffer+1) ;
			(*filename_len_htons) = htons(filename_len) ;
			
			strncpy( comm_buffer+1+sizeof(uint16_t) , filename , filename_len );
			
			/* 发送通讯头和文件名 */
			len = writen( p_forward_session->forward_sock , comm_buffer , 1+sizeof(uint16_t)+filename_len ) ;
			if( len == -1 )
			{
				ERRORLOG( "send comm magic and filename failed , errno[%d]" , errno );
				close( p_forward_session->forward_sock ); p_forward_session->forward_sock = -1 ;
				sleep(1);
				goto _GOTO_RETRY_SEND;
			}
			else
			{
				INFOLOG( "send comm magic and filename ok , [%d]bytes" , filename_len );
				DEBUGHEXLOG( comm_buffer , len , "comm and magic and filename [%d]bytes" , len );
			}
		}
		else
		{
			/* 发送通讯头和文件名 */
			len = writen( p_forward_session->forward_sock , comm_buffer , comm_buffer_len ) ;
			if( len == -1 )
			{
				ERRORLOG( "send comm buffer failed , errno[%d]" , errno );
				close( p_forward_session->forward_sock ); p_forward_session->forward_sock = -1 ;
				sleep(1);
				goto _GOTO_RETRY_SEND;
			}
			else
			{
				INFOLOG( "send comm buffer ok , [%d]bytes" , comm_buffer_len );
				DEBUGHEXLOG( comm_buffer , len , "comm buffer [%d]bytes" , len );
			}
		}
	}
	
	remain_len = append_len ;
	
	while(1)
	{
		if( comm_buffer == NULL )
		{
			if( remain_len > sizeof(block_buf) - 1 )
				block_len = sizeof(block_buf) - 1 ;
			else
				block_len = remain_len ;
			
			if( block_len > 0 )
			{
				len = readn( in , block_buf , block_len ) ;
				if( len == -1 )
				{
					ERRORLOG( "read file failed , errno[%d]" , errno )
					CLOSE_ALL_DUMP_FILE_AND_FORWARD_SOCKET
					return -1;
				}
				else
				{
					INFOLOG( "read file ok , [%d]bytes" , block_len )
					DEBUGHEXLOG( block_buf , len , "file block [%d]bytes" , len )
				}
			}
		}
		else
		{
			len = readn( in , & block_len_htonl , sizeof(block_len_htonl) ) ;
			if( len == -1 )
			{
				ERRORLOG( "recv block len from socket failed , errno[%d]" , errno )
				CLOSE_ALL_DUMP_FILE_AND_FORWARD_SOCKET
				return -1;
			}
			else
			{
				INFOLOG( "recv block len from socket ok , [%d]bytes" , sizeof(block_len_htonl) )
				DEBUGHEXLOG( (char*) & block_len_htonl , len , "block len [%d]bytes" , len )
			}
			
			block_len = ntohl( block_len_htonl ) ;
			if( block_len > 0 )
			{
				len = readn( in , block_buf , block_len ) ;
				if( len == -1 )
				{
					ERRORLOG( "recv block data from socket failed , errno[%d]" , errno )
					CLOSE_ALL_DUMP_FILE_AND_FORWARD_SOCKET
					return -1;
				}
				else
				{
					INFOLOG( "recv block data from socket ok , [%d]bytes" , block_len )
					DEBUGHEXLOG( block_buf , len , "block data [%d]bytes" , len )
				}
			}
		}
		
		if( block_len > 0 )
		{
			list_for_each_entry( p_dump_session , & (p_env->dump_session_list.this_node) , struct DumpSession , this_node )
			{
				len = writen( p_dump_session->tmp_fd , block_buf , block_len ) ;
				if( len == -1 )
				{
					ERRORLOG( "write block data to file failed , errno[%d]" , errno )
					CLOSE_ALL_DUMP_FILE_AND_FORWARD_SOCKET
					return -1;
				}
				else
				{
					INFOLOG( "write block data to file ok , [%d]bytes" , block_len )
					DEBUGHEXLOG( block_buf , len , "block data [%d]bytes" , len )
				}
			}
		}
		
		list_for_each_entry( p_forward_session , & (p_env->forward_session_list.this_node) , struct ForwardSession , this_node )
		{
			block_len_htonl = htonl( block_len ) ;
			
			len = writen( p_forward_session->forward_sock , & block_len_htonl , sizeof(block_len_htonl) ) ;
			if( len == -1 )
			{
				ERRORLOG( "send block len to socket failed , errno[%d]" , errno )
				CLOSE_ALL_DUMP_FILE_AND_FORWARD_SOCKET
				return -1;
			}
			else
			{
				INFOLOG( "send block len to socket ok , [%d]bytes" , sizeof(block_len_htonl) )
				DEBUGHEXLOG( (char*) & block_len_htonl , len , "block len [%d]bytes" , len )
			}
			
			if( block_len > 0 )
			{
				len = writen( p_forward_session->forward_sock , block_buf , block_len ) ;
				if( len == -1 )
				{
					ERRORLOG( "send block data to socket failed , errno[%d]" , errno )
					CLOSE_ALL_DUMP_FILE_AND_FORWARD_SOCKET
					return -1;
				}
				else
				{
					INFOLOG( "send block data to socket ok , [%d]bytes" , block_len )
					DEBUGHEXLOG( block_buf , block_len , "block data [%d]bytes" , len )
				}
			}
		}
		
		if( comm_buffer == NULL )
		{
			if( remain_len == 0 )
				break;
		}
		else
		{
			if( block_len == 0 )
				break;
		}
		
		if( comm_buffer == NULL )
			remain_len -= block_len ;
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
		
		if( p_env->conf.rotate.file_rotate_max_size >= 0 && file_stat.st_size >= p_env->conf.rotate.file_rotate_max_size )
		{
			INFOLOG( "file_stat.st_size[%d] > p_env->conf.rotate.file_rotate_max_size[%d]" , file_stat.st_size , p_env->conf.rotate.file_rotate_max_size )
			RoratingFile( p_dump_session->dump_path , filename , filename_len );
		}
		
		close( p_dump_session->tmp_fd );
	}
	
	return 0;
}

