/*

  Copyright (c) 2015 Martin Sustrik

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.

*/

#if defined __linux__
#define _GNU_SOURCE
#include <netdb.h>
#include <sys/eventfd.h>
#endif

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#if !defined __sun
#include <ifaddrs.h>
#endif
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "dns/dns.h"

#include "ip.h"
#include "libmill.h"
#include "utils.h"

MILL_CT_ASSERT(sizeof(ipaddr) >= sizeof(struct sockaddr_in));
MILL_CT_ASSERT(sizeof(ipaddr) >= sizeof(struct sockaddr_in6));

static struct dns_resolv_conf *mill_dns_conf = NULL;
static struct dns_hosts *mill_dns_hosts = NULL;
static struct dns_hints *mill_dns_hints = NULL;

static ipaddr mill_ipany(int port, int mode)
{
    ipaddr addr;
    if(mill_slow(port < 0 || port > 0xffff)) {
        ((struct sockaddr*)&addr)->sa_family = AF_UNSPEC;
        errno = EINVAL;
        return addr;
    }
    if (mode == 0 || mode == IPADDR_IPV4 || mode == IPADDR_PREF_IPV4) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in*)&addr;
        ipv4->sin_family = AF_INET;
        ipv4->sin_addr.s_addr = htonl(INADDR_ANY);
        ipv4->sin_port = htons((uint16_t)port);
    }
    else {
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6*)&addr;
        ipv6->sin6_family = AF_INET6;
        memcpy(&ipv6->sin6_addr, &in6addr_any, sizeof(in6addr_any));
        ipv6->sin6_port = htons((uint16_t)port);
    }
    errno = 0;
    return addr;
}

/* Convert literal IPv4 address to a binary one. */
static ipaddr mill_ipv4_literal(const char *addr, int port) {
    ipaddr raddr;
    struct sockaddr_in *ipv4 = (struct sockaddr_in*)&raddr;
    int rc = inet_pton(AF_INET, addr, &ipv4->sin_addr);
    mill_assert(rc >= 0);
    if(rc == 1) {
        ipv4->sin_family = AF_INET;
        ipv4->sin_port = htons((uint16_t)port);
        errno = 0;
        return raddr;
    }
    ipv4->sin_family = AF_UNSPEC;
    errno = EINVAL;
    return raddr;
}

/* Convert literal IPv6 address to a binary one. */
static ipaddr mill_ipv6_literal(const char *addr, int port) {
    ipaddr raddr;
    struct sockaddr_in6 *ipv6 = (struct sockaddr_in6*)&raddr;
    int rc = inet_pton(AF_INET6, addr, &ipv6->sin6_addr);
    mill_assert(rc >= 0);
    if(rc == 1) {
        ipv6->sin6_family = AF_INET6;
        ipv6->sin6_port = htons((uint16_t)port);
        errno = 0;
        return raddr;
    }
    ipv6->sin6_family = AF_UNSPEC;
    errno = EINVAL;
    return raddr;
}

/* Convert literal IPv4 or IPv6 address to a binary one. */
static ipaddr mill_ipliteral(const char *addr, int port, int mode) {
    ipaddr raddr;
    struct sockaddr *sa = (struct sockaddr*)&raddr;
    if(mill_slow(!addr || port < 0 || port > 0xffff)) {
        sa->sa_family = AF_UNSPEC;
        errno = EINVAL;
        return raddr;
    }
    switch(mode) {
    case IPADDR_IPV4:
        return mill_ipv4_literal(addr, port);
    case IPADDR_IPV6:
        return mill_ipv6_literal(addr, port);
    case 0:
    case IPADDR_PREF_IPV4:
        raddr = mill_ipv4_literal(addr, port);
        if(errno == 0)
            return raddr;
        return mill_ipv6_literal(addr, port);
    case IPADDR_PREF_IPV6:
        raddr = mill_ipv6_literal(addr, port);
        if(errno == 0)
            return raddr;
        return mill_ipv4_literal(addr, port);
    default:
        mill_assert(0);
    }
}

int mill_ipfamily(ipaddr addr) {
    return ((struct sockaddr*)&addr)->sa_family;
}

int mill_iplen(ipaddr addr) {
    return mill_ipfamily(addr) == AF_INET ?
        sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
}

int mill_ipport(ipaddr addr) {
    return ntohs(mill_ipfamily(addr) == AF_INET ?
        ((struct sockaddr_in*)&addr)->sin_port :
        ((struct sockaddr_in6*)&addr)->sin6_port);
}

/* Convert IP address from network format to ASCII dot notation. */
const char *mill_ipaddrstr_(ipaddr addr, char *ipstr) {
    if (mill_ipfamily(addr) == AF_INET) {
        return inet_ntop(AF_INET, &(((struct sockaddr_in*)&addr)->sin_addr),
            ipstr, INET_ADDRSTRLEN);
    }
    else {
        return inet_ntop(AF_INET6, &(((struct sockaddr_in6*)&addr)->sin6_addr),
            ipstr, INET6_ADDRSTRLEN);
    }
}

ipaddr mill_iplocal_(const char *name, int port, int mode) {
    if(!name)
        return mill_ipany(port, mode);
    ipaddr addr = mill_ipliteral(name, port, mode);
#if defined __sun
    return addr;
#else
    if(errno == 0)
       return addr;
    /* Address is not a literal. It must be an interface name then. */
    struct ifaddrs *ifaces = NULL;
    int rc = getifaddrs (&ifaces);
    mill_assert (rc == 0);
    mill_assert (ifaces);
    /*  Find first IPv4 and first IPv6 address. */
    struct ifaddrs *ipv4 = NULL;
    struct ifaddrs *ipv6 = NULL;
    struct ifaddrs *it;
    for(it = ifaces; it != NULL; it = it->ifa_next) {
        if(!it->ifa_addr)
            continue;
        if(strcmp(it->ifa_name, name) != 0)
            continue;
        switch(it->ifa_addr->sa_family) {
        case AF_INET:
            mill_assert(!ipv4);
            ipv4 = it;
            break;
        case AF_INET6:
            mill_assert(!ipv6);
            ipv6 = it;
            break;
        }
        if(ipv4 && ipv6)
            break;
    }
    /* Choose the correct address family based on mode. */
    switch(mode) {
    case IPADDR_IPV4:
        ipv6 = NULL;
        break;
    case IPADDR_IPV6:
        ipv4 = NULL;
        break;
    case 0:
    case IPADDR_PREF_IPV4:
        if(ipv4)
           ipv6 = NULL;
        break;
    case IPADDR_PREF_IPV6:
        if(ipv6)
           ipv4 = NULL;
        break;
    default:
        mill_assert(0);
    }
    if(ipv4) {
        struct sockaddr_in *inaddr = (struct sockaddr_in*)&addr;
        memcpy(inaddr, ipv4->ifa_addr, sizeof (struct sockaddr_in));
        inaddr->sin_port = htons(port);
        freeifaddrs(ifaces);
        errno = 0;
        return addr;
    }
    if(ipv6) {
        struct sockaddr_in6 *inaddr = (struct sockaddr_in6*)&addr;
        memcpy(inaddr, ipv6->ifa_addr, sizeof (struct sockaddr_in6));
        inaddr->sin6_port = htons(port);
        freeifaddrs(ifaces);
        errno = 0;
        return addr;
    }
    freeifaddrs(ifaces);
    ((struct sockaddr*)&addr)->sa_family = AF_UNSPEC;
    errno = ENODEV;
    return addr;
#endif
}

ipaddr mill_ipremote_(const char *name, int port, int mode, int64_t deadline) {
    int rc;
    ipaddr addr = mill_ipliteral(name, port, mode);
    if(errno == 0)
       return addr;
    /* Load DNS config files, unless they are already chached. */
    if(mill_slow(!mill_dns_conf)) {
        /* TODO: Maybe re-read the configuration once in a while? */
        mill_dns_conf = dns_resconf_local(&rc);
        mill_assert(mill_dns_conf);
        mill_dns_hosts = dns_hosts_local(&rc);
        mill_assert(mill_dns_hosts);
        mill_dns_hints = dns_hints_local(mill_dns_conf, &rc);
        mill_assert(mill_dns_hints);
    }
    /* Let's do asynchronous DNS query here. */
    struct dns_resolver *resolver = dns_res_open(mill_dns_conf, mill_dns_hosts,
        mill_dns_hints, NULL, dns_opts(), &rc);
    mill_assert(resolver);
    mill_assert(port >= 0 && port <= 0xffff);
    char portstr[8];
    snprintf(portstr, sizeof(portstr), "%d", port);
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    struct dns_addrinfo *ai = dns_ai_open(name, portstr, DNS_T_A, &hints,
        resolver, &rc);
    mill_assert(ai);
    dns_res_close(resolver);
    struct addrinfo *ipv4 = NULL;
    struct addrinfo *ipv6 = NULL;
    struct addrinfo *it = NULL;
    while(1) {
        rc = dns_ai_nextent(&it, ai);
        if(rc == EAGAIN) {
            int fd = dns_ai_pollfd(ai);
            mill_assert(fd >= 0);
            int events = fdwait(fd, FDW_IN, deadline);
            /* There's no guarantee that the file descriptor will be reused
               in next iteration. We have to clean the fdwait cache here
               to be on the safe side. */
            fdclean(fd);
            if(mill_slow(!events)) {
                errno = ETIMEDOUT;
                return addr;
            }
            mill_assert(events == FDW_IN);
            continue;
        }
        if(rc == ENOENT)
            break;

        if(!ipv4 && it && it->ai_family == AF_INET) {
            ipv4 = it;
        }
        else if(!ipv6 && it && it->ai_family == AF_INET6) {
            ipv6 = it;
        }
        else {
            free(it);
        }
        
        if(ipv4 && ipv6)
            break;
    }
    switch(mode) {
    case IPADDR_IPV4:
        if(ipv6) {
            free(ipv6);
            ipv6 = NULL;
        }
        break;
    case IPADDR_IPV6:
        if(ipv4) {
            free(ipv4);
            ipv4 = NULL;
        }
        break;
    case 0:
    case IPADDR_PREF_IPV4:
        if(ipv4 && ipv6) {
            free(ipv6);
            ipv6 = NULL;
        }
        break;
    case IPADDR_PREF_IPV6:
        if(ipv6 && ipv4) {
            free(ipv4);
            ipv4 = NULL;
        }
        break;
    default:
        mill_assert(0);
    }
    if(ipv4) {
        struct sockaddr_in *inaddr = (struct sockaddr_in*)&addr;
        memcpy(inaddr, ipv4->ai_addr, sizeof (struct sockaddr_in));
        inaddr->sin_port = htons(port);
        dns_ai_close(ai);
        free(ipv4);
        errno = 0;
        return addr;
    }
    if(ipv6) {
        struct sockaddr_in6 *inaddr = (struct sockaddr_in6*)&addr;
        memcpy(inaddr, ipv6->ai_addr, sizeof (struct sockaddr_in6));
        inaddr->sin6_port = htons(port);
        dns_ai_close(ai);
        free(ipv6);
        errno = 0;
        return addr;
    }
    dns_ai_close(ai);
    ((struct sockaddr*)&addr)->sa_family = AF_UNSPEC;
    errno = EADDRNOTAVAIL;
    return addr;
}

