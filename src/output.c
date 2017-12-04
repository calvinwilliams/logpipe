#include "logpipe_in.h"

static int DumpToFile( struct LogPipeEnv *p_env , struct DumpSession *p_dump_session , char *file_content , int appender_len , char *filename , int filename_len , int in , int trace_offset )
{
	char		path_filename[ PATH_MAX + 1 ] ;
	int		out ;
	int		dump_len ;
	char		block_buf[ LOGPIPE_COMM_BODY_BLOCK + 1 ] ;
	int		block_len ;
	
	int		nret = 0 ;
	
	memset( path_filename , 0x00 , sizeof(path_filename) );
	snprintf( path_filename , sizeof(path_filename)-1 , "%s/%.*s" , p_dump_session->dump_path , filename_len , filename );
	out = open( path_filename , O_CREAT|O_WRONLY|O_APPEND , 00777 ) ;
	if( out == -1 )
	{
		ERRORLOG( "open file[%s] failed , errno[%d]" , path_filename , errno )
		return -1;
	}
	else
	{
		DEBUGLOG( "open file[%s] ok" , path_filename )
	}
	
	if( file_content )
	{
		nret = write( out , file_content , appender_len ) ;
		DEBUGHEXLOG( file_content , appender_len , "write file[%s] [%d]bytes return[%d]" , path_filename , appender_len , nret )
	}
	else
	{
		lseek( in , trace_offset , SEEK_SET );
		for( dump_len = 0 ; dump_len < appender_len ; )
		{
			if( appender_len > sizeof(block_buf) -1 )
				block_len = sizeof(block_buf) -1 ;
			else
				block_len = appender_len ;
			nret = read( in , block_buf , block_len ) ;
			if( nret == -1 )
			{
				ERRORLOG( "read[%s] failed , errno[%d]" , filename , errno )
				close( out );
				return -1;
			}
			else
			{
				DEBUGLOG( "read[%s] ok , [%d]bytes" , filename , block_len )
			}
			
			nret = write( out , block_buf , block_len ) ;
			if( nret == -1 )
			{
				ERRORLOG( "write[%s] failed , errno[%d]" , filename , errno )
				close( out );
				return -1;
			}
			else
			{
				DEBUGLOG( "write[%s] ok , [%d]bytes" , filename , block_len )
			}
			
			dump_len += appender_len ;
		}
	}
	
	close( out );
	DEBUGLOG( "close file[%s]" , path_filename )
	
	return 0;
}

static int ReconnectForwardSocket( struct LogPipeEnv *p_env , struct ForwardSession *p_forward_session )
{
	int		nret = 0 ;
	
	/* 创建套接字 */
	p_forward_session->forward_sock = socket( AF_INET , SOCK_STREAM , IPPROTO_TCP ) ;
	if( p_forward_session->forward_sock == -1 )
	{
		ERRORLOG( "socket failed , errno[%d]" , errno );
		sleep(10);
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
		close( p_forward_session->forward_sock );
		sleep(10);
		return -1;
	}
	else
	{
		INFOLOG( "connect[%s:%d] ok" , p_forward_session->forward_ip , p_forward_session->forward_port );
	}
	
	return 0;
}

static int SendingForwardSocket( struct LogPipeEnv *p_env , struct ForwardSession *p_forward_session , char *raw_data , int raw_data_len , char *filename , int filename_len , int in , int trace_offset , int appender_len )
{
	if( raw_data )
	{
		int	sent_len ;
		int	remain_len ;
		char	*p_send_offset = NULL ;
		int	len ;
		
		for( sent_len = 0 , remain_len = raw_data_len , p_send_offset = raw_data ; sent_len < raw_data_len ; )
		{
			while( p_forward_session->forward_sock == -1 )
				ReconnectForwardSocket( p_env , p_forward_session );
			
			len = send( p_forward_session->forward_sock , p_send_offset , remain_len , 0 ) ;
			if( len == -1 )
			{
				ERRORLOG( "send[%s:%d] failed , errno[%d]" , p_forward_session->forward_ip , p_forward_session->forward_port , errno );
				close( p_forward_session->forward_sock ); p_forward_session->forward_sock = -1 ;
				sleep(10);
				continue;
			}
			
			sent_len += len ;
			remain_len -= len ;
			p_send_offset += len ;
		}
	}
	else
	{
		uint32_t		comm_total_length ;
		uint32_t		comm_total_length_htonl ;
		uint32_t		filename_len_htonl ;
		int			sent_len ;
		char			block_buf[ LOGPIPE_COMM_BODY_BLOCK + 1 ] ;
		int			block_len ;
		
		int			nret = 0 ;
		
		comm_total_length = 1 + sizeof(filename_len_htonl) + filename_len + appender_len ;
		comm_total_length_htonl = htonl( comm_total_length ) ;
		
		while( p_forward_session->forward_sock == -1 )
			ReconnectForwardSocket( p_env , p_forward_session );
		
		/* 发送通讯总长度 */
		
_GOTO_SEND_COMM_TOTAL_LENGTH :
		
		nret = send( p_forward_session->forward_sock , & comm_total_length_htonl , sizeof(comm_total_length_htonl) , 0 ) ;
		if( nret == -1 )
		{
			ERRORLOG( "send comm total length failed , errno[%d]" , errno );
			close( p_forward_session->forward_sock ); p_forward_session->forward_sock = -1 ;
			sleep(10);
			while( p_forward_session->forward_sock == -1 )
				ReconnectForwardSocket( p_env , p_forward_session );
			goto _GOTO_SEND_COMM_TOTAL_LENGTH;
		}
		else
		{
			DEBUGHEXLOG( (char*) & comm_total_length_htonl , sizeof(comm_total_length_htonl) , "send comm total length ok , [%d]bytes" , sizeof(comm_total_length_htonl) );
		}
		
		/* 发送魔法字节 */
		
_GOTO_SEND_MAGIC :
		
		nret = send( p_forward_session->forward_sock , LOGPIPE_COMM_MAGIC , 1 , 0 ) ;
		if( nret == -1 )
		{
			ERRORLOG( "send magic failed , errno[%d]" , errno );
			close( p_forward_session->forward_sock ); p_forward_session->forward_sock = -1 ;
			sleep(10);
			while( p_forward_session->forward_sock == -1 )
				ReconnectForwardSocket( p_env , p_forward_session );
			goto _GOTO_SEND_MAGIC;
		}
		else
		{
			DEBUGHEXLOG( LOGPIPE_COMM_MAGIC , 1 , "send magic ok , [%d]bytes" , 1 );
		}
		
		/* 发送文件名长度 */
		
_GOTO_SEND_FILE_NAME_LENGTH :
		
		filename_len_htonl = htonl(filename_len) ;
		nret = send( p_forward_session->forward_sock , & filename_len_htonl , sizeof(filename_len_htonl) , 0 ) ;
		if( nret == -1 )
		{
			ERRORLOG( "send file name length failed , errno[%d]" , errno );
			close( p_forward_session->forward_sock ); p_forward_session->forward_sock = -1 ;
			sleep(10);
			while( p_forward_session->forward_sock == -1 )
				ReconnectForwardSocket( p_env , p_forward_session );
			goto _GOTO_SEND_FILE_NAME_LENGTH;
		}
		else
		{
			DEBUGHEXLOG( (char*) & filename_len_htonl , sizeof(filename_len_htonl) , "send file name length ok , [%d]bytes" , sizeof(filename_len_htonl) );
		}
		
		/* 发送文件名 */
		
_GOTO_SEND_FILE_NAME :
		
		nret = send( p_forward_session->forward_sock , filename , filename_len , 0 ) ;
		if( nret == -1 )
		{
			ERRORLOG( "send file name failed , errno[%d]" , errno );
			close( p_forward_session->forward_sock ); p_forward_session->forward_sock = -1 ;
			sleep(10);
			while( p_forward_session->forward_sock == -1 )
				ReconnectForwardSocket( p_env , p_forward_session );
			goto _GOTO_SEND_FILE_NAME;
		}
		else
		{
			DEBUGHEXLOG( filename , filename_len , "send file name ok , [%d]bytes" , filename_len );
		}
		
		/* 发送文件新增数据 */
		lseek( in , trace_offset , SEEK_SET );
		for( sent_len = 0 ; sent_len < appender_len ; )
		{
			if( appender_len > sizeof(block_buf) -1 )
				block_len = sizeof(block_buf) -1 ;
			else
				block_len = appender_len ;
			block_len = read( in , block_buf , block_len ) ;
			if( block_len == -1 )
			{
				ERRORLOG( "read[%s] failed , errno[%d]" , filename , errno )
				return -1;
			}
			else
			{
				DEBUGLOG( "read[%s] ok , [%d]bytes" , filename , block_len )
			}
			
_GOTO_SEND_FILE_DATA :
			
			nret = send( p_forward_session->forward_sock , block_buf , block_len , 0 ) ;
			if( nret == -1 )
			{
				ERRORLOG( "send file data failed , errno[%d]" , errno );
				close( p_forward_session->forward_sock ); p_forward_session->forward_sock = -1 ;
				sleep(10);
				while( p_forward_session->forward_sock == -1 )
					ReconnectForwardSocket( p_env , p_forward_session );
				goto _GOTO_SEND_FILE_DATA;
			}
			else
			{
				DEBUGHEXLOG( block_buf , block_len , "send file data block ok , [%d]bytes" , block_len )
			}
			
			sent_len += block_len ;
		}
	}
	
	return 0;
}

int FileToOutputs( struct LogPipeEnv *p_env , struct TraceFile *p_trace_file , int in , int appender_len )
{
	struct list_head	*p_node = NULL ;
	struct list_head	*p_next_node = NULL ;
	struct DumpSession	*p_dump_session = NULL ;
	struct ForwardSession	*p_forward_session = NULL ;
	
	int			nret = 0 ;
	
	/* 导出所有输出端 */
	list_for_each_safe( p_node , p_next_node , & (p_env->dump_session_list.this_node) )
	{
		p_dump_session = list_entry( p_node , struct DumpSession , this_node ) ;
		
		nret = DumpToFile( p_env , p_dump_session , NULL , appender_len , p_trace_file->filename , p_trace_file->filename_len , in , p_trace_file->trace_offset ) ;
		if( nret )
		{
			ERRORLOG( "DumpToFile failed[%d]" , nret )
			return -1;
		}
	}
	
	list_for_each_safe( p_node , p_next_node , & (p_env->forward_session_list.this_node) )
	{
		p_forward_session = list_entry( p_node , struct ForwardSession , this_node ) ;
		
		nret = SendingForwardSocket( p_env , p_forward_session , NULL , -1 , p_trace_file->filename , p_trace_file->filename_len , in , p_trace_file->trace_offset , appender_len ) ;
		if( nret )
		{
			ERRORLOG( "SendingForwardSocket failed[%d]" , nret )
			return -1;
		}
	}
	
	return 0;
}

int CommToOutput( struct LogPipeEnv *p_env , struct AcceptedSession *p_accepted_session )
{
	char			*magic = NULL ;
	uint32_t		*filename_len_htonl = NULL ;
	uint32_t		filename_len_ntohl ;
	char			*filename = NULL ;
	char			*file_content = NULL ;
	int			appender_len ;
	
	struct list_head	*p_node = NULL ;
	struct list_head	*p_next_node = NULL ;
	struct DumpSession	*p_dump_session = NULL ;
	struct ForwardSession	*p_forward_session = NULL ;
	
	int			nret = 0 ;
	
	/* 分解通讯协议 */
	magic = p_accepted_session->comm_buf + sizeof(uint32_t) ;
	DEBUGHEXLOG( magic , 1 , "magic" )
	if( (*magic) != LOGPIPE_COMM_MAGIC[0] )
	{
		ERRORLOG( "comm magic[%d][%c] not match" , (*magic) , (*magic) )
		return -1;
	}
	
	filename_len_htonl = (uint32_t *)(magic+1) ;
	DEBUGHEXLOG( (char*)filename_len_htonl , sizeof(uint32_t) , "file name len[%d]" , sizeof(uint32_t) )
	
	filename_len_ntohl = ntohl( *filename_len_htonl ) ;
	
	filename = (char*)filename_len_htonl + sizeof(uint32_t) ;
	DEBUGHEXLOG( filename , filename_len_ntohl , "file name" )
	
	file_content = filename + filename_len_ntohl ;
	appender_len = p_accepted_session->comm_body_len - 1 - sizeof(filename_len_htonl) - filename_len_ntohl ;
	
	/* 导出所有输出端 */
	list_for_each_safe( p_node , p_next_node , & (p_env->dump_session_list.this_node) )
	{
		p_dump_session = list_entry( p_node , struct DumpSession , this_node ) ;
		
		nret = DumpToFile( p_env , p_dump_session , file_content , appender_len , filename , filename_len_ntohl , -1 , -1 ) ;
		if( nret )
		{
			ERRORLOG( "DumpToFile failed[%d]" , nret )
			return -1;
		}
	}
	
	list_for_each_safe( p_node , p_next_node , & (p_env->forward_session_list.this_node) )
	{
		p_forward_session = list_entry( p_node , struct ForwardSession , this_node ) ;
		
		nret = SendingForwardSocket( p_env , p_forward_session , p_accepted_session->comm_buf , p_accepted_session->comm_data_len , NULL , -1 , -1 , -1 , -1 ) ;
		if( nret )
		{
			ERRORLOG( "SendingForwardSocket failed[%d]" , nret )
			return -1;
		}
	}
	
	return 0;
}
