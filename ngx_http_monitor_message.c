#include <arpa/inet.h>
#include <sys/param.h>
#include "ngx_http_monitor_message.h"

uint64_t htonll(uint64_t n)
{
#if __BYTE_ORDER == __BIG_ENDIAN
    return n;
#else
    return (((uint64_t)htonl(n)) << 32) + htonl(n >> 32);
#endif
}

uint32_t revertByteL(uint32_t val){
	int i = 0;
	int bsize = 4;

	uint32_t ret = 0;

	u_char* ptr = (u_char*)&val;
	u_char* retp = (u_char*)&ret;

	for(; i<bsize; i++){
		retp[i] = ptr[bsize - 1 -i];
	}

	return ret;
}

uint64_t revertBytell(uint64_t val){
	int i = 0;
	int bsize = 8;

	uint64_t ret = 0;

	u_char* ptr = (u_char*)&val;
	u_char* retp = (u_char*)&ret;

	for(; i<bsize; i++){
		retp[i] = ptr[bsize - 1 -i];
	}

	return ret;
}

void toByteWithoutCopyBody (struct Message* msg, ngx_http_monitor_str_t* str) {
	str->buflen = msg->totalLen;

	str->buf = (u_char*)malloc(str->buflen);
	if (str->buf == NULL) {
		return;
	}

	u_char* p = str->buf;

	*((uint32_t*)p) = revertByteL(htonl(msg->totalLen));
	p += 4;

	*((uint8_t*)p) = msg->version;
	p ++;

	*((uint8_t*)p) = msg->collectcode;
	p ++;

	*((uint8_t*)p) = msg->businesscode;
	p ++;

	*((uint8_t*)p) = msg->warnlevel;
	p ++;

	*((uint32_t*)p) = revertByteL(htonl(msg->b_name_s));
	p += 4;

	*((uint32_t*)p) = revertByteL(htonl(msg->ip));
	p += 4;

	*((uint64_t*)p) = revertBytell(htonll(msg->timestamp));
	p += 8;

//	memcpy(p, msg->body, msg->body_s);

	return;
}