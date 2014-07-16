#ifndef _NGX_HTTP_MONITOR_MESSAGE_H_INCLUDE_
#define _NGX_HTTP_MONITOR_MESSAGE_H_INCLUDE_

#include <stdint.h>

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

// A simple queue to buffer Message
typedef struct ngx_http_monitor_str{
    ngx_int_t                    buflen;
    u_char*                      buf;
} ngx_http_monitor_str_t;

typedef struct ngx_http_monitor_str_list ngx_http_monitor_str_list_t;

struct ngx_http_monitor_str_list{
    ngx_http_monitor_str_t*      str;
    ngx_http_monitor_str_list_t* next;
};

// Protocol definition of Message send to monitor server
struct Message
{
	const int32_t HEADER_LENGTH;//= 24;
	int32_t totalLen;

	const int8_t version;		// = 0x01;

	// Top level No: 4(Nginx)
	const int8_t collectcode;	// = 4;

	// Second level No
	int8_t businesscode;		// = 1;

	// 1 High, 4 Middle, 8 Low
	int8_t warnlevel;

	// Length of b_name, not used
	const int32_t b_name_s;		// = 0;
	
	uint32_t ip;				// not used, should ignore it's value
	int64_t timestamp;

	char b_name[0];				// not used
	char body[0];
};

#define MESSAGE_INTIALIZER {24, 0, 1, 4, 1, 1, 0, 0, 0}

void toByteWithoutCopyBody(struct Message* msg, ngx_http_monitor_str_t* strbuf);

// Byte utils
uint64_t htonll(uint64_t n);
uint32_t revertByteL(uint32_t val);

//long getCurrentEpoch();
//long getLocalIp();

#endif