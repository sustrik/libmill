
#include <assert.h>

#include "../stdmill.h"

coroutine test ()
{
    int rc;
    struct addrinfo *info;
    struct in_addr addr;
    endvars;

	go getaddressinfo (&rc, "127.0.0.1", NULL, NULL, &info);
    select {
    case getaddressinfo:
    }

    assert (rc == 0);
    assert (info);
    assert (info->ai_family == AF_INET);
    addr = ((struct sockaddr_in*) info->ai_addr)->sin_addr;
    assert (addr.s_addr == 0x100007f);
    freeaddrinfo (info);
}

int main ()
{
    test();
}

