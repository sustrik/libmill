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

#include "ip.h"
#include "libmill.h"
#include "utils.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

MILL_CT_ASSERT(sizeof(ipaddr) >= sizeof(struct sockaddr_in));
MILL_CT_ASSERT(sizeof(ipaddr) >= sizeof(struct sockaddr_in6));

static ipaddr mill_ipany(int port, int mode)
{
    ipaddr addr;
    if(mill_slow(port < 0 || port > 0xffff)) {
        ((struct sockaddr*)&addr)->sa_family = AF_UNSPEC;
        errno = EINVAL;
        return addr;
    }
    if (mode == IPADDR_IPV4 || mode == IPADDR_PREF_IPV4) {
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

/* Convert literal IPv4 or IPv6 address to a binary one.
   TODO: Take mode into consideration. */
static ipaddr mill_ipliteral(const char *addr, int port, int mode) {
    ipaddr raddr;
    struct sockaddr *sa = (struct sockaddr*)&raddr;

    if(mill_slow(!addr || port < 0 || port > 0xffff)) {
        sa->sa_family = AF_UNSPEC;
        errno = EINVAL;
        return raddr;
    }
    // Try to interpret the string is IPv4 address.
    struct sockaddr_in *ipv4 = (struct sockaddr_in*)sa;
    int rc = inet_pton(AF_INET, addr, &ipv4->sin_addr);
    mill_assert(rc >= 0);
    if(rc == 1) {
        ipv4->sin_family = AF_INET;
        ipv4->sin_port = htons((uint16_t)port);
        errno = 0;
        return raddr;
    }
    // It's not an IPv4 address. Let's try to interpret it as IPv6 address.
    struct sockaddr_in6 *ipv6 = (struct sockaddr_in6*)sa;
    rc = inet_pton(AF_INET6, addr, &ipv6->sin6_addr);
    mill_assert(rc >= 0);
    if(rc == 1) {
        ipv6->sin6_family = AF_INET6;
        ipv6->sin6_port = htons((uint16_t)port);
        errno = 0;
        return raddr;
    }
    // It's neither IPv4, nor IPv6 address.
    sa->sa_family = AF_UNSPEC;
    errno = EINVAL;
    return raddr;
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

ipaddr iplocal(const char *name, int port, int mode) {
    if(!name)
        return mill_ipany(port, mode);
    return mill_ipliteral(name, port, mode);
    /* TODO: Try to resolve the interface name here. */
}

ipaddr ipremote(const char *name, int port, int mode, int64_t deadline) {
    return mill_ipliteral(name, port, mode);
    /* TODO: Do a DNS query here. */
}

