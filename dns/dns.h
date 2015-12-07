/* ==========================================================================
 * dns.h - Recursive, Reentrant DNS Resolver.
 * --------------------------------------------------------------------------
 * Copyright (c) 2009, 2010, 2012, 2013, 2014, 2015  William Ahern
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
 * NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 * ==========================================================================
 */
#ifndef DNS_H
#define DNS_H

#include <stddef.h>		/* size_t offsetof() */
#include <stdio.h>		/* FILE */

#include <string.h>		/* strlen(3) */

#include <time.h>		/* time_t */

#if _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/param.h>		/* BYTE_ORDER BIG_ENDIAN _BIG_ENDIAN */
#include <sys/types.h>		/* socklen_t */
#include <sys/socket.h>		/* struct socket */

#include <poll.h>		/* POLLIN POLLOUT */

#include <netinet/in.h>		/* struct in_addr struct in6_addr */

#include <netdb.h>		/* struct addrinfo */
#endif


/*
 * V E R S I O N
 *
 * Vendor: Entity for which versions numbers are relevant. (If forking
 * change DNS_VENDOR to avoid confusion.)
 *
 * Three versions:
 *
 * REL	Official "release"--bug fixes, new features, etc.
 * ABI	Changes to existing object sizes or parameter types.
 * API	Changes that might effect application source.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define DNS_VENDOR "william@25thandClement.com"

#define DNS_V_REL  0x20150630
#define DNS_V_ABI  0x20150612
#define DNS_V_API  0x20150612


const char *dns_vendor(void);

int dns_v_rel(void);
int dns_v_abi(void);
int dns_v_api(void);


/*
 * E R R O R S
 *
 * Errors and exceptions are always returned through an int. This should
 * hopefully make integration easier in the majority of circumstances, and
 * also cut down on useless compiler warnings.
 *
 * System and library errors are returned together. POSIX guarantees that
 * all system errors are positive integers. Library errors are always
 * negative integers in the range DNS_EBASE to DNS_ELAST, with the high bits
 * set to the three magic ASCII characters "dns".
 *
 * dns_strerror() returns static English string descriptions of all known
 * errors, and punts the remainder to strerror(3).
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define DNS_EBASE -(('d' << 24) | ('n' << 16) | ('s' << 8) | 64)

#define dns_error_t int /* for documentation only */

enum dns_errno {
	DNS_ENOBUFS = DNS_EBASE,
	DNS_EILLEGAL,
	DNS_EORDER,
	DNS_ESECTION,
	DNS_EUNKNOWN,
	DNS_EADDRESS,
	DNS_ENOQUERY,
	DNS_ENOANSWER,
	DNS_EFETCHED,
	DNS_ESERVICE, /* EAI_SERVICE */
	DNS_ENONAME,  /* EAI_NONAME */
	DNS_EFAIL,    /* EAI_FAIL */
	DNS_ELAST,
}; /* dns_errno */

const char *dns_strerror(dns_error_t);

extern int dns_debug;


/*
 * C O M P I L E R  A N N O T A T I O N S
 *
 * GCC with -Wextra, and clang by default, complain about overrides in
 * initializer lists. Overriding previous member initializers is well
 * defined behavior in C. dns.c relies on this behavior to define default,
 * overrideable member values when instantiating configuration objects.
 *
 * dns_quietinit() guards a compound literal expression with pragmas to
 * silence these shrill warnings. This alleviates the burden of requiring
 * third-party projects to adjust their compiler flags.
 *
 * NOTE: If you take the address of the compound literal, take the address
 * of the transformed expression, otherwise the compound literal lifetime is
 * tied to the scope of the GCC statement expression.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if defined __clang__
#define DNS_PRAGMA_PUSH _Pragma("clang diagnostic push")
#define DNS_PRAGMA_QUIET _Pragma("clang diagnostic ignored \"-Winitializer-overrides\"")
#define DNS_PRAGMA_POP _Pragma("clang diagnostic pop")

#define dns_quietinit(...) \
	DNS_PRAGMA_PUSH DNS_PRAGMA_QUIET __VA_ARGS__ DNS_PRAGMA_POP
#elif (__GNUC__ == 4 && __GNUC_MINOR__ >= 6) || __GNUC__ > 4
#define DNS_PRAGMA_PUSH _Pragma("GCC diagnostic push")
#define DNS_PRAGMA_QUIET _Pragma("GCC diagnostic ignored \"-Woverride-init\"")
#define DNS_PRAGMA_POP _Pragma("GCC diagnostic pop")

/* GCC parses the _Pragma operator less elegantly than clang. */
#define dns_quietinit(...) \
	__extension__ ({ DNS_PRAGMA_PUSH DNS_PRAGMA_QUIET __VA_ARGS__; DNS_PRAGMA_POP })
#else
#define DNS_PRAGMA_PUSH
#define DNS_PRAGMA_QUIET
#define DNS_PRAGMA_POP
#define dns_quietinit(...) __VA_ARGS__
#endif

#if defined __GNUC__
#define DNS_PRAGMA_EXTENSION __extension__
#else
#define DNS_PRAGMA_EXTENSION
#endif


/*
 * E V E N T S  I N T E R F A C E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if defined(POLLIN)
#define DNS_POLLIN POLLIN
#else
#define DNS_POLLIN  1
#endif

#if defined(POLLOUT)
#define DNS_POLLOUT POLLOUT
#else
#define DNS_POLLOUT 2
#endif


/*
 * See Application Interface below for configuring libevent bitmasks instead
 * of poll(2) bitmasks.
 */
#define DNS_EVREAD  2
#define DNS_EVWRITE 4


#define DNS_POLL2EV(set) \
	(((set) & DNS_POLLIN)? DNS_EVREAD : 0) | (((set) & DNS_POLLOUT)? DNS_EVWRITE : 0)

#define DNS_EV2POLL(set) \
	(((set) & DNS_EVREAD)? DNS_POLLIN : 0) | (((set) & DNS_EVWRITE)? DNS_POLLOUT : 0)


/*
 * E N U M E R A T I O N  I N T E R F A C E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

enum dns_section {
	DNS_S_QD		= 0x01,
#define DNS_S_QUESTION		DNS_S_QD

	DNS_S_AN		= 0x02,
#define DNS_S_ANSWER		DNS_S_AN

	DNS_S_NS		= 0x04,
#define DNS_S_AUTHORITY		DNS_S_NS

	DNS_S_AR		= 0x08,
#define DNS_S_ADDITIONAL	DNS_S_AR

	DNS_S_ALL		= 0x0f
}; /* enum dns_section */


enum dns_class {
	DNS_C_IN	= 1,

	DNS_C_ANY	= 255
}; /* enum dns_class */


enum dns_type {
	DNS_T_A		= 1,
	DNS_T_NS	= 2,
	DNS_T_CNAME	= 5,
	DNS_T_SOA	= 6,
	DNS_T_PTR	= 12,
	DNS_T_MX	= 15,
	DNS_T_TXT	= 16,
	DNS_T_AAAA	= 28,
	DNS_T_SRV	= 33,
	DNS_T_OPT	= 41,
	DNS_T_SSHFP	= 44,
	DNS_T_SPF	= 99,

	DNS_T_ALL	= 255
}; /* enum dns_type */


enum dns_opcode {
	DNS_OP_QUERY	= 0,
	DNS_OP_IQUERY	= 1,
	DNS_OP_STATUS	= 2,
	DNS_OP_NOTIFY	= 4,
	DNS_OP_UPDATE	= 5,
}; /* dns_opcode */


enum dns_rcode {
	DNS_RC_NOERROR	= 0,
	DNS_RC_FORMERR	= 1,
	DNS_RC_SERVFAIL	= 2,
	DNS_RC_NXDOMAIN	= 3,
	DNS_RC_NOTIMP	= 4,
	DNS_RC_REFUSED	= 5,
	DNS_RC_YXDOMAIN	= 6,
	DNS_RC_YXRRSET	= 7,
	DNS_RC_NXRRSET	= 8,
	DNS_RC_NOTAUTH	= 9,
	DNS_RC_NOTZONE	= 10,
}; /* dns_rcode */


/*
 * NOTE: These string functions need a small buffer in case the literal
 * integer value needs to be printed and returned. UNLESS this buffer is
 * SPECIFIED, the returned string has ONLY BLOCK SCOPE.
 */
#define DNS_STRMAXLEN 47 /* "QUESTION|ANSWER|AUTHORITY|ADDITIONAL" */

const char *dns_strsection(enum dns_section, void *, size_t);
#define dns_strsection3(a, b, c) \
				dns_strsection((a), (b), (c))
#define dns_strsection1(a)	dns_strsection((a), (char [DNS_STRMAXLEN + 1]){ 0 }, DNS_STRMAXLEN + 1)
#define dns_strsection(...)	DNS_PP_CALL(DNS_PP_XPASTE(dns_strsection, DNS_PP_NARG(__VA_ARGS__)), __VA_ARGS__)

enum dns_section dns_isection(const char *);

const char *dns_strclass(enum dns_class, void *, size_t);
#define dns_strclass3(a, b, c)	dns_strclass((a), (b), (c))
#define dns_strclass1(a)	dns_strclass((a), (char [DNS_STRMAXLEN + 1]){ 0 }, DNS_STRMAXLEN + 1)
#define dns_strclass(...)	DNS_PP_CALL(DNS_PP_XPASTE(dns_strclass, DNS_PP_NARG(__VA_ARGS__)), __VA_ARGS__)

enum dns_class dns_iclass(const char *);

const char *dns_strtype(enum dns_type, void *, size_t);
#define dns_strtype3(a, b, c)	dns_strtype((a), (b), (c))
#define dns_strtype1(a)		dns_strtype((a), (char [DNS_STRMAXLEN + 1]){ 0 }, DNS_STRMAXLEN + 1)
#define dns_strtype(...)	DNS_PP_CALL(DNS_PP_XPASTE(dns_strtype, DNS_PP_NARG(__VA_ARGS__)), __VA_ARGS__)

enum dns_type dns_itype(const char *);

const char *dns_stropcode(enum dns_opcode);

enum dns_opcode dns_iopcode(const char *);

const char *dns_strrcode(enum dns_rcode);

enum dns_rcode dns_ircode(const char *);


/*
 * A T O M I C  I N T E R F A C E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

typedef unsigned long dns_atomic_t;

typedef unsigned long dns_refcount_t; /* must be same value type as dns_atomic_t */


/*
 * C R Y P T O  I N T E R F A C E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

extern unsigned (*dns_random)(void);


/*
 * P A C K E T  I N T E R F A C E
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct dns_header {
		unsigned qid:16;

#if (defined BYTE_ORDER && BYTE_ORDER == BIG_ENDIAN) || (defined __sun && defined _BIG_ENDIAN)
		unsigned qr:1;
		unsigned opcode:4;
		unsigned aa:1;
		unsigned tc:1;
		unsigned rd:1;

		unsigned ra:1;
		unsigned unused:3;
		unsigned rcode:4;
#else
		unsigned rd:1;
		unsigned tc:1;
		unsigned aa:1;
		unsigned opcode:4;
		unsigned qr:1;

		unsigned rcode:4;
		unsigned unused:3;
		unsigned ra:1;
#endif

		unsigned qdcount:16;
		unsigned ancount:16;
		unsigned nscount:16;
		unsigned arcount:16;
}; /* struct dns_header */

#define dns_header(p)	(&(p)->header)


#ifndef DNS_P_QBUFSIZ
#define DNS_P_QBUFSIZ	dns_p_calcsize(256 + 4)
#endif

#ifndef DNS_P_DICTSIZE
#define DNS_P_DICTSIZE	16
#endif

struct dns_packet {
	unsigned short dict[DNS_P_DICTSIZE];

	struct dns_s_memo {
		unsigned short base, end;
	} qd, an, ns, ar;

	struct { struct dns_packet *cqe_next, *cqe_prev; } cqe;

	size_t size, end;

	int:16; /* tcp padding */

	DNS_PRAGMA_EXTENSION union {
		struct dns_header header;
		unsigned char data[1];
	};
}; /* struct dns_packet */

#define dns_p_calcsize(n)	(offsetof(struct dns_packet, data) + DNS_PP_MAX(12, (n)))

#define dns_p_sizeof(P)		dns_p_calcsize((P)->end)

/** takes size of maximum desired payload */
#define dns_p_new(n)		(dns_p_init((struct dns_packet *)&(union { unsigned char b[dns_p_calcsize((n))]; struct dns_packet p; }){ { 0 } }, dns_p_calcsize((n))))

/** takes size of entire packet structure as allocated */
struct dns_packet *dns_p_init(struct dns_packet *, size_t);

/** takes size of maximum desired payload */
struct dns_packet *dns_p_make(size_t, int *);

int dns_p_grow(struct dns_packet **);

struct dns_packet *dns_p_copy(struct dns_packet *, const struct dns_packet *);

#define dns_p_opcode(P)		(dns_header(P)->opcode)

#define dns_p_rcode(P)		(dns_header(P)->rcode)

unsigned dns_p_count(struct dns_packet *, enum dns_section);

int dns_p_push(struct dns_packet *, enum dns_section, const void *, size_t, enum dns_type, enum dns_class, unsigned, const void *);

void dns_p_dictadd(struct dns_packet *, unsigned short);

struct dns_packet *dns_p_merge(struct dns_packet *, enum dns_section, struct dns_packet *, enum dns_section, int *);

void dns_p_dump(struct dns_packet *, FILE *);

int dns_p_study(struct dns_packet *);


/*
 * D O M A I N  N A M E  I N T E R F A C E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define DNS_D_MAXLABEL	63	/* + 1 '\0' */
#define DNS_D_MAXNAME	255	/* + 1 '\0' */

#define DNS_D_ANCHOR	1	/* anchor domain w/ root "." */
#define DNS_D_CLEAVE	2	/* cleave sub-domain */
#define DNS_D_TRIM	4	/* remove superfluous dots */

#define dns_d_new3(a, b, f)	dns_d_init(&(char[DNS_D_MAXNAME + 1]){ 0 }, DNS_D_MAXNAME + 1, (a), (b), (f))
#define dns_d_new2(a, f)	dns_d_new3((a), strlen((a)), (f))
#define dns_d_new1(a)		dns_d_new3((a), strlen((a)), DNS_D_ANCHOR)
#define dns_d_new(...)		DNS_PP_CALL(DNS_PP_XPASTE(dns_d_new, DNS_PP_NARG(__VA_ARGS__)), __VA_ARGS__)

char *dns_d_init(void *, size_t, const void *, size_t, int);

size_t dns_d_anchor(void *, size_t, const void *, size_t);

size_t dns_d_cleave(void *, size_t, const void *, size_t);

size_t dns_d_comp(void *, size_t, const void *, size_t, struct dns_packet *, int *);

size_t dns_d_expand(void *, size_t, unsigned short, struct dns_packet *, int *);

unsigned short dns_d_skip(unsigned short, struct dns_packet *);

int dns_d_push(struct dns_packet *, const void *, size_t);

size_t dns_d_cname(void *, size_t, const void *, size_t, struct dns_packet *, int *error);


/*
 * R E S O U R C E  R E C O R D  I N T E R F A C E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct dns_rr {
	enum dns_section section;

	struct {
		unsigned short p;
		unsigned short len;
	} dn;

	enum dns_type type;
	enum dns_class class;
	unsigned ttl;

	struct {
		unsigned short p;
		unsigned short len;
	} rd;
}; /* struct dns_rr */


int dns_rr_copy(struct dns_packet *, struct dns_rr *, struct dns_packet *);

int dns_rr_parse(struct dns_rr *, unsigned short, struct dns_packet *);

unsigned short dns_rr_skip(unsigned short, struct dns_packet *);

int dns_rr_cmp(struct dns_rr *, struct dns_packet *, struct dns_rr *, struct dns_packet *);

size_t dns_rr_print(void *, size_t, struct dns_rr *, struct dns_packet *, int *);


#define dns_rr_i_new(P, ...) \
	dns_rr_i_init(&dns_quietinit((struct dns_rr_i){ 0, __VA_ARGS__ }), (P))

struct dns_rr_i {
	enum dns_section section;
	const void *name;
	enum dns_type type;
	enum dns_class class;
	const void *data;

	int follow;

	int (*sort)();
	unsigned args[2];

	struct {
		unsigned short next;
		unsigned short count;

		unsigned exec;
		unsigned regs[2];
	} state, saved;
}; /* struct dns_rr_i */

int dns_rr_i_packet(struct dns_rr *, struct dns_rr *, struct dns_rr_i *, struct dns_packet *);

int dns_rr_i_order(struct dns_rr *, struct dns_rr *, struct dns_rr_i *, struct dns_packet *);

int dns_rr_i_shuffle(struct dns_rr *, struct dns_rr *, struct dns_rr_i *, struct dns_packet *);

struct dns_rr_i *dns_rr_i_init(struct dns_rr_i *, struct dns_packet *);

#define dns_rr_i_save(i)	((i)->saved = (i)->state)
#define dns_rr_i_rewind(i)	((i)->state = (i)->saved)
#define dns_rr_i_count(i)	((i)->state.count)

unsigned dns_rr_grep(struct dns_rr *, unsigned, struct dns_rr_i *, struct dns_packet *, int *);

#define dns_rr_foreach_(rr, P, ...)	\
	for (struct dns_rr_i DNS_PP_XPASTE(i, __LINE__) = *dns_rr_i_new((P), __VA_ARGS__); dns_rr_grep((rr), 1, &DNS_PP_XPASTE(i, __LINE__), (P), &(int){ 0 }); )

#define dns_rr_foreach(...)	dns_rr_foreach_(__VA_ARGS__)


/*
 * A  R E S O U R C E  R E C O R D
 */

struct dns_a {
	struct in_addr addr;
}; /* struct dns_a */

int dns_a_parse(struct dns_a *, struct dns_rr *, struct dns_packet *);

int dns_a_push(struct dns_packet *, struct dns_a *);

int dns_a_cmp(const struct dns_a *, const struct dns_a *);

size_t dns_a_print(void *, size_t, struct dns_a *);


/*
 * AAAA  R E S O U R C E  R E C O R D
 */

struct dns_aaaa {
	struct in6_addr addr;
}; /* struct dns_aaaa */

int dns_aaaa_parse(struct dns_aaaa *, struct dns_rr *, struct dns_packet *);

int dns_aaaa_push(struct dns_packet *, struct dns_aaaa *);

int dns_aaaa_cmp(const struct dns_aaaa *, const struct dns_aaaa *);

size_t dns_aaaa_print(void *, size_t, struct dns_aaaa *);


/*
 * MX  R E S O U R C E  R E C O R D
 */

struct dns_mx {
	unsigned short preference;
	char host[DNS_D_MAXNAME + 1];
}; /* struct dns_mx */

int dns_mx_parse(struct dns_mx *, struct dns_rr *, struct dns_packet *);

int dns_mx_push(struct dns_packet *, struct dns_mx *);

int dns_mx_cmp(const struct dns_mx *, const struct dns_mx *);

size_t dns_mx_print(void *, size_t, struct dns_mx *);

size_t dns_mx_cname(void *, size_t, struct dns_mx *);


/*
 * NS  R E S O U R C E  R E C O R D
 */

struct dns_ns {
	char host[DNS_D_MAXNAME + 1];
}; /* struct dns_ns */

int dns_ns_parse(struct dns_ns *, struct dns_rr *, struct dns_packet *);

int dns_ns_push(struct dns_packet *, struct dns_ns *);

int dns_ns_cmp(const struct dns_ns *, const struct dns_ns *);

size_t dns_ns_print(void *, size_t, struct dns_ns *);

size_t dns_ns_cname(void *, size_t, struct dns_ns *);


/*
 * CNAME  R E S O U R C E  R E C O R D
 */

struct dns_cname {
	char host[DNS_D_MAXNAME + 1];
}; /* struct dns_cname */

int dns_cname_parse(struct dns_cname *, struct dns_rr *, struct dns_packet *);

int dns_cname_push(struct dns_packet *, struct dns_cname *);

int dns_cname_cmp(const struct dns_cname *, const struct dns_cname *);

size_t dns_cname_print(void *, size_t, struct dns_cname *);

size_t dns_cname_cname(void *, size_t, struct dns_cname *);


/*
 * SOA  R E S O U R C E  R E C O R D
 */

struct dns_soa {
	char mname[DNS_D_MAXNAME + 1];
	char rname[DNS_D_MAXNAME + 1];
	unsigned serial, refresh, retry, expire, minimum;
}; /* struct dns_soa */

int dns_soa_parse(struct dns_soa *, struct dns_rr *, struct dns_packet *);

int dns_soa_push(struct dns_packet *, struct dns_soa *);

int dns_soa_cmp(const struct dns_soa *, const struct dns_soa *);

size_t dns_soa_print(void *, size_t, struct dns_soa *);


/*
 * PTR  R E S O U R C E  R E C O R D
 */

struct dns_ptr {
	char host[DNS_D_MAXNAME + 1];
}; /* struct dns_ptr */

int dns_ptr_parse(struct dns_ptr *, struct dns_rr *, struct dns_packet *);

int dns_ptr_push(struct dns_packet *, struct dns_ptr *);

int dns_ptr_cmp(const struct dns_ptr *, const struct dns_ptr *);

size_t dns_ptr_print(void *, size_t, struct dns_ptr *);

size_t dns_ptr_cname(void *, size_t, struct dns_ptr *);


/*
 * SRV  R E S O U R C E  R E C O R D
 */

struct dns_srv {
	unsigned short priority;
	unsigned short weight;
	unsigned short port;
	char target[DNS_D_MAXNAME + 1];
}; /* struct dns_srv */

int dns_srv_parse(struct dns_srv *, struct dns_rr *, struct dns_packet *);

int dns_srv_push(struct dns_packet *, struct dns_srv *);

int dns_srv_cmp(const struct dns_srv *, const struct dns_srv *);

size_t dns_srv_print(void *, size_t, struct dns_srv *);

size_t dns_srv_cname(void *, size_t, struct dns_srv *);


/*
 * OPT  R E S O U R C E  R E C O R D
 */

#define DNS_OPT_MINDATA 512

#define DNS_OPT_BADVERS 16

struct dns_opt {
	size_t size, len;

	unsigned char rcode, version;
	unsigned short maxsize;

	unsigned char data[DNS_OPT_MINDATA];
}; /* struct dns_opt */

unsigned int dns_opt_ttl(const struct dns_opt *);

unsigned short dns_opt_class(const struct dns_opt *);

struct dns_opt *dns_opt_init(struct dns_opt *, size_t);

int dns_opt_parse(struct dns_opt *, struct dns_rr *, struct dns_packet *);

int dns_opt_push(struct dns_packet *, struct dns_opt *);

int dns_opt_cmp(const struct dns_opt *, const struct dns_opt *);

size_t dns_opt_print(void *, size_t, struct dns_opt *);


/*
 * SSHFP  R E S O U R C E  R E C O R D
 */

struct dns_sshfp {
	enum dns_sshfp_key {
		DNS_SSHFP_RSA = 1,
		DNS_SSHFP_DSA = 2,
	} algo;

	enum dns_sshfp_digest {
		DNS_SSHFP_SHA1 = 1,
	} type;

	union {
		unsigned char sha1[20];
	} digest;
}; /* struct dns_sshfp */

int dns_sshfp_parse(struct dns_sshfp *, struct dns_rr *, struct dns_packet *);

int dns_sshfp_push(struct dns_packet *, struct dns_sshfp *);

int dns_sshfp_cmp(const struct dns_sshfp *, const struct dns_sshfp *);

size_t dns_sshfp_print(void *, size_t, struct dns_sshfp *);


/*
 * TXT  R E S O U R C E  R E C O R D
 */

#ifndef DNS_TXT_MINDATA
#define DNS_TXT_MINDATA	1024
#endif

struct dns_txt {
	size_t size, len;
	unsigned char data[DNS_TXT_MINDATA];
}; /* struct dns_txt */

struct dns_txt *dns_txt_init(struct dns_txt *, size_t);

int dns_txt_parse(struct dns_txt *, struct dns_rr *, struct dns_packet *);

int dns_txt_push(struct dns_packet *, struct dns_txt *);

int dns_txt_cmp(const struct dns_txt *, const struct dns_txt *);

size_t dns_txt_print(void *, size_t, struct dns_txt *);


/*
 * ANY  R E S O U R C E  R E C O R D
 */

union dns_any {
	struct dns_a a;
	struct dns_aaaa aaaa;
	struct dns_mx mx;
	struct dns_ns ns;
	struct dns_cname cname;
	struct dns_soa soa;
	struct dns_ptr ptr;
	struct dns_srv srv;
	struct dns_opt opt;
	struct dns_sshfp sshfp;
	struct dns_txt txt, spf, rdata;
}; /* union dns_any */

#define DNS_ANY_INIT(any) { .rdata = { .size = sizeof *(any) } }

union dns_any *dns_any_init(union dns_any *, size_t);

int dns_any_parse(union dns_any *, struct dns_rr *, struct dns_packet *);

int dns_any_push(struct dns_packet *, union dns_any *, enum dns_type);

int dns_any_cmp(const union dns_any *, enum dns_type, const union dns_any *, enum dns_type);

size_t dns_any_print(void *, size_t, union dns_any *, enum dns_type);

size_t dns_any_cname(void *, size_t, union dns_any *, enum dns_type);


/*
 * H O S T S  I N T E R F A C E
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct dns_hosts;

struct dns_hosts *dns_hosts_open(int *);

void dns_hosts_close(struct dns_hosts *);

dns_refcount_t dns_hosts_acquire(struct dns_hosts *);

dns_refcount_t dns_hosts_release(struct dns_hosts *);

struct dns_hosts *dns_hosts_mortal(struct dns_hosts *);

struct dns_hosts *dns_hosts_local(int *);

int dns_hosts_loadfile(struct dns_hosts *, FILE *);

int dns_hosts_loadpath(struct dns_hosts *, const char *);

int dns_hosts_dump(struct dns_hosts *, FILE *);

int dns_hosts_insert(struct dns_hosts *, int, const void *, const void *, _Bool);

struct dns_packet *dns_hosts_query(struct dns_hosts *, struct dns_packet *, int *);


/*
 * R E S O L V . C O N F  I N T E R F A C E
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct dns_resolv_conf {
	struct sockaddr_storage nameserver[3];

	char search[4][DNS_D_MAXNAME + 1];

	/* (f)ile, (b)ind, (c)ache */
	char lookup[4 * (1 + (4 * 2))];

	struct {
		_Bool edns0;

		unsigned ndots;

		unsigned timeout;

		unsigned attempts;

		_Bool rotate;

		_Bool recurse;

		_Bool smart;

		enum {
			DNS_RESCONF_TCP_ENABLE,
			DNS_RESCONF_TCP_ONLY,
			DNS_RESCONF_TCP_DISABLE,
		} tcp;
	} options;

	struct sockaddr_storage iface;

	struct { /* PRIVATE */
		dns_atomic_t refcount;
	} _;
}; /* struct dns_resolv_conf */

struct dns_resolv_conf *dns_resconf_open(int *);

void dns_resconf_close(struct dns_resolv_conf *);

dns_refcount_t dns_resconf_acquire(struct dns_resolv_conf *);

dns_refcount_t dns_resconf_release(struct dns_resolv_conf *);

struct dns_resolv_conf *dns_resconf_mortal(struct dns_resolv_conf *);

struct dns_resolv_conf *dns_resconf_local(int *);

struct dns_resolv_conf *dns_resconf_root(int *);

int dns_resconf_pton(struct sockaddr_storage *, const char *);

int dns_resconf_loadfile(struct dns_resolv_conf *, FILE *);

int dns_resconf_loadpath(struct dns_resolv_conf *, const char *);

int dns_nssconf_loadfile(struct dns_resolv_conf *, FILE *);

int dns_nssconf_loadpath(struct dns_resolv_conf *, const char *);

int dns_resconf_dump(struct dns_resolv_conf *, FILE *);

int dns_nssconf_dump(struct dns_resolv_conf *, FILE *);

int dns_resconf_setiface(struct dns_resolv_conf *, const char *, unsigned short);

typedef unsigned long dns_resconf_i_t;

size_t dns_resconf_search(void *, size_t, const void *, size_t, struct dns_resolv_conf *, dns_resconf_i_t *);


/*
 * H I N T  S E R V E R  I N T E R F A C E
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct dns_hints;

struct dns_hints *dns_hints_open(struct dns_resolv_conf *, int *);

void dns_hints_close(struct dns_hints *);

dns_refcount_t dns_hints_acquire(struct dns_hints *);

dns_refcount_t dns_hints_release(struct dns_hints *);

struct dns_hints *dns_hints_mortal(struct dns_hints *);

int dns_hints_insert(struct dns_hints *, const char *, const struct sockaddr *, unsigned);

unsigned dns_hints_insert_resconf(struct dns_hints *, const char *, const struct dns_resolv_conf *, int *);

struct dns_hints *dns_hints_local(struct dns_resolv_conf *, int *);

struct dns_hints *dns_hints_root(struct dns_resolv_conf *, int *);

int dns_hints_dump(struct dns_hints *, FILE *);


struct dns_hints_i {
	const char *zone;

	struct {
		unsigned next;
        	unsigned seed;
	} state;
}; /* struct dns_hints_i */

#define dns_hints_i_new(...)	(&(struct dns_hints_i){ __VA_ARGS__ })

unsigned dns_hints_grep(struct sockaddr **, socklen_t *, unsigned, struct dns_hints_i *, struct dns_hints *);


/*
 * C A C H E  I N T E R F A C E
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct dns_cache {
	void *state;

	dns_refcount_t (*acquire)(struct dns_cache *);
	dns_refcount_t (*release)(struct dns_cache *);

	struct dns_packet *(*query)(struct dns_packet *, struct dns_cache *, int *);

	int (*submit)(struct dns_packet *, struct dns_cache *);
	int (*check)(struct dns_cache *);
	struct dns_packet *(*fetch)(struct dns_cache *, int *);

	int (*pollfd)(struct dns_cache *);
	short (*events)(struct dns_cache *);
	void (*clear)(struct dns_cache *);

	union {
		long i;
		void *p;
	} arg[3];

	struct { /* PRIVATE */
		dns_atomic_t refcount;
	} _;
}; /* struct dns_cache */


struct dns_cache *dns_cache_init(struct dns_cache *);

void dns_cache_close(struct dns_cache *);


/*
 * A P P L I C A T I O N  I N T E R F A C E
 *
 * Options to change the behavior of the API. Applies across all the
 * different components.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define DNS_OPTS_INITIALIZER_ { 0, 0 }, 0
#define DNS_OPTS_INITIALIZER  { DNS_OPTS_INITIALIZER_ }
#define DNS_OPTS_INIT(...)    { DNS_OPTS_INITIALIZER_, __VA_ARGS__ }

#define dns_opts(...) (&dns_quietinit((struct dns_options)DNS_OPTS_INIT(__VA_ARGS__)))

struct dns_options {
	/*
	 * If the callback closes *fd, it must set it to -1. Otherwise, the
	 * descriptor is queued and lazily closed at object destruction or
	 * by an explicit call to _clear(). This allows safe use of
	 * kqueue(2), epoll(2), et al -style persistent events.
	 */
	struct {
		void *arg;
		int (*cb)(int *fd, void *arg);
	} closefd;

	/* bitmask for _events() routines */
	enum dns_events {
		DNS_SYSPOLL,
		DNS_LIBEVENT,
	} events;
}; /* struct dns_options */


/*
 * S T A T S  I N T E R F A C E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct dns_stat {
	size_t queries;

	struct {
		struct {
			size_t count, bytes;
		} sent, rcvd;
	} udp, tcp;
}; /* struct dns_stat */


/*
 * S O C K E T  I N T E R F A C E
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct dns_socket;

struct dns_socket *dns_so_open(const struct sockaddr *, int, const struct dns_options *, int *error);

void dns_so_close(struct dns_socket *);

void dns_so_reset(struct dns_socket *);

unsigned short dns_so_mkqid(struct dns_socket *so);

struct dns_packet *dns_so_query(struct dns_socket *, struct dns_packet *, struct sockaddr *, int *);

int dns_so_submit(struct dns_socket *, struct dns_packet *, struct sockaddr *);

int dns_so_check(struct dns_socket *);

struct dns_packet *dns_so_fetch(struct dns_socket *, int *);

time_t dns_so_elapsed(struct dns_socket *);

void dns_so_clear(struct dns_socket *);

int dns_so_events(struct dns_socket *);

int dns_so_pollfd(struct dns_socket *);

int dns_so_poll(struct dns_socket *, int);

const struct dns_stat *dns_so_stat(struct dns_socket *);


/*
 * R E S O L V E R  I N T E R F A C E
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct dns_resolver;

struct dns_resolver *dns_res_open(struct dns_resolv_conf *, struct dns_hosts *hosts, struct dns_hints *, struct dns_cache *, const struct dns_options *, int *);

struct dns_resolver *dns_res_stub(const struct dns_options *, int *);

void dns_res_reset(struct dns_resolver *);

void dns_res_close(struct dns_resolver *);

dns_refcount_t dns_res_acquire(struct dns_resolver *);

dns_refcount_t dns_res_release(struct dns_resolver *);

struct dns_resolver *dns_res_mortal(struct dns_resolver *);

int dns_res_submit(struct dns_resolver *, const char *, enum dns_type, enum dns_class);

int dns_res_submit2(struct dns_resolver *, const char *, size_t, enum dns_type, enum dns_class);

int dns_res_check(struct dns_resolver *);

struct dns_packet *dns_res_fetch(struct dns_resolver *, int *);

time_t dns_res_elapsed(struct dns_resolver *);

void dns_res_clear(struct dns_resolver *);

int dns_res_events(struct dns_resolver *);

int dns_res_pollfd(struct dns_resolver *);

time_t dns_res_timeout(struct dns_resolver *);

int dns_res_poll(struct dns_resolver *, int);

struct dns_packet *dns_res_query(struct dns_resolver *, const char *, enum dns_type, enum dns_class, int, int *);

const struct dns_stat *dns_res_stat(struct dns_resolver *);

void dns_res_sethints(struct dns_resolver *, struct dns_hints *);


/*
 * A D D R I N F O  I N T E R F A C E
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct dns_addrinfo;

struct dns_addrinfo *dns_ai_open(const char *, const char *, enum dns_type, const struct addrinfo *, struct dns_resolver *, int *);

void dns_ai_close(struct dns_addrinfo *);

int dns_ai_nextent(struct addrinfo **, struct dns_addrinfo *);

size_t dns_ai_print(void *, size_t, struct addrinfo *, struct dns_addrinfo *);

time_t dns_ai_elapsed(struct dns_addrinfo *);

void dns_ai_clear(struct dns_addrinfo *);

int dns_ai_events(struct dns_addrinfo *);

int dns_ai_pollfd(struct dns_addrinfo *);

time_t dns_ai_timeout(struct dns_addrinfo *);

int dns_ai_poll(struct dns_addrinfo *, int);

const struct dns_stat *dns_ai_stat(struct dns_addrinfo *);


/*
 * U T I L I T Y  I N T E R F A C E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

size_t dns_strlcpy(char *, const char *, size_t);

size_t dns_strlcat(char *, const char *, size_t);


/*
 * M A C R O  M A G I C S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define DNS_PP_MIN(a, b) (((a) < (b))? (a) : (b))
#define DNS_PP_MAX(a, b) (((a) > (b))? (a) : (b))
#define DNS_PP_NARG_(a, b, c, d, e, f, g, h, i, j, k, N,...) N
#define DNS_PP_NARG(...)	DNS_PP_NARG_(__VA_ARGS__, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define DNS_PP_CALL(F, ...)	F(__VA_ARGS__)
#define DNS_PP_PASTE(x, y)	x##y
#define DNS_PP_XPASTE(x, y)	DNS_PP_PASTE(x, y)
#define DNS_PP_STRINGIFY_(s)	#s
#define DNS_PP_STRINGIFY(s)	DNS_PP_STRINGIFY_(s)
#define DNS_PP_D1  0
#define DNS_PP_D2  1
#define DNS_PP_D3  2
#define DNS_PP_D4  3
#define DNS_PP_D5  4
#define DNS_PP_D6  5
#define DNS_PP_D7  6
#define DNS_PP_D8  7
#define DNS_PP_D9  8
#define DNS_PP_D10 9
#define DNS_PP_D11 10
#define DNS_PP_DEC(N) DNS_PP_XPASTE(DNS_PP_D, N)

#endif /* DNS_H */
