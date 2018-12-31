#include "kafka_with_zookeeper.h"

#include "IDL_zookeeper_broker_ids.dsc.c"

void GetBrokerListFromZookeeper( zhandle_t *zh , struct KafkaWatcherContext *p_kafka_watcher_ctx )
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
		
		memset( p_kafka_watcher_ctx->brokers , 0x00 , sizeof(p_kafka_watcher_ctx->brokers) );
		brokers_offset_ptr = p_kafka_watcher_ctx->brokers ;
		brokers_buf_remain_len = sizeof(p_kafka_watcher_ctx->brokers) ;
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

void KafkaWatcher( zhandle_t *zh , int type , int state , const char *path , void *watcherCtx )
{
	struct KafkaWatcherContext	*p_kafka_watcher_ctx = (struct KafkaWatcherContext *)watcherCtx ;
	
	DEBUGLOGC( "watcher - zh[%p] type[%d] state[%d] path[%s] watcherCtx[%p]" , zh , type , state , path , watcherCtx )
	if( type == ZOO_CHILD_EVENT && strncmp( path , KAFKA_BROKER_PATH , sizeof(KAFKA_BROKER_PATH)-1) == 0 )
	{
		GetBrokerListFromZookeeper( zh , p_kafka_watcher_ctx );
		DEBUGLOGC( "watcher - p_plugin_ctx->brokers[%s]" , p_kafka_watcher_ctx )
		if( p_kafka_watcher_ctx->brokers[0] && p_kafka_watcher_ctx->kafka )
		{
			rd_kafka_brokers_add( p_kafka_watcher_ctx->kafka , p_kafka_watcher_ctx->brokers );
			DEBUGLOGC( "watcher - rd_kafka_brokers_add p_plugin_ctx->brokers[%s]" , p_kafka_watcher_ctx->brokers )
			rd_kafka_poll( p_kafka_watcher_ctx->kafka , 10 );
			DEBUGLOGC( "watcher - rd_kafka_poll p_plugin_ctx->brokers[%s]" , p_kafka_watcher_ctx->brokers )
		}
	}
	
	return;
}

