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

#include "utils.h"

#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>

/* Convert textual IPv4 or IPv6 address to a binary one. */
int mill_resolve(const char *addr, int port,
      struct sockaddr_storage *ss, socklen_t *len) {
    mill_assert(ss);
    if(port < 0 || port > 0xffff) {
        errno = EINVAL;
        return -1;
    }
    // NULL translates to INADDR_ANY.
    // TODO: Consider using in6addr_any. AFAICS that should account for IPv4
    //       addresses as well. However, would it work on all systems?
    if(!addr) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in*)ss;
        ipv4->sin_family = AF_INET;
        ipv4->sin_addr.s_addr = INADDR_ANY;
        ipv4->sin_port = htons((uint16_t)port);
        if(len)
            *len = sizeof(struct sockaddr_in);
        errno = 0;
        return 0;
    }
    // Try to interpret the string is IPv4 address.
    int rc = inet_pton(AF_INET, addr, ss);
    mill_assert(rc >= 0);
    if(rc == 1) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in*)ss;
        ipv4->sin_family = AF_INET;
        ipv4->sin_port = htons((uint16_t)port);
        if(len)
            *len = sizeof(struct sockaddr_in);
        errno = 0;
        return 0;
    }

    // It's not an IPv4 address. Let's try to interpret it as IPv6 address.
    rc = inet_pton(AF_INET6, addr, ss);
    mill_assert(rc >= 0);
    if(rc == 1) {
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6*)ss;
        ipv6->sin6_family = AF_INET6;
        ipv6->sin6_port = htons((uint16_t)port);
        if(len)
            *len = sizeof(struct sockaddr_in6);
        errno = 0;
        return 0;
    }

    // It's neither IPv4, nor IPv6 address.
    errno = EINVAL;
    return -1;
}

