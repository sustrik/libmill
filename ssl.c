/*

  Copyright (c) 2016 John E. Haque

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

#if defined HAVE_SSL

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "debug.h"
#include "libmill.h"
#include "utils.h"

/* Defined in tcp.c, not exposed via libmill.h */
int mill_tcpfd(struct mill_tcpsock_ *s);

/* Only one client context is needed, so let's make it global. */
static SSL_CTX *ssl_cli_ctx;

enum mill_ssltype {
   MILL_SSLLISTENER,
   MILL_SSLCONN
};

struct mill_sslsock_ {
    enum mill_ssltype type;
};

struct mill_ssllistener {
    struct mill_sslsock_ sock;
    tcpsock s;
    SSL_CTX *ctx;
};

struct mill_sslconn {
    struct mill_sslsock_ sock;
    tcpsock s;
    BIO *bio;
};

/* Initialise OpenSSL library. */
static void ssl_init(void) {
    static int initialised = 0;
    if(mill_fast(initialised))
        return;
    initialised = 1;
    OpenSSL_add_all_algorithms();
    SSL_library_init();

    /* TODO: Move to sslconnect() */
    ssl_cli_ctx = SSL_CTX_new(SSLv23_client_method());
    mill_assert(ssl_cli_ctx);
}

struct mill_sslsock_ *mill_ssllisten_(struct mill_ipaddr addr,
      const char *cert_file, const char *key_file, int backlog) {
    ssl_init();
    /* Load certificates. */
    SSL_CTX *ctx = SSL_CTX_new(SSLv23_server_method());
    if(!ctx)
        return NULL;
    if(cert_file && SSL_CTX_use_certificate_chain_file(ctx, cert_file) <= 0)
        return NULL;
    if(key_file && SSL_CTX_use_PrivateKey_file(ctx,
          key_file, SSL_FILETYPE_PEM) <= 0)
        return NULL;
    /* Check for inconsistent private key. */
    if(SSL_CTX_check_private_key(ctx) <= 0)
        return NULL;
    /* Open the listening socket. */
    tcpsock s = tcplisten(addr, backlog);
    if(!s) {
        /* TODO: close the context */
        return NULL;
    }
    /* Create the object. */
    struct mill_ssllistener *l = malloc(sizeof(struct mill_ssllistener));
    if(!l) {
        /* TODO: close the context */
        tcpclose(s);
        errno = ENOMEM;
        return NULL;
    }
    l->sock.type = MILL_SSLLISTENER;
    l->s = s;
    l->ctx = ctx;
    errno = 0;
    return &l->sock;
}

int mill_sslport_(struct mill_sslsock_ *s) {
    if(s->type == MILL_SSLLISTENER) {
        struct mill_ssllistener *l = (struct mill_ssllistener*)s;
        return tcpport(l->s);
    }
    if(s->type == MILL_SSLCONN) {
        struct mill_sslconn *c = (struct mill_sslconn*)s;
        return tcpport(c->s);
    }
    mill_assert(0);
}


static int ssl_wait(struct mill_sslconn *c, int64_t deadline) {
    if(!BIO_should_retry(c->bio)) {
        unsigned long sslerr = ERR_get_error();
        /* XXX: Ref. openssl/source/ssl/ssl_lib.c .. */
        if(ERR_GET_LIB(sslerr) != ERR_LIB_SYS)
            errno = ECONNRESET;
        return -1;
    }

    if(BIO_should_read(c->bio)) {
        int rc = fdwait(mill_tcpfd(c->s), FDW_IN, deadline);
        if(rc == 0) {
            errno = ETIMEDOUT;
            return -1;
        }
        return rc;
    }
    if(BIO_should_write(c->bio)) {
        int rc = fdwait(mill_tcpfd(c->s), FDW_OUT, deadline);
        if(rc == 0) {
            errno = ETIMEDOUT;
            return -1;
        }
        return rc;
    }
    /* mill_assert(!BIO_should_io_special(c->bio)); */
    errno = ECONNRESET;
    return -1;  /* should not happen ? */
}

void mill_sslclose_(struct mill_sslsock_ *s) {
    switch(s->type) {
    case MILL_SSLLISTENER:;
        struct mill_ssllistener *l = (struct mill_ssllistener*)s;
        SSL_CTX_free(l->ctx);
        /* TODO: close the context */
        tcpclose(l->s);
        free(l);
        break;
    case MILL_SSLCONN:;
        struct mill_sslconn *c = (struct mill_sslconn*)s;
        tcpclose(c->s);
        BIO_ssl_shutdown(c->bio);
        BIO_free_all(c->bio);
        free(c);
        break;
    default:
        mill_assert(0);
    }
    errno = 0;
}

static struct mill_sslsock_ *ssl_conn_new(tcpsock s, SSL_CTX *ctx, int client) {
    mill_assert(ctx);
    SSL *ssl = NULL;
    BIO *sbio = BIO_new_ssl(ctx, client);
    if(!sbio)
        return NULL;
    BIO_get_ssl(sbio, &ssl);
    if(!ssl) {
        BIO_free(sbio);
        return NULL;
    }

    /* set .._PARTIAL_WRITE for non-blocking operation */
    SSL_set_mode(ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);
    BIO *cbio = BIO_new_socket(mill_tcpfd(s), BIO_NOCLOSE);
    if(!cbio) {
        BIO_free(sbio);
        return NULL;
    } 
    BIO_push(sbio, cbio);
    struct mill_sslconn *c = malloc(sizeof (struct mill_sslconn));
    if(!c) {
        BIO_free_all(sbio);
        return NULL;
    }

    /*  mill_assert(BIO_get_fd(sbio, NULL) == fd); */

    c->sock.type = MILL_SSLCONN;
    c->bio = sbio;
    c->s = s;
    /* OPTIONAL: call ssl_handshake() to check/verify peer certificate */
    return &c->sock;
}

size_t mill_sslrecv_(struct mill_sslsock_ *s, void *buf, int len,
      int64_t deadline) {
    if(s->type != MILL_SSLCONN)
        mill_panic("trying to use an unconnected socket");
    struct mill_sslconn *c = (struct mill_sslconn*)s;

    int rc;
    if(len < 0) {
        errno = EINVAL;
        return -1;
    }
    while(1) {
        rc = BIO_read(c->bio, buf, len);
        if(rc >= 0)
            return rc;
        if(ssl_wait(c, deadline) < 0)
            return 0;
    }
}

size_t mill_sslrecvuntil_(struct mill_sslsock_ *s, void *buf, size_t len,
      const char *delims, size_t delimcount, int64_t deadline) {
    if(s->type != MILL_SSLCONN)
        mill_panic("trying to receive from an unconnected socket");
    char *pos = (char*)buf;
    size_t i;
    for(i = 0; i != len; ++i, ++pos) {
        size_t res = sslrecv(s, pos, 1, deadline);
        if(res == 1) {
            size_t j;
            for(j = 0; j != delimcount; ++j)
                if(*pos == delims[j])
                    return i + 1;
        }
        if (errno != 0)
            return i + res;
    }
    errno = ENOBUFS;
    return len;
}

size_t mill_sslsend_(struct mill_sslsock_ *s, const void *buf, int len,
      int64_t deadline) {
    if(s->type != MILL_SSLCONN)
        mill_panic("trying to use an unconnected socket");
    struct mill_sslconn *c = (struct mill_sslconn*)s;

    int rc;
    if(len < 0) {
        errno = EINVAL;
        return -1;
    }
    while(1) {
        rc = BIO_write(c->bio, buf, len);
        if (rc >= 0)
            return rc;
        if(ssl_wait(c, deadline) < 0)
            return 0;
    }
}

void mill_sslflush_(struct mill_sslsock_ *s, int64_t deadline) {
    if(s->type != MILL_SSLCONN)
        mill_panic("trying to use an unconnected socket");
    struct mill_sslconn *c = (struct mill_sslconn*)s;

    int rc;
    while(1) {
        rc = BIO_flush(c->bio);
        if (rc >= 0)
            break;
        if(ssl_wait(c, deadline) < 0)
            return;
    }
    errno = 0;
}

struct mill_sslsock_ *mill_sslconnect_(struct mill_ipaddr addr,
      int64_t deadline) {
    ssl_init();
    tcpsock sock = tcpconnect(addr, deadline);
    if(!sock)
        return NULL;
    struct mill_sslsock_ *c = ssl_conn_new(sock, ssl_cli_ctx, 1);
    if(!c) {
        tcpclose(sock);
        errno = ENOMEM;
        return NULL;
    }
    return c;
}

struct mill_sslsock_ *mill_sslaccept_(struct mill_sslsock_ *s, int64_t deadline) {
    if(s->type != MILL_SSLLISTENER)
        mill_panic("trying to accept on a socket that isn't listening");
    struct mill_ssllistener *l = (struct mill_ssllistener*)s;
    tcpsock sock = tcpaccept(l->s, deadline);
    if(!sock)
        return NULL;
    struct mill_sslsock_ *c = ssl_conn_new(sock, l->ctx, 0);
    if(!c) {
        tcpclose(sock);
        errno = ENOMEM;
        return NULL;
    }
    return c;
}

ipaddr mill_ssladdr_(struct mill_sslsock_ *s) {
    if(s->type != MILL_SSLCONN)
        mill_panic("trying to get address from a socket that isn't connected");
    struct mill_sslconn *c = (struct mill_sslconn*)s;
    return tcpaddr(c->s);
}

#endif

