#include "logpipe_in.h"

static int ReadFilesToinotifyWdTree( struct LogpipeEnv *p_env , struct InotifySession *p_inotify_session )
{
	DIR			*dir = NULL ;
	struct dirent		*ent = NULL ;
	
	int			nret = 0 ;
	
	dir = opendir( p_inotify_session->inotify_path ) ;
	if( dir == NULL )
	{
		ERRORLOG( "opendir[%s] failed , errno[%d]" , p_inotify_session->inotify_path , errno )
		return -1;
	}
	
	while(1)
	{
		ent = readdir( dir ) ;
		if( ent == NULL )
			break;
		
		if( ent->d_type & DT_REG )
		{
			nret = AddFileWatcher( p_env , p_inotify_session , ent->d_name ) ;
			if( nret )
			{
				ERRORLOG( "AddFileWatcher[%s] failed , errno[%d]" , ent->d_name , errno )
				return -1;
			}
		}
	}
	
	closedir( dir );
	
	return 0;
}

int InitEnvironment( struct LogpipeEnv *p_env )
{
	struct LogpipeInputPlugin	*p_logpipe_input_plugin = NULL ;
	struct LogpipeOutputPlugin	*p_logpipe_output_plugin = NULL ;
	struct epoll_event		event ;
	
	int				nret = 0 ;
	
	/* 创建epoll */
	p_env->epoll_fd = epoll_create( 1024 ) ;
	if( p_env->epoll_fd == -1 )
	{
		ERRORLOG( "epoll_create failed , errno[%d]" , errno );
		return -1;
	}
	
	/* 执行所有输入端初始化函数 */
	list_for_each_entry( p_logpipe_input_plugin , & (p_env->logpipe_inputs_plugin_list->this_node) , struct LogpipeInputPlugin , this_node )
	{
		p_logpipe_input_plugin->fd = -1 ;
		p_logpipe_input_plugin->context = NULL ;
		nret = p_logpipe_input_plugin->pfuncInitLogpipeInputPlugin( p_env , p_logpipe_input_plugin , & (p_logpipe_input_plugin->plugin_config_items) , & (p_logpipe_input_plugin->context) , & (p_logpipe_input_plugin->fd) ) ;
		if( nret )
		{
			ERRORLOG( "[%s]p_logpipe_input_plugin->pfuncInitLogpipeInputPlugin failed , errno[%d]" , p_logpipe_input_plugin->so_path_filename , errno );
			return -1;
		}
		else
		{
			INFOLOG( "[%s]p_logpipe_input_plugin->pfuncInitLogpipeInputPlugin ok" , p_logpipe_input_plugin->so_path_filename );
		}
		
		if( p_logpipe_input_plugin->fd >= 0 )
		{
			/* 加入侦听可读事件到epoll */
			memset( & event , 0x00 , sizeof(struct epoll_event) );
			event.events = EPOLLIN | EPOLLERR ;
			event.data.ptr = p_logpipe_input_plugin ;
			nret = epoll_ctl( p_env->epoll_fd , EPOLL_CTL_ADD , p_logpipe_input_plugin->fd , & event ) ;
			if( nret == -1 )
			{
				ERRORLOG( "epoll_ctl[%d] add inotify_session[%s] failed , errno[%d]" , p_env->epoll_fd , p_logpipe_input_plugin->fd , errno );
				return -1;
			}
		}
		
		if( p_logpipe_input_plugin->fd < 0 )
			p_env->p_block_input_plugin = p_logpipe_input_plugin ;
	}
	
	/* 执行所有输出端初始化函数 */
	list_for_each_entry( p_logpipe_output_plugin , & (p_env->logpipe_outputs_plugin_list->this_node) , struct LogpipeOutputPlugin , this_node )
	{
		p_logpipe_output_plugin->context = NULL ;
		nret = p_logpipe_output_plugin->pfuncInitLogpipeOutputPlugin( p_env , & (p_logpipe_output_plugin->context) ) ;
		if( nret )
		{
			ERRORLOG( "[%s]p_logpipe_output_plugin->pfuncInitLogpipeOutputPlugin failed , errno[%d]" , p_logpipe_output_plugin->so_path_filename , errno );
			return -1;
		}
		else
		{
			INFOLOG( "[%s]p_logpipe_output_plugin->pfuncInitLogpipeOutputPlugin ok" , p_logpipe_output_plugin->so_path_filename );
		}
	}
	
	
	
	
	
	
	
#if 0
	int			i ;
	struct InotifySession	*p_inotify_session = NULL ;
	struct ListenSession	*p_listen_session = NULL ;
	struct DumpSession	*p_dump_session = NULL ;
	struct ForwardSession	*p_forward_session = NULL ;
	struct epoll_event	event ;
	
	int			nret = 0 ;
	
	INIT_LIST_HEAD( & (p_env->inotify_session_list.this_node) );
	INIT_LIST_HEAD( & (p_env->listen_session_list.this_node) );
	INIT_LIST_HEAD( & (p_env->dump_session_list.this_node) );
	INIT_LIST_HEAD( & (p_env->forward_session_list.this_node) );
	
	/* 装载所有输入端 */
	for( i = 0 ; i < p_env->conf._inputs_count ; i++ )
	{
		if( STRNCMP( p_env->conf.inputs[i].input , == , LOGPIPE_IO_TYPE_FILE , sizeof(LOGPIPE_IO_TYPE_FILE)-1 ) )
		{
			p_inotify_session = (struct InotifySession *)malloc( sizeof(struct InotifySession) ) ;
			if( p_inotify_session == NULL )
			{
				printf( "ERROR : malloc failed , errno[%d]\n" , errno );
				return -1;
			}
			memset( p_inotify_session , 0x00 , sizeof(struct InotifySession) );
			
			p_inotify_session->session_type = LOGPIPE_SESSION_TYPE_INOTIFY ;
			
			strncpy( p_inotify_session->inotify_path , p_env->conf.inputs[i].input+sizeof(LOGPIPE_IO_TYPE_FILE)-1-1 , sizeof(p_inotify_session->inotify_path)-1 );
			
			p_inotify_session->inotify_fd = inotify_init() ;
			if( p_inotify_session->inotify_fd == -1 )
			{
				printf( "ERROR : inotify_init failed , errno[%d]\n" , errno );
				return -1;
			}
			
			p_inotify_session->inotify_path_wd = inotify_add_watch( p_inotify_session->inotify_fd , p_inotify_session->inotify_path , (uint32_t)(IN_CREATE|IN_MOVED_TO|IN_DELETE_SELF|IN_MOVE_SELF) );
			if( p_inotify_session->inotify_path_wd == -1 )
			{
				printf( "ERROR : inotify_add_watch[%s] failed , errno[%d]\n" , p_inotify_session->inotify_path , errno );
				return -1;
			}
			
			nret = ReadFilesToinotifyWdTree( p_env , p_inotify_session ) ;
			if( nret )
			{
				return nret;
			}
			
			list_add_tail( & (p_inotify_session->this_node) , & (p_env->inotify_session_list.this_node) );
			
			/* 加入侦听可读事件到epoll */
			memset( & event , 0x00 , sizeof(struct epoll_event) );
			event.events = EPOLLIN | EPOLLERR ;
			event.data.ptr = p_inotify_session ;
			nret = epoll_ctl( p_env->epoll_fd , EPOLL_CTL_ADD , p_inotify_session->inotify_fd , & event ) ;
			if( nret == -1 )
			{
				printf( "ERROR : epoll_ctl[%d] add inotify_session[%s] failed , errno[%d]\n" , p_env->epoll_fd , p_inotify_session->inotify_path , errno );
				return -1;
			}
		}
		else if( STRNCMP( p_env->conf.inputs[i].input , == , LOGPIPE_IO_TYPE_TCP , sizeof(LOGPIPE_IO_TYPE_TCP)-1 ) )
		{
			p_listen_session = (struct ListenSession *)malloc( sizeof(struct ListenSession) ) ;
			if( p_listen_session == NULL )
			{
				printf( "ERROR : malloc failed , errno[%d]\n" , errno );
				return -1;
			}
			memset( p_listen_session , 0x00 , sizeof(struct ListenSession) );
			
			p_listen_session->session_type = LOGPIPE_SESSION_TYPE_LISTEN ;
			
			/* 创建套接字 */
			p_listen_session->listen_sock = socket( AF_INET , SOCK_STREAM , IPPROTO_TCP ) ;
			if( p_listen_session->listen_sock == -1 )
			{
				printf( "ERROR : socket failed , errno[%d]\n" , errno );
				return -1;
			}
			
			/* 设置套接字选项 */
			{
				int	opts ;
				opts = fcntl( p_listen_session->listen_sock , F_GETFL ) ;
				opts |= O_NONBLOCK ;
				fcntl( p_listen_session->listen_sock , F_SETFL , opts ) ;
			}
			
			{
				int	onoff = 1 ;
				setsockopt( p_listen_session->listen_sock , SOL_SOCKET , SO_REUSEADDR , (void *) & onoff , sizeof(int) );
			}
			
			{
				int	onoff = 1 ;
				setsockopt( p_listen_session->listen_sock , IPPROTO_TCP , TCP_NODELAY , (void*) & onoff , sizeof(int) );
			}
			
			/* 绑定套接字到侦听端口 */
			memset( p_listen_session->listen_ip , 0x00 , sizeof(p_listen_session->listen_ip) );
			p_listen_session->listen_port = 0 ;
			sscanf( p_env->conf.inputs[i].input+sizeof(LOGPIPE_IO_TYPE_TCP)-1 , "%[^:]:%d" , p_listen_session->listen_ip , & (p_listen_session->listen_port) );
			if( p_listen_session->listen_ip[0] == '\0' || p_listen_session->listen_port <= 0 )
			{
				printf( "ERROR : tcp config [%s] invalid\n" , p_env->conf.inputs[i].input );
				return -1;
			}
			
			memset( & (p_listen_session->listen_addr) , 0x00 , sizeof(struct sockaddr_in) );
			p_listen_session->listen_addr.sin_family = AF_INET ;
			if( p_listen_session->listen_ip[0] == '\0' )
				p_listen_session->listen_addr.sin_addr.s_addr = INADDR_ANY ;
			else
				p_listen_session->listen_addr.sin_addr.s_addr = inet_addr(p_listen_session->listen_ip) ;
			p_listen_session->listen_addr.sin_port = htons( (unsigned short)(p_listen_session->listen_port) );
			nret = bind( p_listen_session->listen_sock , (struct sockaddr *) & (p_listen_session->listen_addr) , sizeof(struct sockaddr) ) ;
			if( nret == -1 )
			{
				printf( "ERROR : bind[%s:%d][%d] failed , errno[%d]\n" , p_listen_session->listen_ip , p_listen_session->listen_port , p_listen_session->listen_sock , errno );
				return -1;
			}
			
			/* 处于侦听状态了 */
			nret = listen( p_listen_session->listen_sock , 10240 ) ;
			if( nret == -1 )
			{
				printf( "ERROR : listen[%s:%d][%d] failed , errno[%d]\n" , p_listen_session->listen_ip , p_listen_session->listen_port , p_listen_session->listen_sock , errno );
				return -1;
			}
			
			/* 加入可读事件到epoll */
			memset( & event , 0x00 , sizeof(struct epoll_event) );
			event.events = EPOLLIN | EPOLLERR ;
			event.data.ptr = p_listen_session ;
			nret = epoll_ctl( p_env->epoll_fd , EPOLL_CTL_ADD , p_listen_session->listen_sock , & event ) ;
			if( nret == -1 )
			{
				printf( "ERROR : epoll_ctl[%d] add listen_session[%s:%d] EPOLLIN failed , errno[%d]\n" , p_env->epoll_fd , p_listen_session->listen_ip , p_listen_session->listen_port , errno );
				return -1;
			}
			
			/* 初始化已连接会话链表 */
			INIT_LIST_HEAD( & (p_listen_session->accepted_session_list.this_node) );
			
			list_add_tail( & (p_listen_session->this_node) , & (p_env->listen_session_list.this_node) );
		}
		else
		{
			printf( "ERROR : input[%s] invalid\n" , p_env->conf.inputs[i].input );
			return -1;
		}
	}
	
	/* 装载所有输出端 */
	for( i = 0 ; i < p_env->conf._outputs_count ; i++ )
	{
		if( STRNCMP( p_env->conf.outputs[i].output , == , LOGPIPE_IO_TYPE_FILE , sizeof(LOGPIPE_IO_TYPE_FILE)-1 ) )
		{
			p_dump_session = (struct DumpSession *)malloc( sizeof(struct DumpSession) ) ;
			if( p_dump_session == NULL )
			{
				printf( "ERROR : malloc failed , errno[%d]\n" , errno );
				return -1;
			}
			memset( p_dump_session , 0x00 , sizeof(struct DumpSession) );
			
			p_dump_session->session_type = LOGPIPE_SESSION_TYPE_DUMP ;
			
			strncpy( p_dump_session->dump_path , p_env->conf.outputs[i].output+sizeof(LOGPIPE_IO_TYPE_FILE)-1-1 , sizeof(p_dump_session->dump_path)-1 );
			
			list_add_tail( & (p_dump_session->this_node) , & (p_env->dump_session_list.this_node) );
		}
		else if( STRNCMP( p_env->conf.outputs[i].output , == , LOGPIPE_IO_TYPE_TCP , sizeof(LOGPIPE_IO_TYPE_TCP)-1 ) )
		{
			p_forward_session = (struct ForwardSession *)malloc( sizeof(struct ForwardSession) ) ;
			if( p_forward_session == NULL )
			{
				printf( "ERROR : malloc failed , errno[%d]\n" , errno );
				return -1;
			}
			memset( p_forward_session , 0x00 , sizeof(struct ForwardSession) );
			
			p_forward_session->session_type = LOGPIPE_SESSION_TYPE_FORWARD ;
			
			/* 绑定套接字到侦听端口 */
			memset( p_forward_session->forward_ip , 0x00 , sizeof(p_forward_session->forward_ip) );
			p_forward_session->forward_port = 0 ;
			sscanf( p_env->conf.outputs[i].output+sizeof(LOGPIPE_IO_TYPE_TCP)-1 , "%[^:]:%d" , p_forward_session->forward_ip , & (p_forward_session->forward_port) );
			if( p_forward_session->forward_ip[0] == '\0' || p_forward_session->forward_port <= 0 )
			{
				printf( "ERROR : tcp config [%s] invalid\n" , p_env->conf.outputs[i].output );
				return -1;
			}
			
			memset( & (p_forward_session->forward_addr) , 0x00 , sizeof(struct sockaddr_in) );
			p_forward_session->forward_addr.sin_family = AF_INET ;
			if( p_forward_session->forward_ip[0] == '\0' )
				p_forward_session->forward_addr.sin_addr.s_addr = INADDR_ANY ;
			else
				p_forward_session->forward_addr.sin_addr.s_addr = inet_addr(p_forward_session->forward_ip) ;
			p_forward_session->forward_addr.sin_port = htons( (unsigned short)(p_forward_session->forward_port) );
			
			list_add_tail( & (p_forward_session->this_node) , & (p_env->forward_session_list.this_node) );
		}
		else
		{
			printf( "ERROR : output[%s] invalid\n" , p_env->conf.outputs[i].output );
			return -1;
		}
	}
	
	/* 分配堆内存用于inotify读缓冲区 */
	p_env->inotify_read_bufsize = LOGPIPE_INOTIFY_READ_BUFSIZE ;
	p_env->inotify_read_buffer = (char*)malloc( p_env->inotify_read_bufsize ) ;
	if( p_env->inotify_read_buffer == NULL )
	{
		printf( "ERROR : malloc failed , errno[%d]\n" , errno );
		return -1;
	}
	memset( p_env->inotify_read_buffer , 0x00 , p_env->inotify_read_bufsize );
#endif
	
	return 0;
}

void CleanEnvironment( struct LogpipeEnv *p_env )
{
	struct LogpipeInputPlugin	*p_logpipe_input_plugin = NULL ;
	struct LogpipeOutputPlugin	*p_logpipe_output_plugin = NULL ;
	struct LogpipeInputPlugin	*p_next_logpipe_input_plugin = NULL ;
	struct LogpipeOutputPlugin	*p_next_logpipe_output_plugin = NULL ;
	
	/* 执行所有输入端初始化函数 */
	list_for_each_entry_safe( p_logpipe_input_plugin , p_next_logpipe_input_plugin , & (p_env->logpipe_inputs_plugin_list->this_node) , struct LogpipeInputPlugin , this_node )
	{
		nret = p_logpipe_input_plugin->pfuncCleanLogpipeInputPlugin( p_env , & (p_logpipe_input_plugin->context) ) ;
		if( nret )
		{
			ERRORLOG( "[%s]p_logpipe_input_plugin->pfuncCleanLogpipeInputPlugin failed , errno[%d]" , p_logpipe_input_plugin->so_path_filename , errno );
			return -1;
		}
		
		RemoveAllPluginConfigItem( & (p_logpipe_input_plugin->plugin_config_items) );
	}
	
	/* 执行所有输出端初始化函数 */
	list_for_each_entry_safe( p_logpipe_output_plugin , p_next_logpipe_output_plugin , & (p_env->logpipe_outputs_plugin_list->this_node) , struct LogpipeOutputPlugin , this_node )
	{
		nret = p_logpipe_output_plugin->pfuncCleanLogpipeOutputPlugin( p_env , & (p_logpipe_output_plugin->context) ) ;
		if( nret )
		{
			ERRORLOG( "[%s]p_logpipe_output_plugin->pfuncCleanLogpipeOutputPlugin failed , errno[%d]" , p_logpipe_output_plugin->so_path_filename , errno );
			return -1;
		}
		
		RemoveAllPluginConfigItem( & (p_logpipe_output_plugin->plugin_config_items) );
	}
	
	/* 销毁epoll */
	close( p_env->epoll_fd );
	
	
	
	
	
#if 0
	struct list_head	*p_node = NULL ;
	struct list_head	*p_next_node = NULL ;
	struct list_head	*p_node2 = NULL ;
	struct list_head	*p_next_node2 = NULL ;
	struct InotifySession	*p_inotify_session = NULL ;
	struct AcceptedSession	*p_accepted_session = NULL ;
	struct ListenSession	*p_listen_session = NULL ;
	struct DumpSession	*p_dump_session = NULL ;
	struct ForwardSession	*p_forward_session = NULL ;
	
	/* 卸载所有输入端 */
	list_for_each_safe( p_node , p_next_node , & (p_env->inotify_session_list.this_node) )
	{
		p_inotify_session = list_entry( p_node , struct InotifySession , this_node ) ;
		
		list_del( p_node );
		
		if( p_env->is_monitor )
		{
			close( p_inotify_session->inotify_fd );
			DEBUGLOG( "close inotify fd[%d]" , p_inotify_session->inotify_fd )
		}
		free( p_inotify_session );
	}
	
	list_for_each_safe( p_node , p_next_node , & (p_env->listen_session_list.this_node) )
	{
		p_listen_session = list_entry( p_node , struct ListenSession , this_node ) ;
		
		list_del( p_node );
		
		close( p_listen_session->listen_sock );
		free( p_listen_session );
		
		list_for_each_safe( p_node2 , p_next_node2 , & (p_listen_session->accepted_session_list.this_node) )
		{
			list_del( p_node2 );
			
			close( p_accepted_session->accepted_sock );
			free( p_accepted_session );
		}
	}
	
	/* 卸载所有输出端 */
	list_for_each_safe( p_node , p_next_node , & (p_env->dump_session_list.this_node) )
	{
		p_dump_session = list_entry( p_node , struct DumpSession , this_node ) ;
		
		list_del( p_node );
		
		free( p_dump_session );
	}
	
	list_for_each_safe( p_node , p_next_node , & (p_env->forward_session_list.this_node) )
	{
		p_forward_session = list_entry( p_node , struct ForwardSession , this_node ) ;
		
		list_del( p_node );
		
		close( p_forward_session->forward_sock );
		free( p_forward_session );
	}
	
	/* 释放inotify读缓冲区 */
	free( p_env->inotify_read_buffer );
#endif
	
	return;
}

#if 0
void LogEnvironment( struct LogpipeEnv *p_env )
{
	struct InotifySession	*p_inotify_session = NULL ;
	struct ListenSession	*p_listen_session = NULL ;
	struct DumpSession	*p_dump_session = NULL ;
	struct ForwardSession	*p_forward_session = NULL ;
	
	list_for_each_entry( p_inotify_session , & (p_env->inotify_session_list.this_node) , struct InotifySession , this_node )
	{
		INFOLOG( "input : inotify - inotify_path[%s] inotify_fd[%d] inotify_path_wd[%d]" , p_inotify_session->inotify_path , p_inotify_session->inotify_fd , p_inotify_session->inotify_path_wd )
	}
	
	list_for_each_entry( p_listen_session , & (p_env->listen_session_list.this_node) , struct ListenSession , this_node )
	{
		INFOLOG( "input : listen - listen_ip[%s] listen_port[%d] listen_sock[%d]" , p_listen_session->listen_ip , p_listen_session->listen_port , p_listen_session->listen_sock )
	}
	
	list_for_each_entry( p_dump_session , & (p_env->dump_session_list.this_node) , struct DumpSession , this_node )
	{
		INFOLOG( "output : dump - dump_path[%s]" , p_dump_session->dump_path )
	}
	
	list_for_each_entry( p_forward_session , & (p_env->forward_session_list.this_node) , struct ForwardSession , this_node )
	{
		INFOLOG( "output : forward - forward_ip[%s] forward_port[%d] forward_sock[%d]" , p_forward_session->forward_ip , p_forward_session->forward_port , p_forward_session->forward_sock )
	}
	
	return;
}
#endif

