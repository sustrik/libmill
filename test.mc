
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
    call tcpsocket_accept (&rc, &ls, &s1);
    call tcpsocket_connect(&rc, &s2, (struct sockaddr*) &addr);
    wait (0);
    assert (rc == 0);
    wait (0);
    assert (rc == 0);

    call tcpsocket_send (&rc, &s1, "Hi", 2);
    wait (0);
    assert (rc == 0);

    call tcpsocket_recv (&rc, &s2, buf, 2);
    wait (0);
    assert (rc == 0);

    call tcpsocket_term (&s2);
    wait (0);
    call tcpsocket_term (&s1);
    wait (0);
    call tcpsocket_term (&ls);
    wait (0);
}

int main ()
{
    test ();
    return 0;
}
