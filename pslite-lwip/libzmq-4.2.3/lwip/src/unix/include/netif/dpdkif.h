#ifndef LWIP_DPDKIF_H
#define LWIP_DPDKIF_H

#include "lwip/netif.h"

#ifdef __cplusplus
extern "C" {
#endif
int init_dpdk(void);
err_t dpdk_device_init(struct netif*);

#ifdef __cplusplus
}
#endif

#endif
