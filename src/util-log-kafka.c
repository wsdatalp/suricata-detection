#include "suricata-common.h"
#include "util-log-kafka.h"
#include "util-logopenfile.h"

#ifdef HAVE_LIBRDKAFKA

/** \brief close kafka log
 *  \param log_ctx Log file context
 */
static void SCLogFileCloseKafka(LogFileCtx *log_ctx)
{
    SCLogKafkaContext *kafka_ctx = log_ctx->kafka;

    if (NULL == kafka_ctx) {
        return;
    }

    if (kafka_ctx->rk) {
        /* Poll to handle delivery reports */
        rd_kafka_poll(kafka_ctx->rk, 0);

        /* Wait for messages to be delivered */
        while (rd_kafka_outq_len(kafka_ctx->rk) > 0)
            rd_kafka_poll(kafka_ctx->rk, 100);
    }

    if (kafka_ctx->rkt_flow) {
        /* Destroy topic */
        rd_kafka_topic_destroy(kafka_ctx->rkt_flow);
    }
    if(kafka_ctx->rkt_alarm){
    	rd_kafka_topic_destroy(kafka_ctx->rkt_alarm);
    }


    if (kafka_ctx->rk) {
        /* Destroy the handle */
        rd_kafka_destroy(kafka_ctx->rk);
    }
    return;
}

/**
 * \brief LogFileWriteKafka() writes log data to kafka output.
 * \param lf_ctx Log file context allocated by caller
 * \param string buffer with data to write
 * \param string_len data length
 * \retval 0 on sucess;
 * \retval -1 on failure;
 */
int LogFileWriteKafka(void *lf_ctx, const char *string, size_t string_len)
{
    if(strstr(string, "\"failed\"")){
        return -1;
    }
    if(!strstr(string, "\"flow\":{")){
        return -1;
    }
    if(!strstr(string, "\"alert\":{")){
        return -1;
    }
    LogFileCtx *log_ctx = lf_ctx;
    SCLogKafkaContext *kafka_ctx = log_ctx->kafka;
    char *subStr = "\"groupId\":1";
    if(strstr(string, subStr)){
        if (rd_kafka_produce(kafka_ctx->rkt_flow, RD_KAFKA_PARTITION_UA,
                         RD_KAFKA_MSG_F_COPY,
            /* Payload and length */
                         (void *)string, string_len,
            /* Optional key and its length */
                         NULL, 0,
            /* Message opaque, provided in
             * delivery report callback as
             * msg_opaque. */
                         NULL) == -1)
        {
            SCLogError(SC_ERR_KAFKA,
                       "%% Failed to produce to topic %s "
                       "partition %i: %s\n",
                       log_ctx->kafka_setup.topic_name_flow, 0,
                       rd_kafka_err2str(
                               rd_kafka_errno2err(errno)));
            /* Poll to handle delivery reports */
            rd_kafka_poll(kafka_ctx->rk, 0);
        }
    }else{
        if (rd_kafka_produce(kafka_ctx->rkt_flow, RD_KAFKA_PARTITION_UA,
                         RD_KAFKA_MSG_F_COPY,
            /* Payload and length */
                         (void *)string, string_len,
            /* Optional key and its length */
                         NULL, 0,
            /* Message opaque, provided in
             * delivery report callback as
             * msg_opaque. */
                         NULL) == -1)
        {
            SCLogError(SC_ERR_KAFKA,
                       "%% Failed to produce to topic %s "
                       "partition %i: %s\n",
                       log_ctx->kafka_setup.topic_name_flow, 0,
                       rd_kafka_err2str(
                               rd_kafka_errno2err(errno)));
            /* Poll to handle delivery reports */
            rd_kafka_poll(kafka_ctx->rk, 0);
        }
    	 if (rd_kafka_produce(kafka_ctx->rkt_alarm, RD_KAFKA_PARTITION_UA,
                         RD_KAFKA_MSG_F_COPY,
            /* Payload and length */
                         (void *)string, string_len,
            /* Optional key and its length */
                         NULL, 0,
            /* Message opaque, provided in
 *              * delivery report callback as
 *                           * msg_opaque. */
                         NULL) == -1)
        {
            SCLogError(SC_ERR_KAFKA,
                       "%% Failed to produce to topic %s "
                       "partition %i: %s\n",
                       log_ctx->kafka_setup.topic_name_alarm, 0,
                       rd_kafka_err2str(
                               rd_kafka_errno2err(errno)));
            /* Poll to handle delivery reports */
            rd_kafka_poll(kafka_ctx->rk, 0);
        }
    }

    return -1;
}

/**
 * \brief Message delivery report callback.
 * Called once for each message.
 */
static void msg_delivered (rd_kafka_t *rk,
                           void *payload, size_t len,
                           int error_code,
                           void *opaque, void *msg_opaque)
{
    rk = rk;
    payload = payload;
    len = len;
    opaque = opaque;
    msg_opaque = msg_opaque;
    if (error_code)
        SCLogError(SC_ERR_KAFKA, "%% Message delivery failed: %s\n",
                   rd_kafka_err2str(error_code));
}

/** \brief configure and initializes kafka output logging
 *  \param kafka_node ConfNode structure for the output section in question
 *  \param lf_ctx Log file context allocated by caller
 *  \retval 0 on success
 */
int SCConfLogOpenKafka(ConfNode *kafka_node, void *lf_ctx)
{
    LogFileCtx *log_ctx = lf_ctx;
    SCLogKafkaContext *kafka_ctx = NULL;

    if (NULL == kafka_node) {
        return -1;
    }

    log_ctx->kafka_setup.brokers = ConfNodeLookupChildValue(kafka_node, "brokers");
    log_ctx->kafka_setup.topic_name_flow = ConfNodeLookupChildValue(kafka_node, "topic-flow");
    log_ctx->kafka_setup.topic_name_alarm = ConfNodeLookupChildValue(kafka_node, "topic-alarm");

    /*create kafka ctx*/
    rd_kafka_conf_t *conf;
    rd_kafka_topic_conf_t *topic_conf;
    rd_kafka_topic_conf_t *topic_conf_alarm;
    char tmp[16];
    char errstr[512];
    kafka_ctx = (SCLogKafkaContext*) SCCalloc(1, sizeof(SCLogKafkaContext));
    if (kafka_ctx == NULL) {
        SCLogError(SC_ERR_MEM_ALLOC, "Unable to allocate kafka context");
        exit(EXIT_FAILURE);
    }

    conf = rd_kafka_conf_new();
    snprintf(tmp, sizeof(tmp), "%i", SIGIO);
    if (RD_KAFKA_CONF_OK != rd_kafka_conf_set(conf,
                                              "internal.termination.signal",
                                              tmp,
                                              errstr,
                                              sizeof(errstr))) {
        SCLogError(SC_ERR_KAFKA, "Unable to allocate kafka context");
    }
    if (RD_KAFKA_CONF_OK != rd_kafka_conf_set(conf,
                                              "broker.version.fallback",
                                              "0.8.2",
                                              errstr,
                                              sizeof(errstr))) {
        SCLogError(SC_ERR_KAFKA, "%s", errstr);
    }
    if (RD_KAFKA_CONF_OK != rd_kafka_conf_set(conf,
                                              "queue.buffering.max.messages",
                                              "500000",
                                              errstr,
                                              sizeof(errstr))) {
        SCLogError(SC_ERR_KAFKA, "%s", errstr);
    }

    rd_kafka_conf_set_dr_cb(conf, msg_delivered);
    if (!(kafka_ctx->rk = rd_kafka_new(RD_KAFKA_PRODUCER,
                                       conf,
                                       errstr,
                                       sizeof(errstr)))) {
        SCLogError(SC_ERR_KAFKA, "%% Failed to create new producer: %s", errstr);
        exit(EXIT_FAILURE);
    }
    if (0 == rd_kafka_brokers_add(kafka_ctx->rk,
                                  log_ctx->kafka_setup.brokers)) {
        SCLogError(SC_ERR_KAFKA, "%% No valid brokers specified");
        exit(EXIT_FAILURE);
    }
    topic_conf = rd_kafka_topic_conf_new();
    kafka_ctx->rkt_flow = rd_kafka_topic_new(kafka_ctx->rk,
                                        log_ctx->kafka_setup.topic_name_flow,
                                        topic_conf);
    if (NULL == kafka_ctx->rkt_flow) {
        SCLogError(SC_ERR_KAFKA, "%% Failed to create kafka topic %s",
                   log_ctx->kafka_setup.topic_name_flow);
        exit(EXIT_FAILURE);
    }
    topic_conf_alarm = rd_kafka_topic_conf_new();
    kafka_ctx->rkt_alarm = rd_kafka_topic_new(kafka_ctx->rk,
					log_ctx->kafka_setup.topic_name_alarm,
                                        topic_conf_alarm);
   if(NULL == kafka_ctx->rkt_alarm){
   	SCLogError(SC_ERR_KAFKA, "%% Failed to create kafka topic %s",
                   log_ctx->kafka_setup.topic_name_alarm);
        exit(EXIT_FAILURE);
   }


    log_ctx->kafka = kafka_ctx;
    log_ctx->Close = SCLogFileCloseKafka;

    return 0;
}

#endif
