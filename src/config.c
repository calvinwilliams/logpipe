#include "logpipe_in.h"

void InitConfig()
{
	logpipe_conf	conf ;
	char		config_path_filename[ PATH_MAX + 1 ] ;
	char		*file_content = NULL ;
	
	int		nret = 0 ;
	
	DSCINIT_logpipe_conf( & conf );
	
	snprintf( conf.input[0].inotify_path , sizeof(conf.input[0].inotify_path)-1 , "%s/log" , getenv("HOME") );
	conf._input_count++;
	
	strcpy( conf.input[1].listen_ip , "127.0.0.1" );
	conf.input[1].listen_port = 10101 ;
	conf._input_count++;
	
	snprintf( conf.output[0].dump_path , sizeof(conf.output[0].dump_path)-1 , "%s/log2" , getenv("HOME") );
	conf._output_count++;
	
	strcpy( conf.output[1].forward_ip , "127.0.0.1" );
	conf.output[1].forward_port = 10101 ;
	conf._output_count++;
	
	snprintf( conf.log.log_file , sizeof(conf.log.log_file)-1 , "%s/log3" , getenv("HOME") );
	strcpy( conf.log.log_level , "ERROR" );
	
	nret = DSCSERIALIZE_JSON_DUP_logpipe_conf( & conf , "GB18030" , & file_content , NULL , NULL ) ;
	if( nret )
	{
		printf( "*** ERROR : DSCSERIALIZE_JSON_DUP_logpipe_conf failed[%d] , errno[%d]\n" , nret , errno );
		return;
	}
	
	memset( config_path_filename , 0x00 , sizeof(config_path_filename) );
	snprintf( config_path_filename , sizeof(config_path_filename)-1 , "%s/etc/logpipe.conf" , getenv("HOME") );
	nret = access( config_path_filename , F_OK ) ;
	if( nret == -1 )
	{
		printf( "*** ERROR : file[%s] exist\n" , config_path_filename );
		free( file_content );
		return;
	}
	
	nret = WriteEntireFile( config_path_filename , file_content , -1 ) ;
	free( file_content );
	if( nret )
	{
		printf( "*** ERROR : fopen[%s] failed[%d] , errno[%d]\n" , config_path_filename , nret , errno );
		return;
	}
	
	return;
}

int LoadConfig( struct LogPipeEnv *p_env )
{
	char		*file_content = NULL ;
	int		file_len ;
	
	int		nret = 0 ;
	
	file_content = StrdupEntireFile( p_env->config_path_filename , NULL ) ;
	if( file_content )
	{
		printf( "*** ERROR : open file[%s] failed , errno[%d]\n" , p_env->config_path_filename , errno );
		return -1;
	}
	
	file_len = 0 ;
	nret = DSCDESERIALIZE_JSON_logpipe_conf( "GB18030" , file_content , & file_len , & (p_env->conf) ) ;
	free( file_content );
	if( nret )
	{
		printf( "*** ERROR : DSCDESERIALIZE_JSON_logpipe_conf failed[%d] , errno[%d]\n" , nret , errno );
		return -1;
	}
	
	if( STRCMP( p_env->conf.log.log_level , == , "DEBUG" ) )
		p_env->log_level = LOGLEVEL_DEBUG ;
	else if( STRCMP( p_env->conf.log.log_level , == , "INFO" ) )
		p_env->log_level = LOGLEVEL_INFO ;
	else if( STRCMP( p_env->conf.log.log_level , == , "WARN" ) )
		p_env->log_level = LOGLEVEL_WARN ;
	else if( STRCMP( p_env->conf.log.log_level , == , "ERROR" ) )
		p_env->log_level = LOGLEVEL_ERROR ;
	else if( STRCMP( p_env->conf.log.log_level , == , "FATAL" ) )
		p_env->log_level = LOGLEVEL_FATAL ;
	else
	{
		printf( "*** ERROR : log.log_level[%s] invalid\n" , p_env->conf.log.log_level );
		return -1;
	}
	
	return 0;
}

