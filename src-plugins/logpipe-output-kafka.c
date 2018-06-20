#include "logpipe_api.h"

#include "rdkafka.h"

char	*__LOGPIPE_OUTPUT_KAFKA_VERSION = "0.1.0" ;

/*
列表所有主题
~/expack/kafka/bin/kafka-topics.sh --list --zookeeper 127.0.0.1:2181

生产者发消息
~/expack/kafka/bin/kafka-console-producer.sh --broker-list 127.0.0.1:9092 --topic test_topic

启动消费者
~/expack/kafka/bin/kafka-console-consumer.sh --zookeeper 127.0.0.1:2181 --topic test_topic --from-beginning
*/

struct OutputPluginContext
{
	char			*bootstrap_servers ;
	char			*topic ;
	
	rd_kafka_conf_t		*kafka_conf ;
	rd_kafka_t		*kafka ;
	rd_kafka_topic_t	*kafka_topic ;
	
	char			*uncompress_algorithm ;
	
} ;

static void dr_msg_cb( rd_kafka_t *kafka , const rd_kafka_message_t *kafka_message , void *opaque )
{
	if( kafka_message->err )
	{
		ERRORLOG( "Message delivery failed , err[%d][%s]" , kafka_message->err , rd_kafka_err2str(kafka_message->err) );
	}
	else
	{
		INFOLOG( "Message delivery ok , [%d]bytes [%d]partition" , kafka_message->len , kafka_message->partition );
	}
	
	return;
}

funcLoadOutputPluginConfig LoadOutputPluginConfig ;
int LoadOutputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , struct LogpipePluginConfigItem *p_plugin_config_items , void **pp_context )
{
	struct OutputPluginContext	*p_plugin_ctx = NULL ;
	
	/* 申请内存以存放插件上下文 */
	p_plugin_ctx = (struct OutputPluginContext *)malloc( sizeof(struct OutputPluginContext) ) ;
	if( p_plugin_ctx == NULL )
	{
		ERRORLOG( "malloc failed , errno[%d]" , errno );
		return -1;
	}
	memset( p_plugin_ctx , 0x00 , sizeof(struct OutputPluginContext) );
	
	p_plugin_ctx->bootstrap_servers = QueryPluginConfigItem( p_plugin_config_items , "bootstrap_servers" ) ;
	INFOLOG( "bootstrap_servers[%s]" , p_plugin_ctx->bootstrap_servers )
	if( p_plugin_ctx->bootstrap_servers == NULL || p_plugin_ctx->bootstrap_servers[0] == '\0' )
	{
		ERRORLOG( "expect config for 'bootstrap_servers'" );
		return -1;
	}
	
	p_plugin_ctx->topic = QueryPluginConfigItem( p_plugin_config_items , "topic" ) ;
	INFOLOG( "topic[%s]" , p_plugin_ctx->topic )
	if( p_plugin_ctx->topic == NULL || p_plugin_ctx->topic[0] == '\0' )
	{
		ERRORLOG( "expect config for 'topic'" );
		return -1;
	}
	
	p_plugin_ctx->uncompress_algorithm = QueryPluginConfigItem( p_plugin_config_items , "uncompress_algorithm" ) ;
	if( p_plugin_ctx->uncompress_algorithm )
	{
		if( STRCMP( p_plugin_ctx->uncompress_algorithm , == , "deflate" ) )
		{
			;
		}
		else
		{
			ERRORLOG( "uncompress_algorithm[%s] invalid" , p_plugin_ctx->uncompress_algorithm );
			return -1;
		}
	}
	INFOLOG( "uncompress_algorithm[%s]" , p_plugin_ctx->uncompress_algorithm )
	
	/* 设置插件环境上下文 */
	(*pp_context) = p_plugin_ctx ;
	
	return 0;
}

funcInitOutputPluginContext InitOutputPluginContext ;
int InitOutputPluginContext( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	rd_kafka_conf_res_t		kafka_conf_res ;
	char				errstr[ 512 ] ;
	
	p_plugin_ctx->kafka_conf = rd_kafka_conf_new() ;
	
	kafka_conf_res = rd_kafka_conf_set( p_plugin_ctx->kafka_conf , "bootstrap.servers" , p_plugin_ctx->bootstrap_servers , errstr , sizeof(errstr) ) ;
	if( kafka_conf_res != RD_KAFKA_CONF_OK )
	{
		ERRORLOG( "rd_kafka_conf_set failed[%d] , errstr[%s]" , kafka_conf_res , errstr );
		return -1;
	}
	
	rd_kafka_conf_set_dr_msg_cb( p_plugin_ctx->kafka_conf , dr_msg_cb );
	
	p_plugin_ctx->kafka = rd_kafka_new( RD_KAFKA_PRODUCER , p_plugin_ctx->kafka_conf , errstr , sizeof(errstr) ) ;
	if( p_plugin_ctx->kafka == NULL )
	{
		ERRORLOG( "rd_kafka_new failed , errstr[%s]" , errstr );
		return -1;
	}
	
	p_plugin_ctx->kafka_topic = rd_kafka_topic_new( p_plugin_ctx->kafka , p_plugin_ctx->topic , NULL ) ;
	if( p_plugin_ctx->kafka_topic == NULL )
	{
		ERRORLOG( "rd_kafka_topic_new failed , last_error[%s]" , rd_kafka_err2str(rd_kafka_last_error()) );
		rd_kafka_destroy( p_plugin_ctx->kafka );
		return -1;
	}
	
	return 0;
}

funcOnOutputPluginIdle OnOutputPluginIdle;
int OnOutputPluginIdle( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	rd_kafka_poll( p_plugin_ctx->kafka , 0 );
	
	return 0;
}

funcOnOutputPluginEvent OnOutputPluginEvent;
int OnOutputPluginEvent( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context )
{
	return 0;
}

funcBeforeWriteOutputPlugin BeforeWriteOutputPlugin ;
int BeforeWriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint16_t filename_len , char *filename )
{
	return 0;
}

funcWriteOutputPlugin WriteOutputPlugin ;
int WriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint64_t file_offset , uint64_t file_line , uint64_t block_len , char *block_buf )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	int				nret = 0 ;
	
	/* 如果未启用解压 */
	if( p_plugin_ctx->uncompress_algorithm == NULL )
	{
_GOTO_RETRY_PRODUCE_1 :
		nret = rd_kafka_produce( p_plugin_ctx->kafka_topic , RD_KAFKA_PARTITION_UA , RD_KAFKA_MSG_F_COPY , block_buf , block_len , NULL , 0 , (void*)p_plugin_ctx ) ;
		if( nret == -1 )
		{
			ERRORLOG( "rd_kafka_produce block data to topic[%s] failed , last_error[%s]" , rd_kafka_topic_name(p_plugin_ctx->kafka_topic) , rd_kafka_err2str(rd_kafka_last_error()) )
			if( rd_kafka_last_error() == RD_KAFKA_RESP_ERR__QUEUE_FULL )
			{
				rd_kafka_poll( p_plugin_ctx->kafka , 1000 );
				goto _GOTO_RETRY_PRODUCE_1;
			}
			return 1;
		}
		else
		{
			INFOLOG( "rd_kafka_produce block data to topic[%s] ok , [%d]bytes" , rd_kafka_topic_name(p_plugin_ctx->kafka_topic) , block_len )
			DEBUGHEXLOG( block_buf , block_len , NULL )
		}
		rd_kafka_poll( p_plugin_ctx->kafka , 0 );
	}
	/* 如果启用了解压 */
	else
	{
		if( STRCMP( p_plugin_ctx->uncompress_algorithm , == , "deflate" ) )
		{
			char			block_out_buf[ LOGPIPE_BLOCK_BUFSIZE + 1 ] ;
			uint64_t		block_out_len ;
			
			memset( block_out_buf , 0x00 , sizeof(block_out_buf) );
			nret = UncompressInputPluginData( p_plugin_ctx->uncompress_algorithm , block_buf , block_len , block_out_buf , & block_out_len ) ;
			if( nret )
			{
				ERRORLOG( "UncompressInputPluginData failed[%d]" , nret )
				return -1;
			}
			else
			{
				DEBUGLOG( "UncompressInputPluginData ok" )
			}
			
_GOTO_RETRY_PRODUCE_2 :
			nret = rd_kafka_produce( p_plugin_ctx->kafka_topic , RD_KAFKA_PARTITION_UA , RD_KAFKA_MSG_F_COPY , block_out_buf , block_out_len , NULL , 0 , (void*)p_plugin_ctx ) ;
			if( nret == -1 )
			{
				ERRORLOG( "rd_kafka_produce block data to topic[%s] failed , last_error[%s]" , rd_kafka_topic_name(p_plugin_ctx->kafka_topic) , rd_kafka_err2str(rd_kafka_last_error()) )
				if( rd_kafka_last_error() == RD_KAFKA_RESP_ERR__QUEUE_FULL )
				{
					rd_kafka_poll( p_plugin_ctx->kafka , 1000 );
					goto _GOTO_RETRY_PRODUCE_2;
				}
				return 1;
			}
			else
			{
				INFOLOG( "rd_kafka_produce block data to topic[%s] ok , [%d]bytes" , rd_kafka_topic_name(p_plugin_ctx->kafka_topic) , block_out_len )
				DEBUGHEXLOG( block_out_buf , block_out_len , NULL )
			}
			rd_kafka_poll( p_plugin_ctx->kafka , 0 );
		}
		else
		{
			ERRORLOG( "uncompress_algorithm[%s] invalid" , p_plugin_ctx->uncompress_algorithm );
			return -1;
		}
	}
	
	return 0;
}

funcAfterWriteOutputPlugin AfterWriteOutputPlugin ;
int AfterWriteOutputPlugin( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context , uint16_t filename_len , char *filename )
{
	return 0;
}

funcCleanOutputPluginContext CleanOutputPluginContext ;
int CleanOutputPluginContext( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	rd_kafka_flush( p_plugin_ctx->kafka , 10*1000 );
	
	rd_kafka_topic_destroy( p_plugin_ctx->kafka_topic );
	
	rd_kafka_destroy( p_plugin_ctx->kafka );
	
	return 0;
}

funcUnloadOutputPluginConfig UnloadOutputPluginConfig ;
int UnloadOutputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void **pp_context )
{
	struct OutputPluginContext	**pp_plugin_ctx = (struct OutputPluginContext **)pp_context ;
	
	/* 释放内存以存放插件上下文 */
	free( (*pp_plugin_ctx) ); (*pp_plugin_ctx) = NULL ;
	
	return 0;
}

