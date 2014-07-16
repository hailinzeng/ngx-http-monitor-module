#ifndef _NGX_HTTP_MONITOR_SENDER_H_INCLUDE_
#define _NGX_HTTP_MONITOR_SENDER_H_INCLUDE_

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include "ngx_http_monitor_queue.h"

typedef struct ngx_http_monitor_sender {
    ngx_http_monitor_queue_t*    queue;
    ngx_array_t*                 servers;
    ngx_log_t*                   log;
    
    pthread_t*                   sendthr;
} ngx_http_monitor_sender_t;

ngx_http_monitor_sender_t* ngx_http_monitor_create_sender(ngx_array_t *servers, ngx_int_t backlog, ngx_log_t* log);

void ngx_http_monitor_send(ngx_http_monitor_sender_t* sender, ngx_http_request_t *r);

#endif
