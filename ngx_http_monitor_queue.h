#ifndef _NGX_HTTP_MONITOR_QUEUE_H_INCLUDE_
#define _NGX_HTTP_MONITOR_QUEUE_H_INCLUDE_

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include "ngx_http_monitor_message.h"

// A simple blocking queue

typedef struct ngx_http_monitor_queue ngx_http_monitor_queue_t;

struct ngx_http_monitor_queue{
    pthread_cond_t*               cond;
    pthread_mutex_t*              mutex;

    ngx_http_monitor_str_list_t*  front;
    ngx_http_monitor_str_list_t*  tail;

    ngx_uint_t                    size;
    ngx_log_t*                    log;

    ngx_uint_t                    capacity;//0:no limit
};

ngx_http_monitor_queue_t* ngx_http_monitor_create_queue(ngx_int_t capacity, ngx_log_t* log);
void*                     ngx_http_monitor_destory_queue(ngx_http_monitor_queue_t* queue);

void                    ngx_http_monitor_push_queue(ngx_http_monitor_queue_t* queue, ngx_http_monitor_str_t* str);
ngx_http_monitor_str_t* ngx_http_monitor_pull_queue(ngx_http_monitor_queue_t* queue);

#endif
