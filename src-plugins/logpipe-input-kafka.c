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
	rd_kafka_topic_conf_t		*kafka_topic_conf ;
	rd_kafka_topic_partition_list_t	*kafka_topic_partition_list ;
	
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
	
	char				tmp[16] ;
	rd_kafka_conf_res_t		kafka_conf_res ;
	rd_kafka_resp_err_t		kafla_resp_err ;
	char				errstr[ 512 ] ;
	
	p_plugin_ctx->kafka_conf = rd_kafka_conf_new() ;
	if( p_plugin_ctx->kafka_conf == NULL )
	{
		ERRORLOGC( "rd_kafka_conf_new failed" )
		return -1;
	}
	
	snprintf( tmp , sizeof(tmp) , "%i" , SIGIO );
	rd_kafka_conf_set( p_plugin_ctx->kafka_conf , "internal.termination.signal", tmp , NULL , 0 );
	
	p_plugin_ctx->kafka_topic_conf = rd_kafka_topic_conf_new() ;
	if( p_plugin_ctx->kafka_topic_conf == NULL )
	{
		ERRORLOGC( "rd_kafka_topic_conf_new failed" )
		return -1;
	}
	
	kafka_conf_res = rd_kafka_conf_set( p_plugin_ctx->kafka_conf , "group.id" , p_plugin_ctx->group , errstr , sizeof(errstr) ) ;
	if( kafka_conf_res != RD_KAFKA_CONF_OK )
	{
		ERRORLOGC( "rd_kafka_conf_set \"group.id\" \"%s\" failed[%d] , errstr[%s]" , p_plugin_ctx->group , kafka_conf_res , errstr )
		return -1;
	}
	
	kafka_conf_res = rd_kafka_topic_conf_set( p_plugin_ctx->kafka_topic_conf , "offset.store.method" , "broker" , errstr , sizeof(errstr) ) ;
	if( kafka_conf_res != RD_KAFKA_CONF_OK )
	{
		ERRORLOGC( "rd_kafka_topic_conf_set \"offset.store.method\" \"broker\" failed[%d] , errstr[%s]" , kafka_conf_res , errstr )
		return -1;
	}
	
	rd_kafka_conf_set_default_topic_conf( p_plugin_ctx->kafka_conf , p_plugin_ctx->kafka_topic_conf );
	
	p_plugin_ctx->kafka = rd_kafka_new( RD_KAFKA_CONSUMER , p_plugin_ctx->kafka_conf , errstr , sizeof(errstr) ) ;
	if( p_plugin_ctx->kafka == NULL )
	{
		ERRORLOGC( "rd_kafka_new failed , errstr[%s]" , errstr )
		return -1;
	}
	
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
	rd_kafka_brokers_add( p_plugin_ctx->kafka_watcher_context.kafka , p_plugin_ctx->kafka_watcher_context.brokers );
	rd_kafka_poll( p_plugin_ctx->kafka_watcher_context.kafka , 10 );
#else
	rd_kafka_brokers_add( p_plugin_ctx->kafka , p_plugin_ctx->bootstrap_servers );
	rd_kafka_poll( p_plugin_ctx->kafka , 10 );
#endif
	
	rd_kafka_poll_set_consumer( p_plugin_ctx->kafka );
	
	p_plugin_ctx->kafka_topic_partition_list = rd_kafka_topic_partition_list_new(1) ;
	if( p_plugin_ctx->kafka_topic_partition_list == NULL )
	{
		ERRORLOGC( "rd_kafka_new rd_kafka_topic_partition_list_new failed" )
		return -1;
	}
	
	rd_kafka_topic_partition_list_add( p_plugin_ctx->kafka_topic_partition_list , p_plugin_ctx->topic , -1 );
	
	kafla_resp_err = rd_kafka_subscribe( p_plugin_ctx->kafka , p_plugin_ctx->kafka_topic_partition_list ) ;
	if( kafla_resp_err )
	{
		ERRORLOGC( "rd_kafka_subscribe failed[%d] , errstr[%s]" , kafla_resp_err , rd_kafka_err2str(kafla_resp_err) )
		return -1;
	}
	
	return 0;
}

funcOnInputPluginEvent OnInputPluginEvent ;
int OnInputPluginEvent( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	int				nret = 0 ;
	
	while(1)
	{
		nret = WriteAllOutputPlugins( p_env , p_logpipe_input_plugin , 0 , "" ) ;
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
int ReadInputPlugin( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context , uint64_t *p_file_offset , uint64_t *p_file_line , uint64_t *p_block_len , char *block_buf , uint64_t block_buf_size )
{
	struct InputPluginContext	*p_plugin_ctx = (struct InputPluginContext *)p_context ;
	
	rd_kafka_message_t		*kafka_message = NULL ;
	
	(*p_file_offset) = 0 ;
	(*p_file_line) = 0 ;
	(*p_block_len) = 0 ;
	memset( block_buf , 0x00 , sizeof(block_buf_size) );
	
	kafka_message = rd_kafka_consumer_poll( p_plugin_ctx->kafka , 1000 ) ;
	if( kafka_message == NULL )
	{
		DEBUGLOGC( "rd_kafka_consumer_poll timeout" )
		return 0;;
	}
	
	INFOLOGC( "topic[%s] partition[%"PRId32"] offset[%"PRId64"] key[%.*s] msg[%.*s]"
		, rd_kafka_topic_name(kafka_message->rkt)
		, kafka_message->partition
		, kafka_message->offset
		, kafka_message->key_len,kafka_message->key
		, kafka_message->len,kafka_message->payload )
	
	if( kafka_message->len > 0 )
	{
		(*p_file_offset) = 0 ;
		(*p_file_line) = 0 ;
		(*p_block_len) = MIN(kafka_message->len,block_buf_size-1) ;
		memcpy( block_buf , kafka_message->payload , (*p_block_len) );
	}
	
	rd_kafka_message_destroy( kafka_message );
	
	return LOGPIPE_READ_END_FROM_INPUT;
}

funcCleanInputPluginContext CleanInputPluginContext ;
int CleanInputPluginContext( struct LogpipeEnv *p_env , struct LogpipeInputPlugin *p_logpipe_input_plugin , void *p_context )
{
	struct InputPluginContext	*p_plugin_ctx = (struct InputPluginContext *)p_context ;
	
#ifdef _WITH_ZOOKEEPER
	zookeeper_close( p_plugin_ctx->zh );
#endif
	
	rd_kafka_consumer_close( p_plugin_ctx->kafka );
	
	rd_kafka_topic_partition_list_destroy( p_plugin_ctx->kafka_topic_partition_list );
	
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

