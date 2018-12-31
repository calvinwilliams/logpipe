#ifndef _H_KAFKA_WITH_ZOOKEEPER
#define _H_KAFKA_WITH_ZOOKEEPER

#ifdef __cplusplus
extern "C" {
#endif

#include "logpipe_api.h"

#include "rdkafka.h"

#include "zookeeper.h"

#define KAFKA_BROKER_PATH	"/brokers/ids"

struct KafkaWatcherContext
{
	rd_kafka_t		*kafka ;
	char			brokers[ 1024 ] ;
} ;

void GetBrokerListFromZookeeper( zhandle_t *zh , struct KafkaWatcherContext *p_kafka_watcher_ctx );
void KafkaWatcher( zhandle_t *zh , int type , int state , const char *path , void *watcherCtx );

#ifdef __cplusplus
}
#endif

#endif
