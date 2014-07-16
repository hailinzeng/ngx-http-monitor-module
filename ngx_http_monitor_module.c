#include "ngx_http_monitor_module.h"

static char*     ngx_http_monitor_server_config(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_monitor_filter_init(ngx_conf_t *cf); 
static void*     ngx_http_monitor_create_srv_conf(ngx_conf_t *cf);

static void*     ngx_http_monitor_create_main_conf(ngx_conf_t *cf);
static char*     ngx_http_monitor_init_main_conf(ngx_conf_t *cf, void *conf);

static ngx_http_output_header_filter_pt ngx_http_next_header_filter;

static const unsigned DEFAULT_BACKLOG = 10;

/* Module Directives */
static ngx_command_t ngx_http_monitor_commands[] = {
    {
        ngx_string("monitor_server"),      /* directive name */
        NGX_HTTP_SRV_CONF|NGX_CONF_TAKE2,  /* location context and takes how many arguments */
      
        ngx_http_monitor_server_config,    /* configuration setup function */
      
        NGX_HTTP_SRV_CONF_OFFSET,          /* data saved in which configuration */
        0,                                 /* No offset when storing the module configuration on struct. */
        NULL
    },

    {
        ngx_string("monitor_backlog"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        
        ngx_conf_set_num_slot,             /* set backlog */
        
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_monitor_main_conf_t, backlog),
        NULL
    },

    ngx_null_command                       /* command termination */
};

/* The module context. */
static ngx_http_module_t ngx_http_monitor_module_ctx = {
    NULL,                                /* preconfiguration */
    ngx_http_monitor_filter_init,        /* postconfiguration */

    ngx_http_monitor_create_main_conf,   /* create main configuration */
    ngx_http_monitor_init_main_conf,     /* init main configuration */

    ngx_http_monitor_create_srv_conf,    /* create server configuration */
    NULL,                                /* merge server configuration */

    NULL,                                /* create location configuration */
    NULL                                 /* merge location configuration */
};

/* Module definition. */
ngx_module_t ngx_http_monitor_module = {
    NGX_MODULE_V1,
    &ngx_http_monitor_module_ctx,        /* module context */
    ngx_http_monitor_commands,           /* module directives */
    NGX_HTTP_MODULE,                     /* module type */
    NULL,                                /* init master */
    NULL,                                /* init module */
    NULL,                                /* init process */
    NULL,                                /* init thread */
    NULL,                                /* exit thread */
    NULL,                                /* exit process */
    NULL,                                /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_int_t ngx_http_monitor_header_filter(ngx_http_request_t *r)
{
    // log all non-200 http response
    if (r->headers_out.status != NGX_HTTP_OK) {
        ngx_http_monitor_srv_conf_t* mscf = ngx_http_get_module_srv_conf(r, ngx_http_monitor_module);
        
        ngx_http_monitor_send(mscf->sender, r);
    }

    // call the next filter
    return ngx_http_next_header_filter(r);
}

/**
 * Configuration setup function that installs the content handler.
 *
 * @param cf   arguments passed to the directive
 * @param cmd  current ngx_command_t struct
 * @param conf custom configuration struct
 */
static char *ngx_http_monitor_server_config(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_monitor_srv_conf_t *mscf = conf;

    if (mscf->servers == NULL) {
        ngx_http_monitor_main_conf_t* mmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_monitor_module);
        
        // default 4 ngx_http_monitor_server_t
        mscf->servers = ngx_array_create(cf->pool, 4,
                                         sizeof(ngx_http_monitor_server_t));
        
        mscf->sender = ngx_http_monitor_create_sender(mscf->servers, mmcf->backlog, cf->log);
        
        if (mscf->servers == NULL || mscf->sender == NULL) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "Failed to create sender for monitor directive");
            return NGX_CONF_ERROR;
        }
    }

    // get the next unused, enlarge if full
    ngx_http_monitor_server_t *ms = ngx_array_push(mscf->servers);

    // directive args: ip port
    ngx_str_t *addr = cf->args->elts;
    
    ngx_str_t ip = addr[1];
    ngx_int_t port = ngx_atoi(addr[2].data, addr[2].len);

    struct sockaddr_in* sockaddr = ngx_pcalloc(cf->pool, sizeof(struct sockaddr_in));
    if (sockaddr == NULL ) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "Failed to create socket for monitor directive");
        return NGX_CONF_ERROR;
    }

    sockaddr->sin_addr.s_addr = inet_addr((char*)ip.data);
    if (sockaddr->sin_addr.s_addr == INADDR_NONE) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "Invalid addr for monitor directive \"%V\"", &ip);
        return NGX_CONF_ERROR;
    }
    
    sockaddr->sin_port = htons(port);
    sockaddr->sin_family = AF_INET;

    // FIXME:when to close
    int sockfd = socket(AF_INET,SOCK_DGRAM,0);
    if (sockfd == -1) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "Failed to create socket for monitor directive!");
        return NGX_CONF_ERROR;
    }
    
    if (ngx_nonblocking(sockfd) == -1) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "Failed to set sockfd nonblocked for monitor directive!");
        return NGX_CONF_ERROR;
    }
    
    int ret = connect(sockfd, sockaddr, sizeof(struct sockaddr_in));
    if (ret == -1) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "Failed to connect:%V:%ud for monitor directive", &ip, port);
        return NGX_CONF_ERROR;
    }

    ms->sockfd = sockfd;
    ms->sockaddr = sockaddr;
    ms->socklen = sizeof(struct sockaddr_in);

    return NGX_CONF_OK;
}

/* Filter Installation */
static ngx_int_t ngx_http_monitor_filter_init(ngx_conf_t *cf)
{
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_monitor_header_filter;

    return NGX_OK;
}

static void * ngx_http_monitor_create_main_conf(ngx_conf_t* cf)
{
    ngx_http_monitor_main_conf_t *mscf;

    mscf = ngx_pcalloc(cf->pool, sizeof(ngx_http_monitor_main_conf_t));
    if (mscf == NULL) {
        return NULL;
    }

    mscf->backlog = NGX_CONF_UNSET;
    
    return mscf;
}

static char * ngx_http_monitor_init_main_conf(ngx_conf_t* cf, void* conf)
{
    ngx_http_monitor_main_conf_t  *mscf = conf;

    if (mscf == NULL) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "Failed to malloc in ngx_http_monitor_create_main_conf");

        return NGX_CONF_ERROR;
    }

    // default 10
    if (mscf->backlog == NGX_CONF_UNSET) {
        mscf->backlog = DEFAULT_BACKLOG;
    } else {
        if (mscf->backlog < 0) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "monitor_backlog backlog: backlog %d should not <0", mscf->backlog);

            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}

static void * ngx_http_monitor_create_srv_conf(ngx_conf_t *cf)
{
    ngx_http_monitor_srv_conf_t  *mscf;

    mscf = ngx_pcalloc(cf->pool, sizeof(ngx_http_monitor_srv_conf_t));
    if (mscf == NULL) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "Failed to malloc in ngx_http_monitor_create_srv_conf");
    }
/*
    // ngx_array_create = init header + ngx_array_init
    if (ngx_array_init(mscf->servers, cf->pool, 4,
                       sizeof(ngx_http_monitor_server_t))
        != NGX_OK)
    {
        return NULL;
    }
*/
    return mscf;
}
