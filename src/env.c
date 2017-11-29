#include "logpipe_in.h"

int InitEnvironment( struct LogPipeEnv *p_env )
{
	int			nret = 0 ;
	
	if( p_env->role == LOGPIPE_ROLE_COLLECTOR )
	{
		static uint32_t		inotify_mask = IN_CREATE|IN_MOVED_TO|IN_DELETE_SELF|IN_MOVE_SELF ;
		
		p_env->role_context.collector.inotify_fd = inotify_init() ;
		if( p_env->role_context.collector.inotify_fd == -1 )
		{
			printf( "*** ERROR : inotify_init failed , errno[%d]\n" , errno );
			return -1;
		}
		
		p_env->role_context.collector.inotify_path_wd = inotify_add_watch( p_env->role_context.collector.inotify_fd , p_env->role_context.collector.monitor_path , inotify_mask ) ;
		if( p_env->role_context.collector.inotify_path_wd == -1 )
		{
			printf( "*** ERROR : inotify_add_watch[%s] failed , errno[%d]\n" , p_env->role_context.collector.monitor_path , errno );
			return -1;
		}
	}
	else if( p_env->role == LOGPIPE_ROLE_COLLECTOR )
	{
		struct epoll_event      event ;
		
		/* 创建套接字 */
		p_env->role_context.dumpserver.listen_sock = socket( AF_INET , SOCK_STREAM , IPPROTO_TCP ) ;
		if( p_env->role_context.dumpserver.listen_sock == -1 )
		{
			printf( "*** ERROR : socket failed , errno[%d]\n" , errno );
			return -1;
		}
		
		{
			int	opts ;
			opts = fcntl( p_env->role_context.dumpserver.listen_sock , F_GETFL ) ;
			opts |= O_NONBLOCK ;
			fcntl( p_env->role_context.dumpserver.listen_sock , F_SETFL , opts ) ;
		}
		
		{
			int	onoff = 1 ;
			setsockopt( p_env->role_context.dumpserver.listen_sock , SOL_SOCKET , SO_REUSEADDR , (void *) & onoff , sizeof(int) );
		}
		
		{
			int	onoff = 1 ;
			setsockopt( p_env->role_context.dumpserver.listen_sock , IPPROTO_TCP , TCP_NODELAY , (void*) & onoff , sizeof(int) );
		}
		
		/* 绑定套接字到侦听端口 */
		memset( & (p_env->role_context.dumpserver.listen_addr) , 0x00 , sizeof(struct sockaddr_in) );
		p_env->role_context.dumpserver.listen_addr.sin_family = AF_INET ;
		if( p_env->role_context.dumpserver.listen_ip[0] == '\0' )
			p_env->role_context.dumpserver.listen_addr.sin_addr.s_addr = INADDR_ANY ;
		else
			p_env->role_context.dumpserver.listen_addr.sin_addr.s_addr = inet_addr(p_env->role_context.dumpserver.listen_ip) ;
		p_env->role_context.dumpserver.listen_addr.sin_port = htons( (unsigned short)(p_env->role_context.dumpserver.listen_port) );
		nret = bind( p_env->role_context.dumpserver.listen_sock , (struct sockaddr *) & (p_env->role_context.dumpserver.listen_addr) , sizeof(struct sockaddr) ) ;
		if( nret == -1 )
		{
			printf( "*** ERROR : bind[%s:%d][%d] failed , errno[%d]\n" , p_env->role_context.dumpserver.listen_ip , p_env->role_context.dumpserver.listen_port , p_env->role_context.dumpserver.listen_sock , errno );
			close( p_env->role_context.dumpserver.listen_sock );
			return -1;
		}
		
		/* 处于侦听状态了 */
		nret = listen( p_env->role_context.dumpserver.listen_sock , 10240 ) ;
		if( nret == -1 )
		{
			printf( "*** ERROR : listen[%s:%d][%d] failed , errno[%d]\n" , p_env->role_context.dumpserver.listen_ip , p_env->role_context.dumpserver.listen_port , p_env->role_context.dumpserver.listen_sock , errno );
			close( p_env->role_context.dumpserver.listen_sock );
			return -1;
		}
		
		/* 创建epoll */
		p_env->role_context.dumpserver.epoll_fd = epoll_create( 1024 ) ;
		if( p_env->role_context.dumpserver.epoll_fd == -1 )
		{
			printf( "*** ERROR : epoll_create failed , errno[%d]\n" , errno );
			return -1;
		}
		
		/* 加入侦听可读事件到epoll */
		memset( & event , 0x00 , sizeof(struct epoll_event) );
		event.events = EPOLLIN | EPOLLERR ;
		event.data.ptr = p_env ;
		nret = epoll_ctl( p_env->role_context.dumpserver.epoll_fd , EPOLL_CTL_ADD , p_env->role_context.dumpserver.listen_sock , & event ) ;
		if( nret == -1 )
		{
			printf( "*** ERROR : epoll_ctl[%d] add listen_session failed , errno[%d]\n" , p_env->role_context.dumpserver.epoll_fd , errno );
			close( p_env->role_context.dumpserver.epoll_fd );
			return -1;
		}
	}
	
	return 0;
}

int CleanEnvironment( struct LogPipeEnv *p_env )
{
	if( p_env->role == LOGPIPE_ROLE_COLLECTOR )
	{
		close( p_env->role_context.collector.inotify_fd );
	}
	else if( p_env->role == LOGPIPE_ROLE_DUMPSERVER )
	{
		close( p_env->role_context.dumpserver.listen_sock );
		close( p_env->role_context.dumpserver.epoll_fd );
	}
	
	return 0;
}

