#ifndef _NGX_HTTP_MONITOR_MODULE_H_INCLUDE_
#define _NGX_HTTP_MONITOR_MODULE_H_INCLUDE_

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <netinet/in.h>

#include "ngx_http_monitor_sender.h"

// monitor server
typedef struct ngx_http_monitor_server{
    struct sockaddr_in          *sockaddr;
    socklen_t                    socklen;
    ngx_int_t                    sockfd;
} ngx_http_monitor_server_t;

// nginx module config
typedef struct ngx_http_monitor_srv_conf {
    ngx_array_t*                 servers;
    
    ngx_http_monitor_sender_t*   sender;
} ngx_http_monitor_srv_conf_t;

typedef struct ngx_http_monitor_main_conf {
    ngx_int_t                   backlog;
} ngx_http_monitor_main_conf_t;

#endif
