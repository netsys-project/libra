#ifndef __ZMQ_LWIP_H__
#define __ZMQ_LWIP_H__

#ifdef __cplusplus
extern "C" {
#endif

int zmq_lwip_init(const char *ip, const char *gw, const char *mask);

#ifdef __cplusplus
}
#endif

#endif
