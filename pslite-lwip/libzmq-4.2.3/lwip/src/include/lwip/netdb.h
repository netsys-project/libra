/**
 * @file
 * NETDB API (sockets)
 */

/*
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Simon Goldschmidt
 *
 */
#ifndef LWIP_HDR_NETDB_H
#define LWIP_HDR_NETDB_H

#include "lwip/opt.h"

#if LWIP_DNS && LWIP_SOCKET

#include "lwip/arch.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

#ifdef __cplusplus
extern "C" {
#endif

/* some rarely used options */
#ifndef LWIP_DNS_API_DECLARE_H_ERRNO
#define LWIP_DNS_API_DECLARE_H_ERRNO  1
#endif

#ifndef LWIP_DNS_API_DEFINE_ERRORS
#define LWIP_DNS_API_DEFINE_ERRORS    1
#endif

#ifndef LWIP_DNS_API_DEFINE_FLAGS
#define LWIP_DNS_API_DEFINE_FLAGS     1
#endif

#ifndef LWIP_DNS_API_DECLARE_STRUCTS
#define LWIP_DNS_API_DECLARE_STRUCTS  1
#endif

#if LWIP_DNS_API_DEFINE_ERRORS
/** Errors used by the DNS API functions, lwip_h_errno can be one of them */
#define LWIP_EAI_NONAME      200
#define LWIP_EAI_SERVICE     201
#define LWIP_EAI_FAIL        202
#define LWIP_EAI_MEMORY      203
#define LWIP_EAI_FAMILY      204

#define LWIP_HOST_NOT_FOUND  210
#define LWIP_NO_DATA         211
#define LWIP_NO_RECOVERY     212
#define LWIP_TRY_AGAIN       213
#endif /* LWIP_DNS_API_DEFINE_ERRORS */

#if LWIP_DNS_API_DEFINE_FLAGS
/* input flags for struct lwip_addrinfo */
#define LWIP_AI_PASSIVE      0x01
#define LWIP_AI_CANONNAME    0x02
#define LWIP_AI_NUMERICHOST  0x04
#define LWIP_AI_NUMERICSERV  0x08
#define LWIP_AI_V4MAPPED     0x10
#define LWIP_AI_ALL          0x20
#define LWIP_AI_ADDRCONFIG   0x40
#endif /* LWIP_DNS_API_DEFINE_FLAGS */

#if LWIP_DNS_API_DECLARE_STRUCTS
struct lwip_hostent {
    char  *h_name;      /* Official name of the host. */
    char **h_aliases;   /* A pointer to an array of pointers to alternative host names,
                           terminated by a null pointer. */
    int    h_addrtype;  /* Address type. */
    int    h_length;    /* The length, in bytes, of the address. */
    char **h_addr_list; /* A pointer to an array of pointers to network addresses (in
                           network byte order) for the host, terminated by a null pointer. */
#define h_addr h_addr_list[0] /* for backward compatibility */
};

struct lwip_addrinfo {
    int               ai_flags;      /* Input flags. */
    int               ai_family;     /* Address family of socket. */
    int               ai_socktype;   /* Socket type. */
    int               ai_protocol;   /* Protocol of socket. */
    socklen_t         ai_addrlen;    /* Length of socket address. */
    struct lwip_sockaddr  *ai_addr;       /* Socket address of socket. */
    char             *ai_canonname;  /* Canonical name of service location. */
    struct lwip_addrinfo  *ai_next;       /* Pointer to next in list. */
};
#endif /* LWIP_DNS_API_DECLARE_STRUCTS */

#define NETDB_ELEM_SIZE           (sizeof(struct lwip_addrinfo) + sizeof(struct lwip_sockaddr_storage) + DNS_MAX_NAME_LENGTH + 1)

#if LWIP_DNS_API_DECLARE_H_ERRNO
/* application accessible error code set by the DNS API functions */
extern int lwip_h_errno;
#endif /* LWIP_DNS_API_DECLARE_H_ERRNO*/

 struct lwip_hostent *lwip_gethostbyname(const char *name);
int lwip_gethostbyname_r(const char *name,  struct lwip_hostent *ret, char *buf,
                size_t buflen,  struct lwip_hostent **result, int *lwip_h_errnop);
void lwip_freeaddrinfo(struct lwip_addrinfo *ai);
int lwip_getaddrinfo(const char *nodename,
       const char *servname,
       const struct lwip_addrinfo *hints,
       struct lwip_addrinfo **res);

#if LWIP_COMPAT_SOCKETS
/** @ingroup netdbapi */
#define gethostbyname(name) lwip_gethostbyname(name)
/** @ingroup netdbapi */
#define gethostbyname_r(name, ret, buf, buflen, result, lwip_h_errnop) \
       lwip_gethostbyname_r(name, ret, buf, buflen, result, lwip_h_errnop)
/** @ingroup netdbapi */
#define freeaddrinfo(addrinfo) lwip_freeaddrinfo(addrinfo)
/** @ingroup netdbapi */
#define getaddrinfo(nodname, servname, hints, res) \
       lwip_getaddrinfo(nodname, servname, hints, res)
#endif /* LWIP_COMPAT_SOCKETS */

#ifdef __cplusplus
}
#endif

#endif /* LWIP_DNS && LWIP_SOCKET */

#endif /* LWIP_HDR_NETDB_H */
