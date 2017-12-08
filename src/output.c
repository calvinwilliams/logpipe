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

int ToOutputs( struct LogPipeEnv *p_env , char *filename , uint16_t filename_len , int in , int appender_len )
{
	struct DumpSession	*p_dump_session = NULL ;
	struct DumpSession	*p_close_dump_session = NULL ;
	struct ForwardSession	*p_forward_session = NULL ;
	
	int			sent_len ;
	char			block_buf[ LOGPIPE_COMM_BODY_BLOCK + 1 ] ;
	int			block_len ;
	int			len ;
	
	int			nret = 0 ;
	
	INFOLOG( "filename[%.*s] filename_len[%d] in[%d] appender_len[%d]" , filename_len , filename , filename_len , in , appender_len )
	
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
		uint32_t		comm_head_len_ntohl ;
		uint32_t		*comm_head_len_htonl = NULL ;
		char			*magic = NULL ;
		uint16_t		*filename_len_htons = NULL ;
		char			comm_buffer[ sizeof(uint32_t) + 1 + sizeof(uint16_t) ] ;
		
		comm_head_len_ntohl = 1 + sizeof(uint16_t) + filename_len + appender_len ;
		
		comm_head_len_htonl = (uint32_t*)comm_buffer ;
		(*comm_head_len_htonl) = htonl( comm_head_len_ntohl ) ;
		
		magic = comm_buffer + sizeof(uint32_t) ;
		(*magic) = LOGPIPE_COMM_MAGIC[0] ;
		
		filename_len_htons = (uint16_t*)(comm_buffer+sizeof(uint32_t)+1) ;
		(*filename_len_htons) = htons(filename_len) ;
		
_GOTO_RETRY_SEND :
		
		while( p_forward_session->forward_sock == -1 )
		{
			nret = ConnectForwardSocket( p_env , p_forward_session ) ;
			if( nret < 0 )
				return nret;
		}
		
		/* 发送通讯头 */
		len = writen( p_forward_session->forward_sock , comm_buffer , sizeof(comm_buffer) ) ;
		if( len == -1 )
		{
			ERRORLOG( "send comm head and magic and filename len failed , errno[%d]" , errno );
			close( p_forward_session->forward_sock ); p_forward_session->forward_sock = -1 ;
			sleep(1);
			goto _GOTO_RETRY_SEND;
		}
		else
		{
			DEBUGHEXLOG( comm_buffer , len , "send comm head and magic and filename len ok , [%d]bytes" , len );
		}
		
		/* 发送文件名 */
		len = writen( p_forward_session->forward_sock , filename , filename_len ) ;
		if( len == -1 )
		{
			ERRORLOG( "send file name failed , errno[%d]" , errno );
			close( p_forward_session->forward_sock ); p_forward_session->forward_sock = -1 ;
			sleep(1);
			goto _GOTO_RETRY_SEND;
		}
		else
		{
			DEBUGHEXLOG( filename , len , "send filename ok , [%d]bytes" , len );
		}
	}
	
	for( sent_len = 0 ; sent_len < appender_len ; )
	{
		if( appender_len > sizeof(block_buf) - 1 )
			block_len = sizeof(block_buf) - 1 ;
		else
			block_len = appender_len ;
		len = readn( in , block_buf , block_len ) ;
		if( len == -1 )
		{
			ERRORLOG( "read[%s] failed , errno[%d]" , filename , errno )
			list_for_each_entry( p_close_dump_session , & (p_env->dump_session_list.this_node) , struct DumpSession , this_node )
			{
				close( p_close_dump_session->tmp_fd );
			}
			list_for_each_entry( p_forward_session , & (p_env->forward_session_list.this_node) , struct ForwardSession , this_node )
			{
				close( p_forward_session->forward_sock ); p_forward_session->forward_sock = -1 ;
			}
			return -1;
		}
		else
		{
			DEBUGLOG( "read[%s] ok , [%d]bytes" , filename , block_len )
		}
		
		list_for_each_entry( p_dump_session , & (p_env->dump_session_list.this_node) , struct DumpSession , this_node )
		{
			len = writen( p_dump_session->tmp_fd , block_buf , block_len ) ;
			if( len == -1 )
			{
				ERRORLOG( "write file[%s/%s] failed , errno[%d]" , p_dump_session->dump_path , filename , errno )
				list_for_each_entry( p_dump_session , & (p_env->dump_session_list.this_node) , struct DumpSession , this_node )
				{
					close( p_dump_session->tmp_fd );
				}
				list_for_each_entry( p_forward_session , & (p_env->forward_session_list.this_node) , struct ForwardSession , this_node )
				{
					close( p_forward_session->forward_sock ); p_forward_session->forward_sock = -1 ;
				}
				return -1;
			}
			else
			{
				DEBUGLOG( "write file[%s/%s] ok , [%d]bytes" , p_dump_session->dump_path , filename , block_len )
			}
		}
		
		list_for_each_entry( p_forward_session , & (p_env->forward_session_list.this_node) , struct ForwardSession , this_node )
		{
			len = writen( p_forward_session->forward_sock , block_buf , block_len ) ;
			if( len == -1 )
			{
				ERRORLOG( "write forward socket failed , errno[%d]" , errno )
				list_for_each_entry( p_dump_session , & (p_env->dump_session_list.this_node) , struct DumpSession , this_node )
				{
					close( p_dump_session->tmp_fd );
				}
				list_for_each_entry( p_forward_session , & (p_env->forward_session_list.this_node) , struct ForwardSession , this_node )
				{
					close( p_forward_session->forward_sock ); p_forward_session->forward_sock = -1 ;
				}
				return -1;
			}
			else
			{
				DEBUGLOG( "write forward socket ok , [%d]bytes" , len )
			}
		}
		
		sent_len += appender_len ;
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

