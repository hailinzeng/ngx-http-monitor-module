#include "ngx_http_monitor_queue.h"

#include <pthread.h>
#include <stdlib.h>

ngx_http_monitor_queue_t* ngx_http_monitor_create_queue(ngx_int_t capacity, ngx_log_t* log){
    ngx_http_monitor_queue_t* queue;
    
    queue = (ngx_http_monitor_queue_t*)malloc(sizeof(ngx_http_monitor_queue_t));
    queue->mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    queue->cond  = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
    
    if (queue == NULL || queue->mutex == NULL || queue->cond == NULL) {
        free(queue);
        free(queue->mutex);
        free(queue->cond);

        return NULL;
    }

    pthread_mutex_init(queue->mutex, NULL);
    pthread_cond_init(queue->cond, NULL);

    queue->front = NULL;
    queue->tail  = NULL;

    queue->size  = 0;

    queue->log = log;

    queue->capacity = capacity;
    
    return queue;
}

void* ngx_http_monitor_destory_queue(ngx_http_monitor_queue_t* queue){
    pthread_mutex_destroy(queue->mutex);
    pthread_cond_destroy(queue->cond);

    while(queue->front != NULL){
        ngx_http_monitor_str_list_t* list = queue->front;
        queue->front = list->next;

        ngx_http_monitor_str_t* str = list->str;
        free(list);
        
        free(str->buf);
        free(str);
    }

    free(queue->mutex);
    free(queue->cond);
    free(queue);

    return NULL;
}

void ngx_http_monitor_push_queue(ngx_http_monitor_queue_t* queue, ngx_http_monitor_str_t* str){
    ngx_http_monitor_str_list_t* list = malloc(sizeof(ngx_http_monitor_str_list_t));
    if ( list == NULL ) {
        //failed to alloc
        return;
    }

    list->str = str;
    list->next = NULL;
    
    pthread_mutex_lock(queue->mutex);

    if (queue->capacity != 0 && queue->size == queue->capacity) {
        static struct Message msg = MESSAGE_INTIALIZER;

        ngx_log_error ( NGX_LOG_ERR, queue->log, 0,
                        "ngx_http_monitor_push_queue error: queue size limit %ud reached, str: %s", queue->capacity, str->buf + msg.HEADER_LENGTH); //head: 4 + 3
        
        pthread_mutex_unlock(queue->mutex);

        return;
    }

    if (queue->tail == NULL) {
        queue->front = list;
        queue->tail  = list;
    } else {
        queue->tail->next = list;
        queue->tail = list;
    }

    queue->size ++;
    
    pthread_cond_signal(queue->cond);
    
    pthread_mutex_unlock(queue->mutex);
}

ngx_http_monitor_str_t* ngx_http_monitor_pull_queue(ngx_http_monitor_queue_t* queue){
    ngx_http_monitor_str_t* str;
    ngx_http_monitor_str_list_t* list;
    
    pthread_mutex_lock(queue->mutex);
    
    if (queue->size == 0) {
        pthread_cond_wait(queue->cond, queue->mutex);
    }

    list= queue->front;
    queue->front = list->next;

    if ( queue->front == NULL ){
        queue->tail = NULL;
    }

    queue->size --;

    pthread_mutex_unlock(queue->mutex);
    
    str = list->str;

    free(list);

    return str;
}
