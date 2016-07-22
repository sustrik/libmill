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
int tcpdetach(struct mill_tcpsock *s);

static SSL_CTX *ssl_cli_ctx;
static SSL_CTX *ssl_serv_ctx;

enum mill_ssltype {
   MILL_SSLLISTENER,
   MILL_SSLCONN
};

struct mill_sslsock {
    enum mill_ssltype type;
};

struct mill_ssllistener {
    struct mill_sslsock sock;
    tcpsock s;
};

struct mill_sslconn {
    struct mill_sslsock sock;
    unsigned long sslerr; 
    int fd;
    BIO *bio;
};

struct mill_sslsock *mill_ssllisten_(struct mill_ipaddr addr, int backlog) {
    /* Open the listening socket. */
    tcpsock s = tcplisten(addr, backlog);
    if(!s)
        return NULL;
    /* Create the object. */
    struct mill_ssllistener *l = malloc(sizeof(struct mill_ssllistener));
    if(!l) {
        tcpclose(s);
        errno = ENOMEM;
        return NULL;
    }
    l->sock.type = MILL_SSLLISTENER;
    l->s = s;
    errno = 0;
    return &l->sock;
}

static int ssl_wait(struct mill_sslconn *c, int64_t deadline) {
    if(!BIO_should_retry(c->bio)) {
        c->sslerr = ERR_get_error();
        /* XXX: Ref. openssl/source/ssl/ssl_lib.c .. */
        if(ERR_GET_LIB(c->sslerr) != ERR_LIB_SYS)
            errno = EIO;
        return -1;
    }

    if(BIO_should_read(c->bio)) {
        int rc = fdwait(c->fd, FDW_IN, deadline);
        if(rc == 0) {
            errno = ETIMEDOUT;
            return -1;
        }
        return rc;
    }
    if(BIO_should_write(c->bio)) {
        int rc = fdwait(c->fd, FDW_OUT, deadline);
        if(rc == 0) {
            errno = ETIMEDOUT;
            return -1;
        }
        return rc;
    }
    /* mill_assert(!BIO_should_io_special(c->bio)); */
    errno = EIO;
    return -1;  /* should not happen ? */
}


/* optional: call after ssl_connect()/ssl_accept() */
int mill_sslhandshake_(struct mill_sslsock *s, int64_t deadline) {
    if(s->type != MILL_SSLCONN)
        mill_panic("trying to use an unconnected socket");
    struct mill_sslconn *c = (struct mill_sslconn*)s;

    while(BIO_do_handshake(c->bio) <= 0) {
        if(ssl_wait(c, deadline) < 0)
            return -1;
    }

#if 0
    SSL *ssl;
    BIO_get_ssl(c->bio, &ssl);
    X509 *cert = SSL_get_peer_certificate(ssl);
    if (cert) {
        X509_NAME *certname = X509_get_subject_name(cert);
        X509_NAME_print_ex_fp(stderr, certname, 0, 0);
        X509_free(cert);

        /* verify cert, requires loading known certs.
         * See SSL_CTX_load_verify_locations() call in ssl_init() */
        if (SSL_get_verify_result(ssl) != X509_V_OK) {
            ...
        }
    }
#endif
    return 0;
}

void mill_sslclose_(struct mill_sslsock *s) {
    switch(s->type) {
    case MILL_SSLLISTENER:;
        struct mill_ssllistener *l = (struct mill_ssllistener*)s;
        tcpclose(l->s);
        free(l);
        break;
    case MILL_SSLCONN:;
        struct mill_sslconn *c = (struct mill_sslconn*)s;
        BIO_ssl_shutdown(c->bio);
        BIO_free_all(c->bio);
        free(c);
        break;
    default:
        mill_assert(0);
    }
    errno = 0;
}

const char *mill_sslerrstr_(struct mill_sslsock *s) {
    if(s->type != MILL_SSLCONN)
        mill_panic("trying to use an unconnected socket");
    struct mill_sslconn *c = (struct mill_sslconn*)s;

    static const char unknown_err[] = "Unknown error";
    if(c->sslerr)
        return ERR_error_string(c->sslerr, NULL);
    if(errno)
        return strerror(errno);
    return unknown_err;
}

static struct mill_sslsock *ssl_conn_new(tcpsock s, SSL_CTX *ctx, int client) {
    mill_assert(ctx);
    SSL *ssl = NULL;
    BIO *sbio = BIO_new_ssl(ctx, client);
    if(!sbio)
        return NULL;
    BIO_get_ssl(sbio, & ssl);
    if(!ssl) {
        BIO_free(sbio);
        return NULL;
    }

    /* set .._PARTIAL_WRITE for non-blocking operation */
    SSL_set_mode(ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);
    int fd = tcpdetach(s);
    BIO *cbio = BIO_new_socket(fd, BIO_NOCLOSE);
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
    c->sslerr = 0;
    c->fd = fd;
    /* OPTIONAL: call ssl_handshake() to check/verify peer certificate */
    return &c->sock;
}

int mill_sslrecv_(struct mill_sslsock *s, void *buf, int len,
      int64_t deadline) {
    if(s->type != MILL_SSLCONN)
        mill_panic("trying to use an unconnected socket");
    struct mill_sslconn *c = (struct mill_sslconn*)s;

    int rc;
    if(len < 0) {
        errno = EINVAL;
        return -1;
    }
    do {
        rc = BIO_read(c->bio, buf, len);
        if(rc >= 0)
            break;
        if(ssl_wait(c, deadline) < 0)
            return -1;
    } while (1);
    return rc;
}

int mill_sslsend_(struct mill_sslsock *s, const void *buf, int len,
      int64_t deadline) {
    if(s->type != MILL_SSLCONN)
        mill_panic("trying to use an unconnected socket");
    struct mill_sslconn *c = (struct mill_sslconn*)s;

    int rc;
    if(len < 0) {
        errno = EINVAL;
        return -1;
    }
    do {
        rc = BIO_write(c->bio, buf, len);
        if (rc >= 0)
            break;
        if(ssl_wait(c, deadline) < 0)
            return -1;
    } while(1);
    return rc;
}

static int load_certificates(SSL_CTX *ctx,
      const char *cert_file, const char *key_file) {
    if(SSL_CTX_use_certificate_file(ctx, cert_file, SSL_FILETYPE_PEM) <= 0
          || SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0)
        return 0;
    if (SSL_CTX_check_private_key(ctx) <= 0) /* inconsistent private key */
        return 0;
    return 1;
}

/* use ERR_print_errors(_fp) for SSL error */
static int ssl_init(void) {
    ERR_load_crypto_strings();
    ERR_load_SSL_strings();
    OpenSSL_add_all_algorithms();

    SSL_library_init();

    /* seed the PRNG .. */

    ssl_cli_ctx = SSL_CTX_new(SSLv23_client_method());
    if(ssl_cli_ctx == NULL)
        return 0;
#if 0
    /* XXX: if verifying cert using SSL_get_verify_result(), see ssl_handshake.
     *
     */
    if (!SSL_CTX_load_verify_locations(ssl_cli_ctx, NULL, "/etc/ssl/certs")) {
        ...
    }
#endif
    return 1;
}

/* use ERR_print_errors(_fp) for SSL error */
int mill_sslinit_(const char *cert_file, const char *key_file) {
    if(!ssl_cli_ctx) {
        if (! ssl_init())
            return 0;
    }
    ssl_serv_ctx = SSL_CTX_new(SSLv23_server_method());
    if(!ssl_serv_ctx)
        return 0;
    return load_certificates(ssl_serv_ctx, cert_file, key_file);
}

struct mill_sslsock *mill_sslconnect_(struct mill_ipaddr addr,
      int64_t deadline) {
    if(!ssl_cli_ctx) {
        if(!ssl_init()) {
            errno = EPROTO;
            return NULL;
        }
    }
    tcpsock sock = tcpconnect(addr, deadline);
    if(!sock)
        return NULL;
    struct mill_sslsock *c = ssl_conn_new(sock, ssl_cli_ctx, 1);
    if(!c) {
        tcpclose(sock);
        errno = ENOMEM;
        return NULL;
    }
    return c;
}

struct mill_sslsock *mill_sslaccept_(struct mill_sslsock *s, int64_t deadline) {
    if(s->type != MILL_SSLLISTENER)
        mill_panic("trying to accept on a socket that isn't listening");
    struct mill_ssllistener *l = (struct mill_ssllistener*)s;

    if(!ssl_serv_ctx) {
        errno = EPROTO;
        return NULL;
    }
    tcpsock sock = tcpaccept(l->s, deadline);
    if(!sock)
        return NULL;
    struct mill_sslsock *c = ssl_conn_new(sock, ssl_serv_ctx, 0);
    if(!c) {
        tcpclose(sock);
        errno = ENOMEM;
        return NULL;
    }
    return c;
}

#endif

