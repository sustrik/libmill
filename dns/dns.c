/* ==========================================================================
 * dns.c - Recursive, Reentrant DNS Resolver.
 * --------------------------------------------------------------------------
 * Copyright (c) 2008, 2009, 2010, 2012, 2013, 2014, 2015  William Ahern
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
#if !defined(__FreeBSD__) && !defined(__sun)
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE	600
#endif

#undef _BSD_SOURCE
#define _BSD_SOURCE

#undef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE

#undef _NETBSD_SOURCE
#define _NETBSD_SOURCE
#endif

#include <limits.h>		/* INT_MAX */
#include <stddef.h>		/* offsetof() */
#ifdef _WIN32
#define uint32_t unsigned int
#else
#include <stdint.h>		/* uint32_t */
#endif
#include <stdlib.h>		/* malloc(3) realloc(3) free(3) rand(3) random(3) arc4random(3) */
#include <stdio.h>		/* FILE fopen(3) fclose(3) getc(3) rewind(3) */
#include <string.h>		/* memcpy(3) strlen(3) memmove(3) memchr(3) memcmp(3) strchr(3) strsep(3) strcspn(3) */
#include <strings.h>		/* strcasecmp(3) strncasecmp(3) */
#include <ctype.h>		/* isspace(3) isdigit(3) */
#include <time.h>		/* time_t time(2) difftime(3) */
#include <signal.h>		/* SIGPIPE sigemptyset(3) sigaddset(3) sigpending(2) sigprocmask(2) pthread_sigmask(3) sigtimedwait(2) */
#include <errno.h>		/* errno EINVAL ENOENT */
#undef NDEBUG
#include <assert.h>		/* assert(3) */

#if _WIN32
#ifndef FD_SETSIZE
#define FD_SETSIZE 256
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>		/* FD_SETSIZE socklen_t */
#include <sys/select.h>		/* FD_ZERO FD_SET fd_set select(2) */
#include <sys/socket.h>		/* AF_INET AF_INET6 AF_UNIX struct sockaddr struct sockaddr_in struct sockaddr_in6 socket(2) */
#if defined(AF_UNIX)
#include <sys/un.h>		/* struct sockaddr_un */
#endif
#include <fcntl.h>		/* F_SETFD F_GETFL F_SETFL O_NONBLOCK fcntl(2) */
#include <unistd.h>		/* _POSIX_THREADS gethostname(3) close(2) */
#include <poll.h>		/* POLLIN POLLOUT */
#include <netinet/in.h>		/* struct sockaddr_in struct sockaddr_in6 */
#include <arpa/inet.h>		/* inet_pton(3) inet_ntop(3) htons(3) ntohs(3) */
#include <netdb.h>		/* struct addrinfo */
#endif

#include "dns.h"


/*
 * C O M P I L E R  A N N O T A T I O N S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define DNS_GNUC_PREREQ(M, m) \
	(defined __GNUC__ && ((__GNUC__ > M) || (__GNUC__ == M && __GNUC_MINOR__ >= m)))

#ifndef HAVE_PRAGMA_MESSAGE
#define HAVE_PRAGMA_MESSAGE (DNS_GNUC_PREREQ(4, 4) || __clang__ || _MSC_VER)
#endif

#if __GNUC__
#define DNS_NOTUSED __attribute__((unused))
#define DNS_NORETURN __attribute__((noreturn))
#else
#define DNS_NOTUSED
#define DNS_NORETURN
#endif

#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#elif DNS_GNUC_PREREQ(4, 6)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif


/*
 * S T A N D A R D  M A C R O S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef lengthof
#define lengthof(a)	(sizeof (a) / sizeof (a)[0])
#endif

#ifndef endof
#define endof(a)	(&(a)[lengthof((a))])
#endif


/*
 * M I S C E L L A N E O U S  C O M P A T
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if _WIN32 || _WIN64
#define PRIuZ "Iu"
#else
#define PRIuZ "zu"
#endif

#ifndef DNS_THREAD_SAFE
#if (defined _REENTRANT || defined _THREAD_SAFE) && _POSIX_THREADS > 0
#define DNS_THREAD_SAFE 1
#else
#define DNS_THREAD_SAFE 0
#endif
#endif


/*
 * D E B U G  M A C R O S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int dns_debug = 0;

#if DNS_DEBUG

#undef DNS_DEBUG
#define DNS_DEBUG dns_debug

#define DNS_SAY_(fmt, ...) \
	do { if (DNS_DEBUG > 0) fprintf(stderr, fmt "%.1s", __func__, __LINE__, __VA_ARGS__); } while (0)
#define DNS_SAY(...) DNS_SAY_("@@ (%s:%d) " __VA_ARGS__, "\n")
#define DNS_HAI DNS_SAY("HAI")

#define DNS_SHOW_(P, fmt, ...)	do {					\
	if (DNS_DEBUG > 1) {						\
	fprintf(stderr, "@@ BEGIN * * * * * * * * * * * *\n");		\
	fprintf(stderr, "@@ " fmt "%.0s\n", __VA_ARGS__);		\
	dns_p_dump((P), stderr);					\
	fprintf(stderr, "@@ END * * * * * * * * * * * * *\n\n");	\
	}								\
} while (0)

#define DNS_SHOW(...)	DNS_SHOW_(__VA_ARGS__, "")

#else /* !DNS_DEBUG */

#undef DNS_DEBUG
#define DNS_DEBUG 0

#define DNS_SAY(...)
#define DNS_HAI
#define DNS_SHOW(...)

#endif /* DNS_DEBUG */


/*
 * V E R S I O N  R O U T I N E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

const char *dns_vendor(void) {
	return DNS_VENDOR;
} /* dns_vendor() */


int dns_v_rel(void) {
	return DNS_V_REL;
} /* dns_v_rel() */


int dns_v_abi(void) {
	return DNS_V_ABI;
} /* dns_v_abi() */


int dns_v_api(void) {
	return DNS_V_API;
} /* dns_v_api() */


/*
 * E R R O R  R O U T I N E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if _WIN32

#define DNS_EINTR	WSAEINTR
#define DNS_EINPROGRESS	WSAEINPROGRESS
#define DNS_EISCONN	WSAEISCONN
#define DNS_EWOULDBLOCK	WSAEWOULDBLOCK
#define DNS_EALREADY	WSAEALREADY
#define DNS_EAGAIN	EAGAIN
#define DNS_ETIMEDOUT	WSAETIMEDOUT

#define dns_syerr()	((int)GetLastError())
#define dns_soerr()	((int)WSAGetLastError())

#else

#define DNS_EINTR	EINTR
#define DNS_EINPROGRESS	EINPROGRESS
#define DNS_EISCONN	EISCONN
#define DNS_EWOULDBLOCK	EWOULDBLOCK
#define DNS_EALREADY	EALREADY
#define DNS_EAGAIN	EAGAIN
#define DNS_ETIMEDOUT	ETIMEDOUT

#define dns_syerr()	errno
#define dns_soerr()	errno

#endif


const char *dns_strerror(int error) {
	switch (error) {
	case DNS_ENOBUFS:
		return "DNS packet buffer too small";
	case DNS_EILLEGAL:
		return "Illegal DNS RR name or data";
	case DNS_EORDER:
		return "Attempt to push RR out of section order";
	case DNS_ESECTION:
		return "Invalid section specified";
	case DNS_EUNKNOWN:
		return "Unknown DNS error";
	case DNS_EADDRESS:
		return "Invalid textual address form";
	case DNS_ENOQUERY:
		return "Bad execution state (missing query packet)";
	case DNS_ENOANSWER:
		return "Bad execution state (missing answer packet)";
	case DNS_EFETCHED:
		return "Answer already fetched";
	case DNS_ESERVICE:
		return "The service passed was not recognized for the specified socket type";
	case DNS_ENONAME:
		return "The name does not resolve for the supplied parameters";
	case DNS_EFAIL:
		return "A non-recoverable error occurred when attempting to resolve the name";
	default:
		return strerror(error);
	} /* switch() */
} /* dns_strerror() */


/*
 * A T O M I C  R O U T I N E S
 *
 * Use GCC's __atomic built-ins if possible. Unlike the __sync built-ins, we
 * can use the preprocessor to detect API and, more importantly, ISA
 * support. We want to avoid linking headaches where the API depends on an
 * external library if the ISA (e.g. i386) doesn't support lockless
 * operation.
 *
 * TODO: Support C11's atomic API. Although that may require some finesse
 * with how we define some public types, such as dns_atomic_t and struct
 * dns_resolv_conf.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef HAVE___ATOMIC_FETCH_ADD
#define HAVE___ATOMIC_FETCH_ADD (defined __ATOMIC_RELAXED)
#endif

#ifndef HAVE___ATOMIC_FETCH_SUB
#define HAVE___ATOMIC_FETCH_SUB HAVE___ATOMIC_FETCH_ADD
#endif

#ifndef DNS_ATOMIC_FETCH_ADD
#if HAVE___ATOMIC_FETCH_ADD && __GCC_ATOMIC_LONG_LOCK_FREE == 2
#define DNS_ATOMIC_FETCH_ADD(i) __atomic_fetch_add((i), 1, __ATOMIC_RELAXED)
#else
#pragma message("no atomic_fetch_add available")
#define DNS_ATOMIC_FETCH_ADD(i) ((*(i))++)
#endif
#endif

#ifndef DNS_ATOMIC_FETCH_SUB
#if HAVE___ATOMIC_FETCH_SUB && __GCC_ATOMIC_LONG_LOCK_FREE == 2
#define DNS_ATOMIC_FETCH_SUB(i) __atomic_fetch_sub((i), 1, __ATOMIC_RELAXED)
#else
#pragma message("no atomic_fetch_sub available")
#define DNS_ATOMIC_FETCH_SUB(i) ((*(i))--)
#endif
#endif

static inline unsigned dns_atomic_fetch_add(dns_atomic_t *i) {
	return DNS_ATOMIC_FETCH_ADD(i);
} /* dns_atomic_fetch_add() */


static inline unsigned dns_atomic_fetch_sub(dns_atomic_t *i) {
	return DNS_ATOMIC_FETCH_SUB(i);
} /* dns_atomic_fetch_sub() */


/*
 * C R Y P T O  R O U T I N E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * P R N G
 */

#ifndef DNS_RANDOM
#if defined(HAVE_ARC4RANDOM)	\
 || defined(__OpenBSD__)	\
 || defined(__FreeBSD__)	\
 || defined(__NetBSD__)		\
 || defined(__APPLE__)
#define DNS_RANDOM	arc4random
#elif __linux
#define DNS_RANDOM	random
#else
#define DNS_RANDOM	rand
#endif
#endif

#define DNS_RANDOM_arc4random	1
#define DNS_RANDOM_random	2
#define DNS_RANDOM_rand		3
#define DNS_RANDOM_RAND_bytes	4

#define DNS_RANDOM_OPENSSL	(DNS_RANDOM_RAND_bytes == DNS_PP_XPASTE(DNS_RANDOM_, DNS_RANDOM))

#if DNS_RANDOM_OPENSSL
#include <openssl/rand.h>
#endif

static unsigned dns_random_(void) {
#if DNS_RANDOM_OPENSSL
	unsigned r;

	assert(1 == RAND_bytes((unsigned char *)&r, sizeof r));

	return r;
#else
	return DNS_RANDOM();
#endif
} /* dns_random_() */

unsigned (*dns_random)(void) __attribute__((weak))	= &dns_random_;


/*
 * P E R M U T A T I O N  G E N E R A T O R
 */

#define DNS_K_TEA_KEY_SIZE	16
#define DNS_K_TEA_BLOCK_SIZE	8
#define DNS_K_TEA_CYCLES	32
#define DNS_K_TEA_MAGIC		0x9E3779B9U

struct dns_k_tea {
	uint32_t key[DNS_K_TEA_KEY_SIZE / sizeof (uint32_t)];
	unsigned cycles;
}; /* struct dns_k_tea */


static void dns_k_tea_init(struct dns_k_tea *tea, uint32_t key[], unsigned cycles) {
	memcpy(tea->key, key, sizeof tea->key);

	tea->cycles	= (cycles)? cycles : DNS_K_TEA_CYCLES;
} /* dns_k_tea_init() */


static void dns_k_tea_encrypt(struct dns_k_tea *tea, uint32_t v[], uint32_t *w) {
	uint32_t y, z, sum, n;

	y	= v[0];
	z	= v[1];
	sum	= 0;

	for (n = 0; n < tea->cycles; n++) {
		sum	+= DNS_K_TEA_MAGIC;
		y	+= ((z << 4) + tea->key[0]) ^ (z + sum) ^ ((z >> 5) + tea->key[1]);
		z	+= ((y << 4) + tea->key[2]) ^ (y + sum) ^ ((y >> 5) + tea->key[3]);
	}

	w[0]	= y;
	w[1]	= z;

	return /* void */;
} /* dns_k_tea_encrypt() */


/*
 * Permutation generator, based on a Luby-Rackoff Feistel construction.
 *
 * Specifically, this is a generic balanced Feistel block cipher using TEA
 * (another block cipher) as the pseudo-random function, F. At best it's as
 * strong as F (TEA), notwithstanding the seeding. F could be AES, SHA-1, or
 * perhaps Bernstein's Salsa20 core; I am naively trying to keep things
 * simple.
 *
 * The generator can create a permutation of any set of numbers, as long as
 * the size of the set is an even power of 2. This limitation arises either
 * out of an inherent property of balanced Feistel constructions, or by my
 * own ignorance. I'll tackle an unbalanced construction after I wrap my
 * head around Schneier and Kelsey's paper.
 *
 * CAVEAT EMPTOR. IANAC.
 */
#define DNS_K_PERMUTOR_ROUNDS	8

struct dns_k_permutor {
	unsigned stepi, length, limit;
	unsigned shift, mask, rounds;

	struct dns_k_tea tea;
}; /* struct dns_k_permutor */


static inline unsigned dns_k_permutor_powof(unsigned n) {
	unsigned m, i = 0;

	for (m = 1; m < n; m <<= 1, i++)
		;;

	return i;
} /* dns_k_permutor_powof() */

static void dns_k_permutor_init(struct dns_k_permutor *p, unsigned low, unsigned high) {
	uint32_t key[DNS_K_TEA_KEY_SIZE / sizeof (uint32_t)];
	unsigned width, i;

	p->stepi	= 0;

	p->length	= (high - low) + 1;
	p->limit	= high;

	width		= dns_k_permutor_powof(p->length);
	width		+= width % 2;

	p->shift	= width / 2;
	p->mask		= (1U << p->shift) - 1;
	p->rounds	= DNS_K_PERMUTOR_ROUNDS;

	for (i = 0; i < lengthof(key); i++)
		key[i]	= dns_random();

	dns_k_tea_init(&p->tea, key, 0);

	return /* void */;
} /* dns_k_permutor_init() */


static unsigned dns_k_permutor_F(struct dns_k_permutor *p, unsigned k, unsigned x) {
	uint32_t in[DNS_K_TEA_BLOCK_SIZE / sizeof (uint32_t)], out[DNS_K_TEA_BLOCK_SIZE / sizeof (uint32_t)];

	memset(in, '\0', sizeof in);

	in[0]	= k;
	in[1]	= x;

	dns_k_tea_encrypt(&p->tea, in, out);

	return p->mask & out[0];
} /* dns_k_permutor_F() */


static unsigned dns_k_permutor_E(struct dns_k_permutor *p, unsigned n) {
	unsigned l[2], r[2];
	unsigned i;

	i	= 0;
	l[i]	= p->mask & (n >> p->shift);
	r[i]	= p->mask & (n >> 0);

	do {
		l[(i + 1) % 2]	= r[i % 2];
		r[(i + 1) % 2]	= l[i % 2] ^ dns_k_permutor_F(p, i, r[i % 2]);

		i++;
	} while (i < p->rounds - 1);

	return ((l[i % 2] & p->mask) << p->shift) | ((r[i % 2] & p->mask) << 0);
} /* dns_k_permutor_E() */


DNS_NOTUSED static unsigned dns_k_permutor_D(struct dns_k_permutor *p, unsigned n) {
	unsigned l[2], r[2];
	unsigned i;

	i		= p->rounds - 1;
	l[i % 2]	= p->mask & (n >> p->shift);
	r[i % 2]	= p->mask & (n >> 0);

	do {
		i--;

		r[i % 2]	= l[(i + 1) % 2];
		l[i % 2]	= r[(i + 1) % 2] ^ dns_k_permutor_F(p, i, l[(i + 1) % 2]);
	} while (i > 0);

	return ((l[i % 2] & p->mask) << p->shift) | ((r[i % 2] & p->mask) << 0);
} /* dns_k_permutor_D() */


static unsigned dns_k_permutor_step(struct dns_k_permutor *p) {
	unsigned n;

	do {
		n	= dns_k_permutor_E(p, p->stepi++);
	} while (n >= p->length);

	return n + (p->limit + 1 - p->length);
} /* dns_k_permutor_step() */


/*
 * Simple permutation box. Useful for shuffling rrsets from an iterator.
 * Uses AES s-box to provide good diffusion.
 *
 * Seems to pass muster under runs test.
 *
 * $ for i in 0 1 2 3 4 5 6 7 8 9; do ./dns shuffle-16 > /tmp/out; done
 * $ R -q -f /dev/stdin 2>/dev/null <<-EOF | awk '/p-value/{ print $8 }'
 * 	library(lawstat)
 * 	runs.test(scan(file="/tmp/out"))
 * EOF
 */
static unsigned short dns_k_shuffle16(unsigned short n, unsigned s) {
	static const unsigned char sbox[256] =
	{ 0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5,
	  0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
	  0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
	  0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
	  0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc,
	  0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
	  0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a,
	  0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
	  0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
	  0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
	  0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b,
	  0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
	  0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85,
	  0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
	  0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
	  0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
	  0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17,
	  0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
	  0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88,
	  0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
	  0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
	  0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
	  0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9,
	  0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
	  0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6,
	  0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
	  0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
	  0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
	  0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94,
	  0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
	  0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68,
	  0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16 };
	unsigned char a, b;
	unsigned i;

	a = 0xff & (n >> 0);
	b = 0xff & (n >> 8);

	for (i = 0; i < 4; i++) {
		a ^= 0xff & s;
		a = sbox[a] ^ b;
		b = sbox[b] ^ a;
		s >>= 8;
	}

	return ((0xff00 & (a << 8)) | (0x00ff & (b << 0)));
} /* dns_k_shuffle16() */


/*
 * U T I L I T Y  R O U T I N E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define DNS_MAXINTERVAL 300

struct dns_clock {
	time_t sample, elapsed;
}; /* struct dns_clock */

static void dns_begin(struct dns_clock *clk) {
	clk->sample = time(0);
	clk->elapsed = 0;
} /* dns_begin() */

static time_t dns_elapsed(struct dns_clock *clk) {
	time_t curtime;

	if ((time_t)-1 == time(&curtime))
		return clk->elapsed;

	if (curtime > clk->sample)
		clk->elapsed += (time_t)DNS_PP_MIN(difftime(curtime, clk->sample), DNS_MAXINTERVAL);

	clk->sample = curtime;

	return clk->elapsed;
} /* dns_elapsed() */


DNS_NOTUSED static size_t dns_strnlen(const char *src, size_t m) {
	size_t n = 0;

	while (*src++ && n < m)
		++n;

	return n;
} /* dns_strnlen() */


DNS_NOTUSED static size_t dns_strnlcpy(char *dst, size_t lim, const char *src, size_t max) {
	size_t len = dns_strnlen(src, max), n;

	if (lim > 0) {
		n = DNS_PP_MIN(lim - 1, len);
		memcpy(dst, src, n);
		dst[n] = '\0';
	}

	return len;
} /* dns_strnlcpy() */


#define DNS_HAVE_SOCKADDR_UN (defined AF_UNIX && !defined _WIN32)

static size_t dns_af_len(int af) {
	static const size_t table[AF_MAX]	= {
		[AF_INET6]	= sizeof (struct sockaddr_in6),
		[AF_INET]	= sizeof (struct sockaddr_in),
#if DNS_HAVE_SOCKADDR_UN
		[AF_UNIX]	= sizeof (struct sockaddr_un),
#endif
	};

	return table[af];
} /* dns_af_len() */

#define dns_sa_family(sa)	(((struct sockaddr *)(sa))->sa_family)

#define dns_sa_len(sa)		dns_af_len(dns_sa_family(sa))


#define DNS_SA_NOPORT	&dns_sa_noport
static unsigned short dns_sa_noport;

static unsigned short *dns_sa_port(int af, void *sa) {
	switch (af) {
	case AF_INET6:
		return &((struct sockaddr_in6 *)sa)->sin6_port;
	case AF_INET:
		return &((struct sockaddr_in *)sa)->sin_port;
	default:
		return DNS_SA_NOPORT;
	}
} /* dns_sa_port() */


static void *dns_sa_addr(int af, void *sa, socklen_t *size) {
	switch (af) {
	case AF_INET6: {
		struct in6_addr *in6 = &((struct sockaddr_in6 *)sa)->sin6_addr;

		if (size)
			*size = sizeof *in6;

		return in6;
	}
	case AF_INET: {
		struct in_addr *in = &((struct sockaddr_in *)sa)->sin_addr;

		if (size)
			*size = sizeof *in;

		return in;
	}
	default:
		if (size)
			*size = 0;

		return 0;
	}
} /* dns_sa_addr() */


#if DNS_HAVE_SOCKADDR_UN
#define DNS_SUNPATHMAX (sizeof ((struct sockaddr_un *)0)->sun_path)
#endif

DNS_NOTUSED static void *dns_sa_path(void *sa, socklen_t *size) {
	switch (dns_sa_family(sa)) {
#if DNS_HAVE_SOCKADDR_UN
	case AF_UNIX: {
		char *path = ((struct sockaddr_un *)sa)->sun_path;

		if (size)
			*size = dns_strnlen(path, DNS_SUNPATHMAX);

		return path;
	}
#endif
	default:
		if (size)
			*size = 0;

		return NULL;
	}
} /* dns_sa_path() */


static int dns_sa_cmp(void *a, void *b) {
	int cmp, af;

	if ((cmp = dns_sa_family(a) - dns_sa_family(b)))
		return cmp;

	switch ((af = dns_sa_family(a))) {
	case AF_INET: {
		struct in_addr *a4, *b4;

		if ((cmp = htons(*dns_sa_port(af, a)) - htons(*dns_sa_port(af, b))))
			return cmp;

		a4 = dns_sa_addr(af, a, NULL);
		b4 = dns_sa_addr(af, b, NULL);

		if (ntohl(a4->s_addr) < ntohl(b4->s_addr))
			return -1;
		if (ntohl(a4->s_addr) > ntohl(b4->s_addr))
			return 1;

		return 0;
	}
	case AF_INET6: {
		struct in6_addr *a6, *b6;
		size_t i;

		if ((cmp = htons(*dns_sa_port(af, a)) - htons(*dns_sa_port(af, b))))
			return cmp;

		a6 = dns_sa_addr(af, a, NULL);
		b6 = dns_sa_addr(af, b, NULL);

		/* XXX: do we need to use in6_clearscope()? */
		for (i = 0; i < sizeof a6->s6_addr; i++) {
			if ((cmp = a6->s6_addr[i] - b6->s6_addr[i]))
				return cmp;
		}

		return 0;
	}
#if DNS_HAVE_SOCKADDR_UN
	case AF_UNIX: {
		char a_path[DNS_SUNPATHMAX + 1], b_path[sizeof a_path];

		dns_strnlcpy(a_path, sizeof a_path, dns_sa_path(a, NULL), DNS_SUNPATHMAX);
		dns_strnlcpy(b_path, sizeof b_path, dns_sa_path(b, NULL), DNS_SUNPATHMAX);

		return strcmp(a_path, b_path);
	}
#endif
	default:
		return -1;
	}
} /* dns_sa_cmp() */


#if _WIN32
static int dns_inet_pton(int af, const void *src, void *dst) {
	union { struct sockaddr_in sin; struct sockaddr_in6 sin6; } u;

	u.sin.sin_family	= af;

	if (0 != WSAStringToAddressA((void *)src, af, (void *)0, (struct sockaddr *)&u, &(int){ sizeof u }))
		return -1;

	switch (af) {
	case AF_INET6:
		*(struct in6_addr *)dst	= u.sin6.sin6_addr;

		return 1;
	case AF_INET:
		*(struct in_addr *)dst	= u.sin.sin_addr;

		return 1;
	default:
		return 0;
	}
} /* dns_inet_pton() */

static const char *dns_inet_ntop(int af, const void *src, void *dst, unsigned long lim) {
	union { struct sockaddr_in sin; struct sockaddr_in6 sin6; } u;

	/* NOTE: WSAAddressToString will print .sin_port unless zeroed. */
	memset(&u, 0, sizeof u);

	u.sin.sin_family	= af;

	switch (af) {
	case AF_INET6:
		u.sin6.sin6_addr	= *(struct in6_addr *)src;
		break;
	case AF_INET:
		u.sin.sin_addr		= *(struct in_addr *)src;

		break;
	default:
		return 0;
	}

	if (0 != WSAAddressToStringA((struct sockaddr *)&u, dns_sa_len(&u), (void *)0, dst, &lim))
		return 0;

	return dst;
} /* dns_inet_ntop() */
#else
#define dns_inet_pton(...)	inet_pton(__VA_ARGS__)
#define dns_inet_ntop(...)	inet_ntop(__VA_ARGS__)
#endif


static dns_error_t dns_pton(int af, const void *src, void *dst) {
	switch (dns_inet_pton(af, src, dst)) {
	case 1:
		return 0;
	case -1:
		return dns_soerr();
	default:
		return DNS_EADDRESS;
	}
} /* dns_pton() */


static dns_error_t dns_ntop(int af, const void *src, void *dst, unsigned long lim) {
	return (dns_inet_ntop(af, src, dst, lim))? 0 : dns_soerr();
} /* dns_ntop() */


size_t dns_strlcpy(char *dst, const char *src, size_t lim) {
	char *d		= dst;
	char *e		= &dst[lim];
	const char *s	= src;

	if (d < e) {
		do {
			if ('\0' == (*d++ = *s++))
				return s - src - 1;
		} while (d < e);

		d[-1]	= '\0';
	}

	while (*s++ != '\0')
		;;

	return s - src - 1;
} /* dns_strlcpy() */


size_t dns_strlcat(char *dst, const char *src, size_t lim) {
	char *d = memchr(dst, '\0', lim);
	char *e = &dst[lim];
	const char *s = src;
	const char *p;

	if (d && d < e) {
		do {
			if ('\0' == (*d++ = *s++))
				return d - dst - 1;
		} while (d < e);

		d[-1] = '\0';
	}

	p = s;

	while (*s++ != '\0')
		;;

	return lim + (s - p - 1);
} /* dns_strlcat() */


#if _WIN32

static char *dns_strsep(char **sp, const char *delim) {
	char *p;

	if (!(p = *sp))
		return 0;

	*sp += strcspn(p, delim);

	if (**sp != '\0') {
		**sp = '\0';
		++*sp;
	} else
		*sp = NULL;

	return p;
} /* dns_strsep() */

#else
#define dns_strsep(...)	strsep(__VA_ARGS__)
#endif


#if _WIN32
#define strcasecmp(...)		_stricmp(__VA_ARGS__)
#define strncasecmp(...)	_strnicmp(__VA_ARGS__)
#endif


static int dns_poll(int fd, short events, int timeout) {
	fd_set rset, wset;

	if (!events)
		return 0;

	assert(fd >= 0 && (unsigned)fd < FD_SETSIZE);

	FD_ZERO(&rset);
	FD_ZERO(&wset);

	if (events & DNS_POLLIN)
		FD_SET(fd, &rset);

	if (events & DNS_POLLOUT)
		FD_SET(fd, &wset);

	select(fd + 1, &rset, &wset, 0, (timeout >= 0)? &(struct timeval){ timeout, 0 } : NULL);

	return 0;
} /* dns_poll() */


#if !_WIN32
DNS_NOTUSED static int dns_sigmask(int how, const sigset_t *set, sigset_t *oset) {
#if DNS_THREAD_SAFE
	return pthread_sigmask(how, set, oset);
#else
	return (0 == sigprocmask(how, set, oset))? 0 : errno;
#endif
} /* dns_sigmask() */
#endif


static long dns_send(int fd, const void *src, size_t lim, int flags) {
#if _WIN32 || !defined SIGPIPE || defined SO_NOSIGPIPE
	return send(fd, src, lim, flags);
#elif defined MSG_NOSIGNAL
	return send(fd, src, lim, flags|MSG_NOSIGNAL);
#elif _POSIX_REALTIME_SIGNALS > 0 /* require sigtimedwait */
	/*
	 * SIGPIPE handling similar to the approach described in
	 * http://krokisplace.blogspot.com/2010/02/suppressing-sigpipe-in-library.html
	 */
	sigset_t pending, blocked, piped;
	long count;
	int saved, error;

	sigemptyset(&pending);
	sigpending(&pending);

	if (!sigismember(&pending, SIGPIPE)) {
		sigemptyset(&piped);
		sigaddset(&piped, SIGPIPE);
		sigemptyset(&blocked);

		if ((error = dns_sigmask(SIG_BLOCK, &piped, &blocked)))
			goto error;
	}

	count = send(fd, src, lim, flags);

	if (!sigismember(&pending, SIGPIPE)) {
		saved = errno;

		if (count == -1 && errno == EPIPE) {
			while (-1 == sigtimedwait(&piped, NULL, &(struct timespec){ 0, 0 }) && errno == EINTR)
				;;
		}

		if ((error = dns_sigmask(SIG_SETMASK, &blocked, NULL)))
			goto error;

		errno = saved;
	}

	return count;
error:
	errno = error;

	return -1;
#else
#error "unable to suppress SIGPIPE"
	return send(fd, src, lim, flags);
#endif
} /* dns_send() */


/*
 * P A C K E T  R O U T I N E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

unsigned dns_p_count(struct dns_packet *P, enum dns_section section) {
	unsigned count;

	switch (section) {
	case DNS_S_QD:
		return ntohs(dns_header(P)->qdcount);
	case DNS_S_AN:
		return ntohs(dns_header(P)->ancount);
	case DNS_S_NS:
		return ntohs(dns_header(P)->nscount);
	case DNS_S_AR:
		return ntohs(dns_header(P)->arcount);
	default:
		count = 0;

		if (section & DNS_S_QD)
			count += ntohs(dns_header(P)->qdcount);
		if (section & DNS_S_AN)
			count += ntohs(dns_header(P)->ancount);
		if (section & DNS_S_NS)
			count += ntohs(dns_header(P)->nscount);
		if (section & DNS_S_AR)
			count += ntohs(dns_header(P)->arcount);

		return count;
	}
} /* dns_p_count() */


struct dns_packet *dns_p_init(struct dns_packet *P, size_t size) {
	if (!P)
		return 0;

	assert(size >= offsetof(struct dns_packet, data) + 12);

	memset(P, 0, sizeof *P);
	P->size = size - offsetof(struct dns_packet, data);
	P->end  = 12;

	memset(P->data, '\0', 12);

	return P;
} /* dns_p_init() */


static unsigned short dns_p_qend(struct dns_packet *P) {
	unsigned short qend	= 12;
	unsigned i, count	= dns_p_count(P, DNS_S_QD);

	for (i = 0; i < count && qend < P->end; i++) {
		if (P->end == (qend = dns_d_skip(qend, P)))
			goto invalid;

		if (P->end - qend < 4)
			goto invalid;

		qend	+= 4;
	}

	return DNS_PP_MIN(qend, P->end);
invalid:
	return P->end;
} /* dns_p_qend() */


struct dns_packet *dns_p_make(size_t len, int *error) {
	struct dns_packet *P;
	size_t size = dns_p_calcsize(len);

	if (!(P = dns_p_init(malloc(size), size)))
		*error = dns_syerr();

	return P;
} /* dns_p_make() */


static void dns_p_free(struct dns_packet *P) {
	free(P);
} /* dns_p_free() */


/* convience routine to free any existing packet before storing new packet */
static struct dns_packet *dns_p_setptr(struct dns_packet **dst, struct dns_packet *src) {
	dns_p_free(*dst);

	*dst = src;

	return src;
} /* dns_p_setptr() */


static struct dns_packet *dns_p_movptr(struct dns_packet **dst, struct dns_packet **src) {
	dns_p_setptr(dst, *src);

	*src = NULL;

	return *dst;
} /* dns_p_movptr() */


int dns_p_grow(struct dns_packet **P) {
	struct dns_packet *tmp;
	size_t size;
	int error;

	if (!*P) {
		if (!(*P = dns_p_make(DNS_P_QBUFSIZ, &error)))
			return error;

		return 0;
	}

	size = dns_p_sizeof(*P);
	size |= size >> 1;
	size |= size >> 2;
	size |= size >> 4;
	size |= size >> 8;
	size++;

	if (size > 65536)
		return DNS_ENOBUFS;

	if (!(tmp = realloc(*P, dns_p_calcsize(size))))
		return dns_syerr();

	tmp->size = size;
	*P = tmp;

	return 0;
} /* dns_p_grow() */


struct dns_packet *dns_p_copy(struct dns_packet *P, const struct dns_packet *P0) {
	if (!P)
		return 0;

	P->end	= DNS_PP_MIN(P->size, P0->end);

	memcpy(P->data, P0->data, P->end);

	return P;
} /* dns_p_copy() */


struct dns_packet *dns_p_merge(struct dns_packet *A, enum dns_section Amask, struct dns_packet *B, enum dns_section Bmask, int *error_) {
	size_t bufsiz = DNS_PP_MIN(65535, ((A)? A->end : 0) + ((B)? B->end : 0));
	struct dns_packet *M;
	enum dns_section section;
	struct dns_rr rr, mr;
	int error, copy;

	if (!A && B) {
		A = B;
		Amask = Bmask;
		B = 0;
	}

merge:
	if (!(M = dns_p_make(bufsiz, &error)))
		goto error;

	for (section = DNS_S_QD; (DNS_S_ALL & section); section <<= 1) {
		if (A && (section & Amask)) {
			dns_rr_foreach(&rr, A, .section = section) {
				if ((error = dns_rr_copy(M, &rr, A)))
					goto error;
			}
		}

		if (B && (section & Bmask)) {
			dns_rr_foreach(&rr, B, .section = section) {
				copy = 1;

				dns_rr_foreach(&mr, M, .type = rr.type, .section = DNS_S_ALL) {
					if (!(copy = dns_rr_cmp(&rr, B, &mr, M)))
						break;
				}

				if (copy && (error = dns_rr_copy(M, &rr, B)))
					goto error;
			}
		}
	}

	return M;
error:
	dns_p_setptr(&M, NULL);

	if (error == DNS_ENOBUFS && bufsiz < 65535) {
		bufsiz = DNS_PP_MIN(65535, bufsiz * 2);

		goto merge;
	}

	*error_	= error;

	return 0;
} /* dns_p_merge() */


static unsigned short dns_l_skip(unsigned short, const unsigned char *, size_t);

void dns_p_dictadd(struct dns_packet *P, unsigned short dn) {
	unsigned short lp, lptr, i;

	lp	= dn;

	while (lp < P->end) {
		if (0xc0 == (0xc0 & P->data[lp]) && P->end - lp >= 2 && lp != dn) {
			lptr	= ((0x3f & P->data[lp + 0]) << 8)
				| ((0xff & P->data[lp + 1]) << 0);

			for (i = 0; i < lengthof(P->dict) && P->dict[i]; i++) {
				if (P->dict[i] == lptr) {
					P->dict[i]	= dn;

					return;
				}
			}
		}

		lp	= dns_l_skip(lp, P->data, P->end);
	}

	for (i = 0; i < lengthof(P->dict); i++) {
		if (!P->dict[i]) {
			P->dict[i]	= dn;

			break;
		}
	}
} /* dns_p_dictadd() */


int dns_p_push(struct dns_packet *P, enum dns_section section, const void *dn, size_t dnlen, enum dns_type type, enum dns_class class, unsigned ttl, const void *any) {
	size_t end = P->end;
	int error;

	if ((error = dns_d_push(P, dn, dnlen)))
		goto error;

	if (P->size - P->end < 4)
		goto nobufs;

	P->data[P->end++] = 0xff & (type >> 8);
	P->data[P->end++] = 0xff & (type >> 0);

	P->data[P->end++] = 0xff & (class >> 8);
	P->data[P->end++] = 0xff & (class >> 0);

	if (section == DNS_S_QD)
		goto update;

	if (P->size - P->end < 6)
		goto nobufs;

	P->data[P->end++] = 0x7f & (ttl >> 24);
	P->data[P->end++] = 0xff & (ttl >> 16);
	P->data[P->end++] = 0xff & (ttl >> 8);
	P->data[P->end++] = 0xff & (ttl >> 0);

	if ((error = dns_any_push(P, (union dns_any *)any, type)))
		goto error;

update:
	switch (section) {
	case DNS_S_QD:
		if (dns_p_count(P, DNS_S_AN|DNS_S_NS|DNS_S_AR))
			goto order;

		if (!P->qd.base && (error = dns_p_study(P)))
			goto error;

		dns_header(P)->qdcount = htons(ntohs(dns_header(P)->qdcount) + 1);

		P->qd.end  = P->end;
		P->an.base = P->end;
		P->an.end  = P->end;
		P->ns.base = P->end;
		P->ns.end  = P->end;
		P->ar.base = P->end;
		P->ar.end  = P->end;

		break;
	case DNS_S_AN:
		if (dns_p_count(P, DNS_S_NS|DNS_S_AR))
			goto order;

		if (!P->an.base && (error = dns_p_study(P)))
			goto error;

		dns_header(P)->ancount = htons(ntohs(dns_header(P)->ancount) + 1);

		P->an.end  = P->end;
		P->ns.base = P->end;
		P->ns.end  = P->end;
		P->ar.base = P->end;
		P->ar.end  = P->end;

		break;
	case DNS_S_NS:
		if (dns_p_count(P, DNS_S_AR))
			goto order;

		if (!P->ns.base && (error = dns_p_study(P)))
			goto error;

		dns_header(P)->nscount = htons(ntohs(dns_header(P)->nscount) + 1);

		P->ns.end  = P->end;
		P->ar.base = P->end;
		P->ar.end  = P->end;

		break;
	case DNS_S_AR:
		if (!P->ar.base && (error = dns_p_study(P)))
			goto error;

		dns_header(P)->arcount = htons(ntohs(dns_header(P)->arcount) + 1);

		P->ar.end = P->end;

		break;
	default:
		error = DNS_ESECTION;

		goto error;
	} /* switch() */

	return 0;
nobufs:
	error = DNS_ENOBUFS;

	goto error;
order:
	error = DNS_EORDER;

	goto error;
error:
	P->end = end;

	return error;
} /* dns_p_push() */


static void dns_p_dump3(struct dns_packet *P, struct dns_rr_i *I, FILE *fp) {
	enum dns_section section;
	struct dns_rr rr;
	int error;
	union dns_any any;
	char pretty[sizeof any * 2];
	size_t len;

	fputs(";; [HEADER]\n", fp);
	fprintf(fp, ";;     qr : %s(%d)\n", (dns_header(P)->qr)? "RESPONSE" : "QUERY", dns_header(P)->qr);
	fprintf(fp, ";; opcode : %s(%d)\n", dns_stropcode(dns_header(P)->opcode), dns_header(P)->opcode);
	fprintf(fp, ";;     aa : %s(%d)\n", (dns_header(P)->aa)? "AUTHORITATIVE" : "NON-AUTHORITATIVE", dns_header(P)->aa);
	fprintf(fp, ";;     tc : %s(%d)\n", (dns_header(P)->tc)? "TRUNCATED" : "NOT-TRUNCATED", dns_header(P)->tc);
	fprintf(fp, ";;     rd : %s(%d)\n", (dns_header(P)->rd)? "RECURSION-DESIRED" : "RECURSION-NOT-DESIRED", dns_header(P)->rd);
	fprintf(fp, ";;     ra : %s(%d)\n", (dns_header(P)->ra)? "RECURSION-ALLOWED" : "RECURSION-NOT-ALLOWED", dns_header(P)->ra);
	fprintf(fp, ";;  rcode : %s(%d)\n", dns_strrcode(dns_header(P)->rcode), dns_header(P)->rcode);

	section	= 0;

	while (dns_rr_grep(&rr, 1, I, P, &error)) {
		if (section != rr.section)
			fprintf(fp, "\n;; [%s:%d]\n", dns_strsection(rr.section), dns_p_count(P, rr.section));

		if ((len = dns_rr_print(pretty, sizeof pretty, &rr, P, &error)))
			fprintf(fp, "%s\n", pretty);

		section	= rr.section;
	}
} /* dns_p_dump3() */


void dns_p_dump(struct dns_packet *P, FILE *fp) {
	dns_p_dump3(P, dns_rr_i_new(P, .section = 0), fp);
} /* dns_p_dump() */


static void dns_s_unstudy(struct dns_s_memo *m)
	{ m->base = 0; m->end = 0; }

static void dns_p_unstudy(struct dns_packet *P) {
	dns_s_unstudy(&P->qd);
	dns_s_unstudy(&P->an);
	dns_s_unstudy(&P->ns);
	dns_s_unstudy(&P->ar);
} /* dns_p_unstudy() */

static int dns_s_study(struct dns_s_memo *m, enum dns_section section, unsigned base, struct dns_packet *P) {
	unsigned short count, rp;

	count = dns_p_count(P, section);

	for (rp = base; count && rp < P->end; count--)
		rp = dns_rr_skip(rp, P);

	m->base = base;
	m->end  = rp;

	return 0;
} /* dns_s_study() */

int dns_p_study(struct dns_packet *P) {
	int error;

	if ((error = dns_s_study(&P->qd, DNS_S_QD, 12, P)))
		goto error;

	if ((error = dns_s_study(&P->an, DNS_S_AN, P->qd.end, P)))
		goto error;

	if ((error = dns_s_study(&P->ns, DNS_S_NS, P->an.end, P)))
		goto error;

	if ((error = dns_s_study(&P->ar, DNS_S_AR, P->ns.end, P)))
		goto error;

	return 0;
error:
	dns_p_unstudy(P);

	return error;
} /* dns_p_study() */


/*
 * D O M A I N  N A M E  R O U T I N E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef DNS_D_MAXPTRS
#define DNS_D_MAXPTRS	127	/* Arbitrary; possible, valid depth is something like packet size / 2 + fudge. */
#endif

static size_t dns_l_expand(unsigned char *dst, size_t lim, unsigned short src, unsigned short *nxt, const unsigned char *data, size_t end) {
	unsigned short len;
	unsigned nptrs	= 0;

retry:
	if (src >= end)
		goto invalid;

	switch (0x03 & (data[src] >> 6)) {
	case 0x00:
		len	= (0x3f & (data[src++]));

		if (end - src < len)
			goto invalid;

		if (lim > 0) {
			memcpy(dst, &data[src], DNS_PP_MIN(lim, len));

			dst[DNS_PP_MIN(lim - 1, len)]	= '\0';
		}

		*nxt	= src + len;

		return len;
	case 0x01:
		goto invalid;
	case 0x02:
		goto invalid;
	case 0x03:
		if (++nptrs > DNS_D_MAXPTRS)
			goto invalid;

		if (end - src < 2)
			goto invalid;

		src	= ((0x3f & data[src + 0]) << 8)
			| ((0xff & data[src + 1]) << 0);

		goto retry;
	} /* switch() */

	/* NOT REACHED */
invalid:
	*nxt	= end;

	return 0;
} /* dns_l_expand() */


static unsigned short dns_l_skip(unsigned short src, const unsigned char *data, size_t end) {
	unsigned short len;

	if (src >= end)
		goto invalid;

	switch (0x03 & (data[src] >> 6)) {
	case 0x00:
		len	= (0x3f & (data[src++]));

		if (end - src < len)
			goto invalid;

		return (len)? src + len : end;
	case 0x01:
		goto invalid;
	case 0x02:
		goto invalid;
	case 0x03:
		return end;
	} /* switch() */

	/* NOT REACHED */
invalid:
	return end;
} /* dns_l_skip() */


size_t dns_d_trim(void *dst_, size_t lim, const void *src_, size_t len, int flags) {
	unsigned char *dst = dst_;
	const unsigned char *src = src_;
	size_t dp = 0, sp = 0;
	int lc;

	/* trim any leading dot(s) */
	while (sp < len && src[sp] == '.')
		sp++;

	for (lc = 0; sp < len; lc = src[sp++]) {
		/* trim extra dot(s) */
		if (src[sp] == '.' && lc == '.')
			continue;

		if (dp < lim)
			dst[dp] = src[sp];

		dp++;
	}

	if ((flags & DNS_D_ANCHOR) && lc != '.') {
		if (dp < lim)
			dst[dp] = '.';

		dp++;
	}

	if (lim > 0)
		dst[DNS_PP_MIN(dp, lim - 1)] = '\0';

	return dp;
} /* dns_d_trim() */


char *dns_d_init(void *dst, size_t lim, const void *src, size_t len, int flags) {
	if (flags & DNS_D_TRIM) {
		dns_d_trim(dst, lim, src, len, flags);
	} if (flags & DNS_D_ANCHOR) {
		dns_d_anchor(dst, lim, src, len);
	} else {
		memmove(dst, src, DNS_PP_MIN(lim, len));

		if (lim > 0)
			((char *)dst)[DNS_PP_MIN(len, lim - 1)]	= '\0';
	}

	return dst;
} /* dns_d_init() */


size_t dns_d_anchor(void *dst, size_t lim, const void *src, size_t len) {
	if (len == 0)
		return 0;

	memmove(dst, src, DNS_PP_MIN(lim, len));

	if (((const char *)src)[len - 1] != '.') {
		if (len < lim)
			((char *)dst)[len]	= '.';
		len++;
	}

	if (lim > 0)
		((char *)dst)[DNS_PP_MIN(lim - 1, len)]	= '\0';

	return len;
} /* dns_d_anchor() */


size_t dns_d_cleave(void *dst, size_t lim, const void *src, size_t len) {
	const char *dot;

	/* XXX: Skip any leading dot. Handles cleaving root ".". */
	if (len == 0 || !(dot = memchr((const char *)src + 1, '.', len - 1)))
		return 0;

	len	-= dot - (const char *)src;

	/* XXX: Unless root, skip the label's trailing dot. */
	if (len > 1) {
		src	= ++dot;
		len--;
	} else
		src	= dot;

	memmove(dst, src, DNS_PP_MIN(lim, len));

	if (lim > 0)
		((char *)dst)[DNS_PP_MIN(lim - 1, len)]	= '\0';

	return len;
} /* dns_d_cleave() */


size_t dns_d_comp(void *dst_, size_t lim, const void *src_, size_t len, struct dns_packet *P, int *error) {
	struct { unsigned char *b; size_t p, x; } dst, src;
	unsigned char ch	= '.';

	dst.b	= dst_;
	dst.p	= 0;
	dst.x	= 1;

	src.b	= (unsigned char *)src_;
	src.p	= 0;
	src.x	= 0;

	while (src.x < len) {
		ch	= src.b[src.x];

		if (ch == '.') {
			if (dst.p < lim)
				dst.b[dst.p]	= (0x3f & (src.x - src.p));

			dst.p	= dst.x++;
			src.p	= ++src.x;
		} else {
			if (dst.x < lim)
				dst.b[dst.x]	= ch;

			dst.x++;
			src.x++;
		}
	} /* while() */

	if (src.x > src.p) {
		if (dst.p < lim)
			dst.b[dst.p]	= (0x3f & (src.x - src.p));

		dst.p	= dst.x;
	}

	if (dst.p > 1) {
		if (dst.p < lim)
			dst.b[dst.p]	= 0x00;

		dst.p++;
	}

#if 1
	if (dst.p < lim) {
		struct { unsigned char label[DNS_D_MAXLABEL + 1]; size_t len; unsigned short p, x, y; } a, b;
		unsigned i;

		a.p	= 0;

		while ((a.len = dns_l_expand(a.label, sizeof a.label, a.p, &a.x, dst.b, lim))) {
			for (i = 0; i < lengthof(P->dict) && P->dict[i]; i++) {
				b.p	= P->dict[i];

				while ((b.len = dns_l_expand(b.label, sizeof b.label, b.p, &b.x, P->data, P->end))) {
					a.y	= a.x;
					b.y	= b.x;

					while (a.len && b.len && 0 == strcasecmp((char *)a.label, (char *)b.label)) {
						a.len = dns_l_expand(a.label, sizeof a.label, a.y, &a.y, dst.b, lim);
						b.len = dns_l_expand(b.label, sizeof b.label, b.y, &b.y, P->data, P->end);
					}

					if (a.len == 0 && b.len == 0 && b.p <= 0x3fff) {
						dst.b[a.p++]	= 0xc0
								| (0x3f & (b.p >> 8));
						dst.b[a.p++]	= (0xff & (b.p >> 0));

						return a.p;
					}

					b.p	= b.x;
				} /* while() */
			} /* for() */

			a.p	= a.x;
		} /* while() */
	} /* if () */
#endif

	if (!dst.p)
		*error = DNS_EILLEGAL;

	return dst.p;
} /* dns_d_comp() */


unsigned short dns_d_skip(unsigned short src, struct dns_packet *P) {
	unsigned short len;

	while (src < P->end) {
		switch (0x03 & (P->data[src] >> 6)) {
		case 0x00:	/* FOLLOWS */
			len	= (0x3f & P->data[src++]);

			if (0 == len) {
/* success ==> */		return src;
			} else if (P->end - src > len) {
				src	+= len;

				break;
			} else
				goto invalid;

			/* NOT REACHED */
		case 0x01:	/* RESERVED */
			goto invalid;
		case 0x02:	/* RESERVED */
			goto invalid;
		case 0x03:	/* POINTER */
			if (P->end - src < 2)
				goto invalid;

			src	+= 2;

/* success ==> */	return src;
		} /* switch() */
	} /* while() */

invalid:
	return P->end;
} /* dns_d_skip() */


#include <stdio.h>

size_t dns_d_expand(void *dst, size_t lim, unsigned short src, struct dns_packet *P, int *error) {
	size_t dstp	= 0;
	unsigned nptrs	= 0;
	unsigned char len;

	while (src < P->end) {
		switch ((0x03 & (P->data[src] >> 6))) {
		case 0x00:	/* FOLLOWS */
			len	= (0x3f & P->data[src]);

			if (0 == len) {
				if (dstp == 0) {
					if (dstp < lim)
						((unsigned char *)dst)[dstp]	= '.';

					dstp++;
				}

				/* NUL terminate */
				if (lim > 0)
					((unsigned char *)dst)[DNS_PP_MIN(dstp, lim - 1)]	= '\0';

/* success ==> */		return dstp;
			}

			src++;

			if (P->end - src < len)
				goto toolong;

			if (dstp < lim)
				memcpy(&((unsigned char *)dst)[dstp], &P->data[src], DNS_PP_MIN(len, lim - dstp));

			src	+= len;
			dstp	+= len;

			if (dstp < lim)
				((unsigned char *)dst)[dstp]	= '.';

			dstp++;

			nptrs	= 0;

			continue;
		case 0x01:	/* RESERVED */
			goto reserved;
		case 0x02:	/* RESERVED */
			goto reserved;
		case 0x03:	/* POINTER */
			if (++nptrs > DNS_D_MAXPTRS)
				goto toolong;

			if (P->end - src < 2)
				goto toolong;

			src	= ((0x3f & P->data[src + 0]) << 8)
				| ((0xff & P->data[src + 1]) << 0);

			continue;
		} /* switch() */
	} /* while() */

toolong:
	*error	= DNS_EILLEGAL;

	if (lim > 0)
		((unsigned char *)dst)[DNS_PP_MIN(dstp, lim - 1)]	= '\0';

	return 0;
reserved:
	*error	= DNS_EILLEGAL;

	if (lim > 0)
		((unsigned char *)dst)[DNS_PP_MIN(dstp, lim - 1)]	= '\0';

	return 0;
} /* dns_d_expand() */


int dns_d_push(struct dns_packet *P, const void *dn, size_t len) {
	size_t lim	= P->size - P->end;
	unsigned dp	= P->end;
	int error	= DNS_EILLEGAL; /* silence compiler */

	len	= dns_d_comp(&P->data[dp], lim, dn, len, P, &error);

	if (len == 0)
		return error;
	if (len > lim)
		return DNS_ENOBUFS;

	P->end	+= len;

	dns_p_dictadd(P, dp);

	return 0;
} /* dns_d_push() */


size_t dns_d_cname(void *dst, size_t lim, const void *dn, size_t len, struct dns_packet *P, int *error_) {
	char host[DNS_D_MAXNAME + 1];
	struct dns_rr_i i;
	struct dns_rr rr;
	unsigned depth;
	int error;

	if (sizeof host <= dns_d_anchor(host, sizeof host, dn, len))
		{ error = ENAMETOOLONG; goto error; }

	for (depth = 0; depth < 7; depth++) {
		dns_rr_i_init(memset(&i, 0, sizeof i), P);

		i.section	= DNS_S_ALL & ~DNS_S_QD;
		i.name		= host;
		i.type		= DNS_T_CNAME;

		if (!dns_rr_grep(&rr, 1, &i, P, &error))
			break;

		if ((error = dns_cname_parse((struct dns_cname *)host, &rr, P)))
			goto error;
	}

	return dns_strlcpy(dst, host, lim);
error:
	*error_	= error;

	return 0;
} /* dns_d_cname() */


/*
 * R E S O U R C E  R E C O R D  R O U T I N E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int dns_rr_copy(struct dns_packet *P, struct dns_rr *rr, struct dns_packet *Q) {
	unsigned char dn[DNS_D_MAXNAME + 1];
	union dns_any any;
	size_t len;
	int error;

	if (!(len = dns_d_expand(dn, sizeof dn, rr->dn.p, Q, &error)))
		return error;
	else if (len >= sizeof dn)
		return DNS_EILLEGAL;

	if (rr->section != DNS_S_QD && (error = dns_any_parse(dns_any_init(&any, sizeof any), rr, Q)))
		return error;

	return dns_p_push(P, rr->section, dn, len, rr->type, rr->class, rr->ttl, &any);
} /* dns_rr_copy() */


int dns_rr_parse(struct dns_rr *rr, unsigned short src, struct dns_packet *P) {
	unsigned short p	= src;

	if (src >= P->end)
		goto invalid;

	rr->dn.p	= p;
	rr->dn.len	= (p = dns_d_skip(p, P)) - rr->dn.p;

	if (P->end - p < 4)
		goto invalid;

	rr->type	= ((0xff & P->data[p + 0]) << 8)
			| ((0xff & P->data[p + 1]) << 0);

	rr->class	= ((0xff & P->data[p + 2]) << 8)
			| ((0xff & P->data[p + 3]) << 0);

	p	+= 4;

	if (src < dns_p_qend(P)) {
		rr->section	= DNS_S_QUESTION;

		rr->ttl		= 0;
		rr->rd.p	= 0;
		rr->rd.len	= 0;

		return 0;
	}

	if (P->end - p < 4)
		goto invalid;

	rr->ttl		= ((0x7f & P->data[p + 0]) << 24)
			| ((0xff & P->data[p + 1]) << 16)
			| ((0xff & P->data[p + 2]) << 8)
			| ((0xff & P->data[p + 3]) << 0);

	p	+= 4;

	if (P->end - p < 2)
		goto invalid;

	rr->rd.len	= ((0xff & P->data[p + 0]) << 8)
			| ((0xff & P->data[p + 1]) << 0);
	rr->rd.p	= p + 2;

	p	+= 2;

	if (P->end - p < rr->rd.len)
		goto invalid;

	return 0;
invalid:
	return DNS_EILLEGAL;
} /* dns_rr_parse() */


static unsigned short dns_rr_len(const unsigned short src, struct dns_packet *P) {
	unsigned short rp, rdlen;

	rp	= dns_d_skip(src, P);

	if (P->end - rp < 4)
		return P->end - src;

	rp	+= 4;	/* TYPE, CLASS */

	if (rp <= dns_p_qend(P))
		return rp - src;

	if (P->end - rp < 6)
		return P->end - src;

	rp	+= 6;	/* TTL, RDLEN */

	rdlen	= ((0xff & P->data[rp - 2]) << 8)
		| ((0xff & P->data[rp - 1]) << 0);

	if (P->end - rp < rdlen)
		return P->end - src;

	rp	+= rdlen;

	return rp - src;
} /* dns_rr_len() */


unsigned short dns_rr_skip(unsigned short src, struct dns_packet *P) {
	return src + dns_rr_len(src, P);
} /* dns_rr_skip() */


static enum dns_section dns_rr_section(unsigned short src, struct dns_packet *P) {
	enum dns_section section;
	unsigned count, index;
	unsigned short rp;

	if (src >= P->qd.base && src < P->qd.end)
		return DNS_S_QD;
	if (src >= P->an.base && src < P->an.end)
		return DNS_S_AN;
	if (src >= P->ns.base && src < P->ns.end)
		return DNS_S_NS;
	if (src >= P->ar.base && src < P->ar.end)
		return DNS_S_AR;

	/* NOTE: Possibly bad memoization. Try it the hard-way. */

	for (rp = 12, index = 0; rp < src && rp < P->end; index++)
		rp = dns_rr_skip(rp, P);

	section = DNS_S_QD;
	count   = dns_p_count(P, section);

	while (index >= count && section <= DNS_S_AR) {
		section <<= 1;
		count += dns_p_count(P, section);
	}

	return DNS_S_ALL & section;
} /* dns_rr_section() */


static enum dns_type dns_rr_type(unsigned short src, struct dns_packet *P) {
	struct dns_rr rr;
	int error;

	if ((error = dns_rr_parse(&rr, src, P)))
		return 0;

	return rr.type;
} /* dns_rr_type() */


int dns_rr_cmp(struct dns_rr *r0, struct dns_packet *P0, struct dns_rr *r1, struct dns_packet *P1) {
	char host0[DNS_D_MAXNAME + 1], host1[DNS_D_MAXNAME + 1];
	union dns_any any0, any1;
	int cmp, error;
	size_t len;

	if ((cmp = r0->type - r1->type))
		return cmp;

	if ((cmp = r0->class - r1->class))
		return cmp;

	/*
	 * FIXME: Do label-by-label comparison to handle illegally long names?
	 */

	if (!(len = dns_d_expand(host0, sizeof host0, r0->dn.p, P0, &error))
	||  len >= sizeof host0)
		return -1;

	if (!(len = dns_d_expand(host1, sizeof host1, r1->dn.p, P1, &error))
	||  len >= sizeof host1)
		return 1;

	if ((cmp = strcasecmp(host0, host1)))
		return cmp;

	if (DNS_S_QD & (r0->section | r1->section)) {
		if (r0->section == r1->section)
			return 0;

		return (r0->section == DNS_S_QD)? -1 : 1;
	}

	if ((error = dns_any_parse(&any0, r0, P0)))
		return -1;

	if ((error = dns_any_parse(&any1, r1, P1)))
		return 1;

	return dns_any_cmp(&any0, r0->type, &any1, r1->type);
} /* dns_rr_cmp() */


static _Bool dns_rr_exists(struct dns_rr *rr0, struct dns_packet *P0, struct dns_packet *P1) {
	struct dns_rr rr1;

	dns_rr_foreach(&rr1, P1, .section = rr0->section, .type = rr0->type) {
		if (0 == dns_rr_cmp(rr0, P0, &rr1, P1))
			return 1;
	}

	return 0;
} /* dns_rr_exists() */


static unsigned short dns_rr_offset(struct dns_rr *rr) {
	return rr->dn.p;
} /* dns_rr_offset() */


static _Bool dns_rr_i_match(struct dns_rr *rr, struct dns_rr_i *i, struct dns_packet *P) {
	if (i->section && !(rr->section & i->section))
		return 0;

	if (i->type && rr->type != i->type && i->type != DNS_T_ALL)
		return 0;

	if (i->class && rr->class != i->class && i->class != DNS_C_ANY)
		return 0;

	if (i->name) {
		char dn[DNS_D_MAXNAME + 1];
		size_t len;
		int error;

		if (!(len = dns_d_expand(dn, sizeof dn, rr->dn.p, P, &error))
		||  len >= sizeof dn)
			return 0;

		if (0 != strcasecmp(dn, i->name))
			return 0;
	}

	if (i->data && i->type && rr->section > DNS_S_QD) {
		union dns_any rd;
		int error;

		if ((error = dns_any_parse(&rd, rr, P)))
			return 0;

		if (0 != dns_any_cmp(&rd, rr->type, i->data, i->type))
			return 0;
	}

	return 1;
} /* dns_rr_i_match() */


static unsigned short dns_rr_i_start(struct dns_rr_i *i, struct dns_packet *P) {
	unsigned short rp;
	struct dns_rr r0, rr;
	int error;

	if ((i->section & DNS_S_QD) && P->qd.base)
		rp = P->qd.base;
	else if ((i->section & DNS_S_AN) && P->an.base)
		rp = P->an.base;
	else if ((i->section & DNS_S_NS) && P->ns.base)
		rp = P->ns.base;
	else if ((i->section & DNS_S_AR) && P->ar.base)
		rp = P->ar.base;
	else
		rp = 12;

	for (; rp < P->end; rp = dns_rr_skip(rp, P)) {
		if ((error = dns_rr_parse(&rr, rp, P)))
			continue;

		rr.section = dns_rr_section(rp, P);

		if (!dns_rr_i_match(&rr, i, P))
			continue;

		r0 = rr;

		goto lower;
	}

	return P->end;
lower:
	if (i->sort == &dns_rr_i_packet)
		return dns_rr_offset(&r0);

	while ((rp = dns_rr_skip(rp, P)) < P->end) {
		if ((error = dns_rr_parse(&rr, rp, P)))
			continue;

		rr.section = dns_rr_section(rp, P);

		if (!dns_rr_i_match(&rr, i, P))
			continue;

		if (i->sort(&rr, &r0, i, P) < 0)
			r0 = rr;
	}

	return dns_rr_offset(&r0);
} /* dns_rr_i_start() */


static unsigned short dns_rr_i_skip(unsigned short rp, struct dns_rr_i *i, struct dns_packet *P) {
	struct dns_rr r0, r1, rr;
	int error;

	if ((error = dns_rr_parse(&r0, rp, P)))
		return P->end;

	r0.section = dns_rr_section(rp, P);

	rp = (i->sort == &dns_rr_i_packet)? dns_rr_skip(rp, P) : 12;

	for (; rp < P->end; rp = dns_rr_skip(rp, P)) {
		if ((error = dns_rr_parse(&rr, rp, P)))
			continue;

		rr.section = dns_rr_section(rp, P);

		if (!dns_rr_i_match(&rr, i, P))
			continue;

		if (i->sort(&rr, &r0, i, P) <= 0)
			continue;

		r1 = rr;

		goto lower;
	}

	return P->end;
lower:
	if (i->sort == &dns_rr_i_packet)
		return dns_rr_offset(&r1);

	while ((rp = dns_rr_skip(rp, P)) < P->end) {
		if ((error = dns_rr_parse(&rr, rp, P)))
			continue;

		rr.section = dns_rr_section(rp, P);

		if (!dns_rr_i_match(&rr, i, P))
			continue;

		if (i->sort(&rr, &r0, i, P) <= 0)
			continue;

		if (i->sort(&rr, &r1, i, P) >= 0)
			continue;

		r1 = rr;
	}

	return dns_rr_offset(&r1);
} /* dns_rr_i_skip() */


int dns_rr_i_packet(struct dns_rr *a, struct dns_rr *b, struct dns_rr_i *i, struct dns_packet *P) {
	(void)i;
	(void)P;

	return (int)a->dn.p - (int)b->dn.p;
} /* dns_rr_i_packet() */


int dns_rr_i_order(struct dns_rr *a, struct dns_rr *b, struct dns_rr_i *i, struct dns_packet *P) {
	int cmp;

	(void)i;

	if ((cmp = a->section - b->section))
		return cmp;

	if (a->type != b->type)
		return (int)a->dn.p - (int)b->dn.p;

	return dns_rr_cmp(a, P, b, P);
} /* dns_rr_i_order() */


int dns_rr_i_shuffle(struct dns_rr *a, struct dns_rr *b, struct dns_rr_i *i, struct dns_packet *P) {
	int cmp;

	(void)i;
	(void)P;

	while (!i->state.regs[0])
		i->state.regs[0]	= dns_random();

	if ((cmp = a->section - b->section))
		return cmp;

	return dns_k_shuffle16(a->dn.p, i->state.regs[0]) - dns_k_shuffle16(b->dn.p, i->state.regs[0]);
} /* dns_rr_i_shuffle() */


struct dns_rr_i *dns_rr_i_init(struct dns_rr_i *i, struct dns_packet *P) {
	static const struct dns_rr_i i_initializer;

	(void)P;

	i->state	= i_initializer.state;
	i->saved	= i->state;

	return i;
} /* dns_rr_i_init() */


unsigned dns_rr_grep(struct dns_rr *rr, unsigned lim, struct dns_rr_i *i, struct dns_packet *P, int *error_) {
	unsigned count	= 0;
	int error;

	switch (i->state.exec) {
	case 0:
		if (!i->sort)
			i->sort	= &dns_rr_i_packet;

		i->state.next	= dns_rr_i_start(i, P);
		i->state.exec++;

		/* FALL THROUGH */
	case 1:
		while (count < lim && i->state.next < P->end) {
			if ((error = dns_rr_parse(rr, i->state.next, P)))
				goto error;

			rr->section	= dns_rr_section(i->state.next, P);

			rr++;
			count++;
			i->state.count++;

			i->state.next	= dns_rr_i_skip(i->state.next, i, P);
		} /* while() */

		break;
	} /* switch() */

	return count;
error:
	*error_	= error;

	return count;
} /* dns_rr_grep() */


static size_t dns__printchar(void *dst, size_t lim, size_t cp, unsigned char ch) {
	if (cp < lim)
		((unsigned char *)dst)[cp]	= ch;

	return 1;
} /* dns__printchar() */


static size_t dns__printstring(void *dst, size_t lim, size_t cp, const void *src, size_t len) {
	if (cp < lim)
		memcpy(&((unsigned char *)dst)[cp], src, DNS_PP_MIN(len, lim - cp));

	return len;
} /* dns__printstring() */

#define dns__printstring5(a, b, c, d, e)	dns__printstring((a), (b), (c), (d), (e))
#define dns__printstring4(a, b, c, d)		dns__printstring((a), (b), (c), (d), strlen((d)))
#define dns__printstring(...)			DNS_PP_CALL(DNS_PP_XPASTE(dns__printstring, DNS_PP_NARG(__VA_ARGS__)), __VA_ARGS__)


static void dns__printnul(void *dst, size_t lim, size_t off) {
	if (lim > 0)
		((unsigned char *)dst)[DNS_PP_MIN(off, lim - 1)]	= '\0';
} /* dns__printnul() */


static size_t dns__print10(void *dst, size_t lim, size_t off, unsigned n, unsigned pad) {
	unsigned char tmp[32];
	unsigned dp	= off;
	unsigned cp	= 0;
	unsigned d	= 1000000000;
	unsigned ch;

	pad	= DNS_PP_MAX(1, pad);

	while (d) {
		if ((ch = n / d) || cp > 0) {
			n	-= ch * d;

			tmp[cp]	= '0' + ch;

			cp++;
		}

		d	/= 10;
	}

	while (cp < pad) {
		dp	+= dns__printchar(dst, lim, dp, '0');
		pad--;
	}

	dp	+= dns__printstring(dst, lim, dp, tmp, cp);

	return dp - off;
} /* dns__print10() */


size_t dns_rr_print(void *dst, size_t lim, struct dns_rr *rr, struct dns_packet *P, int *error_) {
	union dns_any any;
	size_t cp, n, rdlen;
	void *rd;
	int error;

	cp	= 0;

	if (rr->section == DNS_S_QD)
		cp	+= dns__printchar(dst, lim, cp, ';');

	if (!(n = dns_d_expand(&((unsigned char *)dst)[cp], (cp < lim)? lim - cp : 0, rr->dn.p, P, &error)))
		goto error;

	cp	+= n;

	if (rr->section != DNS_S_QD) {
		cp	+= dns__printchar(dst, lim, cp, ' ');
		cp	+= dns__print10(dst, lim, cp, rr->ttl, 0);
	}

	cp	+= dns__printchar(dst, lim, cp, ' ');
	cp	+= dns__printstring(dst, lim, cp, dns_strclass(rr->class), strlen(dns_strclass(rr->class)));
	cp	+= dns__printchar(dst, lim, cp, ' ');
	cp	+= dns__printstring(dst, lim, cp, dns_strtype(rr->type), strlen(dns_strtype(rr->type)));

	if (rr->section == DNS_S_QD)
		goto epilog;

	cp	+= dns__printchar(dst, lim, cp, ' ');

	if ((error = dns_any_parse(dns_any_init(&any, sizeof any), rr, P)))
		goto error;

	if (cp < lim) {
		rd	= &((unsigned char *)dst)[cp];
		rdlen	= lim - cp;
	} else {
		rd	= 0;
		rdlen	= 0;
	}

	cp	+= dns_any_print(rd, rdlen, &any, rr->type);

epilog:
	dns__printnul(dst, lim, cp);

	return cp;
error:
	*error_	= error;

	return 0;
} /* dns_rr_print() */


int dns_a_parse(struct dns_a *a, struct dns_rr *rr, struct dns_packet *P) {
	unsigned long addr;

	if (rr->rd.len != 4)
		return DNS_EILLEGAL;

	addr	= ((0xff & P->data[rr->rd.p + 0]) << 24)
		| ((0xff & P->data[rr->rd.p + 1]) << 16)
		| ((0xff & P->data[rr->rd.p + 2]) << 8)
		| ((0xff & P->data[rr->rd.p + 3]) << 0);

	a->addr.s_addr	= htonl(addr);

	return 0;
} /* dns_a_parse() */


int dns_a_push(struct dns_packet *P, struct dns_a *a) {
	unsigned long addr;

	if (P->size - P->end < 6)
		return DNS_ENOBUFS;

	P->data[P->end++]	= 0x00;
	P->data[P->end++]	= 0x04;

	addr	= ntohl(a->addr.s_addr);

	P->data[P->end++]	= 0xff & (addr >> 24);
	P->data[P->end++]	= 0xff & (addr >> 16);
	P->data[P->end++]	= 0xff & (addr >> 8);
	P->data[P->end++]	= 0xff & (addr >> 0);

	return 0;
} /* dns_a_push() */


size_t dns_a_arpa(void *dst, size_t lim, const struct dns_a *a) {
	unsigned long a4	= ntohl(a->addr.s_addr);
	size_t cp		= 0;
	unsigned i;

	for (i = 4; i > 0; i--) {
		cp	+= dns__print10(dst, lim, cp, (0xff & a4), 0);
		cp	+= dns__printchar(dst, lim, cp, '.');
		a4	>>= 8;
	}

	cp	+= dns__printstring(dst, lim, cp, "in-addr.arpa.");

	dns__printnul(dst, lim, cp);

	return cp;
} /* dns_a_arpa() */


int dns_a_cmp(const struct dns_a *a, const struct dns_a *b) {
	if (ntohl(a->addr.s_addr) < ntohl(b->addr.s_addr))
		return -1;
	if (ntohl(a->addr.s_addr) > ntohl(b->addr.s_addr))
		return 1;

	return 0;
} /* dns_a_cmp() */


size_t dns_a_print(void *dst, size_t lim, struct dns_a *a) {
	char addr[INET_ADDRSTRLEN + 1]	= "0.0.0.0";
	size_t len;

	dns_inet_ntop(AF_INET, &a->addr, addr, sizeof addr);

	dns__printnul(dst, lim, (len = dns__printstring(dst, lim, 0, addr)));

	return len;
} /* dns_a_print() */


int dns_aaaa_parse(struct dns_aaaa *aaaa, struct dns_rr *rr, struct dns_packet *P) {
	if (rr->rd.len != sizeof aaaa->addr.s6_addr)
		return DNS_EILLEGAL;

	memcpy(aaaa->addr.s6_addr, &P->data[rr->rd.p], sizeof aaaa->addr.s6_addr);

	return 0;
} /* dns_aaaa_parse() */


int dns_aaaa_push(struct dns_packet *P, struct dns_aaaa *aaaa) {
	if (P->size - P->end < 2 + sizeof aaaa->addr.s6_addr)
		return DNS_ENOBUFS;

	P->data[P->end++]	= 0x00;
	P->data[P->end++]	= 0x10;

	memcpy(&P->data[P->end], aaaa->addr.s6_addr, sizeof aaaa->addr.s6_addr);

	P->end	+= sizeof aaaa->addr.s6_addr;

	return 0;
} /* dns_aaaa_push() */


int dns_aaaa_cmp(const struct dns_aaaa *a, const struct dns_aaaa *b) {
	unsigned i;
	int cmp;

	for (i = 0; i < lengthof(a->addr.s6_addr); i++) {
		if ((cmp = (a->addr.s6_addr[i] - b->addr.s6_addr[i])))
			return cmp;
	}

	return 0;
} /* dns_aaaa_cmp() */


size_t dns_aaaa_arpa(void *dst, size_t lim, const struct dns_aaaa *aaaa) {
	static const unsigned char hex[16]	= "0123456789abcdef";
	size_t cp		= 0;
	unsigned nyble;
	int i, j;

	for (i = sizeof aaaa->addr.s6_addr - 1; i >= 0; i--) {
		nyble	= aaaa->addr.s6_addr[i];

		for (j = 0; j < 2; j++) {
			cp	+= dns__printchar(dst, lim, cp, hex[0x0f & nyble]);
			cp	+= dns__printchar(dst, lim, cp, '.');
			nyble	>>= 4;
		}
	}

	cp	+= dns__printstring(dst, lim, cp, "ip6.arpa.");

	dns__printnul(dst, lim, cp);

	return cp;
} /* dns_aaaa_arpa() */


size_t dns_aaaa_print(void *dst, size_t lim, struct dns_aaaa *aaaa) {
	char addr[INET6_ADDRSTRLEN + 1]	= "::";
	size_t len;

	dns_inet_ntop(AF_INET6, &aaaa->addr, addr, sizeof addr);

	dns__printnul(dst, lim, (len = dns__printstring(dst, lim, 0, addr)));

	return len;
} /* dns_aaaa_print() */


int dns_mx_parse(struct dns_mx *mx, struct dns_rr *rr, struct dns_packet *P) {
	size_t len;
	int error;

	if (rr->rd.len < 3)
		return DNS_EILLEGAL;

	mx->preference	= (0xff00 & (P->data[rr->rd.p + 0] << 8))
			| (0x00ff & (P->data[rr->rd.p + 1] << 0));

	if (!(len = dns_d_expand(mx->host, sizeof mx->host, rr->rd.p + 2, P, &error)))
		return error;
	else if (len >= sizeof mx->host)
		return DNS_EILLEGAL;

	return 0;
} /* dns_mx_parse() */


int dns_mx_push(struct dns_packet *P, struct dns_mx *mx) {
	size_t end, len;
	int error;

	if (P->size - P->end < 5)
		return DNS_ENOBUFS;

	end	= P->end;
	P->end	+= 2;

	P->data[P->end++]	= 0xff & (mx->preference >> 8);
	P->data[P->end++]	= 0xff & (mx->preference >> 0);

	if ((error = dns_d_push(P, mx->host, strlen(mx->host))))
		goto error;

	len	= P->end - end - 2;

	P->data[end + 0]	= 0xff & (len >> 8);
	P->data[end + 1]	= 0xff & (len >> 0);

	return 0;
error:
	P->end	= end;

	return error;
} /* dns_mx_push() */


int dns_mx_cmp(const struct dns_mx *a, const struct dns_mx *b) {
	int cmp;

	if ((cmp = a->preference - b->preference))
		return cmp;

	return strcasecmp(a->host, b->host);
} /* dns_mx_cmp() */


size_t dns_mx_print(void *dst, size_t lim, struct dns_mx *mx) {
	size_t cp	= 0;

	cp	+= dns__print10(dst, lim, cp, mx->preference, 0);
	cp	+= dns__printchar(dst, lim, cp, ' ');
	cp	+= dns__printstring(dst, lim, cp, mx->host, strlen(mx->host));

	dns__printnul(dst, lim, cp);

	return cp;
} /* dns_mx_print() */


size_t dns_mx_cname(void *dst, size_t lim, struct dns_mx *mx) {
	return dns_strlcpy(dst, mx->host, lim);
} /* dns_mx_cname() */


int dns_ns_parse(struct dns_ns *ns, struct dns_rr *rr, struct dns_packet *P) {
	size_t len;
	int error;

	if (!(len = dns_d_expand(ns->host, sizeof ns->host, rr->rd.p, P, &error)))
		return error;
	else if (len >= sizeof ns->host)
		return DNS_EILLEGAL;

	return 0;
} /* dns_ns_parse() */


int dns_ns_push(struct dns_packet *P, struct dns_ns *ns) {
	size_t end, len;
	int error;

	if (P->size - P->end < 3)
		return DNS_ENOBUFS;

	end	= P->end;
	P->end	+= 2;

	if ((error = dns_d_push(P, ns->host, strlen(ns->host))))
		goto error;

	len	= P->end - end - 2;

	P->data[end + 0]	= 0xff & (len >> 8);
	P->data[end + 1]	= 0xff & (len >> 0);

	return 0;
error:
	P->end	= end;

	return error;
} /* dns_ns_push() */


int dns_ns_cmp(const struct dns_ns *a, const struct dns_ns *b) {
	return strcasecmp(a->host, b->host);
} /* dns_ns_cmp() */


size_t dns_ns_print(void *dst, size_t lim, struct dns_ns *ns) {
	size_t cp;

	cp	= dns__printstring(dst, lim, 0, ns->host, strlen(ns->host));

	dns__printnul(dst, lim, cp);

	return cp;
} /* dns_ns_print() */


size_t dns_ns_cname(void *dst, size_t lim, struct dns_ns *ns) {
	return dns_strlcpy(dst, ns->host, lim);
} /* dns_ns_cname() */


int dns_cname_parse(struct dns_cname *cname, struct dns_rr *rr, struct dns_packet *P) {
	return dns_ns_parse((struct dns_ns *)cname, rr, P);
} /* dns_cname_parse() */


int dns_cname_push(struct dns_packet *P, struct dns_cname *cname) {
	return dns_ns_push(P, (struct dns_ns *)cname);
} /* dns_cname_push() */


int dns_cname_cmp(const struct dns_cname *a, const struct dns_cname *b) {
	return strcasecmp(a->host, b->host);
} /* dns_cname_cmp() */


size_t dns_cname_print(void *dst, size_t lim, struct dns_cname *cname) {
	return dns_ns_print(dst, lim, (struct dns_ns *)cname);
} /* dns_cname_print() */


size_t dns_cname_cname(void *dst, size_t lim, struct dns_cname *cname) {
	return dns_strlcpy(dst, cname->host, lim);
} /* dns_cname_cname() */


int dns_soa_parse(struct dns_soa *soa, struct dns_rr *rr, struct dns_packet *P) {
	struct { void *dst; size_t lim; } dn[] =
		{ { soa->mname, sizeof soa->mname },
		  { soa->rname, sizeof soa->rname } };
	unsigned *ts[] =
		{ &soa->serial, &soa->refresh, &soa->retry, &soa->expire, &soa->minimum };
	unsigned short rp;
	unsigned i, j, n;
	int error;

	/* MNAME / RNAME */
	if ((rp = rr->rd.p) >= P->end)
		return DNS_EILLEGAL;

	for (i = 0; i < lengthof(dn); i++) {
		if (!(n = dns_d_expand(dn[i].dst, dn[i].lim, rp, P, &error)))
			return error;
		else if (n >= dn[i].lim)
			return DNS_EILLEGAL;

		if ((rp = dns_d_skip(rp, P)) >= P->end)
			return DNS_EILLEGAL;
	}

	/* SERIAL / REFRESH / RETRY / EXPIRE / MINIMUM */
	for (i = 0; i < lengthof(ts); i++) {
		for (j = 0; j < 4; j++, rp++) {
			if (rp >= P->end)
				return DNS_EILLEGAL;

			*ts[i]	<<= 8;
			*ts[i]	|= (0xff & P->data[rp]);
		}
	}

	return 0;
} /* dns_soa_parse() */


int dns_soa_push(struct dns_packet *P, struct dns_soa *soa) {
	void *dn[]	= { soa->mname, soa->rname };
	unsigned ts[]	= { (0xffffffff & soa->serial),
			    (0x7fffffff & soa->refresh),
			    (0x7fffffff & soa->retry),
			    (0x7fffffff & soa->expire),
			    (0xffffffff & soa->minimum) };
	unsigned i, j;
	size_t end, len;
	int error;

	end	= P->end;

	if ((P->end += 2) >= P->size)
		goto toolong;

	/* MNAME / RNAME */
	for (i = 0; i < lengthof(dn); i++) {
		if ((error = dns_d_push(P, dn[i], strlen(dn[i]))))
			goto error;
	}

	/* SERIAL / REFRESH / RETRY / EXPIRE / MINIMUM */
	for (i = 0; i < lengthof(ts); i++) {
		if ((P->end += 4) >= P->size)
			goto toolong;

		for (j = 1; j <= 4; j++) {
			P->data[P->end - j]	= (0xff & ts[i]);
			ts[i]			>>= 8;
		}
	}

	len			= P->end - end - 2;
	P->data[end + 0]	= (0xff & (len >> 8));
	P->data[end + 1]	= (0xff & (len >> 0));

	return 0;
toolong:
	error	= DNS_ENOBUFS;

	/* FALL THROUGH */
error:
	P->end	= end;

	return error;
} /* dns_soa_push() */


int dns_soa_cmp(const struct dns_soa *a, const struct dns_soa *b) {
	int cmp;

	if ((cmp = strcasecmp(a->mname, b->mname)))
		return cmp;

	if ((cmp = strcasecmp(a->rname, b->rname)))
		return cmp;

	if (a->serial > b->serial)
		return -1;
	else if (a->serial < b->serial)
		return 1;

	if (a->refresh > b->refresh)
		return -1;
	else if (a->refresh < b->refresh)
		return 1;

	if (a->retry > b->retry)
		return -1;
	else if (a->retry < b->retry)
		return 1;

	if (a->expire > b->expire)
		return -1;
	else if (a->expire < b->expire)
		return 1;

	if (a->minimum > b->minimum)
		return -1;
	else if (a->minimum < b->minimum)
		return 1;

	return 0;
} /* dns_soa_cmp() */


size_t dns_soa_print(void *dst, size_t lim, struct dns_soa *soa) {
	size_t cp	= 0;

	cp	+= dns__printstring(dst, lim, cp, soa->mname, strlen(soa->mname));
	cp	+= dns__printchar(dst, lim, cp, ' ');
	cp	+= dns__printstring(dst, lim, cp, soa->rname, strlen(soa->rname));
	cp	+= dns__printchar(dst, lim, cp, ' ');
	cp	+= dns__print10(dst, lim, cp, soa->serial, 0);
	cp	+= dns__printchar(dst, lim, cp, ' ');
	cp	+= dns__print10(dst, lim, cp, soa->refresh, 0);
	cp	+= dns__printchar(dst, lim, cp, ' ');
	cp	+= dns__print10(dst, lim, cp, soa->retry, 0);
	cp	+= dns__printchar(dst, lim, cp, ' ');
	cp	+= dns__print10(dst, lim, cp, soa->expire, 0);
	cp	+= dns__printchar(dst, lim, cp, ' ');
	cp	+= dns__print10(dst, lim, cp, soa->minimum, 0);

	dns__printnul(dst, lim, cp);

	return cp;
} /* dns_soa_print() */


int dns_srv_parse(struct dns_srv *srv, struct dns_rr *rr, struct dns_packet *P) {
	unsigned short rp;
	unsigned i;
	size_t n;
	int error;

	memset(srv, '\0', sizeof *srv);

	rp	= rr->rd.p;

	if (rr->rd.len < 7)
		return DNS_EILLEGAL;

	for (i = 0; i < 2; i++, rp++) {
		srv->priority	<<= 8;
		srv->priority	|= (0xff & P->data[rp]);
	}

	for (i = 0; i < 2; i++, rp++) {
		srv->weight	<<= 8;
		srv->weight	|= (0xff & P->data[rp]);
	}

	for (i = 0; i < 2; i++, rp++) {
		srv->port	<<= 8;
		srv->port	|= (0xff & P->data[rp]);
	}

	if (!(n = dns_d_expand(srv->target, sizeof srv->target, rp, P, &error)))
		return error;
	else if (n >= sizeof srv->target)
		return DNS_EILLEGAL;

	return 0;
} /* dns_srv_parse() */


int dns_srv_push(struct dns_packet *P, struct dns_srv *srv) {
	size_t end, len;
	int error;

	end	= P->end;

	if (P->size - P->end < 2)
		goto toolong;

	P->end	+= 2;

	if (P->size - P->end < 6)
		goto toolong;

	P->data[P->end++]	= 0xff & (srv->priority >> 8);
	P->data[P->end++]	= 0xff & (srv->priority >> 0);

	P->data[P->end++]	= 0xff & (srv->weight >> 8);
	P->data[P->end++]	= 0xff & (srv->weight >> 0);

	P->data[P->end++]	= 0xff & (srv->port >> 8);
	P->data[P->end++]	= 0xff & (srv->port >> 0);

	if (0 == (len = dns_d_comp(&P->data[P->end], P->size - P->end, srv->target, strlen(srv->target), P, &error)))
		goto error;
	else if (P->size - P->end < len)
		goto toolong;

	P->end	+= len;

	if (P->end > 65535)
		goto toolong;

	len	= P->end - end - 2;

	P->data[end + 0]	= 0xff & (len >> 8);
	P->data[end + 1]	= 0xff & (len >> 0);

	return 0;
toolong:
	error	= DNS_ENOBUFS;

	/* FALL THROUGH */
error:
	P->end	= end;

	return error;
} /* dns_srv_push() */


int dns_srv_cmp(const struct dns_srv *a, const struct dns_srv *b) {
	int cmp;

	if ((cmp = a->priority - b->priority))
		return cmp;

	/*
	 * FIXME: We need some sort of random seed to implement the dynamic
	 * weighting required by RFC 2782.
	 */
	if ((cmp = a->weight - b->weight))
		return cmp;

	if ((cmp = a->port - b->port))
		return cmp;

	return strcasecmp(a->target, b->target);
} /* dns_srv_cmp() */


size_t dns_srv_print(void *dst, size_t lim, struct dns_srv *srv) {
	size_t cp	= 0;

	cp	+= dns__print10(dst, lim, cp, srv->priority, 0);
	cp	+= dns__printchar(dst, lim, cp, ' ');
	cp	+= dns__print10(dst, lim, cp, srv->weight, 0);
	cp	+= dns__printchar(dst, lim, cp, ' ');
	cp	+= dns__print10(dst, lim, cp, srv->port, 0);
	cp	+= dns__printchar(dst, lim, cp, ' ');
	cp	+= dns__printstring(dst, lim, cp, srv->target, strlen(srv->target));

	dns__printnul(dst, lim, cp);

	return cp;
} /* dns_srv_print() */


size_t dns_srv_cname(void *dst, size_t lim, struct dns_srv *srv) {
	return dns_strlcpy(dst, srv->target, lim);
} /* dns_srv_cname() */


unsigned int dns_opt_ttl(const struct dns_opt *opt) {
	unsigned int ttl = 0;

	ttl |= (0xffU & opt->rcode) << 24U;
	ttl |= (0xffU & opt->version) << 16U;

	return ttl;
} /* dns_opt_ttl() */


unsigned short dns_opt_class(const struct dns_opt *opt) {
	return opt->maxsize;
} /* dns_opt_class() */


struct dns_opt *dns_opt_init(struct dns_opt *opt, size_t size) {
	assert(size >= offsetof(struct dns_opt, data));

	opt->size = size - offsetof(struct dns_opt, data);
	opt->len  = 0;

	opt->rcode   = 0;
	opt->version = 0;
	opt->maxsize = 512;

	return opt;
} /* dns_opt_init() */


int dns_opt_parse(struct dns_opt *opt, struct dns_rr *rr, struct dns_packet *P) {
	(void)rr;
	(void)P;

	opt->len = 0;

	return 0;
} /* dns_opt_parse() */


int dns_opt_push(struct dns_packet *P, struct dns_opt *opt) {
	(void)P;
	(void)opt;

	return 0;
} /* dns_opt_push() */


int dns_opt_cmp(const struct dns_opt *a, const struct dns_opt *b) {
	(void)a;
	(void)b;

	return 0;
} /* dns_opt_cmp() */


size_t dns_opt_print(void *dst, size_t lim, struct dns_opt *opt) {
	size_t p = 0, src;

	p += dns__printchar(dst, lim, p, '"');

	for (src = 0; src < opt->len; src++) {
		p += dns__printchar(dst, lim, p, '\\');
		p += dns__print10(dst, lim, p, opt->data[src], 3);
	}

	p += dns__printchar(dst, lim, p, '"');

	dns__printnul(dst, lim, p);

	return p;
} /* dns_opt_print() */


int dns_ptr_parse(struct dns_ptr *ptr, struct dns_rr *rr, struct dns_packet *P) {
	return dns_ns_parse((struct dns_ns *)ptr, rr, P);
} /* dns_ptr_parse() */


int dns_ptr_push(struct dns_packet *P, struct dns_ptr *ptr) {
	return dns_ns_push(P, (struct dns_ns *)ptr);
} /* dns_ptr_push() */


size_t dns_ptr_qname(void *dst, size_t lim, int af, void *addr) {
	unsigned len	= (af == AF_INET6)
			? dns_aaaa_arpa(dst, lim, addr)
			: dns_a_arpa(dst, lim, addr);

	dns__printnul(dst, lim, len);

	return len;
} /* dns_ptr_qname() */


int dns_ptr_cmp(const struct dns_ptr *a, const struct dns_ptr *b) {
	return strcasecmp(a->host, b->host);
} /* dns_ptr_cmp() */


size_t dns_ptr_print(void *dst, size_t lim, struct dns_ptr *ptr) {
	return dns_ns_print(dst, lim, (struct dns_ns *)ptr);
} /* dns_ptr_print() */


size_t dns_ptr_cname(void *dst, size_t lim, struct dns_ptr *ptr) {
	return dns_strlcpy(dst, ptr->host, lim);
} /* dns_ptr_cname() */


int dns_sshfp_parse(struct dns_sshfp *fp, struct dns_rr *rr, struct dns_packet *P) {
	unsigned p = rr->rd.p, pe = rr->rd.p + rr->rd.len;

	if (pe - p < 2)
		return DNS_EILLEGAL;

	fp->algo = P->data[p++];
	fp->type = P->data[p++];

	switch (fp->type) {
	case DNS_SSHFP_SHA1:
		if (pe - p < sizeof fp->digest.sha1)
			return DNS_EILLEGAL;

		memcpy(fp->digest.sha1, &P->data[p], sizeof fp->digest.sha1);

		break;
	default:
		break;
	} /* switch() */

	return 0;
} /* dns_sshfp_parse() */


int dns_sshfp_push(struct dns_packet *P, struct dns_sshfp *fp) {
	unsigned p = P->end, pe = P->size, n;

	if (pe - p < 4)
		return DNS_ENOBUFS;

	p += 2;
	P->data[p++] = 0xff & fp->algo;
	P->data[p++] = 0xff & fp->type;

	switch (fp->type) {
	case DNS_SSHFP_SHA1:
		if (pe - p < sizeof fp->digest.sha1)
			return DNS_ENOBUFS;

		memcpy(&P->data[p], fp->digest.sha1, sizeof fp->digest.sha1);
		p += sizeof fp->digest.sha1;

		break;
	default:
		return DNS_EILLEGAL;
	} /* switch() */

	n = p - P->end - 2;
	P->data[P->end++] = 0xff & (n >> 8);
	P->data[P->end++] = 0xff & (n >> 0);
	P->end = p;

	return 0;
} /* dns_sshfp_push() */


int dns_sshfp_cmp(const struct dns_sshfp *a, const struct dns_sshfp *b) {
	int cmp;

	if ((cmp = a->algo - b->algo) || (cmp = a->type - b->type))
		return cmp;

	switch (a->type) {
	case DNS_SSHFP_SHA1:
		return memcmp(a->digest.sha1, b->digest.sha1, sizeof a->digest.sha1);
	default:
		return 0;
	} /* switch() */

	/* NOT REACHED */
} /* dns_sshfp_cmp() */


size_t dns_sshfp_print(void *dst, size_t lim, struct dns_sshfp *fp) {
	static const unsigned char hex[16] = "0123456789abcdef";
	size_t i, p = 0;

	p += dns__print10(dst, lim, p, fp->algo, 0);
	p += dns__printchar(dst, lim, p, ' ');
	p += dns__print10(dst, lim, p, fp->type, 0);
	p += dns__printchar(dst, lim, p, ' ');

	switch (fp->type) {
	case DNS_SSHFP_SHA1:
		for (i = 0; i < sizeof fp->digest.sha1; i++) {
			p += dns__printchar(dst, lim, p, hex[0x0f & (fp->digest.sha1[i] >> 4)]);
			p += dns__printchar(dst, lim, p, hex[0x0f & (fp->digest.sha1[i] >> 0)]);
		}

		break;
	default:
		p += dns__printchar(dst, lim, p, '0');

		break;
	} /* switch() */

	dns__printnul(dst, lim, p);

	return p;
} /* dns_sshfp_print() */


struct dns_txt *dns_txt_init(struct dns_txt *txt, size_t size) {
	assert(size > offsetof(struct dns_txt, data));

	txt->size	= size - offsetof(struct dns_txt, data);
	txt->len	= 0;

	return txt;
} /* dns_txt_init() */


int dns_txt_parse(struct dns_txt *txt, struct dns_rr *rr, struct dns_packet *P) {
	struct { unsigned char *b; size_t p, end; } dst, src;
	unsigned n;

	dst.b	= txt->data;
	dst.p	= 0;
	dst.end	= txt->size;

	src.b	= P->data;
	src.p	= rr->rd.p;
	src.end	= src.p + rr->rd.len;

	while (src.p < src.end) {
		n	= 0xff & P->data[src.p++];

		if (src.end - src.p < n || dst.end - dst.p < n)
			return DNS_EILLEGAL;

		memcpy(&dst.b[dst.p], &src.b[src.p], n);

		dst.p	+= n;
		src.p	+= n;
	}

	txt->len	= dst.p;

	return 0;
} /* dns_txt_parse() */


int dns_txt_push(struct dns_packet *P, struct dns_txt *txt) {
	struct { unsigned char *b; size_t p, end; } dst, src;
	unsigned n;

	dst.b	= P->data;
	dst.p	= P->end;
	dst.end	= P->size;

	src.b	= txt->data;
	src.p	= 0;
	src.end	= txt->len;

	if (dst.end - dst.p < 2)
		return DNS_ENOBUFS;

	n	= txt->len + ((txt->len + 254) / 255);

	dst.b[dst.p++]	= 0xff & (n >> 8);
	dst.b[dst.p++]	= 0xff & (n >> 0);

	while (src.p < src.end) {
		n	= DNS_PP_MIN(255, src.end - src.p);

		if (dst.p >= dst.end)
			return DNS_ENOBUFS;

		dst.b[dst.p++]	= n;

		if (dst.end - dst.p < n)
			return DNS_ENOBUFS;

		memcpy(&dst.b[dst.p], &src.b[src.p], n);

		dst.p	+= n;
		src.p	+= n;
	}

	P->end	= dst.p;

	return 0;
} /* dns_txt_push() */


int dns_txt_cmp(const struct dns_txt *a, const struct dns_txt *b) {
	(void)a;
	(void)b;

	return -1;
} /* dns_txt_cmp() */


size_t dns_txt_print(void *dst_, size_t lim, struct dns_txt *txt) {
	struct { unsigned char *b; size_t p, end; } dst, src;
	unsigned ch;

	dst.b	= dst_;
	dst.end	= lim;
	dst.p	= 0;

	src.b	= txt->data;
	src.end	= txt->len;
	src.p	= 0;

	dst.p	+= dns__printchar(dst.b, dst.end, dst.p, '"');

	while (src.p < src.end) {
		ch	= src.b[src.p];

		if (0 == (src.p++ % 255) && src.p != 1) {
			dst.p	+= dns__printchar(dst.b, dst.end, dst.p, '"');
			dst.p	+= dns__printchar(dst.b, dst.end, dst.p, ' ');
			dst.p	+= dns__printchar(dst.b, dst.end, dst.p, '"');
		}

		if (ch < 32 || ch > 126 || ch == '"' || ch == '\\') {
			dst.p	+= dns__printchar(dst.b, dst.end, dst.p, '\\');
			dst.p	+= dns__print10(dst.b, dst.end, dst.p, ch, 3);
		} else {
			dst.p	+= dns__printchar(dst.b, dst.end, dst.p, ch);
		}
	}

	dst.p	+= dns__printchar(dst.b, dst.end, dst.p, '"');

	dns__printnul(dst.b, dst.end, dst.p);

	return dst.p;
} /* dns_txt_print() */


static const struct {
	enum dns_type type;
	const char *name;
	int (*parse)();
	int (*push)();
	int (*cmp)();
	size_t (*print)();
	size_t (*cname)();
} dns_rrtypes[]	= {
	{ DNS_T_A,      "A",      &dns_a_parse,      &dns_a_push,      &dns_a_cmp,      &dns_a_print,      0                },
	{ DNS_T_AAAA,   "AAAA",   &dns_aaaa_parse,   &dns_aaaa_push,   &dns_aaaa_cmp,   &dns_aaaa_print,   0                },
	{ DNS_T_MX,     "MX",     &dns_mx_parse,     &dns_mx_push,     &dns_mx_cmp,     &dns_mx_print,     &dns_mx_cname    },
	{ DNS_T_NS,     "NS",     &dns_ns_parse,     &dns_ns_push,     &dns_ns_cmp,     &dns_ns_print,     &dns_ns_cname    },
	{ DNS_T_CNAME,  "CNAME",  &dns_cname_parse,  &dns_cname_push,  &dns_cname_cmp,  &dns_cname_print,  &dns_cname_cname },
	{ DNS_T_SOA,    "SOA",    &dns_soa_parse,    &dns_soa_push,    &dns_soa_cmp,    &dns_soa_print,    0                },
	{ DNS_T_SRV,    "SRV",    &dns_srv_parse,    &dns_srv_push,    &dns_srv_cmp,    &dns_srv_print,    &dns_srv_cname   },
	{ DNS_T_OPT,    "OPT",    &dns_opt_parse,    &dns_opt_push,    &dns_opt_cmp,    &dns_opt_print,    0                },
	{ DNS_T_PTR,    "PTR",    &dns_ptr_parse,    &dns_ptr_push,    &dns_ptr_cmp,    &dns_ptr_print,    &dns_ptr_cname   },
	{ DNS_T_TXT,    "TXT",    &dns_txt_parse,    &dns_txt_push,    &dns_txt_cmp,    &dns_txt_print,    0                },
	{ DNS_T_SPF,    "SPF",    &dns_txt_parse,    &dns_txt_push,    &dns_txt_cmp,    &dns_txt_print,    0                },
	{ DNS_T_SSHFP,  "SSHFP",  &dns_sshfp_parse,  &dns_sshfp_push,  &dns_sshfp_cmp,  &dns_sshfp_print,  0                },
}; /* dns_rrtypes[] */


union dns_any *dns_any_init(union dns_any *any, size_t size) {
	return (union dns_any *)dns_txt_init(&any->rdata, size);
} /* dns_any_init() */


int dns_any_parse(union dns_any *any, struct dns_rr *rr, struct dns_packet *P) {
	unsigned i;

	for (i = 0; i < lengthof(dns_rrtypes); i++) {
		if (dns_rrtypes[i].type == rr->type)
			return dns_rrtypes[i].parse(any, rr, P);
	}

	if (rr->rd.len > any->rdata.size)
		return DNS_EILLEGAL;

	memcpy(any->rdata.data, &P->data[rr->rd.p], rr->rd.len);
	any->rdata.len	= rr->rd.len;

	return 0;
} /* dns_any_parse() */


int dns_any_push(struct dns_packet *P, union dns_any *any, enum dns_type type) {
	unsigned i;

	for (i = 0; i < lengthof(dns_rrtypes); i++) {
		if (dns_rrtypes[i].type == type)
			return dns_rrtypes[i].push(P, any);
	}

	if (P->size - P->end < any->rdata.len + 2)
		return DNS_ENOBUFS;

	P->data[P->end++]	= 0xff & (any->rdata.len >> 8);
	P->data[P->end++]	= 0xff & (any->rdata.len >> 0);

	memcpy(&P->data[P->end], any->rdata.data, any->rdata.len);
	P->end	+= any->rdata.len;

	return 0;
} /* dns_any_push() */


int dns_any_cmp(const union dns_any *a, enum dns_type x, const union dns_any *b, enum dns_type y) {
	unsigned i;
	int cmp;

	if ((cmp = x - y))
		return cmp;

	for (i = 0; i < lengthof(dns_rrtypes); i++) {
		if (dns_rrtypes[i].type == x)
			return dns_rrtypes[i].cmp(a, b);
	}

	return -1;
} /* dns_any_cmp() */


size_t dns_any_print(void *dst_, size_t lim, union dns_any *any, enum dns_type type) {
	struct { unsigned char *b; size_t p, end; } dst, src;
	unsigned i, ch;

	for (i = 0; i < lengthof(dns_rrtypes); i++) {
		if (dns_rrtypes[i].type == type)
			return dns_rrtypes[i].print(dst_, lim, any);
	}

	dst.b	= dst_;
	dst.end	= lim;
	dst.p	= 0;

	src.b	= any->rdata.data;
	src.end	= any->rdata.len;
	src.p	= 0;

	dst.p	+= dns__printchar(dst.b, dst.end, dst.p, '"');

	while (src.p < src.end) {
		ch	= src.b[src.p++];

		dst.p	+= dns__printchar(dst.b, dst.end, dst.p, '\\');
		dst.p	+= dns__print10(dst.b, dst.end, dst.p, ch, 3);
	}

	dst.p	+= dns__printchar(dst.b, dst.end, dst.p, '"');

	dns__printnul(dst.b, dst.end, dst.p);

	return dst.p;
} /* dns_any_print() */


size_t dns_any_cname(void *dst, size_t lim, union dns_any *any, enum dns_type type) {
	unsigned i;

	for (i = 0; i < lengthof(dns_rrtypes); i++) {
		if (dns_rrtypes[i].type == type)
			return (dns_rrtypes[i].cname)? dns_rrtypes[i].cname(dst, lim, any) : 0;
	}

	return 0;
} /* dns_any_cname() */


/*
 * H O S T S  R O U T I N E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct dns_hosts {
	struct dns_hosts_entry {
		char host[DNS_D_MAXNAME + 1];
		char arpa[73 + 1];

		int af;

		union {
			struct in_addr a4;
			struct in6_addr a6;
		} addr;

		_Bool alias;

		struct dns_hosts_entry *next;
	} *head, **tail;

	dns_atomic_t refcount;
}; /* struct dns_hosts */


struct dns_hosts *dns_hosts_open(int *error) {
	static const struct dns_hosts hosts_initializer	= { .refcount = 1 };
	struct dns_hosts *hosts;

	if (!(hosts = malloc(sizeof *hosts)))
		goto syerr;

	*hosts	= hosts_initializer;

	hosts->tail	= &hosts->head;

	return hosts;
syerr:
	*error	= dns_syerr();

	free(hosts);

	return 0;
} /* dns_hosts_open() */


void dns_hosts_close(struct dns_hosts *hosts) {
	struct dns_hosts_entry *ent, *xnt;

	if (!hosts || 1 != dns_hosts_release(hosts))
		return;

	for (ent = hosts->head; ent; ent = xnt) {
		xnt	= ent->next;

		free(ent);
	}

	free(hosts);

	return;
} /* dns_hosts_close() */


dns_refcount_t dns_hosts_acquire(struct dns_hosts *hosts) {
	return dns_atomic_fetch_add(&hosts->refcount);
} /* dns_hosts_acquire() */


dns_refcount_t dns_hosts_release(struct dns_hosts *hosts) {
	return dns_atomic_fetch_sub(&hosts->refcount);
} /* dns_hosts_release() */


struct dns_hosts *dns_hosts_mortal(struct dns_hosts *hosts) {
	if (hosts)
		dns_hosts_release(hosts);

	return hosts;
} /* dns_hosts_mortal() */


struct dns_hosts *dns_hosts_local(int *error_) {
	struct dns_hosts *hosts;
	int error;

	if (!(hosts = dns_hosts_open(&error)))
		goto error;

	if ((error = dns_hosts_loadpath(hosts, "/etc/hosts")))
		goto error;

	return hosts;
error:
	*error_	= error;

	dns_hosts_close(hosts);

	return 0;
} /* dns_hosts_local() */


#define dns_hosts_issep(ch)	(isspace(ch))
#define dns_hosts_iscom(ch)	((ch) == '#' || (ch) == ';')

int dns_hosts_loadfile(struct dns_hosts *hosts, FILE *fp) {
	struct dns_hosts_entry ent;
	char word[DNS_PP_MAX(INET6_ADDRSTRLEN, DNS_D_MAXNAME) + 1];
	unsigned wp, wc, skip;
	int ch, error;

	rewind(fp);

	do {
		memset(&ent, '\0', sizeof ent);
		wc	= 0;
		skip	= 0;

		do {
			memset(word, '\0', sizeof word);
			wp	= 0;

			while (EOF != (ch = fgetc(fp)) && ch != '\n') {
				skip	|= !!dns_hosts_iscom(ch);

				if (skip)
					continue;

				if (dns_hosts_issep(ch))
					break;

				if (wp < sizeof word - 1)
					word[wp]	= ch;
				wp++;
			}

			if (!wp)
				continue;

			wc++;

			switch (wc) {
			case 0:
				break;
			case 1:
				ent.af	= (strchr(word, ':'))? AF_INET6 : AF_INET;
				skip	= (1 != dns_inet_pton(ent.af, word, &ent.addr));

				break;
			default:
				if (!wp)
					break;

				dns_d_anchor(ent.host, sizeof ent.host, word, wp);

				if ((error = dns_hosts_insert(hosts, ent.af, &ent.addr, ent.host, (wc > 2))))
					return error;

				break;
			} /* switch() */
		} while (ch != EOF && ch != '\n');
	} while (ch != EOF);

	return 0;
} /* dns_hosts_loadfile() */


int dns_hosts_loadpath(struct dns_hosts *hosts, const char *path) {
	FILE *fp;
	int error;

	if (!(fp = fopen(path, "r")))
		return dns_syerr();

	error	= dns_hosts_loadfile(hosts, fp);

	fclose(fp);

	return error;
} /* dns_hosts_loadpath() */


int dns_hosts_dump(struct dns_hosts *hosts, FILE *fp) {
	struct dns_hosts_entry *ent, *xnt;
	char addr[INET6_ADDRSTRLEN + 1];
	unsigned i;

	for (ent = hosts->head; ent; ent = xnt) {
		xnt	= ent->next;

		dns_inet_ntop(ent->af, &ent->addr, addr, sizeof addr);

		fputs(addr, fp);

		for (i = strlen(addr); i < INET_ADDRSTRLEN; i++)
			fputc(' ', fp);

		fputc(' ', fp);

		fputs(ent->host, fp);
		fputc('\n', fp);
	}

	return 0;
} /* dns_hosts_dump() */


int dns_hosts_insert(struct dns_hosts *hosts, int af, const void *addr, const void *host, _Bool alias) {
	struct dns_hosts_entry *ent;
	int error;

	if (!(ent = malloc(sizeof *ent)))
		goto syerr;

	dns_d_anchor(ent->host, sizeof ent->host, host, strlen(host));

	switch ((ent->af = af)) {
	case AF_INET6:
		memcpy(&ent->addr.a6, addr, sizeof ent->addr.a6);

		dns_aaaa_arpa(ent->arpa, sizeof ent->arpa, addr);

		break;
	case AF_INET:
		memcpy(&ent->addr.a4, addr, sizeof ent->addr.a4);

		dns_a_arpa(ent->arpa, sizeof ent->arpa, addr);

		break;
	default:
		error	= EINVAL;

		goto error;
	} /* switch() */

	ent->alias	= alias;

	ent->next	= 0;
	*hosts->tail	= ent;
	hosts->tail	= &ent->next;

	return 0;
syerr:
	error	= dns_syerr();
error:
	free(ent);

	return error;
} /* dns_hosts_insert() */


struct dns_packet *dns_hosts_query(struct dns_hosts *hosts, struct dns_packet *Q, int *error_) {
	struct dns_packet *P	= dns_p_new(512);
	struct dns_packet *A	= 0;
	struct dns_rr rr;
	struct dns_hosts_entry *ent;
	int error, af;
	char qname[DNS_D_MAXNAME + 1];
	size_t qlen;

	if ((error = dns_rr_parse(&rr, 12, Q)))
		goto error;

	if (!(qlen = dns_d_expand(qname, sizeof qname, rr.dn.p, Q, &error)))
		goto error;
	else if (qlen >= sizeof qname)
		goto toolong;

	if ((error = dns_p_push(P, DNS_S_QD, qname, qlen, rr.type, rr.class, 0, 0)))
		goto error;

	switch (rr.type) {
	case DNS_T_PTR:
		for (ent = hosts->head; ent; ent = ent->next) {
			if (ent->alias || 0 != strcasecmp(qname, ent->arpa))
				continue;

			if ((error = dns_p_push(P, DNS_S_AN, qname, qlen, rr.type, rr.class, 0, ent->host)))
				goto error;
		}

		break;
	case DNS_T_AAAA:
		af	= AF_INET6;

		goto loop;
	case DNS_T_A:
		af	= AF_INET;

loop:		for (ent = hosts->head; ent; ent = ent->next) {
			if (ent->af != af || 0 != strcasecmp(qname, ent->host))
				continue;

			if ((error = dns_p_push(P, DNS_S_AN, qname, qlen, rr.type, rr.class, 0, &ent->addr)))
				goto error;
		}

		break;
	default:
		break;
	} /* switch() */


	if (!(A = dns_p_copy(dns_p_make(P->end, &error), P)))
		goto error;

	return A;
toolong:
	error = DNS_EILLEGAL;
error:
	*error_	= error;

	dns_p_free(A);

	return 0;
} /* dns_hosts_query() */


/*
 * R E S O L V . C O N F  R O U T I N E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct dns_resolv_conf *dns_resconf_open(int *error) {
	static const struct dns_resolv_conf resconf_initializer
		= { .lookup = "bf", .options = { .ndots = 1, .timeout = 5, .attempts = 2, .tcp = DNS_RESCONF_TCP_ENABLE, },
		    .iface = { .ss_family = AF_INET }, };
	struct dns_resolv_conf *resconf;
	struct sockaddr_in *sin;

	if (!(resconf = malloc(sizeof *resconf)))
		goto syerr;

	*resconf = resconf_initializer;

	sin = (struct sockaddr_in *)&resconf->nameserver[0];
	sin->sin_family      = AF_INET;
	sin->sin_addr.s_addr = INADDR_ANY;
	sin->sin_port        = htons(53);
#if defined(SA_LEN)
	sin->sin_len         = sizeof *sin;
#endif

	if (0 != gethostname(resconf->search[0], sizeof resconf->search[0]))
		goto syerr;

	dns_d_anchor(resconf->search[0], sizeof resconf->search[0], resconf->search[0], strlen(resconf->search[0]));
	dns_d_cleave(resconf->search[0], sizeof resconf->search[0], resconf->search[0], strlen(resconf->search[0]));

	/*
	 * XXX: If gethostname() returned a string without any label
	 *      separator, then search[0][0] should be NUL.
	 */

	dns_resconf_acquire(resconf);

	return resconf;
syerr:
	*error	= dns_syerr();

	free(resconf);

	return 0;
} /* dns_resconf_open() */


void dns_resconf_close(struct dns_resolv_conf *resconf) {
	if (!resconf || 1 != dns_resconf_release(resconf))
		return /* void */;

	free(resconf);
} /* dns_resconf_close() */


dns_refcount_t dns_resconf_acquire(struct dns_resolv_conf *resconf) {
	return dns_atomic_fetch_add(&resconf->_.refcount);
} /* dns_resconf_acquire() */


dns_refcount_t dns_resconf_release(struct dns_resolv_conf *resconf) {
	return dns_atomic_fetch_sub(&resconf->_.refcount);
} /* dns_resconf_release() */


struct dns_resolv_conf *dns_resconf_mortal(struct dns_resolv_conf *resconf) {
	if (resconf)
		dns_resconf_release(resconf);

	return resconf;
} /* dns_resconf_mortal() */


struct dns_resolv_conf *dns_resconf_local(int *error_) {
	struct dns_resolv_conf *resconf;
	int error;

	if (!(resconf = dns_resconf_open(&error)))
		goto error;

	if ((error = dns_resconf_loadpath(resconf, "/etc/resolv.conf")))
		goto error;

	if ((error = dns_nssconf_loadpath(resconf, "/etc/nsswitch.conf"))) {
		if (error != ENOENT)
			goto error;
	}

	return resconf;
error:
	*error_	= error;

	dns_resconf_close(resconf);

	return 0;
} /* dns_resconf_local() */


struct dns_resolv_conf *dns_resconf_root(int *error) {
	struct dns_resolv_conf *resconf;

	if ((resconf = dns_resconf_local(error)))
		resconf->options.recurse = 1;

	return resconf;
} /* dns_resconf_root() */


static time_t dns_resconf_timeout(const struct dns_resolv_conf *resconf) {
	return (time_t)DNS_PP_MIN(INT_MAX, resconf->options.timeout);
} /* dns_resconf_timeout() */


enum dns_resconf_keyword {
	DNS_RESCONF_NAMESERVER,
	DNS_RESCONF_DOMAIN,
	DNS_RESCONF_SEARCH,
	DNS_RESCONF_LOOKUP,
	DNS_RESCONF_FILE,
	DNS_RESCONF_BIND,
	DNS_RESCONF_CACHE,
	DNS_RESCONF_OPTIONS,
	DNS_RESCONF_EDNS0,
	DNS_RESCONF_NDOTS,
	DNS_RESCONF_TIMEOUT,
	DNS_RESCONF_ATTEMPTS,
	DNS_RESCONF_ROTATE,
	DNS_RESCONF_RECURSE,
	DNS_RESCONF_SMART,
	DNS_RESCONF_TCP,
	DNS_RESCONF_TCPx,
	DNS_RESCONF_INTERFACE,
	DNS_RESCONF_ZERO,
	DNS_RESCONF_ONE,
	DNS_RESCONF_ENABLE,
	DNS_RESCONF_ONLY,
	DNS_RESCONF_DISABLE,
}; /* enum dns_resconf_keyword */

static enum dns_resconf_keyword dns_resconf_keyword(const char *word) {
	static const char *words[]	= {
		[DNS_RESCONF_NAMESERVER]	= "nameserver",
		[DNS_RESCONF_DOMAIN]		= "domain",
		[DNS_RESCONF_SEARCH]		= "search",
		[DNS_RESCONF_LOOKUP]		= "lookup",
		[DNS_RESCONF_FILE]		= "file",
		[DNS_RESCONF_BIND]		= "bind",
		[DNS_RESCONF_CACHE]		= "cache",
		[DNS_RESCONF_OPTIONS]		= "options",
		[DNS_RESCONF_EDNS0]		= "edns0",
		[DNS_RESCONF_ROTATE]		= "rotate",
		[DNS_RESCONF_RECURSE]		= "recurse",
		[DNS_RESCONF_SMART]		= "smart",
		[DNS_RESCONF_TCP]		= "tcp",
		[DNS_RESCONF_INTERFACE]		= "interface",
		[DNS_RESCONF_ZERO]		= "0",
		[DNS_RESCONF_ONE]		= "1",
		[DNS_RESCONF_ENABLE]		= "enable",
		[DNS_RESCONF_ONLY]		= "only",
		[DNS_RESCONF_DISABLE]		= "disable",
	};
	unsigned i;

	for (i = 0; i < lengthof(words); i++) {
		if (words[i] && 0 == strcasecmp(words[i], word))
			return i;
	}

	if (0 == strncasecmp(word, "ndots:", sizeof "ndots:" - 1))
		return DNS_RESCONF_NDOTS;

	if (0 == strncasecmp(word, "timeout:", sizeof "timeout:" - 1))
		return DNS_RESCONF_TIMEOUT;

	if (0 == strncasecmp(word, "attempts:", sizeof "attempts:" - 1))
		return DNS_RESCONF_ATTEMPTS;

	if (0 == strncasecmp(word, "tcp:", sizeof "tcp:" - 1))
		return DNS_RESCONF_TCPx;

	return -1;
} /* dns_resconf_keyword() */


/** OpenBSD-style "[1.2.3.4]:53" nameserver syntax */
int dns_resconf_pton(struct sockaddr_storage *ss, const char *src) {
	struct { char buf[128], *p; } addr = { "", addr.buf };
	unsigned short port = 0;
	int ch, af = AF_INET, error;

	while ((ch = *src++)) {
		switch (ch) {
		case ' ':
			/* FALL THROUGH */
		case '\t':
			break;
		case '[':
			break;
		case ']':
			while ((ch = *src++)) {
				if (isdigit((unsigned char)ch)) {
					port *= 10;
					port += ch - '0';
				}
			}

			goto inet;
		case ':':
			af = AF_INET6;

			/* FALL THROUGH */
		default:
			if (addr.p < endof(addr.buf) - 1)
				*addr.p++ = ch;

			break;
		} /* switch() */
	} /* while() */
inet:

	if ((error = dns_pton(af, addr.buf, dns_sa_addr(af, ss, NULL))))
		return error;

	port = (!port)? 53 : port;
	*dns_sa_port(af, ss) = htons(port);
	dns_sa_family(ss) = af;

	return 0;
} /* dns_resconf_pton() */

#define dns_resconf_issep(ch)	(isspace(ch) || (ch) == ',')
#define dns_resconf_iscom(ch)	((ch) == '#' || (ch) == ';')

int dns_resconf_loadfile(struct dns_resolv_conf *resconf, FILE *fp) {
	unsigned sa_count	= 0;
	char words[6][DNS_D_MAXNAME + 1];
	unsigned wp, wc, i, j, n;
	int ch, error;

	rewind(fp);

	do {
		memset(words, '\0', sizeof words);
		wp	= 0;
		wc	= 0;

		while (EOF != (ch = getc(fp)) && ch != '\n') {
			if (dns_resconf_issep(ch)) {
				if (wp > 0) {
					wp	= 0;

					if (++wc >= lengthof(words))
						goto skip;
				}
			} else if (dns_resconf_iscom(ch)) {
skip:
				do {
					ch	= getc(fp);
				} while (ch != EOF && ch != '\n');

				break;
			} else {
				dns__printchar(words[wc], sizeof words[wc], wp, ch);
				wp++;
			}
		}

		if (wp > 0)
			wc++;

		if (wc < 2)
			continue;

		switch (dns_resconf_keyword(words[0])) {
		case DNS_RESCONF_NAMESERVER:
			if (sa_count >= lengthof(resconf->nameserver))
				continue;

			if ((error = dns_resconf_pton(&resconf->nameserver[sa_count], words[1])))
				continue;

			sa_count++;

			break;
		case DNS_RESCONF_DOMAIN:
		case DNS_RESCONF_SEARCH:
			memset(resconf->search, '\0', sizeof resconf->search);

			for (i = 1, j = 0; i < wc && j < lengthof(resconf->search); i++, j++)
				dns_d_anchor(resconf->search[j], sizeof resconf->search[j], words[i], strlen(words[i]));

			break;
		case DNS_RESCONF_LOOKUP:
			for (i = 1, j = 0; i < wc && j < lengthof(resconf->lookup); i++) {
				switch (dns_resconf_keyword(words[i])) {
				case DNS_RESCONF_FILE:
					resconf->lookup[j++]	= 'f';

					break;
				case DNS_RESCONF_BIND:
					resconf->lookup[j++]	= 'b';

					break;
				case DNS_RESCONF_CACHE:
					resconf->lookup[j++]	= 'c';

					break;
				default:
					break;
				} /* switch() */
			} /* for() */

			break;
		case DNS_RESCONF_OPTIONS:
			for (i = 1; i < wc; i++) {
				switch (dns_resconf_keyword(words[i])) {
				case DNS_RESCONF_EDNS0:
					resconf->options.edns0	= 1;

					break;
				case DNS_RESCONF_NDOTS:
					for (j = sizeof "ndots:" - 1, n = 0; isdigit((int)words[i][j]); j++) {
						n	*= 10;
						n	+= words[i][j] - '0';
					} /* for() */

					resconf->options.ndots	= n;

					break;
				case DNS_RESCONF_TIMEOUT:
					for (j = sizeof "timeout:" - 1, n = 0; isdigit((int)words[i][j]); j++) {
						n	*= 10;
						n	+= words[i][j] - '0';
					} /* for() */

					resconf->options.timeout	= n;

					break;
				case DNS_RESCONF_ATTEMPTS:
					for (j = sizeof "attempts:" - 1, n = 0; isdigit((int)words[i][j]); j++) {
						n	*= 10;
						n	+= words[i][j] - '0';
					} /* for() */

					resconf->options.attempts	= n;

					break;
				case DNS_RESCONF_ROTATE:
					resconf->options.rotate		= 1;

					break;
				case DNS_RESCONF_RECURSE:
					resconf->options.recurse	= 1;

					break;
				case DNS_RESCONF_SMART:
					resconf->options.smart		= 1;

					break;
				case DNS_RESCONF_TCP:
					resconf->options.tcp		= DNS_RESCONF_TCP_ONLY;

					break;
				case DNS_RESCONF_TCPx:
					switch (dns_resconf_keyword(&words[i][sizeof "tcp:" - 1])) {
					case DNS_RESCONF_ENABLE:
						resconf->options.tcp	= DNS_RESCONF_TCP_ENABLE;

						break;
					case DNS_RESCONF_ONE:
					case DNS_RESCONF_ONLY:
						resconf->options.tcp	= DNS_RESCONF_TCP_ONLY;

						break;
					case DNS_RESCONF_ZERO:
					case DNS_RESCONF_DISABLE:
						resconf->options.tcp	= DNS_RESCONF_TCP_DISABLE;

						break;
					default:
						break;
					} /* switch() */

					break;
				default:
					break;
				} /* switch() */
			} /* for() */

			break;
		case DNS_RESCONF_INTERFACE:
			for (i = 0, n = 0; isdigit((int)words[2][i]); i++) {
				n	*= 10;
				n	+= words[2][i] - '0';
			}

			dns_resconf_setiface(resconf, words[1], n);

			break;
		default:
			break;
		} /* switch() */
	} while (ch != EOF);

	return 0;
} /* dns_resconf_loadfile() */


int dns_resconf_loadpath(struct dns_resolv_conf *resconf, const char *path) {
	FILE *fp;
	int error;

	if (!(fp = fopen(path, "r")))
		return dns_syerr();

	error	= dns_resconf_loadfile(resconf, fp);

	fclose(fp);

	return error;
} /* dns_resconf_loadpath() */


struct dns_anyconf {
	char *token[16];
	unsigned count;
	char buffer[1024], *tp, *cp;
}; /* struct dns_anyconf */


static void dns_anyconf_reset(struct dns_anyconf *cf) {
	cf->count = 0;
	cf->tp = cf->cp = cf->buffer;
} /* dns_anyconf_reset() */


static int dns_anyconf_push(struct dns_anyconf *cf) {
	if (!(cf->cp < endof(cf->buffer) && cf->count < lengthof(cf->token)))
		return ENOMEM;

	*cf->cp++ = '\0';
	cf->token[cf->count++] = cf->tp;
	cf->tp = cf->cp;

	return 0;
} /* dns_anyconf_push() */


static void dns_anyconf_pop(struct dns_anyconf *cf) {
	if (cf->count > 0) {
		--cf->count;
		cf->tp = cf->cp = cf->token[cf->count];
		cf->token[cf->count] = 0;
	}
} /* dns_anyconf_pop() */


static int dns_anyconf_addc(struct dns_anyconf *cf, int ch) {
	if (!(cf->cp < endof(cf->buffer)))
		return ENOMEM;

	*cf->cp++ = ch;

	return 0;
} /* dns_anyconf_addc() */


static _Bool dns_anyconf_match(const char *pat, int mc) {
	_Bool match;
	int pc;

	if (*pat == '^') {
		match = 0;
		++pat;
	} else {
		match = 1;
	}

	while ((pc = *(const unsigned char *)pat++)) {
		switch (pc) {
		case '%':
			if (!(pc = *(const unsigned char *)pat++))
				return !match;

			switch (pc) {
			case 'a':
				if (isalpha(mc))
					return match;
				break;
			case 'd':
				if (isdigit(mc))
					return match;
				break;
			case 'w':
				if (isalnum(mc))
					return match;
				break;
			case 's':
				if (isspace(mc))
					return match;
				break;
			default:
				if (mc == pc)
					return match;
				break;
			} /* switch() */

			break;
		default:
			if (mc == pc)
				return match;
			break;
		} /* switch() */
	} /* while() */

	return !match;
} /* dns_anyconf_match() */


static int dns_anyconf_peek(FILE *fp) {
	int ch;
	ch = getc(fp);
	ungetc(ch, fp);
	return ch;
} /* dns_anyconf_peek() */


static size_t dns_anyconf_skip(const char *pat, FILE *fp) {
	size_t count = 0;
	int ch;

	while (EOF != (ch = getc(fp))) {
		if (dns_anyconf_match(pat, ch)) {
			count++;
			continue;
		}

		ungetc(ch, fp);

		break;
	}

	return count;
} /* dns_anyconf_skip() */


static size_t dns_anyconf_scan(struct dns_anyconf *cf, const char *pat, FILE *fp, int *error) {
	size_t len;
	int ch;

	while (EOF != (ch = getc(fp))) {
		if (dns_anyconf_match(pat, ch)) {
			if ((*error = dns_anyconf_addc(cf, ch)))
				return 0;

			continue;
		} else {
			ungetc(ch, fp);

			break;
		}
	}

	if ((len = cf->cp - cf->tp)) {
		if ((*error = dns_anyconf_push(cf)))
			return 0;

		return len;
	} else {
		*error = 0;

		return 0;
	}
} /* dns_anyconf_scan() */


DNS_NOTUSED static void dns_anyconf_dump(struct dns_anyconf *cf, FILE *fp) {
	unsigned i;

	fprintf(fp, "tokens:");

	for (i = 0; i < cf->count; i++) {
		fprintf(fp, " %s", cf->token[i]);
	}

	fputc('\n', fp);
} /* dns_anyconf_dump() */


enum dns_nssconf_keyword {
	DNS_NSSCONF_INVALID = 0,
	DNS_NSSCONF_HOSTS   = 1,
	DNS_NSSCONF_SUCCESS,
	DNS_NSSCONF_NOTFOUND,
	DNS_NSSCONF_UNAVAIL,
	DNS_NSSCONF_TRYAGAIN,
	DNS_NSSCONF_CONTINUE,
	DNS_NSSCONF_RETURN,
	DNS_NSSCONF_FILES,
	DNS_NSSCONF_DNS,
	DNS_NSSCONF_MDNS,

	DNS_NSSCONF_LAST,
}; /* enum dns_nssconf_keyword */

static enum dns_nssconf_keyword dns_nssconf_keyword(const char *word) {
	static const char *list[] = {
		[DNS_NSSCONF_HOSTS]    = "hosts",
		[DNS_NSSCONF_SUCCESS]  = "success",
		[DNS_NSSCONF_NOTFOUND] = "notfound",
		[DNS_NSSCONF_UNAVAIL]  = "unavail",
		[DNS_NSSCONF_TRYAGAIN] = "tryagain",
		[DNS_NSSCONF_CONTINUE] = "continue",
		[DNS_NSSCONF_RETURN]   = "return",
		[DNS_NSSCONF_FILES]    = "files",
		[DNS_NSSCONF_DNS]      = "dns",
		[DNS_NSSCONF_MDNS]     = "mdns",
	};
	unsigned i;

	for (i = 1; i < lengthof(list); i++) {
		if (list[i] && 0 == strcasecmp(list[i], word))
			return i;
	}

	return DNS_NSSCONF_INVALID;
} /* dns_nssconf_keyword() */


static enum dns_nssconf_keyword dns_nssconf_c2k(int ch) {
	static const char map[] = {
		['S'] = DNS_NSSCONF_SUCCESS,
		['N'] = DNS_NSSCONF_NOTFOUND,
		['U'] = DNS_NSSCONF_UNAVAIL,
		['T'] = DNS_NSSCONF_TRYAGAIN,
		['C'] = DNS_NSSCONF_CONTINUE,
		['R'] = DNS_NSSCONF_RETURN,
		['f'] = DNS_NSSCONF_FILES,
		['F'] = DNS_NSSCONF_FILES,
		['d'] = DNS_NSSCONF_DNS,
		['D'] = DNS_NSSCONF_DNS,
		['b'] = DNS_NSSCONF_DNS,
		['B'] = DNS_NSSCONF_DNS,
		['m'] = DNS_NSSCONF_MDNS,
		['M'] = DNS_NSSCONF_MDNS,
	};

	return (ch >= 0 && ch < (int)lengthof(map))? map[ch] : DNS_NSSCONF_INVALID;
} /* dns_nssconf_c2k() */


DNS_PRAGMA_PUSH
DNS_PRAGMA_QUIET

static int dns_nssconf_k2c(int k) {
	static const char map[DNS_NSSCONF_LAST] = {
		[DNS_NSSCONF_SUCCESS]  = 'S',
		[DNS_NSSCONF_NOTFOUND] = 'N',
		[DNS_NSSCONF_UNAVAIL]  = 'U',
		[DNS_NSSCONF_TRYAGAIN] = 'T',
		[DNS_NSSCONF_CONTINUE] = 'C',
		[DNS_NSSCONF_RETURN]   = 'R',
		[DNS_NSSCONF_FILES]    = 'f',
		[DNS_NSSCONF_DNS]      = 'b',
		[DNS_NSSCONF_MDNS]     = 'm',
	};

	return (k >= 0 && k < (int)lengthof(map))? (map[k]? map[k] : '?') : '?';
} /* dns_nssconf_k2c() */

static const char *dns_nssconf_k2s(int k) {
	static const char *const map[DNS_NSSCONF_LAST] = {
		[DNS_NSSCONF_SUCCESS]  = "SUCCESS",
		[DNS_NSSCONF_NOTFOUND] = "NOTFOUND",
		[DNS_NSSCONF_UNAVAIL]  = "UNAVAIL",
		[DNS_NSSCONF_TRYAGAIN] = "TRYAGAIN",
		[DNS_NSSCONF_CONTINUE] = "continue",
		[DNS_NSSCONF_RETURN]   = "return",
		[DNS_NSSCONF_FILES]    = "files",
		[DNS_NSSCONF_DNS]      = "dns",
		[DNS_NSSCONF_MDNS]     = "mdns",
	};

	return (k >= 0 && k < (int)lengthof(map))? (map[k]? map[k] : "") : "";
} /* dns_nssconf_k2s() */

DNS_PRAGMA_POP


int dns_nssconf_loadfile(struct dns_resolv_conf *resconf, FILE *fp) {
	enum dns_nssconf_keyword source, status, action;
	char lookup[sizeof resconf->lookup] = "", *lp;
	struct dns_anyconf cf;
	size_t i;
	int error;

	while (!feof(fp) && !ferror(fp)) {
		dns_anyconf_reset(&cf);

		dns_anyconf_skip("%s", fp);

		if (!dns_anyconf_scan(&cf, "%w_", fp, &error))
			goto nextent;

		if (DNS_NSSCONF_HOSTS != dns_nssconf_keyword(cf.token[0]))
			goto nextent;

		dns_anyconf_pop(&cf);

		if (!dns_anyconf_skip(": \t", fp))
			goto nextent;

		*(lp = lookup) = '\0';

		while (dns_anyconf_scan(&cf, "%w_", fp, &error)) {
			dns_anyconf_skip(" \t", fp);

			if ('[' == dns_anyconf_peek(fp)) {
				dns_anyconf_skip("[ \t", fp);

				while (dns_anyconf_scan(&cf, "%w_", fp, &error)) {
					dns_anyconf_skip("= \t", fp);
					if (!dns_anyconf_scan(&cf, "%w_", fp, &error)) {
						dns_anyconf_pop(&cf); /* discard status */
						dns_anyconf_skip("^#;]\n", fp); /* skip to end of criteria */
						break;
					}
					dns_anyconf_skip(" \t", fp);
				}

				dns_anyconf_skip("] \t", fp);
			}

			if ((size_t)(endof(lookup) - lp) < cf.count + 1) /* +1 for '\0' */
				goto nextsrc;

			source = dns_nssconf_keyword(cf.token[0]);

			switch (source) {
			case DNS_NSSCONF_DNS:
			case DNS_NSSCONF_MDNS:
			case DNS_NSSCONF_FILES:
				*lp++ = dns_nssconf_k2c(source);
				break;
			default:
				goto nextsrc;
			}

			for (i = 1; i + 1 < cf.count; i += 2) {
				status = dns_nssconf_keyword(cf.token[i]);
				action = dns_nssconf_keyword(cf.token[i + 1]);

				switch (status) {
				case DNS_NSSCONF_SUCCESS:
				case DNS_NSSCONF_NOTFOUND:
				case DNS_NSSCONF_UNAVAIL:
				case DNS_NSSCONF_TRYAGAIN:
					*lp++ = dns_nssconf_k2c(status);
					break;
				default:
					continue;
				}

				switch (action) {
				case DNS_NSSCONF_CONTINUE:
				case DNS_NSSCONF_RETURN:
					break;
				default:
					action = (status == DNS_NSSCONF_SUCCESS)
					       ? DNS_NSSCONF_RETURN
					       : DNS_NSSCONF_CONTINUE;
					break;
				}

				*lp++ = dns_nssconf_k2c(action);
			}
nextsrc:
			*lp = '\0';
			dns_anyconf_reset(&cf);
		}
nextent:
		dns_anyconf_skip("^\n", fp);
	}

	if (*lookup)
		strncpy(resconf->lookup, lookup, sizeof resconf->lookup);

	return 0;
} /* dns_nssconf_loadfile() */


int dns_nssconf_loadpath(struct dns_resolv_conf *resconf, const char *path) {
	FILE *fp;
	int error;

	if (!(fp = fopen(path, "r")))
		return dns_syerr();

	error = dns_nssconf_loadfile(resconf, fp);

	fclose(fp);

	return error;
} /* dns_nssconf_loadpath() */


struct dns_nssconf_source {
	enum dns_nssconf_keyword source, success, notfound, unavail, tryagain;
}; /* struct dns_nssconf_source */

typedef unsigned dns_nssconf_i;

static inline int dns_nssconf_peek(const struct dns_resolv_conf *resconf, dns_nssconf_i state) {
	return (state < lengthof(resconf->lookup) && resconf->lookup[state])? resconf->lookup[state] : 0;
} /* dns_nssconf_peek() */

static _Bool dns_nssconf_next(struct dns_nssconf_source *src, const struct dns_resolv_conf *resconf, dns_nssconf_i *state) {
	int source, status, action;

	src->source = DNS_NSSCONF_INVALID;
	src->success = DNS_NSSCONF_RETURN;
	src->notfound = DNS_NSSCONF_CONTINUE;
	src->unavail = DNS_NSSCONF_CONTINUE;
	src->tryagain = DNS_NSSCONF_CONTINUE;

	while ((source = dns_nssconf_peek(resconf, *state))) {
		source = dns_nssconf_c2k(source);
		++*state;

		switch (source) {
		case DNS_NSSCONF_FILES:
		case DNS_NSSCONF_DNS:
		case DNS_NSSCONF_MDNS:
			src->source = source;
			break;
		default:
			continue;
		}

		while ((status = dns_nssconf_peek(resconf, *state)) && (action = dns_nssconf_peek(resconf, *state + 1))) {
			status = dns_nssconf_c2k(status);
			action = dns_nssconf_c2k(action);

			switch (action) {
			case DNS_NSSCONF_RETURN:
			case DNS_NSSCONF_CONTINUE:
				break;
			default:
				goto done;
			}

			switch (status) {
			case DNS_NSSCONF_SUCCESS:
				src->success = action;
				break;
			case DNS_NSSCONF_NOTFOUND:
				src->notfound = action;
				break;
			case DNS_NSSCONF_UNAVAIL:
				src->unavail = action;
				break;
			case DNS_NSSCONF_TRYAGAIN:
				src->tryagain = action;
				break;
			default:
				goto done;
			}

			*state += 2;
		}

		break;
	}
done:
	return src->source != DNS_NSSCONF_INVALID;
} /* dns_nssconf_next() */


static int dns_nssconf_dump_status(int status, int action, unsigned *count, FILE *fp) {
	switch (status) {
	case DNS_NSSCONF_SUCCESS:
		if (action == DNS_NSSCONF_RETURN)
			return 0;
		break;
	default:
		if (action == DNS_NSSCONF_CONTINUE)
			return 0;
		break;
	}

	fputc(' ', fp);

	if (!*count)
		fputc('[', fp);

	fprintf(fp, "%s=%s", dns_nssconf_k2s(status), dns_nssconf_k2s(action));

	++*count;

	return 0;
} /* dns_nssconf_dump_status() */


int dns_nssconf_dump(struct dns_resolv_conf *resconf, FILE *fp) {
	struct dns_nssconf_source src;
	dns_nssconf_i i = 0;

	fputs("hosts:", fp);

	while (dns_nssconf_next(&src, resconf, &i)) {
		unsigned n = 0;

		fprintf(fp, " %s", dns_nssconf_k2s(src.source));

		dns_nssconf_dump_status(DNS_NSSCONF_SUCCESS, src.success, &n, fp);
		dns_nssconf_dump_status(DNS_NSSCONF_NOTFOUND, src.notfound, &n, fp);
		dns_nssconf_dump_status(DNS_NSSCONF_UNAVAIL, src.unavail, &n, fp);
		dns_nssconf_dump_status(DNS_NSSCONF_TRYAGAIN, src.tryagain, &n, fp);

		if (n)
			fputc(']', fp);
	}

	fputc('\n', fp);

	return 0;
} /* dns_nssconf_dump() */


int dns_resconf_setiface(struct dns_resolv_conf *resconf, const char *addr, unsigned short port) {
	int af = (strchr(addr, ':'))? AF_INET6 : AF_INET;
	int error;

	if ((error = dns_pton(af, addr, dns_sa_addr(af, &resconf->iface, NULL))))
		return error;

	*dns_sa_port(af, &resconf->iface)	= htons(port);
	resconf->iface.ss_family		= af;

	return 0;
} /* dns_resconf_setiface() */


size_t dns_resconf_search(void *dst, size_t lim, const void *qname, size_t qlen, struct dns_resolv_conf *resconf, dns_resconf_i_t *state) {
	unsigned srchi		= 0xff & (*state >> 8);
	unsigned ndots		= 0xff & (*state >> 16);
	unsigned slen, len	= 0;
	const char *qp, *qe;

//	assert(0xff > lengthof(resconf->search));

	switch (0xff & *state) {
	case 0:
		qp	= qname;
		qe	= qp + qlen;

		while ((qp = memchr(qp, '.', qe - qp)))
			{ ndots++; qp++; }

		++*state;

		if (ndots >= resconf->options.ndots) {
			len	= dns_d_anchor(dst, lim, qname, qlen);

			break;
		}

		/* FALL THROUGH */
	case 1:
		if (srchi < lengthof(resconf->search) && (slen = strlen(resconf->search[srchi]))) {
			len	= dns__printstring(dst, lim, 0, qname, qlen);
			len	= dns_d_anchor(dst, lim, dst, len);
			len	+= dns__printstring(dst, lim, len, resconf->search[srchi], slen);

			srchi++;

			break;
		}

		++*state;

		/* FALL THROUGH */
	case 2:
		++*state;

		if (ndots < resconf->options.ndots) {
			len	= dns_d_anchor(dst, lim, qname, qlen);

			break;
		}

		/* FALL THROUGH */
	default:
		break;
	} /* switch() */

	dns__printnul(dst, lim, len);

	*state	= ((0xff & *state) << 0)
		| ((0xff & srchi) << 8)
		| ((0xff & ndots) << 16);

	return len;
} /* dns_resconf_search() */


int dns_resconf_dump(struct dns_resolv_conf *resconf, FILE *fp) {
	unsigned i;
	int af;

	for (i = 0; i < lengthof(resconf->nameserver) && (af = resconf->nameserver[i].ss_family) != AF_UNSPEC; i++) {
		char addr[INET6_ADDRSTRLEN + 1]	= "[INVALID]";
		unsigned short port;

		dns_inet_ntop(af, dns_sa_addr(af, &resconf->nameserver[i], NULL), addr, sizeof addr);
		port = ntohs(*dns_sa_port(af, &resconf->nameserver[i]));

		if (port == 53)
			fprintf(fp, "nameserver %s\n", addr);
		else
			fprintf(fp, "nameserver [%s]:%hu\n", addr, port);
	}


	fprintf(fp, "search");

	for (i = 0; i < lengthof(resconf->search) && resconf->search[i][0]; i++)
		fprintf(fp, " %s", resconf->search[i]);

	fputc('\n', fp);


	fputs("; ", fp);
	dns_nssconf_dump(resconf, fp);

	fprintf(fp, "lookup");

	for (i = 0; i < lengthof(resconf->lookup) && resconf->lookup[i]; i++) {
		switch (resconf->lookup[i]) {
		case 'b':
			fprintf(fp, " bind"); break;
		case 'f':
			fprintf(fp, " file"); break;
		case 'c':
			fprintf(fp, " cache"); break;
		}
	}

	fputc('\n', fp);


	fprintf(fp, "options ndots:%u timeout:%u attempts:%u", resconf->options.ndots, resconf->options.timeout, resconf->options.attempts);

	if (resconf->options.edns0)
		fprintf(fp, " edns0");
	if (resconf->options.rotate)
		fprintf(fp, " rotate");
	if (resconf->options.recurse)
		fprintf(fp, " recurse");
	if (resconf->options.smart)
		fprintf(fp, " smart");

	switch (resconf->options.tcp) {
	case DNS_RESCONF_TCP_ENABLE:
		break;
	case DNS_RESCONF_TCP_ONLY:
		fprintf(fp, " tcp");
		break;
	case DNS_RESCONF_TCP_DISABLE:
		fprintf(fp, " tcp:disable");
		break;
	}

	fputc('\n', fp);


	if ((af = resconf->iface.ss_family) != AF_UNSPEC) {
		char addr[INET6_ADDRSTRLEN + 1]	= "[INVALID]";

		dns_inet_ntop(af, dns_sa_addr(af, &resconf->iface, NULL), addr, sizeof addr);

		fprintf(fp, "interface %s %hu\n", addr, ntohs(*dns_sa_port(af, &resconf->iface)));
	}

	return 0;
} /* dns_resconf_dump() */


/*
 * H I N T  S E R V E R  R O U T I N E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct dns_hints_soa {
	unsigned char zone[DNS_D_MAXNAME + 1];

	struct {
		struct sockaddr_storage ss;
		unsigned priority;
	} addrs[16];

	unsigned count;

	struct dns_hints_soa *next;
}; /* struct dns_hints_soa */


struct dns_hints {
	dns_atomic_t refcount;

	struct dns_hints_soa *head;
}; /* struct dns_hints */


struct dns_hints *dns_hints_open(struct dns_resolv_conf *resconf, int *error) {
	static const struct dns_hints H_initializer;
	struct dns_hints *H;

	(void)resconf;

	if (!(H = malloc(sizeof *H)))
		goto syerr;

	*H	= H_initializer;

	dns_hints_acquire(H);

	return H;
syerr:
	*error	= dns_syerr();

	free(H);

	return 0;
} /* dns_hints_open() */


void dns_hints_close(struct dns_hints *H) {
	struct dns_hints_soa *soa, *nxt;

	if (!H || 1 != dns_hints_release(H))
		return /* void */;

	for (soa = H->head; soa; soa = nxt) {
		nxt	= soa->next;

		free(soa);
	}

	free(H);

	return /* void */;
} /* dns_hints_close() */


dns_refcount_t dns_hints_acquire(struct dns_hints *H) {
	return dns_atomic_fetch_add(&H->refcount);
} /* dns_hints_acquire() */


dns_refcount_t dns_hints_release(struct dns_hints *H) {
	return dns_atomic_fetch_sub(&H->refcount);
} /* dns_hints_release() */


struct dns_hints *dns_hints_mortal(struct dns_hints *hints) {
	if (hints)
		dns_hints_release(hints);

	return hints;
} /* dns_hints_mortal() */


struct dns_hints *dns_hints_local(struct dns_resolv_conf *resconf, int *error_) {
	struct dns_hints *hints		= 0;
	int error;

	if (resconf)
		dns_resconf_acquire(resconf);
	else if (!(resconf = dns_resconf_local(&error)))
		goto error;

	if (!(hints = dns_hints_open(resconf, &error)))
		goto error;

	error	= 0;

	if (0 == dns_hints_insert_resconf(hints, ".", resconf, &error) && error)
		goto error;

	dns_resconf_close(resconf);

	return hints;
error:
	*error_	= error;

	dns_resconf_close(resconf);
	dns_hints_close(hints);

	return 0;
} /* dns_hints_local() */


struct dns_hints *dns_hints_root(struct dns_resolv_conf *resconf, int *error_) {
	static const struct {
		int af;
		char addr[INET6_ADDRSTRLEN];
	} root_hints[] = {
		{ AF_INET,	"198.41.0.4"		},	/* A.ROOT-SERVERS.NET. */
		{ AF_INET6,	"2001:503:ba3e::2:30"	},	/* A.ROOT-SERVERS.NET. */
		{ AF_INET,	"192.228.79.201"	},	/* B.ROOT-SERVERS.NET. */
		{ AF_INET6,	"2001:500:84::b"	},	/* B.ROOT-SERVERS.NET. */
		{ AF_INET,	"192.33.4.12"		},	/* C.ROOT-SERVERS.NET. */
		{ AF_INET6,	"2001:500:2::c"		},	/* C.ROOT-SERVERS.NET. */
		{ AF_INET,	"199.7.91.13"		},	/* D.ROOT-SERVERS.NET. */
		{ AF_INET6,	"2001:500:2d::d"	},	/* D.ROOT-SERVERS.NET. */
		{ AF_INET,	"192.203.230.10"	},	/* E.ROOT-SERVERS.NET. */
		{ AF_INET,	"192.5.5.241"		},	/* F.ROOT-SERVERS.NET. */
		{ AF_INET6,	"2001:500:2f::f"	},	/* F.ROOT-SERVERS.NET. */
		{ AF_INET,	"192.112.36.4"		},	/* G.ROOT-SERVERS.NET. */
		{ AF_INET,	"128.63.2.53"		},	/* H.ROOT-SERVERS.NET. */
		{ AF_INET6,	"2001:500:1::803f:235"	},	/* H.ROOT-SERVERS.NET. */
		{ AF_INET,	"192.36.148.17"		},	/* I.ROOT-SERVERS.NET. */
		{ AF_INET6,	"2001:7FE::53"		},	/* I.ROOT-SERVERS.NET. */
		{ AF_INET,	"192.58.128.30"		},	/* J.ROOT-SERVERS.NET. */
		{ AF_INET6,	"2001:503:c27::2:30"	},	/* J.ROOT-SERVERS.NET. */
		{ AF_INET,	"193.0.14.129"		},	/* K.ROOT-SERVERS.NET. */
		{ AF_INET6,	"2001:7FD::1"		},	/* K.ROOT-SERVERS.NET. */
		{ AF_INET,	"199.7.83.42"		},	/* L.ROOT-SERVERS.NET. */
		{ AF_INET6,	"2001:500:3::42"	},	/* L.ROOT-SERVERS.NET. */
		{ AF_INET,	"202.12.27.33"		},	/* M.ROOT-SERVERS.NET. */
		{ AF_INET6,	"2001:DC3::35"		},	/* M.ROOT-SERVERS.NET. */
	};
	struct dns_hints *hints		= 0;
	struct sockaddr_storage ss;
	unsigned i;
	int error, af;

	if (!(hints = dns_hints_open(resconf, &error)))
		goto error;

	for (i = 0; i < lengthof(root_hints); i++) {
		af	= root_hints[i].af;

		if ((error = dns_pton(af, root_hints[i].addr, dns_sa_addr(af, &ss, NULL))))
			goto error;

		*dns_sa_port(af, &ss)	= htons(53);
		ss.ss_family		= af;

		if ((error = dns_hints_insert(hints, ".", (struct sockaddr *)&ss, 1)))
			goto error;
	}

	return hints;
error:
	*error_	= error;

	dns_hints_close(hints);

	return 0;
} /* dns_hints_root() */


static struct dns_hints_soa *dns_hints_fetch(struct dns_hints *H, const char *zone) {
	struct dns_hints_soa *soa;

	for (soa = H->head; soa; soa = soa->next) {
		if (0 == strcasecmp(zone, (char *)soa->zone))
			return soa;
	}

	return 0;
} /* dns_hints_fetch() */


int dns_hints_insert(struct dns_hints *H, const char *zone, const struct sockaddr *sa, unsigned priority) {
	static const struct dns_hints_soa soa_initializer;
	struct dns_hints_soa *soa;
	unsigned i;

	if (!(soa = dns_hints_fetch(H, zone))) {
		if (!(soa = malloc(sizeof *soa)))
			return dns_syerr();

		*soa	= soa_initializer;

		dns__printstring(soa->zone, sizeof soa->zone, 0, zone);

		soa->next	= H->head;
		H->head		= soa;
	}

	i	= soa->count % lengthof(soa->addrs);

	memcpy(&soa->addrs[i].ss, sa, dns_sa_len(sa));

	soa->addrs[i].priority	= DNS_PP_MAX(1, priority);

	if (soa->count < lengthof(soa->addrs))
		soa->count++;

	return 0;
} /* dns_hints_insert() */


unsigned dns_hints_insert_resconf(struct dns_hints *H, const char *zone, const struct dns_resolv_conf *resconf, int *error_) {
	unsigned i, n, p;
	int error;

	for (i = 0, n = 0, p = 1; i < lengthof(resconf->nameserver) && resconf->nameserver[i].ss_family != AF_UNSPEC; i++, n++) {
		if ((error = dns_hints_insert(H, zone, (struct sockaddr *)&resconf->nameserver[i], p)))
			goto error;

		p	+= !resconf->options.rotate;
	}

	return n;
error:
	*error_	= error;

	return n;
} /* dns_hints_insert_resconf() */


static int dns_hints_i_cmp(unsigned a, unsigned b, struct dns_hints_i *i, struct dns_hints_soa *soa) {
	int cmp;

	if ((cmp = soa->addrs[a].priority - soa->addrs[b].priority))
		return cmp;

	return dns_k_shuffle16(a, i->state.seed) - dns_k_shuffle16(b, i->state.seed);
} /* dns_hints_i_cmp() */


static unsigned dns_hints_i_start(struct dns_hints_i *i, struct dns_hints_soa *soa) {
	unsigned p0, p;

	p0	= 0;

	for (p = 1; p < soa->count; p++) {
		if (dns_hints_i_cmp(p, p0, i, soa) < 0)
			p0	= p;
	}

	return p0;
} /* dns_hints_i_start() */


static unsigned dns_hints_i_skip(unsigned p0, struct dns_hints_i *i, struct dns_hints_soa *soa) {
	unsigned pZ, p;

	for (pZ = 0; pZ < soa->count; pZ++) {
		if (dns_hints_i_cmp(pZ, p0, i, soa) > 0)
			goto cont;
	}

	return soa->count;
cont:
	for (p = pZ + 1; p < soa->count; p++) {
		if (dns_hints_i_cmp(p, p0, i, soa) <= 0)
			continue;

		if (dns_hints_i_cmp(p, pZ, i, soa) >= 0)
			continue;

		pZ	= p;
	}


	return pZ;
} /* dns_hints_i_skip() */


struct dns_hints_i *dns_hints_i_init(struct dns_hints_i *i, struct dns_hints *hints) {
	static const struct dns_hints_i i_initializer;
	struct dns_hints_soa *soa;

	i->state	= i_initializer.state;

	do {
		i->state.seed	= dns_random();
	} while (0 == i->state.seed);

	if ((soa = dns_hints_fetch(hints, i->zone))) {
		i->state.next	= dns_hints_i_start(i, soa);
	}

	return i;
} /* dns_hints_i_init() */


unsigned dns_hints_grep(struct sockaddr **sa, socklen_t *sa_len, unsigned lim, struct dns_hints_i *i, struct dns_hints *H) {
	struct dns_hints_soa *soa;
	unsigned n;

	if (!(soa = dns_hints_fetch(H, i->zone)))
		return 0;

	n	= 0;

	while (i->state.next < soa->count && n < lim) {
		*sa	= (struct sockaddr *)&soa->addrs[i->state.next].ss;
		*sa_len	= dns_sa_len(*sa);

		sa++;
		sa_len++;
		n++;

		i->state.next	= dns_hints_i_skip(i->state.next, i, soa);
	}

	return n;
} /* dns_hints_grep() */


struct dns_packet *dns_hints_query(struct dns_hints *hints, struct dns_packet *Q, int *error_) {
	struct dns_packet *A, *P;
	struct dns_rr rr;
	char zone[DNS_D_MAXNAME + 1];
	size_t zlen;
	struct dns_hints_i i;
	struct sockaddr *sa;
	socklen_t slen;
	int error;

	if (!dns_rr_grep(&rr, 1, dns_rr_i_new(Q, .section = DNS_S_QUESTION), Q, &error))
		goto error;

	if (!(zlen = dns_d_expand(zone, sizeof zone, rr.dn.p, Q, &error)))
		goto error;
	else if (zlen >= sizeof zone)
		goto toolong;

	P			= dns_p_new(512);
	dns_header(P)->qr	= 1;

	if ((error = dns_rr_copy(P, &rr, Q)))
		goto error;

	if ((error = dns_p_push(P, DNS_S_AUTHORITY, ".", strlen("."), DNS_T_NS, DNS_C_IN, 0, "hints.local.")))
		goto error;

	do {
		i.zone	= zone;

		dns_hints_i_init(&i, hints);

		while (dns_hints_grep(&sa, &slen, 1, &i, hints)) {
			int af		= sa->sa_family;
			int rtype	= (af == AF_INET6)? DNS_T_AAAA : DNS_T_A;

			if ((error = dns_p_push(P, DNS_S_ADDITIONAL, "hints.local.", strlen("hints.local."), rtype, DNS_C_IN, 0, dns_sa_addr(af, sa, NULL))))
				goto error;
		}
	} while ((zlen = dns_d_cleave(zone, sizeof zone, zone, zlen)));

	if (!(A = dns_p_copy(dns_p_make(P->end, &error), P)))
		goto error;

	return A;
toolong:
	error = DNS_EILLEGAL;
error:
	*error_	= error;

	return 0;
} /* dns_hints_query() */


/** ugly hack to support specifying ports other than 53 in resolv.conf. */
static unsigned short dns_hints_port(struct dns_hints *hints, int af, void *addr) {
	struct dns_hints_soa *soa;
	void *addrsoa;
	socklen_t addrlen;
	unsigned short port;
	unsigned i;

	for (soa = hints->head; soa; soa = soa->next) {
		for (i = 0; i < soa->count; i++) {
			if (af != soa->addrs[i].ss.ss_family)
				continue;

			if (!(addrsoa = dns_sa_addr(af, &soa->addrs[i].ss, &addrlen)))
				continue;

			if (memcmp(addr, addrsoa, addrlen))
				continue;

			port = *dns_sa_port(af, &soa->addrs[i].ss);

			return (port)? port : htons(53);
		}
	}

	return htons(53);
} /* dns_hints_port() */


int dns_hints_dump(struct dns_hints *hints, FILE *fp) {
	struct dns_hints_soa *soa;
	char addr[INET6_ADDRSTRLEN];
	unsigned i;
	int af, error;

	for (soa = hints->head; soa; soa = soa->next) {
		fprintf(fp, "ZONE \"%s\"\n", soa->zone);

		for (i = 0; i < soa->count; i++) {
			af = soa->addrs[i].ss.ss_family;

			if ((error = dns_ntop(af, dns_sa_addr(af, &soa->addrs[i].ss, NULL), addr, sizeof addr)))
				return error;

			fprintf(fp, "\t(%d) [%s]:%hu\n", (int)soa->addrs[i].priority, addr, ntohs(*dns_sa_port(af, &soa->addrs[i].ss)));
		}
	}

	return 0;
} /* dns_hints_dump() */


/*
 * C A C H E  R O U T I N E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static dns_refcount_t dns_cache_acquire(struct dns_cache *cache) {
	return dns_atomic_fetch_add(&cache->_.refcount);
} /* dns_cache_acquire() */


static dns_refcount_t dns_cache_release(struct dns_cache *cache) {
	return dns_atomic_fetch_sub(&cache->_.refcount);
} /* dns_cache_release() */


static struct dns_packet *dns_cache_query(struct dns_packet *query, struct dns_cache *cache, int *error) {
	(void)query;
	(void)cache;
	(void)error;

	return NULL;
} /* dns_cache_submit() */


static int dns_cache_submit(struct dns_packet *query, struct dns_cache *cache) {
	(void)query;
	(void)cache;

	return 0;
} /* dns_cache_submit() */


static int dns_cache_check(struct dns_cache *cache) {
	(void)cache;

	return 0;
} /* dns_cache_check() */


static struct dns_packet *dns_cache_fetch(struct dns_cache *cache, int *error) {
	(void)cache;
	(void)error;

	return NULL;
} /* dns_cache_fetch() */


static int dns_cache_pollfd(struct dns_cache *cache) {
	(void)cache;

	return -1;
} /* dns_cache_pollfd() */


static short dns_cache_events(struct dns_cache *cache) {
	(void)cache;

	return 0;
} /* dns_cache_events() */


static void dns_cache_clear(struct dns_cache *cache) {
	(void)cache;

	return;
} /* dns_cache_clear() */


struct dns_cache *dns_cache_init(struct dns_cache *cache) {
	static const struct dns_cache c_init = {
		.acquire = &dns_cache_acquire,
		.release = &dns_cache_release,
		.query   = &dns_cache_query,
		.submit  = &dns_cache_submit,
		.check   = &dns_cache_check,
		.fetch   = &dns_cache_fetch,
		.pollfd  = &dns_cache_pollfd,
		.events  = &dns_cache_events,
		.clear   = &dns_cache_clear,
		._ = { .refcount = 1, },
	};

	*cache = c_init;

	return cache;
} /* dns_cache_init() */


void dns_cache_close(struct dns_cache *cache) {
	if (cache)
		cache->release(cache);
} /* dns_cache_close() */


/*
 * S O C K E T  R O U T I N E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static void dns_socketclose(int *fd, const struct dns_options *opts) {
	if (opts && opts->closefd.cb)
		opts->closefd.cb(fd, opts->closefd.arg);

	if (*fd != -1) {
#if _WIN32
		closesocket(*fd);
#else
		close(*fd);
#endif
		*fd	= -1;
	}
} /* dns_socketclose() */


#define DNS_SO_MAXTRY	7

static int dns_socket(struct sockaddr *local, int type, int *error_) {
	int error, fd	= -1;
#if defined(O_NONBLOCK)
	int flags;
#elif defined(FIONBIO)
	unsigned long opt;
#endif

	if (-1 == (fd = socket(local->sa_family, type, 0)))
		goto soerr;

#if defined(F_SETFD)
	if (-1 == fcntl(fd, F_SETFD, 1))
		goto syerr;
#endif

#if defined(O_NONBLOCK)
	if (-1 == (flags = fcntl(fd, F_GETFL)))
		goto syerr;

	if (-1 == fcntl(fd, F_SETFL, flags | O_NONBLOCK))
		goto syerr;
#elif defined(FIONBIO)
	opt	= 1;

	if (0 != ioctlsocket(fd, FIONBIO, &opt))
		goto soerr;
#endif

#if defined(SO_NOSIGPIPE)
	if (type != SOCK_DGRAM) {
		if (0 != setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &(int){ 1 }, sizeof (int)))
			goto soerr;
	}
#endif

	if (local->sa_family != AF_INET && local->sa_family != AF_INET6)
		return fd;

	if (type != SOCK_DGRAM)
		return fd;

	if (*dns_sa_port(local->sa_family, local) == 0) {
		struct sockaddr_storage tmp;
		unsigned i, port;

		memcpy(&tmp, local, dns_sa_len(local));

		for (i = 0; i < DNS_SO_MAXTRY; i++) {
			port	= 1025 + (dns_random() % 64510);

			*dns_sa_port(tmp.ss_family, &tmp)	= htons(port);

			if (0 == bind(fd, (struct sockaddr *)&tmp, dns_sa_len(&tmp)))
				return fd;
		}
	}

	if (0 == bind(fd, local, dns_sa_len(local)))
		return fd;

	/* FALL THROUGH */
soerr:
	error	= dns_soerr();

	goto error;
#if defined(F_SETFD) || defined(O_NONBLOCK)
syerr:
	error	= dns_syerr();

	goto error;
#endif
error:
	*error_	= error;

	dns_socketclose(&fd, NULL);

	return -1;
} /* dns_socket() */


enum {
	DNS_SO_UDP_INIT	= 1,
	DNS_SO_UDP_CONN,
	DNS_SO_UDP_SEND,
	DNS_SO_UDP_RECV,
	DNS_SO_UDP_DONE,

	DNS_SO_TCP_INIT,
	DNS_SO_TCP_CONN,
	DNS_SO_TCP_SEND,
	DNS_SO_TCP_RECV,
	DNS_SO_TCP_DONE,
};

struct dns_socket {
	struct dns_options opts;

	int udp;
	int tcp;

	int *old;
	unsigned onum, olim;

	int type;

	struct sockaddr_storage local, remote;

	struct dns_k_permutor qids;

	struct dns_stat stat;

	/*
	 * NOTE: dns_so_reset() zeroes everything from here down.
	 */
	int state;

	unsigned short qid;
	char qname[DNS_D_MAXNAME + 1];
	size_t qlen;
	enum dns_type qtype;
	enum dns_class qclass;

	struct dns_packet *query;
	size_t qout;

	struct dns_clock elapsed;

	struct dns_packet *answer;
	size_t alen, apos;
}; /* struct dns_socket */


/*
 * NOTE: Actual closure delayed so that kqueue(2) and epoll(2) callers have
 * a chance to recognize a state change after installing a persistent event
 * and where sequential descriptors with the same integer value returned
 * from _pollfd() would be ambiguous. See dns_so_closefds().
 */
static int dns_so_closefd(struct dns_socket *so, int *fd) {
	int error;

	if (*fd == -1)
		return 0;

	if (so->opts.closefd.cb) {
		if ((error = so->opts.closefd.cb(fd, so->opts.closefd.arg))) {
			return error;
		} else if (*fd == -1)
			return 0;
	}

	if (!(so->onum < so->olim)) {
		unsigned olim = DNS_PP_MAX(4, so->olim * 2);
		void *old;

		if (!(old = realloc(so->old, sizeof so->old[0] * olim)))
			return dns_syerr();

		so->old  = old;
		so->olim = olim;
	}

	so->old[so->onum++] = *fd;
	*fd = -1;

	return 0;
} /* dns_so_closefd() */


#define DNS_SO_CLOSE_UDP 0x01
#define DNS_SO_CLOSE_TCP 0x02
#define DNS_SO_CLOSE_OLD 0x04
#define DNS_SO_CLOSE_ALL (DNS_SO_CLOSE_UDP|DNS_SO_CLOSE_TCP|DNS_SO_CLOSE_OLD)

static void dns_so_closefds(struct dns_socket *so, int which) {
	if (DNS_SO_CLOSE_UDP & which)
		dns_socketclose(&so->udp, &so->opts);
	if (DNS_SO_CLOSE_TCP & which)
		dns_socketclose(&so->tcp, &so->opts);
	if (DNS_SO_CLOSE_OLD & which) {
		unsigned i;
		for (i = 0; i < so->onum; i++)
			dns_socketclose(&so->old[i], &so->opts);
		so->onum = 0;
		free(so->old);
		so->old  = 0;
		so->olim = 0;
	}
} /* dns_so_closefds() */


static void dns_so_destroy(struct dns_socket *);

static struct dns_socket *dns_so_init(struct dns_socket *so, const struct sockaddr *local, int type, const struct dns_options *opts, int *error) {
	static const struct dns_socket so_initializer = { .opts = DNS_OPTS_INITIALIZER, .udp = -1, .tcp = -1, };

	*so		= so_initializer;
	so->type	= type;

	if (opts)
		so->opts = *opts;

	if (local)
		memcpy(&so->local, local, dns_sa_len(local));

	if (-1 == (so->udp = dns_socket((struct sockaddr *)&so->local, SOCK_DGRAM, error)))
		goto error;

	dns_k_permutor_init(&so->qids, 1, 65535);

	return so;
error:
	dns_so_destroy(so);

	return 0;
} /* dns_so_init() */


struct dns_socket *dns_so_open(const struct sockaddr *local, int type, const struct dns_options *opts, int *error) {
	struct dns_socket *so;

	if (!(so = malloc(sizeof *so)))
		goto syerr;

	if (!dns_so_init(so, local, type, opts, error))
		goto error;

	return so;
syerr:
	*error	= dns_syerr();
error:
	dns_so_close(so);

	return 0;
} /* dns_so_open() */


static void dns_so_destroy(struct dns_socket *so) {
	dns_so_reset(so);
	dns_so_closefds(so, DNS_SO_CLOSE_ALL);
} /* dns_so_destroy() */


void dns_so_close(struct dns_socket *so) {
	if (!so)
		return;

	dns_so_destroy(so);

	free(so);
} /* dns_so_close() */


void dns_so_reset(struct dns_socket *so) {
	dns_p_setptr(&so->answer, NULL);

	memset(&so->state, '\0', sizeof *so - offsetof(struct dns_socket, state));
} /* dns_so_reset() */


unsigned short dns_so_mkqid(struct dns_socket *so) {
	return dns_k_permutor_step(&so->qids);
} /* dns_so_mkqid() */


#define DNS_SO_MINBUF	768

static int dns_so_newanswer(struct dns_socket *so, size_t len) {
	size_t size	= offsetof(struct dns_packet, data) + DNS_PP_MAX(len, DNS_SO_MINBUF);
	void *p;

	if (!(p = realloc(so->answer, size)))
		return dns_syerr();

	so->answer	= dns_p_init(p, size);

	return 0;
} /* dns_so_newanswer() */


int dns_so_submit(struct dns_socket *so, struct dns_packet *Q, struct sockaddr *host) {
	struct dns_rr rr;
	int error	= -1;

	dns_so_reset(so);

	if ((error = dns_rr_parse(&rr, 12, Q)))
		goto error;

	if (!(so->qlen = dns_d_expand(so->qname, sizeof so->qname, rr.dn.p, Q, &error)))
		goto error;
	/*
	 * NOTE: don't bail if expansion is too long; caller may be
	 * intentionally sending long names. However, we won't be able to
	 * verify it on return.
	 */

	so->qtype	= rr.type;
	so->qclass	= rr.class;

	if ((error = dns_so_newanswer(so, DNS_SO_MINBUF)))
		goto syerr;

	memcpy(&so->remote, host, dns_sa_len(host));

	so->query	= Q;
	so->qout	= 0;

	dns_begin(&so->elapsed);

	if (dns_header(so->query)->qid == 0)
		dns_header(so->query)->qid	= dns_so_mkqid(so);

	so->qid		= dns_header(so->query)->qid;
	so->state	= (so->type == SOCK_STREAM)? DNS_SO_TCP_INIT : DNS_SO_UDP_INIT;

	so->stat.queries++;

	return 0;
syerr:
	error	= dns_syerr();
error:
	dns_so_reset(so);

	return error;
} /* dns_so_submit() */


static int dns_so_verify(struct dns_socket *so, struct dns_packet *P) {
	char qname[DNS_D_MAXNAME + 1];
	size_t qlen;
	struct dns_rr rr;
	int error	= -1;

	if (so->qid != dns_header(so->answer)->qid)
		return DNS_EUNKNOWN;

	if (!dns_p_count(so->answer, DNS_S_QD))
		return DNS_EUNKNOWN;

	if (0 != dns_rr_parse(&rr, 12, so->answer))
		return DNS_EUNKNOWN;

	if (rr.type != so->qtype || rr.class != so->qclass)
		return DNS_EUNKNOWN;

	if (!(qlen = dns_d_expand(qname, sizeof qname, rr.dn.p, P, &error)))
		return error;
	else if (qlen >= sizeof qname || qlen != so->qlen)
		return DNS_EUNKNOWN;

	if (0 != strcasecmp(so->qname, qname))
		return DNS_EUNKNOWN;

	return 0;
} /* dns_so_verify() */


static _Bool dns_so_tcp_keep(struct dns_socket *so) {
	struct sockaddr_storage remote;

	if (so->tcp == -1)
		return 0;

	if (0 != getpeername(so->tcp, (struct sockaddr *)&remote, &(socklen_t){ sizeof remote }))
		return 0;

	return 0 == dns_sa_cmp(&remote, &so->remote);
} /* dns_so_tcp_keep() */


#if defined __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warray-bounds"
#endif

static int dns_so_tcp_send(struct dns_socket *so) {
	unsigned char *qsrc;
	size_t qend;
	long n;

	so->query->data[-2] = 0xff & (so->query->end >> 8);
	so->query->data[-1] = 0xff & (so->query->end >> 0);

	qsrc = &so->query->data[-2] + so->qout;
	qend = so->query->end + 2;

	while (so->qout < qend) {
		if (0 > (n = dns_send(so->tcp, (void *)&qsrc[so->qout], qend - so->qout, 0)))
			return dns_soerr();

		so->qout += n;
		so->stat.tcp.sent.bytes += n;
	}

	so->stat.tcp.sent.count++;

	return 0;
} /* dns_so_tcp_send() */


static int dns_so_tcp_recv(struct dns_socket *so) {
	unsigned char *asrc;
	size_t aend, alen;
	int error;
	long n;

	aend = so->alen + 2;

	while (so->apos < aend) {
		asrc = &so->answer->data[-2];

		if (0 > (n = recv(so->tcp, (void *)&asrc[so->apos], aend - so->apos, 0)))
			return dns_soerr();
		else if (n == 0)
			return DNS_EUNKNOWN;	/* FIXME */

		so->apos += n;
		so->stat.tcp.rcvd.bytes += n;

		if (so->alen == 0 && so->apos >= 2) {
			alen = ((0xff & so->answer->data[-2]) << 8)
			     | ((0xff & so->answer->data[-1]) << 0);

			if ((error = dns_so_newanswer(so, alen)))
				return error;

			so->alen = alen;
			aend = alen + 2;
		}
	}

	so->answer->end	= so->alen;
	so->stat.tcp.rcvd.count++;

	return 0;
} /* dns_so_tcp_recv() */

#if __clang__
#pragma clang diagnostic pop
#endif


int dns_so_check(struct dns_socket *so) {
	int error;
	long n;

retry:
	switch (so->state) {
	case DNS_SO_UDP_INIT:
		so->state++;
	case DNS_SO_UDP_CONN:
		if (0 != connect(so->udp, (struct sockaddr *)&so->remote, dns_sa_len(&so->remote)))
			goto soerr;

		so->state++;
	case DNS_SO_UDP_SEND:
		if (0 > (n = send(so->udp, (void *)so->query->data, so->query->end, 0)))
			goto soerr;

		so->stat.udp.sent.bytes += n;
		so->stat.udp.sent.count++;

		so->state++;
	case DNS_SO_UDP_RECV:
		if (0 > (n = recv(so->udp, (void *)so->answer->data, so->answer->size, 0)))
			goto soerr;

		so->stat.udp.rcvd.bytes += n;
		so->stat.udp.rcvd.count++;

		if ((so->answer->end = n) < 12)
			goto trash;

		if ((error = dns_so_verify(so, so->answer)))
			goto trash;

		so->state++;
	case DNS_SO_UDP_DONE:
		if (!dns_header(so->answer)->tc || so->type == SOCK_DGRAM)
			return 0;

		so->state++;
	case DNS_SO_TCP_INIT:
		if (dns_so_tcp_keep(so)) {
			so->state = DNS_SO_TCP_SEND;

			goto retry;
		}

		if ((error = dns_so_closefd(so, &so->tcp)))
			goto error;

		if (-1 == (so->tcp = dns_socket((struct sockaddr *)&so->local, SOCK_STREAM, &error)))
			goto error;

		so->state++;
	case DNS_SO_TCP_CONN:
		if (0 != connect(so->tcp, (struct sockaddr *)&so->remote, dns_sa_len(&so->remote))) {
			if (dns_soerr() != DNS_EISCONN)
				goto soerr;
		}

		so->state++;
	case DNS_SO_TCP_SEND:
		if ((error = dns_so_tcp_send(so)))
			goto error;

		so->state++;
	case DNS_SO_TCP_RECV:
		if ((error = dns_so_tcp_recv(so)))
			goto error;

		so->state++;
	case DNS_SO_TCP_DONE:
		/* close unless DNS_RESCONF_TCP_ONLY (see dns_res_tcp2type) */
		if (so->type != SOCK_STREAM) {
			if ((error = dns_so_closefd(so, &so->tcp)))
				goto error;
		}

		if (so->answer->end < 12)
			return DNS_EILLEGAL;

		if ((error = dns_so_verify(so, so->answer)))
			goto error;

		return 0;
	default:
		error	= DNS_EUNKNOWN;

		goto error;
	} /* switch() */

trash:
	goto retry;
soerr:
	error	= dns_soerr();

	goto error;
error:
	switch (error) {
	case DNS_EINTR:
		goto retry;
	case DNS_EINPROGRESS:
		/* FALL THROUGH */
	case DNS_EALREADY:
		/* FALL THROUGH */
#if DNS_EWOULDBLOCK != DNS_EAGAIN
	case DNS_EWOULDBLOCK:
		/* FALL THROUGH */
#endif
		error	= DNS_EAGAIN;

		break;
	} /* switch() */

	return error;
} /* dns_so_check() */


struct dns_packet *dns_so_fetch(struct dns_socket *so, int *error) {
	struct dns_packet *answer;

	switch (so->state) {
	case DNS_SO_UDP_DONE:
	case DNS_SO_TCP_DONE:
		answer		= so->answer;
		so->answer	= 0;

		return answer;
	default:
		*error	= DNS_EUNKNOWN;

		return 0;
	}
} /* dns_so_fetch() */


struct dns_packet *dns_so_query(struct dns_socket *so, struct dns_packet *Q, struct sockaddr *host, int *error_) {
	struct dns_packet *A;
	int error;

	if (!so->state) {
		if ((error = dns_so_submit(so, Q, host)))
			goto error;
	}

	if ((error = dns_so_check(so)))
		goto error;

	if (!(A = dns_so_fetch(so, &error)))
		goto error;

	dns_so_reset(so);

	return A;
error:
	*error_	= error;

	return 0;
} /* dns_so_query() */


time_t dns_so_elapsed(struct dns_socket *so) {
	return dns_elapsed(&so->elapsed);
} /* dns_so_elapsed() */


void dns_so_clear(struct dns_socket *so) {
	dns_so_closefds(so, DNS_SO_CLOSE_OLD);
} /* dns_so_clear() */


static int dns_so_events2(struct dns_socket *so, enum dns_events type) {
	int events = 0;

	switch (so->state) {
	case DNS_SO_UDP_CONN:
	case DNS_SO_UDP_SEND:
		events |= DNS_POLLOUT;

		break;
	case DNS_SO_UDP_RECV:
		events |= DNS_POLLIN;

		break;
	case DNS_SO_TCP_CONN:
	case DNS_SO_TCP_SEND:
		events |= DNS_POLLOUT;

		break;
	case DNS_SO_TCP_RECV:
		events |= DNS_POLLIN;

		break;
	} /* switch() */

	switch (type) {
	case DNS_LIBEVENT:
		return DNS_POLL2EV(events);
	default:
		return events;
	} /* switch() */
} /* dns_so_events2() */


int dns_so_events(struct dns_socket *so) {
	return dns_so_events2(so, so->opts.events);
} /* dns_so_events() */


int dns_so_pollfd(struct dns_socket *so) {
	switch (so->state) {
	case DNS_SO_UDP_CONN:
	case DNS_SO_UDP_SEND:
	case DNS_SO_UDP_RECV:
		return so->udp;
	case DNS_SO_TCP_CONN:
	case DNS_SO_TCP_SEND:
	case DNS_SO_TCP_RECV:
		return so->tcp;
	} /* switch() */

	return -1;
} /* dns_so_pollfd() */


int dns_so_poll(struct dns_socket *so, int timeout) {
	return dns_poll(dns_so_pollfd(so), dns_so_events2(so, DNS_SYSPOLL), timeout);
} /* dns_so_poll() */


const struct dns_stat *dns_so_stat(struct dns_socket *so) {
	return &so->stat;
} /* dns_so_stat() */


/*
 * R E S O L V E R  R O U T I N E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

enum dns_res_state {
	DNS_R_INIT,
	DNS_R_GLUE,
	DNS_R_SWITCH,		/* (B)IND, (F)ILE, (C)ACHE */

	DNS_R_FILE,		/* Lookup in local hosts database */

	DNS_R_CACHE,		/* Lookup in application cache */
	DNS_R_SUBMIT,
	DNS_R_CHECK,
	DNS_R_FETCH,

	DNS_R_BIND,		/* Lookup in the network */
	DNS_R_SEARCH,
	DNS_R_HINTS,
	DNS_R_ITERATE,
	DNS_R_FOREACH_NS,
	DNS_R_RESOLV0_NS,	/* Prologue: Setup next frame and recurse */
	DNS_R_RESOLV1_NS,	/* Epilog: Inspect answer */
	DNS_R_FOREACH_A,
	DNS_R_QUERY_A,
	DNS_R_CNAME0_A,
	DNS_R_CNAME1_A,

	DNS_R_FINISH,
	DNS_R_SMART0_A,
	DNS_R_SMART1_A,
	DNS_R_DONE,
	DNS_R_SERVFAIL,
}; /* enum dns_res_state */


#define DNS_R_MAXDEPTH	8
#define DNS_R_ENDFRAME	(DNS_R_MAXDEPTH - 1)

struct dns_resolver {
	struct dns_socket so;

	struct dns_resolv_conf *resconf;
	struct dns_hosts *hosts;
	struct dns_hints *hints;
	struct dns_cache *cache;

	dns_atomic_t refcount;

	/* Reset zeroes everything below here. */

	char qname[DNS_D_MAXNAME + 1];
	size_t qlen;

	enum dns_type qtype;
	enum dns_class qclass;

	struct dns_clock elapsed;

	dns_resconf_i_t search;

	struct dns_rr_i smart;

	struct dns_packet *nodata; /* answer if nothing better */

	unsigned sp;

	struct dns_res_frame {
		enum dns_res_state state;

		int error;
		int which;	/* (B)IND, (F)ILE; index into resconf->lookup */

		unsigned attempts;

		struct dns_packet *query, *answer, *hints;

		struct dns_rr_i hints_i, hints_j;
		struct dns_rr hints_ns, ans_cname;
	} stack[DNS_R_MAXDEPTH];
}; /* struct dns_resolver */


static int dns_res_tcp2type(int tcp) {
	switch (tcp) {
	case DNS_RESCONF_TCP_ONLY:
		return SOCK_STREAM;
	case DNS_RESCONF_TCP_DISABLE:
		return SOCK_DGRAM;
	default:
		return 0;
	}
} /* dns_res_tcp2type() */

struct dns_resolver *dns_res_open(struct dns_resolv_conf *resconf, struct dns_hosts *hosts, struct dns_hints *hints, struct dns_cache *cache, const struct dns_options *opts, int *_error) {
	static const struct dns_resolver R_initializer
		= { .refcount = 1, };
	struct dns_resolver *R	= 0;
	int type, error;

	/*
	 * Grab ref count early because the caller may have passed us a mortal
	 * reference, and we want to do the right thing if we return early
	 * from an error.
	 */
	if (resconf)
		dns_resconf_acquire(resconf);
	if (hosts)
		dns_hosts_acquire(hosts);
	if (hints)
		dns_hints_acquire(hints);
	if (cache)
		dns_cache_acquire(cache);

	/*
	 * Don't try to load it ourselves because a NULL object might be an
	 * error from, say, dns_resconf_root(), and loading
	 * dns_resconf_local() by default would create undesirable surpises.
	 */
	if (!resconf || !hosts || !hints)
		goto _error;

	if (!(R = malloc(sizeof *R)))
		goto syerr;

	*R	= R_initializer;
	type	= dns_res_tcp2type(resconf->options.tcp);

	if (!dns_so_init(&R->so, (struct sockaddr *)&resconf->iface, type, opts, &error))
		goto error;

	R->resconf	= resconf;
	R->hosts	= hosts;
	R->hints	= hints;
	R->cache	= cache;

	return R;
syerr:
	error	= dns_syerr();
error:
	*_error	= error;
_error:
	dns_res_close(R);

	dns_resconf_close(resconf);
	dns_hosts_close(hosts);
	dns_hints_close(hints);
	dns_cache_close(cache);

	return 0;
} /* dns_res_open() */


struct dns_resolver *dns_res_stub(const struct dns_options *opts, int *error) {
	struct dns_resolv_conf *resconf	= 0;
	struct dns_hosts *hosts		= 0;
	struct dns_hints *hints		= 0;
	struct dns_resolver *res	= 0;

	if (!(resconf = dns_resconf_local(error)))
		goto epilog;

	if (!(hosts = dns_hosts_local(error)))
		goto epilog;

	if (!(hints = dns_hints_local(resconf, error)))
		goto epilog;

	if (!(res = dns_res_open(resconf, hosts, hints, NULL, opts, error)))
		goto epilog;

epilog:
	dns_resconf_close(resconf);
	dns_hosts_close(hosts);
	dns_hints_close(hints);

	return res;
} /* dns_res_stub() */


static void dns_res_reset_frame(struct dns_resolver *R, struct dns_res_frame *frame) {
	(void)R;

	dns_p_setptr(&frame->query, NULL);
	dns_p_setptr(&frame->answer, NULL);
	dns_p_setptr(&frame->hints, NULL);

	memset(frame, '\0', sizeof *frame);
} /* dns_res_reset_frame() */


void dns_res_reset(struct dns_resolver *R) {
	unsigned i;

	dns_so_reset(&R->so);

	dns_p_setptr(&R->nodata, NULL);

	for (i = 0; i < lengthof(R->stack); i++)
		dns_res_reset_frame(R, &R->stack[i]);

	memset(&R->qname, '\0', sizeof *R - offsetof(struct dns_resolver, qname));
} /* dns_res_reset() */


void dns_res_close(struct dns_resolver *R) {
	if (!R || 1 < dns_res_release(R))
		return;

	dns_res_reset(R);

	dns_so_destroy(&R->so);

	dns_hints_close(R->hints);
	dns_hosts_close(R->hosts);
	dns_resconf_close(R->resconf);
	dns_cache_close(R->cache);

	free(R);
} /* dns_res_close() */


dns_refcount_t dns_res_acquire(struct dns_resolver *R) {
	return dns_atomic_fetch_add(&R->refcount);
} /* dns_res_acquire() */


dns_refcount_t dns_res_release(struct dns_resolver *R) {
	return dns_atomic_fetch_sub(&R->refcount);
} /* dns_res_release() */


struct dns_resolver *dns_res_mortal(struct dns_resolver *res) {
	if (res)
		dns_res_release(res);
	return res;
} /* dns_res_mortal() */


static struct dns_packet *dns_res_merge(struct dns_packet *P0, struct dns_packet *P1, int *error_) {
	size_t bufsiz	= P0->end + P1->end;
	struct dns_packet *P[3]	= { P0, P1, 0 };
	struct dns_rr rr[3];
	int error, copy, i;
	enum dns_section section;

retry:
	if (!(P[2] = dns_p_make(bufsiz, &error)))
		goto error;

	dns_rr_foreach(&rr[0], P[0], .section = DNS_S_QD) {
		if ((error = dns_rr_copy(P[2], &rr[0], P[0])))
			goto error;
	}

	for (section = DNS_S_AN; (DNS_S_ALL & section); section <<= 1) {
		for (i = 0; i < 2; i++) {
			dns_rr_foreach(&rr[i], P[i], .section = section) {
				copy	= 1;

				dns_rr_foreach(&rr[2], P[2], .type = rr[i].type, .section = (DNS_S_ALL & ~DNS_S_QD)) {
					if (0 == dns_rr_cmp(&rr[i], P[i], &rr[2], P[2])) {
						copy	= 0;

						break;
					}
				}

				if (copy && (error = dns_rr_copy(P[2], &rr[i], P[i]))) {
					if (error == DNS_ENOBUFS && bufsiz < 65535) {
						dns_p_setptr(&P[2], NULL);

						bufsiz	= DNS_PP_MAX(65535, bufsiz * 2);

						goto retry;
					}

					goto error;
				}
			} /* foreach(rr) */
		} /* foreach(packet) */
	} /* foreach(section) */

	return P[2];
error:
	*error_	= error;

	dns_p_free(P[2]);

	return 0;
} /* dns_res_merge() */


static struct dns_packet *dns_res_glue(struct dns_resolver *R, struct dns_packet *Q) {
	struct dns_packet *P	= dns_p_new(512);
	char qname[DNS_D_MAXNAME + 1];
	size_t qlen;
	enum dns_type qtype;
	struct dns_rr rr;
	unsigned sp;
	int error;

	if (!(qlen = dns_d_expand(qname, sizeof qname, 12, Q, &error))
	||  qlen >= sizeof qname)
		return 0;

	if (!(qtype = dns_rr_type(12, Q)))
		return 0;

	if ((error = dns_p_push(P, DNS_S_QD, qname, strlen(qname), qtype, DNS_C_IN, 0, 0)))
		return 0;

	for (sp = 0; sp <= R->sp; sp++) {
		if (!R->stack[sp].answer)
			continue;

		dns_rr_foreach(&rr, R->stack[sp].answer, .name = qname, .type = qtype, .section = (DNS_S_ALL & ~DNS_S_QD)) {
			rr.section	= DNS_S_AN;

			if ((error = dns_rr_copy(P, &rr, R->stack[sp].answer)))
				return 0;
		}
	}

	if (dns_p_count(P, DNS_S_AN) > 0)
		goto copy;

	/* Otherwise, look for a CNAME */
	for (sp = 0; sp <= R->sp; sp++) {
		if (!R->stack[sp].answer)
			continue;

		dns_rr_foreach(&rr, R->stack[sp].answer, .name = qname, .type = DNS_T_CNAME, .section = (DNS_S_ALL & ~DNS_S_QD)) {
			rr.section	= DNS_S_AN;

			if ((error = dns_rr_copy(P, &rr, R->stack[sp].answer)))
				return 0;
		}
	}

	if (!dns_p_count(P, DNS_S_AN))
		return 0;

copy:
	return dns_p_copy(dns_p_make(P->end, &error), P);
} /* dns_res_glue() */


static struct dns_packet *dns_res_mkquery(struct dns_resolver *R, const char *qname, enum dns_type qtype, enum dns_class qclass, int *error_) {
	struct dns_packet *Q	= 0;
	int error;

	if (!(Q = dns_p_init(malloc(DNS_P_QBUFSIZ), DNS_P_QBUFSIZ)))
		goto syerr;

	if ((error = dns_p_push(Q, DNS_S_QD, qname, strlen(qname), qtype, qclass, 0, 0)))
		goto error;

	dns_header(Q)->rd	= !R->resconf->options.recurse;

	return Q;
syerr:
	error	= dns_syerr();
error:
	dns_p_free(Q);

	*error_	= error;

	return 0;
} /* dns_res_mkquery() */


/*
 * Sort NS records by three criteria:
 *
 * 	1) Whether glue is present.
 * 	2) Whether glue record is original or of recursive lookup.
 * 	3) Randomly shuffle records which share the above criteria.
 *
 * NOTE: Assumes only NS records passed, AND ASSUMES no new NS records will
 *       be added during an iteration.
 *
 * FIXME: Only groks A glue, not AAAA glue.
 */
static int dns_res_nameserv_cmp(struct dns_rr *a, struct dns_rr *b, struct dns_rr_i *i, struct dns_packet *P) {
	_Bool glued[2] = { 0 };
	struct dns_rr x = { 0 }, y = { 0 };
	struct dns_ns ns;
	int cmp, error;

	if (!(error = dns_ns_parse(&ns, a, P)))
		glued[0] = !!dns_rr_grep(&x, 1, dns_rr_i_new(P, .section = (DNS_S_ALL & ~DNS_S_QD), .name = ns.host, .type = DNS_T_A), P, &error);

	if (!(error = dns_ns_parse(&ns, b, P)))
		glued[1] = !!dns_rr_grep(&y, 1, dns_rr_i_new(P, .section = (DNS_S_ALL & ~DNS_S_QD), .name = ns.host, .type = DNS_T_A), P, &error);

	if ((cmp = glued[1] - glued[0])) {
		return cmp;
	} else if ((cmp = (dns_rr_offset(&y) < i->args[0]) - (dns_rr_offset(&x) < i->args[0]))) {
		return cmp;
	} else {
		return dns_rr_i_shuffle(a, b, i, P);
	}
} /* dns_res_nameserv_cmp() */


#define dgoto(sp, i)	\
	do { R->stack[(sp)].state = (i); goto exec; } while (0)

static int dns_res_exec(struct dns_resolver *R) {
	struct dns_res_frame *F;
	struct dns_packet *P;
	char host[DNS_D_MAXNAME + 1];
	size_t len;
	struct dns_rr rr;
	struct sockaddr_in sin;
	int error;

exec:

	F	= &R->stack[R->sp];

	switch (F->state) {
	case DNS_R_INIT:
		F->state++;
	case DNS_R_GLUE:
		if (R->sp == 0)
			dgoto(R->sp, DNS_R_SWITCH);

		if (!F->query)
			goto noquery;

		if (!(F->answer = dns_res_glue(R, F->query)))
			dgoto(R->sp, DNS_R_SWITCH);

		if (!(len = dns_d_expand(host, sizeof host, 12, F->query, &error)))
			goto error;
		else if (len >= sizeof host)
			goto toolong;

		dns_rr_foreach(&rr, F->answer, .name = host, .type = dns_rr_type(12, F->query), .section = DNS_S_AN) {
			dgoto(R->sp, DNS_R_FINISH);
		}

		dns_rr_foreach(&rr, F->answer, .name = host, .type = DNS_T_CNAME, .section = DNS_S_AN) {
			F->ans_cname	= rr;

			dgoto(R->sp, DNS_R_CNAME0_A);
		}

		F->state++;
	case DNS_R_SWITCH:
		while (F->which < (int)sizeof R->resconf->lookup && R->resconf->lookup[F->which]) {
			switch (R->resconf->lookup[F->which++]) {
			case 'b': case 'B':
				dgoto(R->sp, DNS_R_BIND);
			case 'f': case 'F':
				dgoto(R->sp, DNS_R_FILE);
			case 'c': case 'C':
				if (R->cache)
					dgoto(R->sp, DNS_R_CACHE);

				break;
			default:
				break;
			}
		}

		/*
		 * FIXME: Examine more closely whether our logic is correct
		 * and DNS_R_SERVFAIL is the correct default response.
		 *
		 * Case 1: We got here because we never got an answer on the
		 *   wire. All queries timed-out and we reached maximum
		 *   attempts count. See DNS_R_FOREACH_NS. In that case
		 *   DNS_R_SERVFAIL is the correct state, unless we want to
		 *   return DNS_ETIMEDOUT.
		 *
		 * Case 2: We were a stub resolver and got an unsatisfactory
		 *   answer (empty ANSWER section) which caused us to jump
		 *   back to DNS_R_SEARCH and ultimately to DNS_R_SWITCH. We
		 *   return the answer returned from the wire, which we
		 *   stashed in R->nodata.
		 *
		 * Case 3: We reached maximum attempts count as in case #1,
		 *   but never got an authoritative response which caused us
		 *   to short-circuit. See end of DNS_R_QUERY_A case. We
		 *   should probably prepare R->nodata as in case #2.
		 */
		if (R->sp == 0 && R->nodata) { /* XXX: can we just return nodata regardless? */
			dns_p_movptr(&F->answer, &R->nodata);
			dgoto(R->sp, DNS_R_FINISH);
		}

		dgoto(R->sp, DNS_R_SERVFAIL);
	case DNS_R_FILE:
		if (R->sp > 0) {
			if (!dns_p_setptr(&F->answer, dns_hosts_query(R->hosts, F->query, &error)))
				goto error;

			if (dns_p_count(F->answer, DNS_S_AN) > 0)
				dgoto(R->sp, DNS_R_FINISH);

			dns_p_setptr(&F->answer, NULL);
		} else {
			R->search	= 0;

			while ((len = dns_resconf_search(host, sizeof host, R->qname, R->qlen, R->resconf, &R->search))) {
/*
 * FIXME: Some sort of bug, either with this code or with GCC 3.3.5 on
 * OpenBSD 4.4, overwites the stack guard. If the bug is in this file, it
 * appears to be localized somewhere around here. It can also be mitigated
 * in dns_hosts_query(). In any event, the bug manifests only when using
 * compound literals. alloca(), malloc(), calloc(), etc, all work fine.
 * Valgrind (tested on Linux) cannot detect any issues, but stack issues are
 * not Valgrind's forte. Neither can I spot anything in the assembly, but
 * that's not my forte.
 */
#if __OpenBSD__ && __GNUC__
				struct dns_packet *query	= __builtin_alloca(DNS_P_QBUFSIZ);

				dns_p_init(query, DNS_P_QBUFSIZ);
#else
				struct dns_packet *query	= dns_p_new(DNS_P_QBUFSIZ);
#endif

				if ((error = dns_p_push(query, DNS_S_QD, host, len, R->qtype, R->qclass, 0, 0)))
					goto error;

				if (!dns_p_setptr(&F->answer, dns_hosts_query(R->hosts, query, &error)))
					goto error;

				if (dns_p_count(F->answer, DNS_S_AN) > 0)
					dgoto(R->sp, DNS_R_FINISH);

				dns_p_setptr(&F->answer, NULL);
			}
		}

		dgoto(R->sp, DNS_R_SWITCH);
	case DNS_R_CACHE:
		error = 0;

		if (!F->query && !(F->query = dns_res_mkquery(R, R->qname, R->qtype, R->qclass, &error)))
			goto error;

		if (dns_p_setptr(&F->answer, R->cache->query(F->query, R->cache, &error))) {
			if (dns_p_count(F->answer, DNS_S_AN) > 0)
				dgoto(R->sp, DNS_R_FINISH);

			dns_p_setptr(&F->answer, NULL);

			dgoto(R->sp, DNS_R_SWITCH);
		} else if (error)
			goto error;

		F->state++;
	case DNS_R_SUBMIT:
		if ((error = R->cache->submit(F->query, R->cache)))
			goto error;

		F->state++;
	case DNS_R_CHECK:
		if ((error = R->cache->check(R->cache)))
			goto error;

		F->state++;
	case DNS_R_FETCH:
		error = 0;

		if (dns_p_setptr(&F->answer, R->cache->fetch(R->cache, &error))) {
			if (dns_p_count(F->answer, DNS_S_AN) > 0)
				dgoto(R->sp, DNS_R_FINISH);

			dns_p_setptr(&F->answer, NULL);

			dgoto(R->sp, DNS_R_SWITCH);
		} else if (error)
			goto error;

		dgoto(R->sp, DNS_R_SWITCH);
	case DNS_R_BIND:
		if (R->sp > 0) {
			if (!F->query)
				goto noquery;

			dgoto(R->sp, DNS_R_HINTS);
		}

		R->search	= 0;

		F->state++;
	case DNS_R_SEARCH:
		/*
		 * XXX: We probably should only apply the domain search
		 * algorithm if R->sp == 0.
		 */
		if (!(len = dns_resconf_search(host, sizeof host, R->qname, R->qlen, R->resconf, &R->search)))
			dgoto(R->sp, DNS_R_SWITCH);

		if (!(P = dns_p_make(DNS_P_QBUFSIZ, &error)))
			goto error;

		dns_header(P)->rd	= !R->resconf->options.recurse;

		dns_p_setptr(&F->query, P);

		if ((error = dns_p_push(F->query, DNS_S_QD, host, len, R->qtype, R->qclass, 0, 0)))
			goto error;

		F->state++;
	case DNS_R_HINTS:
		if (!dns_p_setptr(&F->hints, dns_hints_query(R->hints, F->query, &error)))
			goto error;

		F->state++;
	case DNS_R_ITERATE:
		dns_rr_i_init(&F->hints_i, F->hints);

		F->hints_i.section	= DNS_S_AUTHORITY;
		F->hints_i.type		= DNS_T_NS;
		F->hints_i.sort		= &dns_res_nameserv_cmp;
		F->hints_i.args[0]	= F->hints->end;

		F->state++;
	case DNS_R_FOREACH_NS:
		dns_rr_i_save(&F->hints_i);

		/* Load our next nameserver host. */
		if (!dns_rr_grep(&F->hints_ns, 1, &F->hints_i, F->hints, &error)) {
			if (++F->attempts < R->resconf->options.attempts)
				dgoto(R->sp, DNS_R_ITERATE);

			dgoto(R->sp, DNS_R_SWITCH);
		}

		dns_rr_i_init(&F->hints_j, F->hints);

		/* Assume there are glue records */
		dgoto(R->sp, DNS_R_FOREACH_A);
	case DNS_R_RESOLV0_NS:
		/* Have we reached our max depth? */
		if (&F[1] >= endof(R->stack))
			dgoto(R->sp, DNS_R_FOREACH_NS);

		dns_res_reset_frame(R, &F[1]);

		if (!(F[1].query = dns_p_make(DNS_P_QBUFSIZ, &error)))
			goto error;

		if ((error = dns_ns_parse((struct dns_ns *)host, &F->hints_ns, F->hints)))
			goto error;

		if ((error = dns_p_push(F[1].query, DNS_S_QD, host, strlen(host), DNS_T_A, DNS_C_IN, 0, 0)))
			goto error;

		F->state++;

		dgoto(++R->sp, DNS_R_INIT);
	case DNS_R_RESOLV1_NS:
		if (!(len = dns_d_expand(host, sizeof host, 12, F[1].query, &error)))
			goto error;
		else if (len >= sizeof host)
			goto toolong;

		dns_rr_foreach(&rr, F[1].answer, .name = host, .type = DNS_T_A, .section = (DNS_S_ALL & ~DNS_S_QD)) {
			rr.section	= DNS_S_AR;

			if ((error = dns_rr_copy(F->hints, &rr, F[1].answer)))
				goto error;

			dns_rr_i_rewind(&F->hints_i);	/* Now there's glue. */
		}

		dgoto(R->sp, DNS_R_FOREACH_NS);
	case DNS_R_FOREACH_A:
		/*
		 * NOTE: Iterator initialized in DNS_R_FOREACH_NS because
		 * this state is re-entrant, but we need to reset
		 * .name to a valid pointer each time.
		 */
		if ((error = dns_ns_parse((struct dns_ns *)host, &F->hints_ns, F->hints)))
			goto error;

		F->hints_j.name		= host;
		F->hints_j.type		= DNS_T_A;
		F->hints_j.section	= DNS_S_ALL & ~DNS_S_QD;

		if (!dns_rr_grep(&rr, 1, &F->hints_j, F->hints, &error)) {
			if (!dns_rr_i_count(&F->hints_j))
				dgoto(R->sp, DNS_R_RESOLV0_NS);

			dgoto(R->sp, DNS_R_FOREACH_NS);
		}

		sin.sin_family	= AF_INET;

		if ((error = dns_a_parse((struct dns_a *)&sin.sin_addr, &rr, F->hints)))
			goto error;

		if (R->sp == 0)
			sin.sin_port = dns_hints_port(R->hints, AF_INET, (struct sockaddr *)&sin.sin_addr);
		else
			sin.sin_port = htons(53);

		if (DNS_DEBUG) {
			char addr[INET_ADDRSTRLEN + 1];
			dns_a_print(addr, sizeof addr, (struct dns_a *)&sin.sin_addr);
			DNS_SHOW(F->query, "ASKING: %s/%s @ DEPTH: %u)", host, addr, R->sp);
		}

		if ((error = dns_so_submit(&R->so, F->query, (struct sockaddr *)&sin)))
			goto error;

		F->state++;
	case DNS_R_QUERY_A:
		if (dns_so_elapsed(&R->so) >= dns_resconf_timeout(R->resconf))
			dgoto(R->sp, DNS_R_FOREACH_A);

		if ((error = dns_so_check(&R->so)))
			goto error;

		if (!dns_p_setptr(&F->answer, dns_so_fetch(&R->so, &error)))
			goto error;

		if (DNS_DEBUG) {
			DNS_SHOW(F->answer, "ANSWER @ DEPTH: %u)", R->sp);
		}

		if ((error = dns_rr_parse(&rr, 12, F->query)))
			goto error;

		if (!(len = dns_d_expand(host, sizeof host, rr.dn.p, F->query, &error)))
			goto error;
		else if (len >= sizeof host)
			goto toolong;

		dns_rr_foreach(&rr, F->answer, .section = DNS_S_AN, .name = host, .type = rr.type) {
			dgoto(R->sp, DNS_R_FINISH);	/* Found */
		}

		dns_rr_foreach(&rr, F->answer, .section = DNS_S_AN, .name = host, .type = DNS_T_CNAME) {
			F->ans_cname	= rr;

			dgoto(R->sp, DNS_R_CNAME0_A);
		}

		/*
		 * XXX: The condition here should probably check whether
		 * R->sp == 0, because DNS_R_SEARCH runs regardless of
		 * options.recurse. See DNS_R_BIND.
		 */
		if (!R->resconf->options.recurse) {
			/* Make first answer our tentative answer */
			if (!R->nodata)
				dns_p_movptr(&R->nodata, &F->answer);

			dgoto(R->sp, DNS_R_SEARCH);
		}

		dns_rr_foreach(&rr, F->answer, .section = DNS_S_NS, .type = DNS_T_NS) {
			dns_p_movptr(&F->hints, &F->answer);

			dgoto(R->sp, DNS_R_ITERATE);
		}

		/* XXX: Should this go further up? */
		if (dns_header(F->answer)->aa)
			dgoto(R->sp, DNS_R_FINISH);

		/* XXX: Should we copy F->answer to R->nodata? */

		dgoto(R->sp, DNS_R_FOREACH_A);
	case DNS_R_CNAME0_A:
		if (&F[1] >= endof(R->stack))
			dgoto(R->sp, DNS_R_FINISH);

		if ((error = dns_cname_parse((struct dns_cname *)host, &F->ans_cname, F->answer)))
			goto error;

		dns_res_reset_frame(R, &F[1]);

		if (!(F[1].query = dns_p_make(DNS_P_QBUFSIZ, &error)))
			goto error;

		if ((error = dns_p_push(F[1].query, DNS_S_QD, host, strlen(host), dns_rr_type(12, F->query), DNS_C_IN, 0, 0)))
			goto error;

		F->state++;

		dgoto(++R->sp, DNS_R_INIT);
	case DNS_R_CNAME1_A:
		if (!(P = dns_res_merge(F->answer, F[1].answer, &error)))
			goto error;

		dns_p_setptr(&F->answer, P);

		dgoto(R->sp, DNS_R_FINISH);
	case DNS_R_FINISH:
		if (!F->answer)
			goto noanswer;

		if (!R->resconf->options.smart || R->sp > 0)
			dgoto(R->sp, DNS_R_DONE);

		R->smart.section	= DNS_S_AN;
		R->smart.type		= R->qtype;

		dns_rr_i_init(&R->smart, F->answer);

		F->state++;
	case DNS_R_SMART0_A:
		if (&F[1] >= endof(R->stack))
			dgoto(R->sp, DNS_R_DONE);

		while (dns_rr_grep(&rr, 1, &R->smart, F->answer, &error)) {
			union {
				struct dns_ns ns;
				struct dns_mx mx;
				struct dns_srv srv;
			} rd;
			const char *qname;
			enum dns_type qtype;
			enum dns_class qclass;

			switch (rr.type) {
			case DNS_T_NS:
				if ((error = dns_ns_parse(&rd.ns, &rr, F->answer)))
					goto error;

				qname	= rd.ns.host;
				qtype	= DNS_T_A;
				qclass	= DNS_C_IN;

				break;
			case DNS_T_MX:
				if ((error = dns_mx_parse(&rd.mx, &rr, F->answer)))
					goto error;

				qname	= rd.mx.host;
				qtype	= DNS_T_A;
				qclass	= DNS_C_IN;

				break;
			case DNS_T_SRV:
				if ((error = dns_srv_parse(&rd.srv, &rr, F->answer)))
					goto error;

				qname	= rd.srv.target;
				qtype	= DNS_T_A;
				qclass	= DNS_C_IN;

				break;
			default:
				continue;
			} /* switch() */

			dns_res_reset_frame(R, &F[1]);

			if (!(F[1].query = dns_res_mkquery(R, qname, qtype, qclass, &error)))
				goto error;

			F->state++;

			dgoto(++R->sp, DNS_R_INIT);
		} /* while() */

		/*
		 * NOTE: SMTP specification says to fallback to A record.
		 *
		 * XXX: Should we add a mock MX answer?
		 */
		if (R->qtype == DNS_T_MX && R->smart.state.count == 0) {
			dns_res_reset_frame(R, &F[1]);

			if (!(F[1].query = dns_res_mkquery(R, R->qname, DNS_T_A, DNS_C_IN, &error)))
				goto error;

			R->smart.state.count++;
			F->state++;

			dgoto(++R->sp, DNS_R_INIT);
		}

		dgoto(R->sp, DNS_R_DONE);
	case DNS_R_SMART1_A:
		if (!F[1].answer)
			goto noanswer;

		/*
		 * FIXME: For CNAME chains (which are typically illegal in
		 * this context), we should rewrite the record host name
		 * to the original smart qname. All the user cares about
		 * is locating that A/AAAA record.
		 */
		dns_rr_foreach(&rr, F[1].answer, .section = DNS_S_AN, .type = DNS_T_A) {
			rr.section	= DNS_S_AR;

			if (dns_rr_exists(&rr, F[1].answer, F->answer))
				continue;

			while ((error = dns_rr_copy(F->answer, &rr, F[1].answer))) {
				if (error != DNS_ENOBUFS)
					goto error;
				if ((error = dns_p_grow(&F->answer)))
					goto error;
			}
		}

		dgoto(R->sp, DNS_R_SMART0_A);
	case DNS_R_DONE:
		if (!F->answer)
			goto noanswer;

		if (R->sp > 0)
			dgoto(--R->sp, F[-1].state);

		break;
	case DNS_R_SERVFAIL:
		if (!dns_p_setptr(&F->answer, dns_p_make(DNS_P_QBUFSIZ, &error)))
			goto error;

		dns_header(F->answer)->qr	= 1;
		dns_header(F->answer)->rcode	= DNS_RC_SERVFAIL;

		if ((error = dns_p_push(F->answer, DNS_S_QD, R->qname, strlen(R->qname), R->qtype, R->qclass, 0, 0)))
			goto error;

		dgoto(R->sp, DNS_R_DONE);
	default:
		error	= EINVAL;

		goto error;
	} /* switch () */

	return 0;
noquery:
	error = DNS_ENOQUERY;

	goto error;
noanswer:
	error = DNS_ENOANSWER;

	goto error;
toolong:
	error = DNS_EILLEGAL;

	/* FALL THROUGH */
error:
	return error;
} /* dns_res_exec() */

#undef goto


void dns_res_clear(struct dns_resolver *R) {
	switch (R->stack[R->sp].state) {
	case DNS_R_CHECK:
		R->cache->clear(R->cache);
		break;
	default:
		dns_so_clear(&R->so);
		break;
	}
} /* dns_res_clear() */


static int dns_res_events2(struct dns_resolver *R, enum dns_events type) {
	int events;

	switch (R->stack[R->sp].state) {
	case DNS_R_CHECK:
		events = R->cache->events(R->cache);

		return (type == DNS_LIBEVENT)? DNS_POLL2EV(events) : events;
	default:
		return dns_so_events2(&R->so, type);
	}
} /* dns_res_events2() */


int dns_res_events(struct dns_resolver *R) {
	return dns_res_events2(R, R->so.opts.events);
} /* dns_res_events() */


int dns_res_pollfd(struct dns_resolver *R) {
	switch (R->stack[R->sp].state) {
	case DNS_R_CHECK:
		return R->cache->pollfd(R->cache);
	default:
		return dns_so_pollfd(&R->so);
	}
} /* dns_res_pollfd() */


time_t dns_res_timeout(struct dns_resolver *R) {
	time_t elapsed;

	switch (R->stack[R->sp].state) {
#if 0
	case DNS_R_QUERY_AAAA:
#endif
	case DNS_R_QUERY_A:
		elapsed = dns_so_elapsed(&R->so);

		if (elapsed <= dns_resconf_timeout(R->resconf))
			return R->resconf->options.timeout - elapsed;

		break;
	default:
		break;
	} /* switch() */

	/*
	 * NOTE: We're not in a pollable state, or the user code hasn't
	 * called dns_res_check properly. The calling code is probably
	 * broken. Put them into a slow-burn pattern.
	 */
	return 1;
} /* dns_res_timeout() */


time_t dns_res_elapsed(struct dns_resolver *R) {
	return dns_elapsed(&R->elapsed);
} /* dns_res_elapsed() */


int dns_res_poll(struct dns_resolver *R, int timeout) {
	return dns_poll(dns_res_pollfd(R), dns_res_events2(R, DNS_SYSPOLL), timeout);
} /* dns_res_poll() */


int dns_res_submit2(struct dns_resolver *R, const char *qname, size_t qlen, enum dns_type qtype, enum dns_class qclass) {
	dns_res_reset(R);

	/* Don't anchor; that can conflict with searchlist generation. */
	dns_d_init(R->qname, sizeof R->qname, qname, (R->qlen = qlen), 0);

	R->qtype	= qtype;
	R->qclass	= qclass;

	dns_begin(&R->elapsed);

	return 0;
} /* dns_res_submit2() */


int dns_res_submit(struct dns_resolver *R, const char *qname, enum dns_type qtype, enum dns_class qclass) {
	return dns_res_submit2(R, qname, strlen(qname), qtype, qclass);
} /* dns_res_submit() */


int dns_res_check(struct dns_resolver *R) {
	int error;

	if (R->stack[0].state != DNS_R_DONE) {
		if ((error = dns_res_exec(R)))
			return error;
	}

	return 0;
} /* dns_res_check() */


struct dns_packet *dns_res_fetch(struct dns_resolver *R, int *error) {
	struct dns_packet *answer;

	if (R->stack[0].state != DNS_R_DONE) {
		*error = DNS_EUNKNOWN;

		return 0;
	}

	if (!(answer = R->stack[0].answer)) {
		*error = DNS_EFETCHED;

		return 0;
	}

	R->stack[0].answer = 0;

	return answer;
} /* dns_res_fetch() */


struct dns_packet *dns_res_query(struct dns_resolver *res, const char *qname, enum dns_type qtype, enum dns_class qclass, int timeout, int *error_) {
	int error;

	if ((error = dns_res_submit(res, qname, qtype, qclass)))
		goto error;

	while ((error = dns_res_check(res))) {
		if (dns_res_elapsed(res) > timeout)
			error = DNS_ETIMEDOUT;

		if (error != DNS_EAGAIN)
			goto error;

		if ((error = dns_res_poll(res, 1)))
			goto error;
	}

	return dns_res_fetch(res, error_);
error:
	*error_ = error;

	return 0;
} /* dns_res_query() */


const struct dns_stat *dns_res_stat(struct dns_resolver *res) {
	return dns_so_stat(&res->so);
} /* dns_res_stat() */


void dns_res_sethints(struct dns_resolver *res, struct dns_hints *hints) {
	dns_hints_acquire(hints); /* acquire first in case same hints object */
	dns_hints_close(res->hints);
	res->hints = hints;
} /* dns_res_sethints() */


/*
 * A D D R I N F O  R O U T I N E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct dns_addrinfo {
	struct addrinfo hints;
	struct dns_resolver *res;

	char qname[DNS_D_MAXNAME + 1];
	enum dns_type qtype;
	unsigned short qport, port;

	struct dns_packet *answer;
	struct dns_packet *glue;

	struct dns_rr_i i, g;
	struct dns_rr rr;

	char cname[DNS_D_MAXNAME + 1];

	int state;
	int found;

	struct dns_stat st;
}; /* struct dns_addrinfo */


static dns_error_t dns_ai_parseport(unsigned short *port, const char *serv, const struct addrinfo *hints) {
	const char *cp = serv;
	unsigned long n = 0;

	while (*cp >= '0' && *cp <= '9' && n < 65536) {
		n *= 10;
		n += *cp++ - '0';
	}

	if (*cp == '\0') {
		if (cp == serv || n >= 65536)
			return DNS_ESERVICE;

		*port = n;

		return 0;
	}

	if (hints->ai_flags & AI_NUMERICSERV)
		return DNS_ESERVICE;

	/* TODO: try getaddrinfo(NULL, serv, { .ai_flags = AI_NUMERICSERV }) */

	return DNS_ESERVICE;
} /* dns_ai_parseport() */


struct dns_addrinfo *dns_ai_open(const char *host, const char *serv, enum dns_type qtype, const struct addrinfo *hints, struct dns_resolver *res, int *error_) {
	static const struct dns_addrinfo ai_initializer;
	struct dns_addrinfo *ai;
	int error;

	if (res) {
		dns_res_acquire(res);
	} else if (!(hints->ai_flags & AI_NUMERICHOST)) {
		/*
		 * NOTE: it's assumed that *_error is set from a previous
		 * API function call, such as dns_res_stub(). Should change
		 * this semantic, but it's applied elsewhere, too.
		 */
		return NULL;
	}

	if (!(ai = malloc(sizeof *ai)))
		goto syerr;

	*ai = ai_initializer;
	ai->hints = *hints;

	ai->res = res;
	res = NULL;

	if (sizeof ai->qname <= dns_strlcpy(ai->qname, host, sizeof ai->qname))
		{ error = ENAMETOOLONG; goto error; }

	ai->qtype = qtype;
	ai->qport = 0;

	if (serv && (error = dns_ai_parseport(&ai->qport, serv, hints)))
		goto error;

	ai->port = ai->qport;

	return ai;
syerr:
	error = dns_syerr();
error:
	*error_ = error;

	dns_ai_close(ai);
	dns_res_close(res);

	return NULL;
} /* dns_ai_open() */


void dns_ai_close(struct dns_addrinfo *ai) {
	if (!ai)
		return;

	dns_res_close(ai->res);

	if (ai->answer != ai->glue)
		dns_p_free(ai->glue);

	dns_p_free(ai->answer);
	free(ai);
} /* dns_ai_close() */


static int dns_ai_setent(struct addrinfo **ent, union dns_any *any, enum dns_type type, struct dns_addrinfo *ai) {
	struct sockaddr *saddr;
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	const char *cname;
	size_t clen;

	switch (type) {
	case DNS_T_A:
		saddr	= memset(&sin, '\0', sizeof sin);

		sin.sin_family	= AF_INET;
		sin.sin_port	= htons(ai->port);

		memcpy(&sin.sin_addr, any, sizeof sin.sin_addr);

		break;
	case DNS_T_AAAA:
		saddr	= memset(&sin6, '\0', sizeof sin6);

		sin6.sin6_family	= AF_INET6;
		sin6.sin6_port		= htons(ai->port);

		memcpy(&sin6.sin6_addr, any, sizeof sin6.sin6_addr);

		break;
	default:
		return EINVAL;
	} /* switch() */

	if (ai->hints.ai_flags & AI_CANONNAME) {
		cname	= (*ai->cname)? ai->cname : ai->qname;
		clen	= strlen(cname);
	} else {
		cname	= NULL;
		clen	= 0;
	}

	if (!(*ent = malloc(sizeof **ent + dns_sa_len(saddr) + ((ai->hints.ai_flags & AI_CANONNAME)? clen + 1 : 0))))
		return dns_syerr();

	memset(*ent, '\0', sizeof **ent);

	(*ent)->ai_family	= saddr->sa_family;
	(*ent)->ai_socktype	= ai->hints.ai_socktype;
	(*ent)->ai_protocol	= ai->hints.ai_protocol;

	(*ent)->ai_addr		= memcpy((unsigned char *)*ent + sizeof **ent, saddr, dns_sa_len(saddr));
	(*ent)->ai_addrlen	= dns_sa_len(saddr);

	if (ai->hints.ai_flags & AI_CANONNAME)
		(*ent)->ai_canonname	= memcpy((unsigned char *)*ent + sizeof **ent + dns_sa_len(saddr), cname, clen + 1);

	ai->found++;

	return 0;
} /* dns_ai_setent() */


enum dns_ai_state {
	DNS_AI_S_INIT,
	DNS_AI_S_NUMERIC,
	DNS_AI_S_SUBMIT,
	DNS_AI_S_CHECK,
	DNS_AI_S_FETCH,
	DNS_AI_S_FOREACH_I,
	DNS_AI_S_FOREACH_G,
	DNS_AI_S_SUBMIT_G,
	DNS_AI_S_CHECK_G,
	DNS_AI_S_FETCH_G,
	DNS_AI_S_DONE,
}; /* enum dns_ai_state */

#define dns_ai_goto(which)	do { ai->state = (which); goto exec; } while (0)

int dns_ai_nextent(struct addrinfo **ent, struct dns_addrinfo *ai) {
	struct dns_packet *ans, *glue;
	struct dns_rr rr;
	char qname[DNS_D_MAXNAME + 1];
	union dns_any any;
	size_t len;
	int error;

	*ent = 0;

exec:

	switch (ai->state) {
	case DNS_AI_S_INIT:
		ai->state++;
	case DNS_AI_S_NUMERIC:
		if (1 == dns_inet_pton(AF_INET, ai->qname, &any.a)) {
			ai->state = DNS_AI_S_DONE;

			return dns_ai_setent(ent, &any, DNS_T_A, ai);
		}

		if (1 == dns_inet_pton(AF_INET6, ai->qname, &any.aaaa)) {
			ai->state = DNS_AI_S_DONE;

			return dns_ai_setent(ent, &any, DNS_T_AAAA, ai);
		}

		if (ai->hints.ai_flags & AI_NUMERICHOST)
			dns_ai_goto(DNS_AI_S_DONE);

		ai->state++;
	case DNS_AI_S_SUBMIT:
		assert(ai->res);

		if ((error = dns_res_submit(ai->res, ai->qname, ai->qtype, DNS_C_IN)))
			return error;

		ai->state++;
	case DNS_AI_S_CHECK:
		if ((error = dns_res_check(ai->res)))
			return error;

		ai->state++;
	case DNS_AI_S_FETCH:
		if (!(ai->answer = dns_res_fetch(ai->res, &error)))
			return error;

		if ((error = dns_p_study(ai->answer)))
			return error;

		ai->glue = ai->answer;

		dns_rr_i_init(&ai->i, ai->answer);

		ai->i.section = DNS_S_AN;
		ai->i.type    = ai->qtype;
		ai->i.sort    = &dns_rr_i_order;

		ai->state++;
	case DNS_AI_S_FOREACH_I:
		/* Search generator may have changed our qname. */
		if (!(len = dns_d_expand(qname, sizeof qname, 12, ai->answer, &error)))
			return error;
		else if (len >= sizeof qname)
			return DNS_EILLEGAL;

		if (!dns_d_cname(ai->cname, sizeof ai->cname, qname, strlen(qname), ai->answer, &error))
			return error;

		ai->i.name = ai->cname;

		if (!dns_rr_grep(&rr, 1, &ai->i, ai->answer, &error))
			dns_ai_goto(DNS_AI_S_DONE);

		if ((error = dns_any_parse(&any, &rr, ai->answer)))
			return error;

		ai->port = ai->qport;

		switch (rr.type) {
		case DNS_T_A:
		case DNS_T_AAAA:
			return dns_ai_setent(ent, &any, rr.type, ai);
		default:
			if (!dns_any_cname(ai->cname, sizeof ai->cname, &any, rr.type))
				dns_ai_goto(DNS_AI_S_FOREACH_I);

			/*
			 * Find the "real" canonical name. Some authorities
			 * publish aliases where an RFC defines a canonical
			 * name. We trust that the resolver followed any
			 * CNAME chains on it's own, regardless of whether
			 * the "smart" option is enabled.
			 */
			if (!dns_d_cname(ai->cname, sizeof ai->cname, ai->cname, strlen(ai->cname), ai->answer, &error))
				return error;

			if (rr.type == DNS_T_SRV)
				ai->port = any.srv.port;

			break;
		} /* switch() */

		dns_rr_i_init(&ai->g, ai->glue);

		ai->g.section = DNS_S_ALL & ~DNS_S_QD;
		ai->g.name    = ai->cname;
		ai->g.type    = (ai->hints.ai_family == AF_INET6)? DNS_T_AAAA : DNS_T_A;

		ai->state++;
	case DNS_AI_S_FOREACH_G:
		if (!dns_rr_grep(&rr, 1, &ai->g, ai->glue, &error)) {
			if (dns_rr_i_count(&ai->g) > 0)
				dns_ai_goto(DNS_AI_S_FOREACH_I);
			else
				dns_ai_goto(DNS_AI_S_SUBMIT_G);
		}

		if ((error = dns_any_parse(&any, &rr, ai->glue)))
			return error;

		return dns_ai_setent(ent, &any, rr.type, ai);
	case DNS_AI_S_SUBMIT_G:
		if (dns_rr_grep(&rr, 1, dns_rr_i_new(ai->glue, .section = DNS_S_QD, .name = ai->g.name, .type = ai->g.type), ai->glue, &error))
			dns_ai_goto(DNS_AI_S_FOREACH_I);

		if ((error = dns_res_submit(ai->res, ai->g.name, ai->g.type, DNS_C_IN)))
			return error;

		ai->state++;
	case DNS_AI_S_CHECK_G:
		if ((error = dns_res_check(ai->res)))
			return error;

		ai->state++;
	case DNS_AI_S_FETCH_G:
		if (!(ans = dns_res_fetch(ai->res, &error)))
			return error;

		dns_p_study(ans);

		glue = dns_p_merge(ai->glue, DNS_S_ALL, ans, DNS_S_ALL, &error);

		dns_p_free(ans);

		if (!glue)
			return error;

		if (ai->glue != ai->answer)
			dns_p_free(ai->glue);

		ai->glue = glue;

		dns_rr_i_init(&ai->g, ai->glue);

		/* ai->g.name should already point to ai->cname */
		if (!dns_d_cname(ai->cname, sizeof ai->cname, ai->cname, strlen(ai->cname), ai->glue, &error))
			dns_ai_goto(DNS_AI_S_FOREACH_I);

		/* NOTE: Keep all the other iterator filters */

		dns_ai_goto(DNS_AI_S_FOREACH_G);
	case DNS_AI_S_DONE:
		if (ai->found) {
			return ENOENT; /* TODO: Just return 0 */
		} else if (ai->answer) {
			switch (dns_header(ai->answer)->rcode) {
			case DNS_RC_NOERROR:
				/* FALL THROUGH */
			case DNS_RC_NXDOMAIN:
				return DNS_ENONAME;
			default:
				return DNS_EFAIL;
			}
		} else {
			return DNS_EFAIL;
		}
	default:
		return EINVAL;
	} /* switch() */
} /* dns_ai_nextent() */


time_t dns_ai_elapsed(struct dns_addrinfo *ai) {
	return (ai->res)? dns_res_elapsed(ai->res) : 0;
} /* dns_ai_elapsed() */


void dns_ai_clear(struct dns_addrinfo *ai) {
	if (ai->res)
		dns_res_clear(ai->res);
} /* dns_ai_clear() */


int dns_ai_events(struct dns_addrinfo *ai) {
	return (ai->res)? dns_res_events(ai->res) : 0;
} /* dns_ai_events() */


int dns_ai_pollfd(struct dns_addrinfo *ai) {
	return (ai->res)? dns_res_pollfd(ai->res) : -1;
} /* dns_ai_pollfd() */


time_t dns_ai_timeout(struct dns_addrinfo *ai) {
	return (ai->res)? dns_res_timeout(ai->res) : 0;
} /* dns_ai_timeout() */


int dns_ai_poll(struct dns_addrinfo *ai, int timeout) {
	return (ai->res)? dns_res_poll(ai->res, timeout) : 0;
} /* dns_ai_poll() */


size_t dns_ai_print(void *dst, size_t lim, struct addrinfo *ent, struct dns_addrinfo *ai) {
	char addr[DNS_PP_MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN) + 1];
	size_t cp	= 0;

	cp	+= dns__printstring(dst, lim, cp, "[ ");
	cp	+= dns__printstring(dst, lim, cp, ai->qname);
	cp	+= dns__printstring(dst, lim, cp, " IN ");
	cp	+= dns__printstring(dst, lim, cp, dns_strtype(ai->qtype));
	cp	+= dns__printstring(dst, lim, cp, " ]\n");

	cp	+= dns__printstring(dst, lim, cp, ".ai_family    = ");

	switch (ent->ai_family) {
	case AF_INET:
		cp	+= dns__printstring(dst, lim, cp, "AF_INET");
		break;
	case AF_INET6:
		cp	+= dns__printstring(dst, lim, cp, "AF_INET6");
		break;
	default:
		cp	+= dns__print10(dst, lim, cp, ent->ai_family, 0);
		break;
	}

	cp	+= dns__printchar(dst, lim, cp, '\n');

	cp	+= dns__printstring(dst, lim, cp, ".ai_socktype  = ");

	switch (ent->ai_socktype) {
	case SOCK_STREAM:
		cp	+= dns__printstring(dst, lim, cp, "SOCK_STREAM");
		break;
	case SOCK_DGRAM:
		cp	+= dns__printstring(dst, lim, cp, "SOCK_DGRAM");
		break;
	default:
		cp	+= dns__print10(dst, lim, cp, ent->ai_socktype, 0);
		break;
	}

	cp	+= dns__printchar(dst, lim, cp, '\n');

	cp	+= dns__printstring(dst, lim, cp, ".ai_addr      = [");

	dns_inet_ntop(dns_sa_family(ent->ai_addr), dns_sa_addr(dns_sa_family(ent->ai_addr), ent->ai_addr, NULL), addr, sizeof addr);

	cp	+= dns__printstring(dst, lim, cp, addr);
	cp	+= dns__printstring(dst, lim, cp, "]:");

	cp	+= dns__print10(dst, lim, cp, ntohs(*dns_sa_port(dns_sa_family(ent->ai_addr), ent->ai_addr)), 0);
	cp	+= dns__printchar(dst, lim, cp, '\n');

	cp	+= dns__printstring(dst, lim, cp, ".ai_canonname = ");
	cp	+= dns__printstring(dst, lim, cp, (ent->ai_canonname)? ent->ai_canonname : "[NULL]");
	cp	+= dns__printchar(dst, lim, cp, '\n');

	dns__printnul(dst, lim, cp);

	return cp;
} /* dns_ai_print() */


const struct dns_stat *dns_ai_stat(struct dns_addrinfo *ai) {
	return (ai->res)? dns_res_stat(ai->res) : &ai->st;
} /* dns_ai_stat() */


/*
 * M I S C E L L A N E O U S  R O U T I N E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static const struct {
	char name[16];
	enum dns_section type;
} dns_sections[] = {
	{ "QUESTION",   DNS_S_QUESTION },
	{ "QD",         DNS_S_QUESTION },
	{ "ANSWER",     DNS_S_ANSWER },
	{ "AN",         DNS_S_ANSWER },
	{ "AUTHORITY",  DNS_S_AUTHORITY },
	{ "NS",         DNS_S_AUTHORITY },
	{ "ADDITIONAL", DNS_S_ADDITIONAL },
	{ "AR",         DNS_S_ADDITIONAL },
};

const char *(dns_strsection)(enum dns_section section, void *dst, size_t lim) {
	unsigned i, p = 0;

	for (i = 0; i < lengthof(dns_sections); i++) {
		if (dns_sections[i].type & section) {
			if (p > 0)
				p += dns__printchar(dst, lim, p, '|');

			p += dns__printstring(dst, lim, p, dns_sections[i].name);

			section &= ~dns_sections[i].type;
		}
	}

	if (!p)
		p += dns__print10(dst, lim, 0, (0xffff & section), 0);

	dns__printnul(dst, lim, p);

	return dst;
} /* dns_strsection() */


enum dns_section dns_isection(const char *src) {
	enum dns_section section = 0;
	char sbuf[128];
	char *name, *next;
	unsigned i;

	dns_strlcpy(sbuf, src, sizeof sbuf);
	next = sbuf;

	while ((name = dns_strsep(&next, "|+, \t"))) {
		for (i = 0; i < lengthof(dns_sections); i++) {
			if (!strcasecmp(dns_sections[i].name, name)) {
				section |= dns_sections[i].type;
				break;
			}
		}
	}

	return section;
} /* dns_isection() */


static const struct {
	char name[8];
	enum dns_class type;
} dns_classes[] = {
	{ "IN", DNS_C_IN },
};

const char *(dns_strclass)(enum dns_class type, void *dst, size_t lim) {
	unsigned i;

	for (i = 0; i < lengthof(dns_classes); i++) {
		if (dns_classes[i].type == type) {
			dns__printnul(dst, lim, dns__printstring(dst, lim, 0, dns_classes[i].name));

			return dst;
		}
	}

	dns__printnul(dst, lim, dns__print10(dst, lim, 0, (0xffff & type), 0));

	return dst;
} /* dns_strclass() */


enum dns_class dns_iclass(const char *name) {
	unsigned i;

	for (i = 0; i < lengthof(dns_classes); i++) {
		if (!strcasecmp(dns_classes[i].name, name))
			return dns_classes[i].type;
	}

	return 0;
} /* dns_iclass() */


const char *(dns_strtype)(enum dns_type type, void *dst, size_t lim) {
	unsigned i;

	for (i = 0; i < lengthof(dns_rrtypes); i++) {
		if (dns_rrtypes[i].type == type) {
			dns__printnul(dst, lim, dns__printstring(dst, lim, 0, dns_rrtypes[i].name));

			return dst;
		}
	}

	dns__printnul(dst, lim, dns__print10(dst, lim, 0, (0xffff & type), 0));

	return dst;
} /* dns_strtype() */


enum dns_type dns_itype(const char *type) {
	unsigned i;

	for (i = 0; i < lengthof(dns_rrtypes); i++) {
		if (!strcasecmp(dns_rrtypes[i].name, type))
			return dns_rrtypes[i].type;
	}

	return 0;
} /* dns_itype() */


static char dns_opcodes[16][16] = {
	[DNS_OP_QUERY]  = "QUERY",
	[DNS_OP_IQUERY] = "IQUERY",
	[DNS_OP_STATUS] = "STATUS",
	[DNS_OP_NOTIFY] = "NOTIFY",
	[DNS_OP_UPDATE] = "UPDATE",
};

const char *dns_stropcode(enum dns_opcode opcode) {
	opcode &= 0xf;

	if ('\0' == dns_opcodes[opcode][0])
		dns__printnul(dns_opcodes[opcode], sizeof dns_opcodes[opcode], dns__print10(dns_opcodes[opcode], sizeof dns_opcodes[opcode], 0, opcode, 0));

	return dns_opcodes[opcode];
} /* dns_stropcode() */


enum dns_opcode dns_iopcode(const char *name) {
	unsigned opcode;

	for (opcode = 0; opcode < lengthof(dns_opcodes); opcode++) {
		if (!strcasecmp(name, dns_opcodes[opcode]))
			return opcode;
	}

	return lengthof(dns_opcodes) - 1;
} /* dns_iopcode() */


static char dns_rcodes[16][16] = {
	[DNS_RC_NOERROR]  = "NOERROR",
	[DNS_RC_FORMERR]  = "FORMERR",
	[DNS_RC_SERVFAIL] = "SERVFAIL",
	[DNS_RC_NXDOMAIN] = "NXDOMAIN",
	[DNS_RC_NOTIMP]   = "NOTIMP",
	[DNS_RC_REFUSED]  = "REFUSED",
	[DNS_RC_YXDOMAIN] = "YXDOMAIN",
	[DNS_RC_YXRRSET]  = "YXRRSET",
	[DNS_RC_NXRRSET]  = "NXRRSET",
	[DNS_RC_NOTAUTH]  = "NOTAUTH",
	[DNS_RC_NOTZONE]  = "NOTZONE",
};

const char *dns_strrcode(enum dns_rcode rcode) {
	rcode &= 0xf;

	if ('\0' == dns_rcodes[rcode][0])
		dns__printnul(dns_rcodes[rcode], sizeof dns_rcodes[rcode], dns__print10(dns_rcodes[rcode], sizeof dns_rcodes[rcode], 0, rcode, 0));

	return dns_rcodes[rcode];
} /* dns_strrcode() */


enum dns_rcode dns_ircode(const char *name) {
	unsigned rcode;

	for (rcode = 0; rcode < lengthof(dns_rcodes); rcode++) {
		if (!strcasecmp(name, dns_rcodes[rcode]))
			return rcode;
	}

	return lengthof(dns_rcodes) - 1;
} /* dns_ircode() */



/*
 * C O M M A N D - L I N E / R E G R E S S I O N  R O U T I N E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#if DNS_MAIN

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include <ctype.h>

#if _WIN32
#include <getopt.h>
#endif

#if !_WIN32
#include <err.h>
#endif


struct {
	struct {
		const char *path[8];
		unsigned count;
	} resconf, nssconf, hosts, cache;

	const char *qname;
	enum dns_type qtype;

	int (*sort)();

	int verbose;
} MAIN = {
	.sort	= &dns_rr_i_packet,
};


void hexdump(const unsigned char *src, size_t len, FILE *fp) {
	static const unsigned char hex[]	= "0123456789abcdef";
	static const unsigned char tmpl[]	= "                                                    |                |\n";
	unsigned char ln[sizeof tmpl];
	const unsigned char *sp, *se;
	unsigned char *h, *g;
	unsigned i, n;

	sp	= src;
	se	= sp + len;

	while (sp < se) {
		memcpy(ln, tmpl, sizeof ln);

		h	= &ln[2];
		g	= &ln[53];

		for (n = 0; n < 2; n++) {
			for (i = 0; i < 8 && se - sp > 0; i++, sp++) {
				h[0]	= hex[0x0f & (*sp >> 4)];
				h[1]	= hex[0x0f & (*sp >> 0)];
				h	+= 3;

				*g++	= (isgraph(*sp))? *sp : '.';
			}

			h++;
		}

		fputs((char *)ln, fp);
	}

	return /* void */;
} /* hexdump() */


DNS_NORETURN static void panic(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);

#if _WIN32
	vfprintf(stderr, fmt, ap);

	exit(EXIT_FAILURE);
#else
	verrx(EXIT_FAILURE, fmt, ap);
#endif
} /* panic() */

#define panic_(fn, ln, fmt, ...)	\
	panic(fmt "%0s", (fn), (ln), __VA_ARGS__)
#define panic(...)			\
	panic_(__func__, __LINE__, "(%s:%d) " __VA_ARGS__, "")


static void *grow(unsigned char *p, size_t size) {
	void *tmp;

	if (!(tmp = realloc(p, size)))
		panic("realloc(%"PRIuZ"): %s", size, dns_strerror(errno));

	return tmp;
} /* grow() */


static size_t add(size_t a, size_t b) {
	if (~a < b)
		panic("%"PRIuZ" + %"PRIuZ": integer overflow", a, b);

	return a + b;
} /* add() */


static size_t append(unsigned char **dst, size_t osize, const void *src, size_t len) {
	size_t size = add(osize, len);

	*dst = grow(*dst, size);
	memcpy(*dst + osize, src, len);

	return size;
} /* append() */


static size_t slurp(unsigned char **dst, size_t osize, FILE *fp, const char *path) {
	size_t size = osize;
	unsigned char buf[1024];
	size_t count;

	while ((count = fread(buf, 1, sizeof buf, fp)))
		size = append(dst, size, buf, count);

	if (ferror(fp))
		panic("%s: %s", path, dns_strerror(errno));

	return size;
} /* slurp() */


static struct dns_resolv_conf *resconf(void) {
	static struct dns_resolv_conf *resconf;
	const char *path;
	unsigned i;
	int error;

	if (resconf)
		return resconf;

	if (!(resconf = dns_resconf_open(&error)))
		panic("dns_resconf_open: %s", dns_strerror(error));

	if (!MAIN.resconf.count)
		MAIN.resconf.path[MAIN.resconf.count++]	= "/etc/resolv.conf";

	for (i = 0; i < MAIN.resconf.count; i++) {
		path	= MAIN.resconf.path[i];

		if (0 == strcmp(path, "-"))
			error	= dns_resconf_loadfile(resconf, stdin);
		else
			error	= dns_resconf_loadpath(resconf, path);

		if (error)
			panic("%s: %s", path, dns_strerror(error));
	}

	for (i = 0; i < MAIN.nssconf.count; i++) {
		path	= MAIN.nssconf.path[i];

		if (0 == strcmp(path, "-"))
			error	= dns_nssconf_loadfile(resconf, stdin);
		else
			error	= dns_nssconf_loadpath(resconf, path);

		if (error)
			panic("%s: %s", path, dns_strerror(error));
	}

	if (!MAIN.nssconf.count) {
		path = "/etc/nsswitch.conf";

		if (!(error = dns_nssconf_loadpath(resconf, path)))
			MAIN.nssconf.path[MAIN.nssconf.count++] = path;
		else if (error != ENOENT)
			panic("%s: %s", path, dns_strerror(error));
	}

	return resconf;
} /* resconf() */


static struct dns_hosts *hosts(void) {
	static struct dns_hosts *hosts;
	const char *path;
	unsigned i;
	int error;

	if (hosts)
		return hosts;

	if (!MAIN.hosts.count) {
		MAIN.hosts.path[MAIN.hosts.count++]	= "/etc/hosts";

		/* Explicitly test dns_hosts_local() */
		if (!(hosts = dns_hosts_local(&error)))
			panic("%s: %s", "/etc/hosts", dns_strerror(error));

		return hosts;
	}

	if (!(hosts = dns_hosts_open(&error)))
		panic("dns_hosts_open: %s", dns_strerror(error));

	for (i = 0; i < MAIN.hosts.count; i++) {
		path	= MAIN.hosts.path[i];

		if (0 == strcmp(path, "-"))
			error	= dns_hosts_loadfile(hosts, stdin);
		else
			error	= dns_hosts_loadpath(hosts, path);

		if (error)
			panic("%s: %s", path, dns_strerror(error));
	}

	return hosts;
} /* hosts() */


#if DNS_CACHE
#include "cache.h"

struct dns_cache *cache(void) {
	static struct cache *cache;
	const char *path;
	unsigned i;
	int error;

	if (cache)
		return cache_resi(cache);
	if (!MAIN.cache.count)
		return NULL;

	if (!(cache = cache_open(&error)))
		panic("%s: %s", MAIN.cache.path[0], dns_strerror(error));

	for (i = 0; i < MAIN.cache.count; i++) {
		path = MAIN.cache.path[i];

		if (!strcmp(path, "-")) {
			if ((error = cache_loadfile(cache, stdin, NULL, 0)))
				panic("%s: %s", path, dns_strerror(error));
		} else if ((error = cache_loadpath(cache, path, NULL, 0)))
			panic("%s: %s", path, dns_strerror(error));
	}

	return cache_resi(cache);
} /* cache() */
#else
struct dns_cache *cache(void) { return NULL; }
#endif


static void print_packet(struct dns_packet *P, FILE *fp) {
	dns_p_dump3(P, dns_rr_i_new(P, .sort = MAIN.sort), fp);

	if (MAIN.verbose > 2)
		hexdump(P->data, P->end, fp);
} /* print_packet() */


static int parse_packet(int argc DNS_NOTUSED, char *argv[] DNS_NOTUSED) {
	struct dns_packet *P	= dns_p_new(512);
	struct dns_packet *Q	= dns_p_new(512);
	enum dns_section section;
	struct dns_rr rr;
	int error;
	union dns_any any;
	char pretty[sizeof any * 2];
	size_t len;

	P->end	= fread(P->data, 1, P->size, stdin);

	fputs(";; [HEADER]\n", stdout);
	fprintf(stdout, ";;     qr : %s(%d)\n", (dns_header(P)->qr)? "RESPONSE" : "QUERY", dns_header(P)->qr);
	fprintf(stdout, ";; opcode : %s(%d)\n", dns_stropcode(dns_header(P)->opcode), dns_header(P)->opcode);
	fprintf(stdout, ";;     aa : %s(%d)\n", (dns_header(P)->aa)? "AUTHORITATIVE" : "NON-AUTHORITATIVE", dns_header(P)->aa);
	fprintf(stdout, ";;     tc : %s(%d)\n", (dns_header(P)->tc)? "TRUNCATED" : "NOT-TRUNCATED", dns_header(P)->tc);
	fprintf(stdout, ";;     rd : %s(%d)\n", (dns_header(P)->rd)? "RECURSION-DESIRED" : "RECURSION-NOT-DESIRED", dns_header(P)->rd);
	fprintf(stdout, ";;     ra : %s(%d)\n", (dns_header(P)->ra)? "RECURSION-ALLOWED" : "RECURSION-NOT-ALLOWED", dns_header(P)->ra);
	fprintf(stdout, ";;  rcode : %s(%d)\n", dns_strrcode(dns_header(P)->rcode), dns_header(P)->rcode);

	section	= 0;

	dns_rr_foreach(&rr, P, .sort = MAIN.sort) {
		if (section != rr.section)
			fprintf(stdout, "\n;; [%s:%d]\n", dns_strsection(rr.section), dns_p_count(P, rr.section));

		if ((len = dns_rr_print(pretty, sizeof pretty, &rr, P, &error)))
			fprintf(stdout, "%s\n", pretty);

		dns_rr_copy(Q, &rr, P);

		section	= rr.section;
	}

	fputs("; ; ; ; ; ; ; ;\n\n", stdout);

	section	= 0;

#if 0
	dns_rr_foreach(&rr, Q, .name = "ns8.yahoo.com.") {
#else
	struct dns_rr rrset[32];
	struct dns_rr_i *rri	= dns_rr_i_new(Q, .name = dns_d_new("ns8.yahoo.com", DNS_D_ANCHOR), .sort = MAIN.sort);
	unsigned rrcount	= dns_rr_grep(rrset, lengthof(rrset), rri, Q, &error);

	for (unsigned i = 0; i < rrcount; i++) {
		rr	= rrset[i];
#endif
		if (section != rr.section)
			fprintf(stdout, "\n;; [%s:%d]\n", dns_strsection(rr.section), dns_p_count(Q, rr.section));

		if ((len = dns_rr_print(pretty, sizeof pretty, &rr, Q, &error)))
			fprintf(stdout, "%s\n", pretty);

		section	= rr.section;
	}

	if (MAIN.verbose > 1) {
		fprintf(stderr, "orig:%"PRIuZ"\n", P->end);
		hexdump(P->data, P->end, stdout);

		fprintf(stderr, "copy:%"PRIuZ"\n", Q->end);
		hexdump(Q->data, Q->end, stdout);
	}

	return 0;
} /* parse_packet() */


static int parse_domain(int argc, char *argv[]) {
	char *dn;

	dn	= (argc > 1)? argv[1] : "f.l.google.com";

	printf("[%s]\n", dn);

	dn	= dns_d_new(dn);

	do {
		puts(dn);
	} while (dns_d_cleave(dn, strlen(dn) + 1, dn, strlen(dn)));

	return 0;
} /* parse_domain() */


static int trim_domain(int argc, char **argv) {
	for (argc--, argv++; argc > 0; argc--, argv++) {
		char name[DNS_D_MAXNAME + 1];

		dns_d_trim(name, sizeof name, *argv, strlen(*argv), DNS_D_ANCHOR);

		puts(name);
	}

	return 0;
} /* trim_domain() */


static int expand_domain(int argc, char *argv[]) {
	unsigned short rp = 0;
	unsigned char *src = NULL;
	unsigned char *dst;
	struct dns_packet *pkt;
	size_t lim = 0, len;
	int error;

	if (argc > 1)
		rp = atoi(argv[1]);

	len = slurp(&src, 0, stdin, "-");

	if (!(pkt = dns_p_make(len, &error)))
		panic("malloc(%"PRIuZ"): %s", len, dns_strerror(error));

	memcpy(pkt->data, src, len);
	pkt->end = len;

	lim = 1;
	dst = grow(NULL, lim);

	while (lim <= (len = dns_d_expand(dst, lim, rp, pkt, &error))) {
		lim = add(len, 1);
		dst = grow(dst, lim);
	}

	if (!len)
		panic("expand: %s", dns_strerror(error));

	fwrite(dst, 1, len, stdout);
	fflush(stdout);

	free(src);
	free(dst);
	free(pkt);

	return 0;
} /* expand_domain() */


static int show_resconf(int argc DNS_NOTUSED, char *argv[] DNS_NOTUSED) {
	unsigned i;

	resconf(); /* load it */

	fputs("; SOURCES\n", stdout);

	for (i = 0; i < MAIN.resconf.count; i++)
		fprintf(stdout, ";   %s\n", MAIN.resconf.path[i]);

	for (i = 0; i < MAIN.nssconf.count; i++)
		fprintf(stdout, ";   %s\n", MAIN.nssconf.path[i]);

	fputs(";\n", stdout);

	dns_resconf_dump(resconf(), stdout);

	return 0;
} /* show_resconf() */


static int show_nssconf(int argc DNS_NOTUSED, char *argv[] DNS_NOTUSED) {
	unsigned i;

	resconf();

	fputs("# SOURCES\n", stdout);

	for (i = 0; i < MAIN.resconf.count; i++)
		fprintf(stdout, "#   %s\n", MAIN.resconf.path[i]);

	for (i = 0; i < MAIN.nssconf.count; i++)
		fprintf(stdout, "#   %s\n", MAIN.nssconf.path[i]);

	fputs("#\n", stdout);

	dns_nssconf_dump(resconf(), stdout);

	return 0;
} /* show_nssconf() */


static int show_hosts(int argc DNS_NOTUSED, char *argv[] DNS_NOTUSED) {
	unsigned i;

	hosts();

	fputs("# SOURCES\n", stdout);

	for (i = 0; i < MAIN.hosts.count; i++)
		fprintf(stdout, "#   %s\n", MAIN.hosts.path[i]);

	fputs("#\n", stdout);

	dns_hosts_dump(hosts(), stdout);

	return 0;
} /* show_hosts() */


static int query_hosts(int argc, char *argv[]) {
	struct dns_packet *Q	= dns_p_new(512);
	struct dns_packet *A;
	char qname[DNS_D_MAXNAME + 1];
	size_t qlen;
	int error;

	if (!MAIN.qname)
		MAIN.qname	= (argc > 1)? argv[1] : "localhost";
	if (!MAIN.qtype)
		MAIN.qtype	= DNS_T_A;

	hosts();

	if (MAIN.qtype == DNS_T_PTR && !strstr(MAIN.qname, "arpa")) {
		union { struct in_addr a; struct in6_addr a6; } addr;
		int af	= (strchr(MAIN.qname, ':'))? AF_INET6 : AF_INET;

		if ((error = dns_pton(af, MAIN.qname, &addr)))
			panic("%s: %s", MAIN.qname, dns_strerror(error));

		qlen	= dns_ptr_qname(qname, sizeof qname, af, &addr);
	} else
		qlen	= dns__printstring(qname, sizeof qname, 0, MAIN.qname);

	if ((error = dns_p_push(Q, DNS_S_QD, qname, qlen, MAIN.qtype, DNS_C_IN, 0, 0)))
		panic("%s: %s", qname, dns_strerror(error));

	if (!(A = dns_hosts_query(hosts(), Q, &error)))
		panic("%s: %s", qname, dns_strerror(error));

	print_packet(A, stdout);

	free(A);

	return 0;
} /* query_hosts() */


static int search_list(int argc, char *argv[]) {
	const char *qname	= (argc > 1)? argv[1] : "f.l.google.com";
	unsigned long i		= 0;
	char name[DNS_D_MAXNAME + 1];

	printf("[%s]\n", qname);

	while (dns_resconf_search(name, sizeof name, qname, strlen(qname), resconf(), &i))
		puts(name);

	return 0;
} /* search_list() */


int permute_set(int argc, char *argv[]) {
	unsigned lo, hi, i;
	struct dns_k_permutor p;

	hi	= (--argc > 0)? atoi(argv[argc]) : 8;
	lo	= (--argc > 0)? atoi(argv[argc]) : 0;

	fprintf(stderr, "[%u .. %u]\n", lo, hi);

	dns_k_permutor_init(&p, lo, hi);

	for (i = lo; i <= hi; i++)
		fprintf(stdout, "%u\n", dns_k_permutor_step(&p));
//		printf("%u -> %u -> %u\n", i, dns_k_permutor_E(&p, i), dns_k_permutor_D(&p, dns_k_permutor_E(&p, i)));

	return 0;
} /* permute_set() */


int shuffle_16(int argc, char *argv[]) {
	unsigned n, r;

	if (--argc > 0) {
		n = 0xffff & atoi(argv[argc]);
		r = (--argc > 0)? (unsigned)atoi(argv[argc]) : dns_random();

		fprintf(stdout, "%hu\n", dns_k_shuffle16(n, r));
	} else {
		r = dns_random();

		for (n = 0; n < 65536; n++)
			fprintf(stdout, "%hu\n", dns_k_shuffle16(n, r));
	}

	return 0;
} /* shuffle_16() */


int dump_random(int argc, char *argv[]) {
	unsigned char b[32];
	unsigned i, j, n, r;

	n	= (argc > 1)? atoi(argv[1]) : 32;

	while (n) {
		i	= 0;

		do {
			r	= dns_random();

			for (j = 0; j < sizeof r && i < n && i < sizeof b; i++, j++) {
				b[i]	= 0xff & r;
				r	>>= 8;
			}
		} while (i < n && i < sizeof b);

		hexdump(b, i, stdout);

		n	-= i;
	}

	return 0;
} /* dump_random() */


static int send_query(int argc, char *argv[]) {
	struct dns_packet *A, *Q	= dns_p_new(512);
	char host[INET6_ADDRSTRLEN + 1];
	struct sockaddr_storage ss;
	struct dns_socket *so;
	int error, type;

	if (argc > 1) {
		ss.ss_family	= (strchr(argv[1], ':'))? AF_INET6 : AF_INET;

		if ((error = dns_pton(ss.ss_family, argv[1], dns_sa_addr(ss.ss_family, &ss, NULL))))
			panic("%s: %s", argv[1], dns_strerror(error));

		*dns_sa_port(ss.ss_family, &ss)	= htons(53);
	} else
		memcpy(&ss, &resconf()->nameserver[0], dns_sa_len(&resconf()->nameserver[0]));

	if (!dns_inet_ntop(ss.ss_family, dns_sa_addr(ss.ss_family, &ss, NULL), host, sizeof host))
		panic("bad host address, or none provided");

	if (!MAIN.qname)
		MAIN.qname	= "ipv6.google.com";
	if (!MAIN.qtype)
		MAIN.qtype	= DNS_T_AAAA;

	if ((error = dns_p_push(Q, DNS_S_QD, MAIN.qname, strlen(MAIN.qname), MAIN.qtype, DNS_C_IN, 0, 0)))
		panic("dns_p_push: %s", dns_strerror(error));

	dns_header(Q)->rd	= 1;

	if (strstr(argv[0], "udp"))
		type	= SOCK_DGRAM;
	else if (strstr(argv[0], "tcp"))
		type	= SOCK_STREAM;
	else
		type	= dns_res_tcp2type(resconf()->options.tcp);

	fprintf(stderr, "querying %s for %s IN %s\n", host, MAIN.qname, dns_strtype(MAIN.qtype));

	if (!(so = dns_so_open((struct sockaddr *)&resconf()->iface, type, dns_opts(), &error)))
		panic("dns_so_open: %s", dns_strerror(error));

	while (!(A = dns_so_query(so, Q, (struct sockaddr *)&ss, &error))) {
		if (error != DNS_EAGAIN)
			panic("dns_so_query: %s (%d)", dns_strerror(error), error);
		if (dns_so_elapsed(so) > 10)
			panic("query timed-out");

		dns_so_poll(so, 1);
	}

	print_packet(A, stdout);

	dns_so_close(so);

	return 0;
} /* send_query() */


static int print_arpa(int argc, char *argv[]) {
	const char *ip	= (argc > 1)? argv[1] : "::1";
	int af		= (strchr(ip, ':'))? AF_INET6 : AF_INET;
	union { struct in_addr a4; struct in6_addr a6; } addr;
	char host[DNS_D_MAXNAME + 1];

	if (1 != dns_inet_pton(af, ip, &addr) || 0 == dns_ptr_qname(host, sizeof host, af, &addr))
		panic("%s: invalid address", ip);

	fprintf(stdout, "%s\n", host);

	return 0;
} /* print_arpa() */


static int show_hints(int argc, char *argv[]) {
	struct dns_hints *(*load)(struct dns_resolv_conf *, int *);
	const char *which, *how, *who;
	struct dns_hints *hints;
	int error;

	which	= (argc > 1)? argv[1] : "local";
	how	= (argc > 2)? argv[2] : "plain";
	who	= (argc > 3)? argv[3] : "google.com";

	load	= (0 == strcmp(which, "local"))
		? &dns_hints_local
		: &dns_hints_root;

	if (!(hints = load(resconf(), &error)))
		panic("%s: %s", argv[0], dns_strerror(error));

	if (0 == strcmp(how, "plain")) {
		dns_hints_dump(hints, stdout);
	} else {
		struct dns_packet *query, *answer;

		query	= dns_p_new(512);

		if ((error = dns_p_push(query, DNS_S_QUESTION, who, strlen(who), DNS_T_A, DNS_C_IN, 0, 0)))
			panic("%s: %s", who, dns_strerror(error));

		if (!(answer = dns_hints_query(hints, query, &error)))
			panic("%s: %s", who, dns_strerror(error));

		print_packet(answer, stdout);

		free(answer);
	}

	dns_hints_close(hints);

	return 0;
} /* show_hints() */


static int resolve_query(int argc DNS_NOTUSED, char *argv[]) {
	_Bool recurse = !!strstr(argv[0], "recurse");
	struct dns_hints *(*hints)() = (recurse)? &dns_hints_root : &dns_hints_local;
	struct dns_resolver *R;
	struct dns_packet *ans;
	const struct dns_stat *st;
	int error;

	if (!MAIN.qname)
		MAIN.qname = "www.google.com";
	if (!MAIN.qtype)
		MAIN.qtype = DNS_T_A;

	resconf()->options.recurse = recurse;

	if (!(R = dns_res_open(resconf(), hosts(), dns_hints_mortal(hints(resconf(), &error)), cache(), dns_opts(), &error)))
		panic("%s: %s", MAIN.qname, dns_strerror(error));

	if ((error = dns_res_submit(R, MAIN.qname, MAIN.qtype, DNS_C_IN)))
		panic("%s: %s", MAIN.qname, dns_strerror(error));

	while ((error = dns_res_check(R))) {
		if (error != DNS_EAGAIN)
			panic("dns_res_check: %s (%d)", dns_strerror(error), error);
		if (dns_res_elapsed(R) > 30)
			panic("query timed-out");

		dns_res_poll(R, 1);
	}

	ans = dns_res_fetch(R, &error);
	print_packet(ans, stdout);
	free(ans);

	st = dns_res_stat(R);
	putchar('\n');
	printf(";; queries:  %"PRIuZ"\n", st->queries);
	printf(";; udp sent: %"PRIuZ" in %"PRIuZ" bytes\n", st->udp.sent.count, st->udp.sent.bytes);
	printf(";; udp rcvd: %"PRIuZ" in %"PRIuZ" bytes\n", st->udp.rcvd.count, st->udp.rcvd.bytes);
	printf(";; tcp sent: %"PRIuZ" in %"PRIuZ" bytes\n", st->tcp.sent.count, st->tcp.sent.bytes);
	printf(";; tcp rcvd: %"PRIuZ" in %"PRIuZ" bytes\n", st->tcp.rcvd.count, st->tcp.rcvd.bytes);

	dns_res_close(R);

	return 0;
} /* resolve_query() */


static int resolve_addrinfo(int argc DNS_NOTUSED, char *argv[]) {
	_Bool recurse = !!strstr(argv[0], "recurse");
	struct dns_hints *(*hints)() = (recurse)? &dns_hints_root : &dns_hints_local;
	struct dns_resolver *res = NULL;
	struct dns_addrinfo *ai = NULL;
	struct addrinfo ai_hints = { .ai_family = PF_UNSPEC, .ai_socktype = SOCK_STREAM, .ai_flags = AI_CANONNAME };
	struct addrinfo *ent;
	char pretty[512];
	int error;

	if (!MAIN.qname)
		MAIN.qname = "www.google.com";
	if (!MAIN.qtype)
		MAIN.qtype = DNS_T_A;

	resconf()->options.recurse = recurse;

	if (!(res = dns_res_open(resconf(), hosts(), dns_hints_mortal(hints(resconf(), &error)), cache(), dns_opts(), &error)))
		panic("%s: %s", MAIN.qname, dns_strerror(error));

	if (!(ai = dns_ai_open(MAIN.qname, "80", MAIN.qtype, &ai_hints, res, &error)))
		panic("%s: %s", MAIN.qname, dns_strerror(error));

	do {
		switch (error = dns_ai_nextent(&ent, ai)) {
		case 0:
			dns_ai_print(pretty, sizeof pretty, ent, ai);

			fputs(pretty, stdout);

			free(ent);

			break;
		case ENOENT:
			break;
		case DNS_EAGAIN:
			if (dns_ai_elapsed(ai) > 30)
				panic("query timed-out");

			dns_ai_poll(ai, 1);

			break;
		default:
			panic("dns_ai_nextent: %s (%d)", dns_strerror(error), error);
		}
	} while (error != ENOENT);

	dns_res_close(res);
	dns_ai_close(ai);

	return 0;
} /* resolve_addrinfo() */


static int echo_port(int argc DNS_NOTUSED, char *argv[] DNS_NOTUSED) {
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
	} port;
	int fd;

	memset(&port, 0, sizeof port);
	port.sin.sin_family = AF_INET;
	port.sin.sin_port = htons(5354);
	port.sin.sin_addr.s_addr = inet_addr("127.0.0.1");

	if (-1 == (fd = socket(PF_INET, SOCK_DGRAM, 0)))
		panic("socket: %s", strerror(errno));

	if (0 != bind(fd, &port.sa, sizeof port.sa))
		panic("127.0.0.1:5353: %s", dns_strerror(errno));

	for (;;) {
		struct dns_packet *pkt = dns_p_new(512);
		struct sockaddr_storage ss;
		socklen_t slen = sizeof ss;
		ssize_t count;
#if defined(MSG_WAITALL) /* MinGW issue */
		int rflags = MSG_WAITALL;
#else
		int rflags = 0;
#endif

		count = recvfrom(fd, (char *)pkt->data, pkt->size, rflags, (struct sockaddr *)&ss, &slen);

		if (!count || count < 0)
			panic("recvfrom: %s", strerror(errno));

		pkt->end = count;

		dns_p_dump(pkt, stdout);

		(void)sendto(fd, (char *)pkt->data, pkt->end, 0, (struct sockaddr *)&ss, slen);
	}

	return 0;
} /* echo_port() */


static int isection(int argc, char *argv[]) {
	const char *name = (argc > 1)? argv[1] : "";
	int type;

	type = dns_isection(name);
	name = dns_strsection(type);

	printf("%s (%d)\n", name, type);

	return 0;
} /* isection() */


static int iclass(int argc, char *argv[]) {
	const char *name = (argc > 1)? argv[1] : "";
	int type;

	type = dns_iclass(name);
	name = dns_strclass(type);

	printf("%s (%d)\n", name, type);

	return 0;
} /* iclass() */


static int itype(int argc, char *argv[]) {
	const char *name = (argc > 1)? argv[1] : "";
	int type;

	type = dns_itype(name);
	name = dns_strtype(type);

	printf("%s (%d)\n", name, type);

	return 0;
} /* itype() */


static int iopcode(int argc, char *argv[]) {
	const char *name = (argc > 1)? argv[1] : "";
	int type;

	type = dns_iopcode(name);
	name = dns_stropcode(type);

	printf("%s (%d)\n", name, type);

	return 0;
} /* iopcode() */


static int ircode(int argc, char *argv[]) {
	const char *name = (argc > 1)? argv[1] : "";
	int type;

	type = dns_ircode(name);
	name = dns_strrcode(type);

	printf("%s (%d)\n", name, type);

	return 0;
} /* ircode() */


#define SIZE1(x) { DNS_PP_STRINGIFY(x), sizeof (x) }
#define SIZE2(x, ...) SIZE1(x), SIZE1(__VA_ARGS__)
#define SIZE3(x, ...) SIZE1(x), SIZE2(__VA_ARGS__)
#define SIZE4(x, ...) SIZE1(x), SIZE3(__VA_ARGS__)
#define SIZE(...) DNS_PP_CALL(DNS_PP_XPASTE(SIZE, DNS_PP_NARG(__VA_ARGS__)), __VA_ARGS__)

static int sizes(int argc DNS_NOTUSED, char *argv[] DNS_NOTUSED) {
	static const struct { const char *name; size_t size; } type[] = {
		SIZE(struct dns_header, struct dns_packet, struct dns_rr, struct dns_rr_i),
		SIZE(struct dns_a, struct dns_aaaa, struct dns_mx, struct dns_ns),
		SIZE(struct dns_cname, struct dns_soa, struct dns_ptr, struct dns_srv),
		SIZE(struct dns_sshfp, struct dns_txt, union dns_any),
		SIZE(struct dns_resolv_conf, struct dns_hosts, struct dns_hints, struct dns_hints_i),
		SIZE(struct dns_options, struct dns_socket, struct dns_resolver, struct dns_addrinfo),
		SIZE(struct dns_cache), SIZE(size_t), SIZE(void *), SIZE(long)
	};
	unsigned i, max;

	for (i = 0, max = 0; i < lengthof(type); i++)
		max = DNS_PP_MAX(max, strlen(type[i].name));

	for (i = 0; i < lengthof(type); i++)
		printf("%*s : %"PRIuZ"\n", max, type[i].name, type[i].size);

	return 0;
} /* sizes() */


static const struct { const char *cmd; int (*run)(); const char *help; } cmds[] = {
	{ "parse-packet",	&parse_packet,		"parse binary packet from stdin" },
	{ "parse-domain",	&parse_domain,		"anchor and iteratively cleave domain" },
	{ "trim-domain",	&trim_domain,		"trim and anchor domain name" },
	{ "expand-domain",	&expand_domain,		"expand domain at offset NN in packet from stdin" },
	{ "show-resconf",	&show_resconf,		"show resolv.conf data" },
	{ "show-hosts",		&show_hosts,		"show hosts data" },
	{ "show-nssconf",	&show_nssconf,		"show nsswitch.conf data" },
	{ "query-hosts",	&query_hosts,		"query A, AAAA or PTR in hosts data" },
	{ "search-list",	&search_list,		"generate query search list from domain" },
	{ "permute-set",	&permute_set,		"generate random permutation -> (0 .. N or N .. M)" },
	{ "shuffle-16",		&shuffle_16,		"simple 16-bit permutation" },
	{ "dump-random",	&dump_random,		"generate random bytes" },
	{ "send-query",		&send_query,		"send query to host" },
	{ "send-query-udp",	&send_query,		"send udp query to host" },
	{ "send-query-tcp",	&send_query,		"send tcp query to host" },
	{ "print-arpa",		&print_arpa,		"print arpa. zone name of address" },
	{ "show-hints",		&show_hints,		"print hints: show-hints [local|root] [plain|packet]" },
	{ "resolve-stub",	&resolve_query,		"resolve as stub resolver" },
	{ "resolve-recurse",	&resolve_query,		"resolve as recursive resolver" },
	{ "addrinfo-stub",	&resolve_addrinfo,	"resolve through getaddrinfo clone" },
	{ "addrinfo-recurse",	&resolve_addrinfo,	"resolve through getaddrinfo clone" },
/*	{ "resolve-nameinfo",	&resolve_query,		"resolve as recursive resolver" }, */
	{ "echo",		&echo_port,		"server echo mode, for nmap fuzzing" },
	{ "isection",		&isection,		"parse section string" },
	{ "iclass",		&iclass,		"parse class string" },
	{ "itype",		&itype,			"parse type string" },
	{ "iopcode",		&iopcode,		"parse opcode string" },
	{ "ircode",		&ircode,		"parse rcode string" },
	{ "sizes",		&sizes,			"print data structure sizes" },
};


static void print_usage(const char *progname, FILE *fp) {
	static const char *usage	=
		" [OPTIONS] COMMAND [ARGS]\n"
		"  -c PATH   Path to resolv.conf\n"
		"  -n PATH   Path to nsswitch.conf\n"
		"  -l PATH   Path to local hosts\n"
		"  -z PATH   Path to zone cache\n"
		"  -q QNAME  Query name\n"
		"  -t QTYPE  Query type\n"
		"  -s HOW    Sort records\n"
		"  -v        Be more verbose (-vv show packets; -vvv hexdump packets)\n"
		"  -V        Print version info\n"
		"  -h        Print this usage message\n"
		"\n";
	unsigned i, n, m;

	fputs(progname, fp);
	fputs(usage, fp);

	for (i = 0, m = 0; i < lengthof(cmds); i++) {
		if (strlen(cmds[i].cmd) > m)
			m	= strlen(cmds[i].cmd);
	}

	for (i = 0; i < lengthof(cmds); i++) {
		fprintf(fp, "  %s  ", cmds[i].cmd);

		for (n = strlen(cmds[i].cmd); n < m; n++)
			putc(' ', fp);

		fputs(cmds[i].help, fp);
		putc('\n', fp);
	}

	fputs("\nReport bugs to William Ahern <william@25thandClement.com>\n", fp);
} /* print_usage() */


static void print_version(const char *progname, FILE *fp) {
	fprintf(fp, "%s (dns.c) %.8X\n", progname, dns_v_rel());
	fprintf(fp, "vendor  %s\n", dns_vendor());
	fprintf(fp, "release %.8X\n", dns_v_rel());
	fprintf(fp, "abi     %.8X\n", dns_v_abi());
	fprintf(fp, "api     %.8X\n", dns_v_api());
} /* print_version() */


int main(int argc, char **argv) {
	extern int optind;
	extern char *optarg;
	const char *progname	= argv[0];
	unsigned i;
	int ch;

	while (-1 != (ch = getopt(argc, argv, "q:t:c:n:l:z:s:vVh"))) {
		switch (ch) {
		case 'c':
			assert(MAIN.resconf.count < lengthof(MAIN.resconf.path));

			MAIN.resconf.path[MAIN.resconf.count++]	= optarg;

			break;
		case 'n':
			assert(MAIN.nssconf.count < lengthof(MAIN.nssconf.path));

			MAIN.nssconf.path[MAIN.nssconf.count++]	= optarg;

			break;
		case 'l':
			assert(MAIN.hosts.count < lengthof(MAIN.hosts.path));

			MAIN.hosts.path[MAIN.hosts.count++]	= optarg;

			break;
		case 'z':
			assert(MAIN.cache.count < lengthof(MAIN.cache.path));

			MAIN.cache.path[MAIN.cache.count++]	= optarg;

			break;
		case 'q':
			MAIN.qname	= optarg;

			break;
		case 't':
			for (i = 0; i < lengthof(dns_rrtypes); i++) {
				if (0 == strcasecmp(dns_rrtypes[i].name, optarg))
					{ MAIN.qtype = dns_rrtypes[i].type; break; }
			}

			if (MAIN.qtype)
				break;

			for (i = 0; isdigit((int)optarg[i]); i++) {
				MAIN.qtype	*= 10;
				MAIN.qtype	+= optarg[i] - '0';
			}

			if (!MAIN.qtype)
				panic("%s: invalid query type", optarg);

			break;
		case 's':
			if (0 == strcasecmp(optarg, "packet"))
				MAIN.sort	= &dns_rr_i_packet;
			else if (0 == strcasecmp(optarg, "shuffle"))
				MAIN.sort	= &dns_rr_i_shuffle;
			else if (0 == strcasecmp(optarg, "order"))
				MAIN.sort	= &dns_rr_i_order;
			else
				panic("%s: invalid sort method", optarg);

			break;
		case 'v':
			dns_debug = ++MAIN.verbose;

			break;
		case 'V':
			print_version(progname, stdout);

			return 0;
		case 'h':
			print_usage(progname, stdout);

			return 0;
		default:
			print_usage(progname, stderr);

			return EXIT_FAILURE;
		} /* switch() */
	} /* while() */

	argc	-= optind;
	argv	+= optind;

	for (i = 0; i < lengthof(cmds) && argv[0]; i++) {
		if (0 == strcmp(cmds[i].cmd, argv[0]))
			return cmds[i].run(argc, argv);
	}

	print_usage(progname, stderr);

	return EXIT_FAILURE;
} /* main() */


#endif /* DNS_MAIN */


/*
 * pop file-scoped compiler annotations
 */
#if __clang__
#pragma clang diagnostic pop
#elif DNS_GNUC_PREREQ(4, 6)
#pragma GCC diagnostic pop
#endif

