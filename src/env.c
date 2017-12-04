#include "logpipe_in.h"

static int ReadFilesToinotifyWdTree( struct InotifySession *p_inotify_session )
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
			nret = AddFileWatcher( p_inotify_session , ent->d_name ) ;
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

int InitEnvironment( struct LogPipeEnv *p_env )
{
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
	
	/* 创建epoll */
	p_env->epoll_fd = epoll_create( 1024 ) ;
	if( p_env->epoll_fd == -1 )
	{
		printf( "*** ERROR : epoll_create failed , errno[%d]\n" , errno );
		return -1;
	}
	
	/* 装载所有输入端 */
	for( i = 0 ; i < p_env->conf._input_count ; i++ )
	{
		if( p_env->conf.input[i].inotify_path[0] )
		{
			p_inotify_session = (struct InotifySession *)malloc( sizeof(struct InotifySession) ) ;
			if( p_inotify_session == NULL )
			{
				printf( "*** ERROR : malloc failed , errno[%d]\n" , errno );
				return -1;
			}
			memset( p_inotify_session , 0x00 , sizeof(struct InotifySession) );
			
			p_inotify_session->session_type = LOGPIPE_SESSION_TYPE_MONITOR ;
			
			strncpy( p_inotify_session->inotify_path , p_env->conf.input[i].inotify_path , sizeof(p_inotify_session->inotify_path)-1 );
			
			p_inotify_session->inotify_fd = inotify_init() ;
			if( p_inotify_session->inotify_fd == -1 )
			{
				printf( "*** ERROR : inotify_init failed , errno[%d]\n" , errno );
				return -1;
			}
			
			p_inotify_session->inotify_path_wd = inotify_add_watch( p_inotify_session->inotify_fd , p_inotify_session->inotify_path , (uint32_t)(IN_CLOSE_WRITE|IN_DELETE_SELF|IN_MOVE_SELF|IN_IGNORED) ) ;
			if( p_inotify_session->inotify_path_wd == -1 )
			{
				printf( "*** ERROR : inotify_add_watch[%s] failed , errno[%d]\n" , p_inotify_session->inotify_path , errno );
				return -1;
			}
			
			nret = ReadFilesToinotifyWdTree( p_inotify_session ) ;
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
				printf( "*** ERROR : epoll_ctl[%d] add inotify_session[%s] failed , errno[%d]\n" , p_env->epoll_fd , p_inotify_session->inotify_path , errno );
				return -1;
			}
		}
		else if( p_env->conf.input[i].listen_ip[0] && p_env->conf.input[i].listen_port > 0 )
		{
			p_listen_session = (struct ListenSession *)malloc( sizeof(struct ListenSession) ) ;
			if( p_listen_session == NULL )
			{
				printf( "*** ERROR : malloc failed , errno[%d]\n" , errno );
				return -1;
			}
			memset( p_listen_session , 0x00 , sizeof(struct ListenSession) );
			
			p_listen_session->session_type = LOGPIPE_SESSION_TYPE_LISTEN ;
			
			/* 创建套接字 */
			p_listen_session->listen_sock = socket( AF_INET , SOCK_STREAM , IPPROTO_TCP ) ;
			if( p_listen_session->listen_sock == -1 )
			{
				printf( "*** ERROR : socket failed , errno[%d]\n" , errno );
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
			memset( & (p_listen_session->listen_addr) , 0x00 , sizeof(struct sockaddr_in) );
			p_listen_session->listen_addr.sin_family = AF_INET ;
			if( p_env->conf.input[i].listen_ip[0] == '\0' )
				p_listen_session->listen_addr.sin_addr.s_addr = INADDR_ANY ;
			else
				p_listen_session->listen_addr.sin_addr.s_addr = inet_addr(p_env->conf.input[i].listen_ip) ;
			p_listen_session->listen_addr.sin_port = htons( (unsigned short)(p_env->conf.input[i].listen_port) );
			nret = bind( p_listen_session->listen_sock , (struct sockaddr *) & (p_listen_session->listen_addr) , sizeof(struct sockaddr) ) ;
			if( nret == -1 )
			{
				printf( "*** ERROR : bind[%s:%d][%d] failed , errno[%d]\n" , p_env->conf.input[i].listen_ip , p_env->conf.input[i].listen_port , p_listen_session->listen_sock , errno );
				return -1;
			}
			
			/* 处于侦听状态了 */
			nret = listen( p_listen_session->listen_sock , 10240 ) ;
			if( nret == -1 )
			{
				printf( "*** ERROR : listen[%s:%d][%d] failed , errno[%d]\n" , p_env->conf.input[i].listen_ip , p_env->conf.input[i].listen_port , p_listen_session->listen_sock , errno );
				return -1;
			}
			
			/* 加入侦听可读事件到epoll */
			memset( & event , 0x00 , sizeof(struct epoll_event) );
			event.events = EPOLLIN | EPOLLERR ;
			event.data.ptr = p_listen_session ;
			nret = epoll_ctl( p_env->epoll_fd , EPOLL_CTL_ADD , p_listen_session->listen_sock , & event ) ;
			if( nret == -1 )
			{
				printf( "*** ERROR : epoll_ctl[%d] add listen_session[%s:%d] failed , errno[%d]\n" , p_env->epoll_fd , p_env->conf.input[i].listen_ip , p_env->conf.input[i].listen_port , errno );
				return -1;
			}
			
			/* 初始化已连接会话链表 */
			INIT_LIST_HEAD( & (p_listen_session->accepted_session_list.this_node) );
		}
		else
		{
			printf( "*** ERROR : input[%d].* invalid\n" , i );
			return -1;
		}
	}
	
	/* 装载所有输出端 */
	for( i = 0 ; i < p_env->conf._output_count ; i++ )
	{
		if( p_env->conf.output[i].dump_path[0] )
		{
			p_dump_session = (struct DumpSession *)malloc( sizeof(struct DumpSession) ) ;
			if( p_dump_session == NULL )
			{
				printf( "*** ERROR : malloc failed , errno[%d]\n" , errno );
				return -1;
			}
			memset( p_dump_session , 0x00 , sizeof(struct DumpSession) );
			
			p_dump_session->session_type = LOGPIPE_SESSION_TYPE_MONITOR ;
			
			strncpy( p_dump_session->dump_path , p_env->conf.output[i].dump_path , sizeof(p_dump_session->dump_path)-1 );
			
			list_add_tail( & (p_dump_session->this_node) , & (p_env->dump_session_list.this_node) );
		}
		else if( p_env->conf.output[i].forward_ip[0] && p_env->conf.output[i].forward_port > 0 )
		{
			p_forward_session = (struct ForwardSession *)malloc( sizeof(struct ForwardSession) ) ;
			if( p_forward_session == NULL )
			{
				printf( "*** ERROR : malloc failed , errno[%d]\n" , errno );
				return -1;
			}
			memset( p_forward_session , 0x00 , sizeof(struct ListenSession) );
			
			p_forward_session->session_type = LOGPIPE_SESSION_TYPE_LISTEN ;
			
			/* 创建套接字 */
			p_forward_session->forward_sock = socket( AF_INET , SOCK_STREAM , IPPROTO_TCP ) ;
			if( p_forward_session->forward_sock == -1 )
			{
				printf( "*** ERROR : socket failed , errno[%d]\n" , errno );
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
			
			/* 绑定套接字到侦听端口 */
			strcpy( p_forward_session->forward_ip , p_env->conf.output[i].forward_ip );
			p_forward_session->forward_port = p_env->conf.output[i].forward_port ;
			
			memset( & (p_forward_session->forward_addr) , 0x00 , sizeof(struct sockaddr_in) );
			p_forward_session->forward_addr.sin_family = AF_INET ;
			if( p_env->conf.output[i].forward_ip[0] == '\0' )
				p_forward_session->forward_addr.sin_addr.s_addr = INADDR_ANY ;
			else
				p_forward_session->forward_addr.sin_addr.s_addr = inet_addr(p_env->conf.output[i].forward_ip) ;
			p_forward_session->forward_addr.sin_port = htons( (unsigned short)(p_env->conf.output[i].forward_port) );
			nret = connect( p_forward_session->forward_sock , (struct sockaddr *) & (p_forward_session->forward_addr) , sizeof(struct sockaddr) ) ;
			if( nret == -1 )
			{
				printf( "*** ERROR : connect[%s:%d] failed , errno[%d]\n" , p_env->conf.output[i].forward_ip , p_env->conf.output[i].forward_port , errno );
				return -1;
			}
		}
		else
		{
			printf( "*** ERROR : output[%d].* invalid\n" , i );
			return -1;
		}
	}
	
	return 0;
}

int CleanEnvironment( struct LogPipeEnv *p_env )
{
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
		
		close( p_inotify_session->inotify_fd );
		free( p_inotify_session );
		
		list_del( p_node );
	}
	
	list_for_each_safe( p_node , p_next_node , & (p_env->listen_session_list.this_node) )
	{
		p_listen_session = list_entry( p_node , struct ListenSession , this_node ) ;
		
		close( p_listen_session->listen_sock );
		free( p_listen_session );
		
		list_for_each_safe( p_node2 , p_next_node2 , & (p_listen_session->accepted_session_list.this_node) )
		{
			close( p_accepted_session->accepted_sock );
			free( p_accepted_session->comm_buf );
			free( p_accepted_session );
			
			list_del( p_node2 );
		}
		
		list_del( p_node );
	}
	
	/* 卸载所有输出端 */
	list_for_each_safe( p_node , p_next_node , & (p_env->dump_session_list.this_node) )
	{
		p_dump_session = list_entry( p_node , struct DumpSession , this_node ) ;
		
		free( p_dump_session );
		
		list_del( p_node );
	}
	
	list_for_each_safe( p_node , p_next_node , & (p_env->forward_session_list.this_node) )
	{
		p_forward_session = list_entry( p_node , struct ForwardSession , this_node ) ;
		
		close( p_forward_session->forward_sock );
		free( p_forward_session );
		
		list_del( p_node );
	}
	
	return 0;
}

