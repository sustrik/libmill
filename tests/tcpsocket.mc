
#include "stdmill.h"

coroutine test ()
{
    int rc;
    struct sockaddr_in addr;
    struct tcpsocket ls;
    struct tcpsocket s1;
    struct tcpsocket s2;
    char buf [2];
    endvars;

    rc = uv_ip4_addr ("127.0.0.1", 7000, &addr);
    assert (rc == 0);
    rc = tcpsocket_init (&ls);
    assert (rc == 0);
    rc = tcpsocket_init (&s1);
    assert (rc == 0);
    rc = tcpsocket_init (&s2);
    assert (rc == 0);
    rc = tcpsocket_bind (&ls, (struct sockaddr*) &addr, 0);
    assert (rc == 0);
    rc = tcpsocket_listen (&ls, 10);
    assert (rc == 0);
    go tcpsocket_accept (&rc, &ls, &s1);
    go tcpsocket_connect(&rc, &s2, (struct sockaddr*) &addr);
    select {
    case tcpsocket_accept:
    case tcpsocket_connect:
    }
    assert (rc == 0);
    select {
    case tcpsocket_accept:
    case tcpsocket_connect:
    }
    assert (rc == 0);

    go tcpsocket_send (&rc, &s1, "Hi", 2);
    select {
    case tcpsocket_send:
    }
    assert (rc == 0);

    go tcpsocket_recv (&rc, &s2, buf, 2);
    select {
    case tcpsocket_recv:
    }
    assert (rc == 0);

    go tcpsocket_term (&s2);
    select {
    case tcpsocket_term:
    }
    go tcpsocket_term (&s1);
    select {
    case tcpsocket_term:
    }
    go tcpsocket_term (&ls);
    select {
    case tcpsocket_term:
    }
}

int main ()
{
    test ();
    return 0;
}
