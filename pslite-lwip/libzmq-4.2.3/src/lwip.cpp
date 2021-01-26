#include <unistd.h>

#include "lwipopts.h"
#include "lwip/init.h"
#include "lwip/debug.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/sys.h"
#include "lwip/stats.h"
#include "lwip/ip.h"
#include "lwip/ip_addr.h"
#include "lwip/inet_chksum.h"
#include "lwip/tcpip.h"
#include "lwip/sockets.h"
#include "lwip/netif.h"
#include "netif/etharp.h"
#include "netif/dpdkif.h"
#include "zmqlwip.h"

/* Host IP configuration */
static ip4_addr_t ipaddr, netmask, gateway;

static struct netif netdev;

static unsigned is_init = 0;


static void
lwip_port_init(void *arg)
{
	sys_sem_t *sem;

	sem = (sys_sem_t *)arg;

	netif_add(&netdev, &ipaddr, &netmask, &gateway, NULL, dpdk_device_init, tcpip_input);
	netif_set_default(&netdev);
	netif_set_up(&netdev);

	sys_sem_signal(sem);
}

int zmq_lwip_init(const char *ip, const char *gw, const char *mask) {

	if (is_init)
		return 0;
	is_init = 1;

	int ret = 0;

	ret = init_dpdk();
	if (ret < 0)
		return -1;

	/* convert ip address */
	if (!ip4addr_aton(ip, &ipaddr)) {
		fprintf(stderr, "Failed to convert IPv4 address %s\n", ip);
		return -1;
	}

	if (!ip4addr_aton(gw, &gateway)) {
		fprintf(stderr, "Failed to convert IPv4 address %s\n", gw);
		return -1;
	}
	if (!ip4addr_aton(mask, &netmask)) {
		fprintf(stderr, "Failed to convert IPv4 address %s\n", mask);
		return -1;
	}

	fprintf(stdout, "Lwip TCP/IP initialize: %s(%u) %s %s\n",
					ip, ipaddr.addr, gw, mask);

	sys_sem_t sem;

	if (sys_sem_new(&sem, 0) != ERR_OK) {
		fprintf(stderr, "Failed to create semaphore\n");
		return -1;
	}

	tcpip_init(lwip_port_init, &sem);

	fprintf(stdout, "Lwip TCP/IP initialized\n");
	sys_sem_wait(&sem);
	sys_sem_free(&sem);


	return 0;
}
