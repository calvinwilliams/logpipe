#include "logpipe_api.h"

#include "rdkafka.h"

#ifdef _WITH_ZOOKEEPER
#include "kafka_with_zookeeper.h"
#endif

int	__LOGPIPE_INPUT_KAFKA_VERSION_0_1_0 = 1 ;

/* 编译logpipe-input-kafka.so 
make logpipe-input-kafka.so  && rm -f $HOME/so/logpipe-input-kafka.so && cp -f logpipe-input-kafka.so $HOME/so/
*/

/* 编译logpipe-input-kafka-with-zookeeper.so
make logpipe-input-kafka-with-zookeeper.so && rm -f $HOME/so/logpipe-input-kafka-with-zookeeper.so && cp -f logpipe-input-kafka-with-zookeeper.so $HOME/so/
*/

struct InputPluginContext
{
#ifdef _WITH_ZOOKEEPER
	char				*zookeeper ;
	zhandle_t			*zh ;
	struct KafkaWatcherContext	kafka_watcher_context ;
#else
	char				*bootstrap_servers ;
#endif
	
	char				*group ;
	char				*topic ;
	
	rd_kafka_conf_t			*kafka_conf ;
	rd_kafka_t			*kafka ;
	rd_kafka_topic_t		*kafka_topic ;
	
	char				*uncompress_algorithm ;
	
} ;

funcLoadInputPluginConfig LoadInputPluginConfig ;
int LoadInputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , struct LogpipePluginConfigItem *p_plugin_config_items , void **pp_context )
{
	struct InputPluginContext	*p_plugin_ctx = NULL ;
	
	/* 申请内存以存放插件上下文 */
	p_plugin_ctx = (struct InputPluginContext *)malloc( sizeof(struct InputPluginContext) ) ;
	if( p_plugin_ctx == NULL )
	{
		ERRORLOGC( "malloc failed , errno[%d]" , errno )
		return -1;
	}
	memset( p_plugin_ctx , 0x00 , sizeof(struct InputPluginContext) );
	
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
	
	p_plugin_ctx->group = QueryPluginConfigItem( p_plugin_config_items , "group" ) ;
	INFOLOGC( "group[%s]" , p_plugin_ctx->group )
	if( p_plugin_ctx->group == NULL || p_plugin_ctx->group[0] == '\0' )
	{
		ERRORLOGC( "expect config for 'group'" )
		return -1;
	}
	
	p_plugin_ctx->topic = QueryPluginConfigItem( p_plugin_config_items , "topic" ) ;
	INFOLOGC( "topic[%s]" , p_plugin_ctx->topic )
	if( p_plugin_ctx->topic == NULL || p_plugin_ctx->topic[0] == '\0' )
	{
		ERRORLOGC( "expect config for 'topic'" )
		return -1;
	}
	
	/* 设置插件环境上下文 */
	(*pp_context) = p_plugin_ctx ;
	
	return 0;
}

funcInitInputPluginContext InitInputPluginContext ;
int InitInputPluginContext( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	struct InputPluginContext	*p_plugin_ctx = (struct InputPluginContext *)p_context ;
	
	rd_kafka_topic_conf_t		*topic_conf = NULL ;
	rd_kafka_topic_partition_list_t	*topics = NULL ;
	
	rd_kafka_conf_res_t		kafka_conf_res ;
	char				errstr[ 512 ] ;
	
	int				nret = 0 ;
	
	p_plugin_ctx->kafka_conf = rd_kafka_conf_new() ;
	
#ifdef _WITH_ZOOKEEPER
	p_plugin_ctx->kafka_watcher_context.kafka = p_plugin_ctx->kafka ;
	p_plugin_ctx->zh = zookeeper_init( p_plugin_ctx->zookeeper , & KafkaWatcher , 10000 , 0 , & (p_plugin_ctx->kafka_watcher_context) , 0 ) ;
	if( p_plugin_ctx->zh == NULL )
	{
		ERRORLOGC( "zookeeper_init failed" )
		return -1;
	}
	else
	{
		INFOLOGC( "zookeeper_init '%s' ok" , p_plugin_ctx->zookeeper )
	}
	
	GetBrokerListFromZookeeper( p_plugin_ctx->zh , & (p_plugin_ctx->kafka_watcher_context) ) ;
	
	kafka_conf_res = rd_kafka_conf_set( p_plugin_ctx->kafka_conf , "metadata.broker.list" , p_plugin_ctx->kafka_watcher_context.brokers , errstr , sizeof(errstr) ) ;
	if( kafka_conf_res != RD_KAFKA_CONF_OK )
	{
		ERRORLOGC( "rd_kafka_conf_set metadata.broker.list '%s' failed[%d] , errstr[%s]" , p_plugin_ctx->kafka_watcher_context.brokers , kafka_conf_res , errstr )
		return -1;
	}
	else
	{
		INFOLOGC( "rd_kafka_conf_set metadata.broker.list '%s' ok" , p_plugin_ctx->kafka_watcher_context.brokers )
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
	
	nret = rd_kafka_conf_set( p_plugin_ctx->kafka_conf , "group.id" , p_plugin_ctx->group , errstr , sizeof(errstr) ) ;
	if( nret != RD_KAFKA_CONF_OK )
	{
		ERRORLOGC( "rd_kafka_conf_set failed , errstr[%s]" , errstr )
		rd_kafka_destroy( p_plugin_ctx->kafka );
		return -1;
	}
	
	rd_kafka_conf_set_default_topic_conf( p_plugin_ctx->kafka_conf , topic_conf );
	
	p_plugin_ctx->kafka = rd_kafka_new( RD_KAFKA_CONSUMER , p_plugin_ctx->kafka_conf , errstr , sizeof(errstr) ) ;
	if( p_plugin_ctx->kafka == NULL )
	{
		ERRORLOGC( "rd_kafka_new failed , errstr[%s]" , errstr )
		return -1;
	}
	
	rd_kafka_poll_set_consumer( p_plugin_ctx->kafka );
	
	p_plugin_ctx->kafka_topic = rd_kafka_topic_new( p_plugin_ctx->kafka , p_plugin_ctx->topic , NULL ) ;
	if( p_plugin_ctx->kafka_topic == NULL )
	{
		ERRORLOGC( "rd_kafka_topic_new failed , last_error[%s]" , rd_kafka_err2str(rd_kafka_last_error()) )
		rd_kafka_destroy( p_plugin_ctx->kafka );
		return -1;
	}
	
	topic_conf = rd_kafka_topic_conf_new() ;
	if( topic_conf == NULL )
	{
		ERRORLOGC( "rd_kafka_topic_conf_new failed , errno[%s]" , errno )
		rd_kafka_destroy( p_plugin_ctx->kafka );
		return -1;
	}
	
	nret = rd_kafka_topic_conf_set( topic_conf , "offset.store.method" , "broker" , errstr , sizeof(errstr) ) ;
	if( nret != RD_KAFKA_CONF_OK )
	{
		ERRORLOGC( "rd_kafka_topic_conf_set failed , errstr[%s]" , errstr )
		rd_kafka_destroy( p_plugin_ctx->kafka );
		return -1;
	}
	
	topics = rd_kafka_topic_partition_list_new(1) ;
	rd_kafka_topic_partition_list_add( topics , p_plugin_ctx->topic , -1 );
	
	rd_kafka_subscribe( p_plugin_ctx->kafka , topics );
	
#if 0
	// nret = rd_kafka_consume_start( p_plugin_ctx->kafka_topic , RD_KAFKA_PARTITION_UA , RD_KAFKA_OFFSET_END ) ;
	nret = rd_kafka_consume_start( p_plugin_ctx->kafka_topic , 1 , RD_KAFKA_OFFSET_END ) ;
	if( nret == -1 )
	{
		ERRORLOGC( "rd_kafka_consume_start failed , last_error[%s]" , rd_kafka_err2str(rd_kafka_last_error()) )
		rd_kafka_destroy( p_plugin_ctx->kafka );
		return -1;
	}
#endif
	
	return 0;
}

funcOnInputPluginEvent OnInputPluginEvent ;
int OnInputPluginEvent( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	int				nret = 0 ;
	
	while(1)
	{
		nret = WriteAllOutputPlugins( p_env , p_logpipe_input_plugin , 8 , "demo.log" ) ;
		if( nret < 0 )
		{
			ERRORLOGC( "WriteAllOutputPlugins failed[%d]" , nret )
			return -1;
		}
		else if( nret > 0 )
		{
			WARNLOGC( "WriteAllOutputPlugins return[%d]" , nret )
			return 0;
		}
		else
		{
			INFOLOGC( "WriteAllOutputPlugins ok" )
		}
	}
	
	return 0;
}

funcReadInputPlugin ReadInputPlugin ;
int ReadInputPlugin( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context , uint64_t *p_file_offset , uint64_t *p_file_line , uint64_t *p_block_len , char *block_buf , uint64_t block_bufsize )
{
	struct InputPluginContext	*p_plugin_ctx = (struct InputPluginContext *)p_context ;
	
	rd_kafka_message_t		*kafka_message = NULL ;
	
	kafka_message = rd_kafka_consume( p_plugin_ctx->kafka_topic , RD_KAFKA_PARTITION_UA , 1000 ) ;
	if( kafka_message == NULL )
	{
		DEBUGLOGC( "rd_kafka_consume timeout" )
		return 0;;
	}
	
	INFOLOGC( "topic[%s] partition[%"PRId32"] offset[%"PRId64"] key[%.*s] msg[%.*s]"
		, rd_kafka_topic_name(kafka_message->rkt)
		, kafka_message->partition
		, kafka_message->offset
		, kafka_message->key_len,kafka_message->key
		, kafka_message->len,kafka_message->payload )
	
	rd_kafka_message_destroy( kafka_message );
	
	return 0;
}

funcCleanInputPluginContext CleanInputPluginContext ;
int CleanInputPluginContext( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	struct InputPluginContext	*p_plugin_ctx = (struct InputPluginContext *)p_context ;
	
#ifdef _WITH_ZOOKEEPER
	zookeeper_close( p_plugin_ctx->zh );
#endif
	
	rd_kafka_consume_stop( p_plugin_ctx->kafka_topic , RD_KAFKA_PARTITION_UA );
	
	rd_kafka_topic_destroy( p_plugin_ctx->kafka_topic );
	
	rd_kafka_destroy( p_plugin_ctx->kafka );
	
	return 0;
}

funcUnloadInputPluginConfig UnloadInputPluginConfig ;
int UnloadInputPluginConfig( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void **pp_context )
{
	struct InputPluginContext	**pp_plugin_ctx = (struct InputPluginContext **)pp_context ;
	
	/* 释放内存以存放插件上下文 */
	free( (*pp_plugin_ctx) ); (*pp_plugin_ctx) = NULL ;
	
	return 0;
}

