/*
 * logpipe - Distribute log collector
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "logpipe_in.h"

static sig_atomic_t		g_QUIT_flag = 0 ;

static void sig_set_flag( int sig_no )
{
	/* 接收到不同信号设置不同的全局标志，延后到主流程中处理 */
	if( sig_no == SIGTERM )
	{
		g_QUIT_flag = 1 ; /* 退出 */
	}
	
	return;
}

static void SetStartOnceEnv( struct LogpipePluginConfigItem *start_once )
{
	struct LogpipePluginConfigItem	*item = NULL ;
	
	list_for_each_entry( item , & (start_once->this_node) , struct LogpipePluginConfigItem , this_node )
	{
		setenv( item->key , item->value , 1 );
	}
	
	return;
}

static void UnsetStartOnceEnv( struct LogpipePluginConfigItem *start_once )
{
	struct LogpipePluginConfigItem	*item = NULL ;
	
	list_for_each_entry( item , & (start_once->this_node) , struct LogpipePluginConfigItem , this_node )
	{
		unsetenv( item->key );
	}
	
	return;
}

int monitor( struct LogpipeEnv *p_env )
{
	struct sigaction	act ;
	
	pid_t			pid , pid2 ;
	int			status ;
	
	int			nret = 0 ;
	
	/* 设置信号 */
	if( ! p_env->no_daemon )
	{
		signal( SIGINT , SIG_IGN );
	}
	signal( SIGCLD , SIG_DFL );
	signal( SIGCHLD , SIG_DFL );
	signal( SIGPIPE , SIG_IGN );
	act.sa_handler = & sig_set_flag ;
	sigemptyset( & (act.sa_mask) );
	act.sa_flags = 0 ;
	sigaction( SIGTERM , & act , NULL );
	act.sa_flags = SA_RESTART ;
	signal( SIGCLD , SIG_DFL );
	
	SetStartOnceEnv( & (p_env->start_once_for_plugin_config_items) );
	
	while( g_QUIT_flag == 0 )
	{
		nret = pipe( p_env->quit_pipe ) ;
		if( nret == -1 )
		{
			FATALLOG( "pipe failed , errno[%d]" , errno )
			return -1;
		}
		
		/* 创建工作进程 */
		pid = fork() ;
		if( pid == -1 )
		{
			FATALLOG( "fork failed , errno[%d]" , errno )
			return -1;
		}
		else if( pid == 0 )
		{
			close( p_env->quit_pipe[1] );
			INFOLOG( "child : [%ld] fork [%ld]" , getppid() , getpid() )
			return -worker( p_env );
		}
		else
		{
			close( p_env->quit_pipe[0] );
			INFOLOG( "parent : [%ld] fork [%ld]" , getpid() , pid )
			UnsetStartOnceEnv( & (p_env->start_once_for_plugin_config_items) );
		}
		
_GOTO_WAITPID :
		
		/* 堵塞等待工作进程结束 */
		DEBUGLOG( "waitpid ..." )
		pid2 = waitpid( pid , & status , 0 );
		if( pid2 == -1 )
		{
			if( errno == EINTR )
			{
				/* 如果被退出信号中断，退出 */
				if( g_QUIT_flag )
				{
					if( p_env->quit_pipe[1] >= 0 )
					{
						close( p_env->quit_pipe[1] ); p_env->quit_pipe[1] = -1 ;
					}
					else
					{
						kill( pid , SIGKILL );
					}
					goto _GOTO_WAITPID;
				}
			}
			
			FATALLOG( "waitpid failed , errno[%d]" , errno )
			return -1;
		}
		else if( pid2 != pid )
		{
			FATALLOG( "unexpect other child[%d]" , pid2 )
		}
		
		/* 检查工作进程是否正常结束 */
		if( WEXITSTATUS(status) == 0 && WIFSIGNALED(status) == 0 && WTERMSIG(status) == 0 )
		{
			INFOLOG( "waitpid[%d] WEXITSTATUS[%d] WIFSIGNALED[%d] WTERMSIG[%d]" , pid , WEXITSTATUS(status) , WIFSIGNALED(status) , WTERMSIG(status) )
		}
		else
		{
			FATALLOG( "waitpid[%d] WEXITSTATUS[%d] WIFSIGNALED[%d] WTERMSIG[%d]" , pid , WEXITSTATUS(status) , WIFSIGNALED(status) , WTERMSIG(status) )
		}
		
		if( p_env->quit_pipe[1] >= 0 )
		{
			close( p_env->quit_pipe[1] ); p_env->quit_pipe[1] = -1 ;
		}
		
		sleep(1);
		
		/* 重新创建命令管道，创建工作进程 */
	}
	
	return 0;
}

int _monitor( void *pv )
{
	struct LogpipeEnv	*p_env = (struct LogpipeEnv *)pv ;
	
	int			nret = 0 ;
	
	SetLogFile( p_env->log_file );
	SetLogLevel( p_env->log_level );
	
	INFOLOG( "--- monitor begin ---------" )
	
	nret = monitor( p_env ) ;
	
	INFOLOG( "--- monitor end ---------" )
	
	return nret;
}

