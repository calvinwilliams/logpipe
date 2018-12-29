#include "logpipe_api.h"

#include "rdkafka.h"

#ifdef _WITH_ZOOKEEPER
#include "zookeeper.h"
#endif

#include "IDL_zookeeper_broker_ids.dsc.c"

#define KAFKA_BROKER_PATH	"/brokers/ids"

int	__LOGPIPE_OUTPUT_KAFKA_VERSION_0_1_0 = 1 ;

/* 启动zookeeper
cd ~/zookeeper
bin/zkServer.sh start
bin/zkServer.sh status
bin/zkServer.sh stop
bin/zkCli.sh -server zookeeper1:2181
quit
*/

/* 启动kafka
cd ~/kafka
bin/kafka-server-start.sh config/server.properties &
*/

/* 列表所有主题
~/expack/kafka/bin/kafka-topics.sh --list --zookeeper 127.0.0.1:2181
or
bin/kafka-topics.sh --create --zookeeper zookeeper1:2181,zookeeper2:2182,zookeeper3:2183 --replication-factor 3 --partitions 3 --topic test_topic
bin/kafka-topics.sh --describe --zookeeper zookeeper1:2181,zookeeper2:2182,zookeeper3:2183 --topic test_topic
bin/kafka-topics.sh --list --zookeeper zookeeper1:2181,zookeeper2:2182,zookeeper3:2183
*/

/* 生产者发消息
~/expack/kafka/bin/kafka-console-producer.sh --broker-list 127.0.0.1:9092 --topic test_topic
or
bin/kafka-console-producer.sh --broker-list kafka1:9092,kafka2:9093,kafka3:9094 --topic test_topic
*/

/* 启动消费者
~/expack/kafka/bin/kafka-console-consumer.sh --zookeeper 127.0.0.1:2181 --topic test_topic --from-beginning
or
bin/kafka-console-consumer.sh --bootstrap-server kafka1:9092,kafka2:9093,kafka3:9094 --topic test_topic --from-beginning
*/

/* 编译logpipe-output-kafka.so 
make logpipe-output-kafka.so  && rm -f $HOME/so/logpipe-output-kafka.so && cp -f logpipe-output-kafka.so $HOME/so/
*/

/* 编译logpipe-output-kafka-with-zookeeper.so
make logpipe-output-kafka-with-zookeeper.so && rm -f $HOME/so/logpipe-output-kafka-with-zookeeper.so && cp -f logpipe-output-kafka-with-zookeeper.so $HOME/so/
*/

/* 清理日志
rmlog ; rm -f logpipe_case6_input_file_and_output_kafka.log.*
*/

struct OutputPluginContext
{
#ifdef _WITH_ZOOKEEPER
	char			*zookeeper ;
	zhandle_t		*zh ;
	char			brokers[ 1024 ] ;
#else
	char			*bootstrap_servers ;
#endif
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
		ERRORLOGC( "Message delivery failed , err[%d][%s]" , kafka_message->err , rd_kafka_err2str(kafka_message->err) )
	}
	else
	{
		INFOLOGC( "Message delivery ok , [%d]bytes [%d]partition" , kafka_message->len , kafka_message->partition )
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
		ERRORLOGC( "malloc failed , errno[%d]" , errno )
		return -1;
	}
	memset( p_plugin_ctx , 0x00 , sizeof(struct OutputPluginContext) );
	
#ifdef _WITH_ZOOKEEPER
	p_plugin_ctx->zookeeper = QueryPluginConfigItem( p_plugin_config_items , "zookeeper" ) ;
	INFOLOGC( "zookeeper[%s]" , p_plugin_ctx->zookeeper )
	if( p_plugin_ctx->zookeeper == NULL || p_plugin_ctx->zookeeper[0] == '\0' )
	{
		ERRORLOGC( "expect config for 'zookeeper'" )
		return -1;
	}
#else
	p_plugin_ctx->bootstrap_servers = QueryPluginConfigItem( p_plugin_config_items , "bootstrap_servers" ) ;
	INFOLOGC( "bootstrap_servers[%s]" , p_plugin_ctx->bootstrap_servers )
	if( p_plugin_ctx->bootstrap_servers == NULL || p_plugin_ctx->bootstrap_servers[0] == '\0' )
	{
		ERRORLOGC( "expect config for 'bootstrap_servers'" )
		return -1;
	}
#endif
	
	p_plugin_ctx->topic = QueryPluginConfigItem( p_plugin_config_items , "topic" ) ;
	INFOLOGC( "topic[%s]" , p_plugin_ctx->topic )
	if( p_plugin_ctx->topic == NULL || p_plugin_ctx->topic[0] == '\0' )
	{
		ERRORLOGC( "expect config for 'topic'" )
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
			ERRORLOGC( "uncompress_algorithm[%s] invalid" , p_plugin_ctx->uncompress_algorithm )
			return -1;
		}
	}
	INFOLOGC( "uncompress_algorithm[%s]" , p_plugin_ctx->uncompress_algorithm )
	
	/* 设置插件环境上下文 */
	(*pp_context) = p_plugin_ctx ;
	
	return 0;
}

static void set_brokerlist_from_zookeeper( zhandle_t *zh , char *brokers , int brokers_bufsize )
{
	char			*brokers_offset_ptr = NULL ;
	int			brokers_buf_remain_len ;
	int			len ;
	
	int			i ;
	char			path[ 256 ] ;
	char			msg[ 1024 ] ;
	int			msg_len ;
	zookeeper_broker_ids	st ;
	
	int			nret = 0 ;
	
	if( zh )
	{
		struct String_vector	brokerlist ;
		
		nret = zoo_get_children( zh , KAFKA_BROKER_PATH , 1 , & brokerlist ) ;
		if( nret != ZOK )
		{
			ERRORLOGC( "zoo_get_children '%s' failed[%d]" , KAFKA_BROKER_PATH , nret )
			return;
		}
		else
		{
			INFOLOGC( "zoo_get_children '%s' ok , brokerlist.count[%d]" , KAFKA_BROKER_PATH , brokerlist.count )
		}
		
		memset( brokers , 0x00 , brokers_bufsize );
		brokers_offset_ptr = brokers ;
		brokers_buf_remain_len = brokers_bufsize-1 ;
		for( i = 0 ; i < brokerlist.count ; i++ )
		{
			memset( path , 0x00 , sizeof(path) );
			snprintf( path , sizeof(path) , "%s/%s", KAFKA_BROKER_PATH , brokerlist.data[i] );
			memset( msg , 0x00 , sizeof(msg) );
			msg_len = sizeof(msg) ;
			zoo_get( zh , path , 0 , msg , & msg_len , NULL );
			if( msg_len > 0 )
			{
				DEBUGLOGC( "msg[%.*s]" , msg_len , msg )
				memset( & st , 0x00 , sizeof(zookeeper_broker_ids) );
				msg_len = 0 ;
				nret = DSCDESERIALIZE_JSON_zookeeper_broker_ids( "GB18030" , msg , & msg_len , & st ) ;
				if( nret )
				{
					ERRORLOGC( "DSCDESERIALIZE_JSON_zookeeper_broker_ids failed[%d]" , nret )
					return;
				}
				else
				{
					INFOLOGC( "DSCDESERIALIZE_JSON_zookeeper_broker_ids ok" )
				}
				
				if( i > 0 )
				{
					len = snprintf( brokers_offset_ptr , brokers_buf_remain_len , "," ) ;
					if( len > 0 )
					{
						brokers_offset_ptr += len ;
						brokers_buf_remain_len -= len ;
					}
				}
				
				len = snprintf( brokers_offset_ptr , brokers_buf_remain_len , "%s:%d" , st.host , st.port ) ;
				if( len > 0 )
				{
					brokers_offset_ptr += len ;
					brokers_buf_remain_len -= len ;
				}
			}
		}
		
		deallocate_String_vector( & brokerlist );
	}
	
	return;
}

static void watcher( zhandle_t *zh , int type , int state , const char *path , void *watcherCtx )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)watcherCtx ;
	
	DEBUGLOGC( "watcher - zh[%p] type[%d] state[%d] path[%s] watcherCtx[%p]" , zh , type , state , path , watcherCtx )
	if( type == ZOO_CHILD_EVENT && strncmp( path , KAFKA_BROKER_PATH , sizeof(KAFKA_BROKER_PATH)-1) == 0 )
	{
		set_brokerlist_from_zookeeper( zh , p_plugin_ctx->brokers , sizeof(p_plugin_ctx->brokers) );
		DEBUGLOGC( "watcher - p_plugin_ctx->brokers[%s]" , p_plugin_ctx->brokers )
		if( p_plugin_ctx->brokers[0] && p_plugin_ctx->kafka )
		{
			rd_kafka_brokers_add( p_plugin_ctx->kafka , p_plugin_ctx->brokers );
			DEBUGLOGC( "watcher - rd_kafka_brokers_add p_plugin_ctx->brokers[%s]" , p_plugin_ctx->brokers )
			rd_kafka_poll( p_plugin_ctx->kafka , 10 );
			DEBUGLOGC( "watcher - rd_kafka_poll p_plugin_ctx->brokers[%s]" , p_plugin_ctx->brokers )
		}
	}
	
	return;
}

funcInitOutputPluginContext InitOutputPluginContext ;
int InitOutputPluginContext( struct LogpipeEnv *p_env , struct LogpipeOutputPlugin *p_logpipe_output_plugin , void *p_context )
{
	struct OutputPluginContext	*p_plugin_ctx = (struct OutputPluginContext *)p_context ;
	
	rd_kafka_conf_res_t		kafka_conf_res ;
	char				errstr[ 512 ] ;
	
	p_plugin_ctx->kafka_conf = rd_kafka_conf_new() ;
	
#ifdef _WITH_ZOOKEEPER
	p_plugin_ctx->zh = zookeeper_init( p_plugin_ctx->zookeeper , watcher , 10000 , 0 , p_plugin_ctx , 0 ) ;
	if( p_plugin_ctx->zh == NULL )
	{
		ERRORLOGC( "zookeeper_init failed" )
		return -1;
	}
	else
	{
		INFOLOGC( "zookeeper_init '%s' ok" , p_plugin_ctx->zookeeper )
	}
	
	set_brokerlist_from_zookeeper( p_plugin_ctx->zh , p_plugin_ctx->brokers , sizeof(p_plugin_ctx->brokers) ) ;
	
	kafka_conf_res = rd_kafka_conf_set( p_plugin_ctx->kafka_conf , "metadata.broker.list" , p_plugin_ctx->brokers , errstr , sizeof(errstr) ) ;
	if( kafka_conf_res != RD_KAFKA_CONF_OK )
	{
		ERRORLOGC( "rd_kafka_conf_set metadata.broker.list '%s' failed[%d] , errstr[%s]" , p_plugin_ctx->brokers , kafka_conf_res , errstr )
		return -1;
	}
	else
	{
		INFOLOGC( "rd_kafka_conf_set metadata.broker.list '%s' ok" , p_plugin_ctx->brokers )
	}
#else
	kafka_conf_res = rd_kafka_conf_set( p_plugin_ctx->kafka_conf , "bootstrap.servers" , p_plugin_ctx->bootstrap_servers , errstr , sizeof(errstr) ) ;
	if( kafka_conf_res != RD_KAFKA_CONF_OK )
	{
		ERRORLOGC( "rd_kafka_conf_set bootstrap.servers '%s' failed[%d] , errstr[%s]" , p_plugin_ctx->bootstrap_servers , kafka_conf_res , errstr )
		return -1;
	}
	else
	{
		INFOLOGC( "rd_kafka_conf_set bootstrap.servers '%s' ok" , p_plugin_ctx->bootstrap_servers )
	}
#endif
	
	rd_kafka_conf_set_dr_msg_cb( p_plugin_ctx->kafka_conf , dr_msg_cb );
	
	p_plugin_ctx->kafka = rd_kafka_new( RD_KAFKA_PRODUCER , p_plugin_ctx->kafka_conf , errstr , sizeof(errstr) ) ;
	if( p_plugin_ctx->kafka == NULL )
	{
		ERRORLOGC( "rd_kafka_new failed , errstr[%s]" , errstr )
		return -1;
	}
	
	p_plugin_ctx->kafka_topic = rd_kafka_topic_new( p_plugin_ctx->kafka , p_plugin_ctx->topic , NULL ) ;
	if( p_plugin_ctx->kafka_topic == NULL )
	{
		ERRORLOGC( "rd_kafka_topic_new failed , last_error[%s]" , rd_kafka_err2str(rd_kafka_last_error()) )
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
			ERRORLOGC( "rd_kafka_produce block data to topic[%s] failed , last_error[%s]" , rd_kafka_topic_name(p_plugin_ctx->kafka_topic) , rd_kafka_err2str(rd_kafka_last_error()) )
			if( rd_kafka_last_error() == RD_KAFKA_RESP_ERR__QUEUE_FULL )
			{
				rd_kafka_poll( p_plugin_ctx->kafka , 1000 );
				goto _GOTO_RETRY_PRODUCE_1;
			}
			return 1;
		}
		else
		{
			INFOLOGC( "rd_kafka_produce block data to topic[%s] ok , [%d]bytes" , rd_kafka_topic_name(p_plugin_ctx->kafka_topic) , block_len )
			DEBUGHEXLOGC( block_buf , block_len , NULL )
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
				ERRORLOGC( "UncompressInputPluginData failed[%d]" , nret )
				return -1;
			}
			else
			{
				DEBUGLOGC( "UncompressInputPluginData ok" )
			}
			
_GOTO_RETRY_PRODUCE_2 :
			nret = rd_kafka_produce( p_plugin_ctx->kafka_topic , RD_KAFKA_PARTITION_UA , RD_KAFKA_MSG_F_COPY , block_out_buf , block_out_len , NULL , 0 , (void*)p_plugin_ctx ) ;
			if( nret == -1 )
			{
				ERRORLOGC( "rd_kafka_produce block data to topic[%s] failed , last_error[%s]" , rd_kafka_topic_name(p_plugin_ctx->kafka_topic) , rd_kafka_err2str(rd_kafka_last_error()) )
				if( rd_kafka_last_error() == RD_KAFKA_RESP_ERR__QUEUE_FULL )
				{
					rd_kafka_poll( p_plugin_ctx->kafka , 1000 );
					goto _GOTO_RETRY_PRODUCE_2;
				}
				return 1;
			}
			else
			{
				INFOLOGC( "rd_kafka_produce block data to topic[%s] ok , [%d]bytes" , rd_kafka_topic_name(p_plugin_ctx->kafka_topic) , block_out_len )
				DEBUGHEXLOGC( block_out_buf , block_out_len , NULL )
			}
			rd_kafka_poll( p_plugin_ctx->kafka , 0 );
		}
		else
		{
			ERRORLOGC( "uncompress_algorithm[%s] invalid" , p_plugin_ctx->uncompress_algorithm )
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
	
	zookeeper_close( p_plugin_ctx->zh );
	
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

