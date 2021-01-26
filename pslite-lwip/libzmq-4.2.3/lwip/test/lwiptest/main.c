/* C runtime includes */
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

/* lwIP core includes */
#include "lwip/opt.h"

#include "lwip/sys.h"
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/init.h"
#include "lwip/tcpip.h"
#include "lwip/netif.h"
#include "lwip/api.h"
#include "lwip/tcp.h"
#include "netif/dpdkif.h"

#include "socket_api.h"

#define IS_CLIENT 0

#define SERVER_IP "192.168.57.21"
#define SERVER_PORT 3170

#define LOCAL_MASK "255.255.255.0"

#if IS_CLIENT
#define LOCAL_IP "192.168.57.22"
#define LOCAL_GW "192.168.57.0"
#else
#define LOCAL_IP SERVER_IP
#define LOCAL_GW "192.168.57.0"
#endif

static struct netif netif;
static int is_err = 0;

static void
test_init(void *arg)
{
	int ret;
	sys_sem_t *init_sem = (sys_sem_t *)arg;
	ip4_addr_t ipaddr, gw, netmask;

	ret = ip4addr_aton(LOCAL_IP, &ipaddr);
	if (!ret) {
		fprintf(stderr, "Failed to parse local IP %s\n", LOCAL_IP);
		is_err = 1;
		goto test_init_exit;
	}

	ret = ip4addr_aton(LOCAL_GW, &gw);
	if (!ret) {
		fprintf(stderr, "Failed to parse local gateway %s\n", LOCAL_GW);
		is_err = 1;
		goto test_init_exit;
	}

	ret = ip4addr_aton(LOCAL_MASK, &netmask);
	if (!ret) {
		fprintf(stderr, "Failed to parse netmask %s\n", LOCAL_MASK);
		is_err = 1;
		goto test_init_exit;
	}

	netif_add(&netif, &ipaddr, &netmask, &gw, NULL, dpdk_device_init, tcpip_input);
	netif_set_default(&netif);
	netif_set_up(&netif);

test_init_exit:
	sys_sem_signal(init_sem);
}


static void server_thread(void *arg)
{
	LWIP_UNUSED_ARG(arg);
	sockets_server(SERVER_IP, SERVER_PORT);
}

static void client_thread(void *arg)
{
	LWIP_UNUSED_ARG(arg);
	sockets_client(SERVER_IP, SERVER_PORT);
}


int main(void)
{
	sys_sem_t init_sem;
	err_t err;
	int ret = 0;
	is_err = 0;
	sys_thread_t server, client;

	ret = init_dpdk();

	err = sys_sem_new(&init_sem, 0);
	LWIP_ASSERT("failed to create init_sem", err == ERR_OK);
	LWIP_UNUSED_ARG(err);

	tcpip_init(test_init, &init_sem);
	sys_sem_wait(&init_sem);
	sys_sem_free(&init_sem);

	if (is_err)
		return 1;

	fprintf(stdout, "LwIP TCP/IP stack initialized.\n");

//	server_thread(NULL);
//	server = sys_thread_new("test_server", server_thread, NULL, 0, 0);
//	if (server == NULL) {
//		fprintf(stderr, "Failed to create server thread\n");
//		return 1;
//	}
//	client = sys_thread_new("test_client", client_thread, NULL, 0, 0);
//	if (client == NULL) {
//		fprintf(stderr, "Failed to create client thread\n");
//		return 1;
//	}
//	pause();
#if IS_CLIENT
	sockets_client(SERVER_IP, SERVER_PORT);
#else
	sockets_server(SERVER_IP, SERVER_PORT);
#endif
	pause();
	return 0;
}
