
#include <stdmill.mh>

/* Test accept after connection is established. */
coroutine test1 ()
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
    go tcpsocket_connect(&rc, &s2, (struct sockaddr*) &addr);
    select {
    case tcpsocket_connect:
    }
    assert (rc == 0);
    go msleep (100);
    select {
    case msleep:
    }

    go tcpsocket_accept (&rc, &s1, &ls);
    select {
    case tcpsocket_accept:
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

/* Test accept before connection is established. */
coroutine test2 ()
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

    /* Try accepting on a socket that's not listening. */
    go tcpsocket_accept (&rc, &s1, &ls);
    select {
    case tcpsocket_accept:
        assert (rc == -123456);
    }

    rc = tcpsocket_listen (&ls, 10);
    assert (rc == 0);
    go tcpsocket_accept (&rc, &s1, &ls);

    /* Test attempt to do 2 accepts in parallel. */
    go tcpsocket_accept (&rc, &s1, &ls);
    select {
    case tcpsocket_accept:
        assert (rc == -123456);
    }
    
    go tcpsocket_connect(&rc, &s2, (struct sockaddr*) &addr);
    select {
    case tcpsocket_connect:
    case tcpsocket_accept:
    }
    assert (rc == 0);

    select {
    case tcpsocket_connect:
    case tcpsocket_accept:
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
    test1 ();
    test2 ();
    return 0;
}

