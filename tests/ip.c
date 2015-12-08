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

#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "../libmill.h"

int main() {
    ipaddr addr = ipremote("www.example.org", 80, IPADDR_IPV4, now() + 1000);
    if(errno != ETIMEDOUT && errno != EADDRNOTAVAIL) {
       assert(errno == 0);
       struct sockaddr_in *ain = (struct sockaddr_in*)&addr;
       assert(ain->sin_family == AF_INET);
       assert(ain->sin_port == htons(80));
    }
    ipaddr addr6 = ipremote("www.example.org", 80, IPADDR_IPV6, now() + 1000);
    if(errno != ETIMEDOUT && errno != EADDRNOTAVAIL) {
       assert(errno == 0);
       struct sockaddr_in6 *ain6 = (struct sockaddr_in6*)&addr6;
       assert(ain6->sin6_family == AF_INET6);
       assert(ain6->sin6_port == htons(80));
    }
    return 0;
}

