#include "logpipe_in.h"

void InitConfig()
{
	logpipe_conf	conf ;
	char		config_path_filename[ PATH_MAX + 1 ] ;
	char		*file_content = NULL ;
	
	int		nret = 0 ;
	
	DSCINIT_logpipe_conf( & conf );
	
	snprintf( conf.inputs[0].input , sizeof(conf.inputs[0].input)-1 , "file://%s/log" , getenv("HOME") );
	conf._inputs_count++;
	
	strcpy( conf.inputs[1].input , "tcp://127.0.0.1:10101" );
	conf._inputs_count++;
	
	snprintf( conf.outputs[0].output , sizeof(conf.outputs[0].output)-1 , "file://%s/log2" , getenv("HOME") );
	conf._outputs_count++;
	
	strcpy( conf.outputs[1].output , "tcp://127.0.0.1:10101" );
	conf._outputs_count++;
	
	conf.rotate.file_rotate_max_size = 0 ;
	
	strcpy( conf.comm.compress_algorithm , "deflate" );
	
	snprintf( conf.log.log_file , sizeof(conf.log.log_file)-1 , "%s/log3/logpipe.log" , getenv("HOME") );
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
	if( nret == 0 )
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
	if( file_content == NULL )
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
	
	if( STRCMP( p_env->conf.comm.compress_algorithm , == , LOGPIPE_COMM_HEAD_COMPRESS_ALGORITHM_S ) )
	{
		p_env->compress_algorithm = 'Z' ;
	}
	else if( STRCMP( p_env->conf.comm.compress_algorithm , == , "" ) )
	{
		p_env->compress_algorithm = 0 ;
	}
	else
	{
		printf( "*** ERROR : comm.compress_algorithm[%s] invalid\n" , p_env->conf.comm.compress_algorithm );
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

