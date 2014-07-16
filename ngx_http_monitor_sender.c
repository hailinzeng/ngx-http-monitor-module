#include "ngx_http_monitor_sender.h"
#include "ngx_http_monitor_module.h"
#include "ngx_http_monitor_message.h"

#include <sys/epoll.h>
#include <stdio.h>
#include <pthread.h>

#include <stdlib.h>

// json body
static const char NGX_HTTP_MONITOR_FORMATER[]    = "{\"host\":\"%V\", \"uri\":\"%V\", \"status\":%ui}";
static const char NGX_HTTP_MONITOR_FORMATER_US[] = "{\"host\":\"%V\", \"uri\":\"%V\", \"upstream\":\"%V\", \"status\":%ui}";

static void  ngx_http_monitor_send_startloop(ngx_http_monitor_sender_t* sender, ngx_http_request_t *r);
static void* ngx_http_monitor_send_mainloop(void* args);

ngx_http_monitor_sender_t* ngx_http_monitor_create_sender(ngx_array_t *servers, ngx_int_t backlog, ngx_log_t* log){
    ngx_http_monitor_sender_t* sender;
    
    sender = (ngx_http_monitor_sender_t*)malloc(sizeof(ngx_http_monitor_sender_t));

//  ngx_log_t* log = malloc(sizeof(ngx_log_t));
//  log->file = ngx_conf_open_file();
    
    sender->queue = ngx_http_monitor_create_queue(backlog, log);
    
    if(sender == NULL || sender->queue == NULL){
        free(sender);
        return NULL;
    }

    sender->servers = servers;

//  create on first push
    sender->sendthr = NULL;

    return sender;
}

static void ngx_http_monitor_destory_sender(void* args){
    ngx_http_monitor_sender_t* sender = (ngx_http_monitor_sender_t*)args;
    
    ngx_http_monitor_destory_queue(sender->queue);

    free(sender->sendthr);

    free(sender);
}

void ngx_http_monitor_send(ngx_http_monitor_sender_t* sender, ngx_http_request_t *r){
    ngx_http_monitor_str_t* str = (ngx_http_monitor_str_t*)malloc(sizeof(ngx_http_monitor_str_t));
    if(str == NULL){
        ngx_log_error(NGX_LOG_ERR,r->connection->log,0,"Failed to malloc for buf in ngx_http_monitor_send!");
        return;
    }

    struct Message msg = MESSAGE_INTIALIZER;

    if(r->upstream != NULL){
        msg.totalLen = msg.HEADER_LENGTH + sizeof(NGX_HTTP_MONITOR_FORMATER_US) + 
                        r->headers_in.server.len - 2 + r->unparsed_uri.len - 2 + r->upstream->peer.name->len - 2 + 3 - 3 -1;
    } else {
        msg.totalLen = msg.HEADER_LENGTH + sizeof(NGX_HTTP_MONITOR_FORMATER) + 
                        r->headers_in.server.len - 2 + r->unparsed_uri.len - 2 + 3 - 3 -1;
    }

    struct hostent *he = gethostbyname((const char*)ngx_cycle->hostname.data);
//  struct hostent *he = gethostbyname((const char*)r->headers_in.host->value.data);
    
    struct in_addr *addr = (struct in_addr*)he->h_addr;

    msg.warnlevel = 1;
    msg.ip = revertByteL(ntohl(addr->s_addr));
    msg.timestamp = (int64_t) ngx_time();
    
    toByteWithoutCopyBody(&msg, str);

    if (str->buf == NULL) {
        ngx_log_error(NGX_LOG_ERR,r->connection->log,0,"Failed to malloc for buf in ngx_http_monitor_send!");

        free(str);
        return;
    }
    
    // %V                        ngx_str_t *
    // %[0][width][u][x|X]d      int/u_int
    if (r->upstream == NULL) {
        ngx_snprintf(str->buf + msg.HEADER_LENGTH, str->buflen - msg.HEADER_LENGTH, NGX_HTTP_MONITOR_FORMATER,
                    &r->headers_in.server, &r->unparsed_uri, r->headers_out.status);
    } else {
        ngx_snprintf(str->buf + msg.HEADER_LENGTH, str->buflen - msg.HEADER_LENGTH, NGX_HTTP_MONITOR_FORMATER_US,
                    &r->headers_in.server, &r->unparsed_uri, r->upstream->peer.name, r->headers_out.status);
    }

    // no need
    // str->buf[str->buflen -1] = '\0';

    if (sender->sendthr == NULL) {
        ngx_http_monitor_send_startloop(sender, r);
    }

    ngx_http_monitor_push_queue(sender->queue, str);
}


static void ngx_http_monitor_send_startloop(ngx_http_monitor_sender_t* sender, ngx_http_request_t *r){
    pthread_mutex_lock(sender->queue->mutex);

    if (sender->sendthr == NULL) {
        sender->sendthr = (pthread_t*)malloc(sizeof(pthread_t));
        
        if (sender->sendthr == NULL) {
            ngx_log_error(NGX_LOG_ERR,r->connection->log,0,"Failed to malloc for sendthr in ngx_http_monitor_send!");

            pthread_mutex_unlock(sender->queue->mutex);
            
            return;
        }

        pthread_attr_t detach;
        pthread_attr_init(&detach);
        pthread_attr_setdetachstate(&detach, PTHREAD_CREATE_DETACHED);

        int ret = pthread_create(sender->sendthr, &detach, ngx_http_monitor_send_mainloop, sender);
        if (ret != 0) {
            ngx_log_error(NGX_LOG_ERR,r->connection->log,0,"Failed to create thread in ngx_http_monitor_send!");
            
            free(sender->sendthr);
            sender->sendthr = NULL;

            pthread_mutex_unlock(sender->queue->mutex);

            return;
        }
    }

    pthread_mutex_unlock(sender->queue->mutex);
}

static void* ngx_http_monitor_send_mainloop(void* args){
    ngx_http_monitor_sender_t* sender = (ngx_http_monitor_sender_t*)args;
    
    pthread_cleanup_push(ngx_http_monitor_destory_sender, sender);
    
    ngx_http_monitor_server_t* uscfp = sender->servers->elts;

    int totalfd = sender->servers->nelts;
    int maxfd = 0;
    int i;
    
    u_char* sendp;
    ngx_int_t sendlen;
    
    struct epoll_event event;
    struct epoll_event events[totalfd];
    
    //since linux 2.6.8, the size arg is unused, but must be > 0
//  int epollfd = epoll_create1(0); // won't compile
    int epollfd = epoll_create(1024);
    if (epollfd == -1) {
        // error
        return NULL;
    }
    
    for (i = 0; i < totalfd; i++) {
        int sockfd = uscfp[i].sockfd;
        
        event.events = EPOLLOUT | EPOLLET;
        event.data.fd = sockfd;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event);

        if (sockfd > maxfd) {
            maxfd = sockfd;
        }
    }

    while (!ngx_quit && !ngx_terminate) {
        ngx_http_monitor_str_t* str = ngx_http_monitor_pull_queue( sender->queue );
        
        ngx_uint_t leftfd = totalfd;

        // FIXME:
        while (leftfd > 0) {
            int nfds = epoll_wait(epollfd, events, maxfd + 1, 1000);

            if (nfds <= 0) {
                // error
                continue;
            }

            for (i = 0; i < nfds; i++) {
                struct sockaddr_in* addr = uscfp[i].sockaddr;
                socklen_t socklen = uscfp[i].socklen;
                int sockfd = uscfp[i].sockfd;

                sendp = str->buf;
                sendlen = str->buflen;

                while (sendlen > 0) {
                    int ret = sendto ( sockfd, sendp, sendlen, 0, addr, socklen );
                    if (ret == -1) {
                        if (EAGAIN == errno) {
                            continue;
                        }

                        ngx_log_error ( NGX_LOG_ERR, sender->log, 0, "Failed to send messege to peer:%s %d, %s",
                                        addr->sin_addr.s_addr,addr->sin_port, strerror(errno));
                    }

                    sendp += ret;
                    sendlen -= ret;
                }
                
                event.events = EPOLLOUT | EPOLLET;
                event.data.fd = sockfd;
                epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, &event);
            }

            leftfd -= nfds;
        }

        free(str->buf);
        free(str);

        for (i = 0; i < totalfd; i++) {
            int sockfd = uscfp[i].sockfd;
            
            event.events = EPOLLOUT | EPOLLET;
            event.data.fd = sockfd;
            epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event);
        }
    }

    pthread_cleanup_pop(0);

    return NULL;
}
