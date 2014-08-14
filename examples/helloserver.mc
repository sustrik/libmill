
#include <stdmill.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct connection
{
    struct tcpsocket s;
};

coroutine dialogue (struct connection *conn)
{
    int rc;
    const char *hi = "Hello, world!\n";
    endvars;

    go tcpsocket_send (&rc, &conn->s, hi, strlen (hi));
    select {
    case tcpsocket_send:
        assert (rc == 0);
    }

    go tcpsocket_term (&conn->s);
    select {
    case tcpsocket_term:
    }

    free (conn);
}

coroutine server ()
{
    int rc;
    struct sockaddr_in addr;
    struct tcpsocket ls;
    struct connection *conn;
    endvars;

    /* Open the listening socket. */
    rc = uv_ip4_addr ("127.0.0.1", 7000, &addr);
    assert (rc == 0);
    rc = tcpsocket_init (&ls);
    assert (rc == 0);
    rc = tcpsocket_bind (&ls, (struct sockaddr*) &addr, 0);
    assert (rc == 0);
    rc = tcpsocket_listen (&ls, 10);
    assert (rc == 0);

    while (1) {

        /* Create a connection object for the new incoming connection. */
        conn = malloc (sizeof (struct connection));
        assert (conn);
        rc = tcpsocket_init (&conn->s);
        assert (rc == 0);
        go tcpsocket_accept (&rc, &ls, &conn->s);
        while (1) {
            select {
            case tcpsocket_accept:
                assert (rc == 0);
                printf ("Connection opened!\n");
                go dialogue (conn);
                break;
            case dialogue:
                printf ("Connection closed!\n");
            }
        }        
    }
}

int main ()
{
    server ();
    return 0;
}

