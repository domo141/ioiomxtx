#if 0 /* -*- mode: c; c-file-style: "stroustrup"; tab-width: 8; -*-
 set -euf; uname=`exec uname`
 case $uname in *WIN*) echo $0: does not build on $uname; exit 0 ;; esac
 case ${1-} in dbg) sfx=-dbg DEFS=-DDBG; shift ;; *) sfx= DEFS=-DDBG=0 ;; esac
 trg=${0##*''/}; trg=${trg%.c}$sfx.so; test ! -e "$trg" || rm "$trg"
 case ${1-} in '') set x -O2; shift; esac
 #case ${1-} in '') set x -ggdb; shift; esac
 x_exec () { printf %s\\n "$*" >&2; exec "$@"; }
 x_exec ${CC:-gcc} -std=c99 -shared -fPIC -o "$trg" "$0" $DEFS $@ -ldl
 exit $?
 */
#endif
/*
 * $ ldpreload-moshclienthax.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *      Copyright (c) 2017 Tomi Ollila
 *          All rights reserved
 *
 * Created: Sat 17 Feb 2018 20:28:43 -0800 too
 * Last modified: Sat 17 Feb 2018 23:00:52 +0200 too
 */

/* when creating af_inet sock_dgram socket, bind it to
   127.0.hip,lop so remote can resolve port from there
   env var holds that data --  mvp */

#if 0 // change to '#if 1' whenever there is desire to see these...
#pragma GCC diagnostic warning "-Wpadded"
#pragma GCC diagnostic warning "-Wpedantic"
#endif

// gcc -dM -E -xc /dev/null | grep -i gnuc
// clang -dM -E -xc /dev/null | grep -i gnuc
#if defined (__GNUC__)

// to relax, change 'error' to 'warning' -- or even 'ignored'
// selectively. use #pragma GCC diagnostic push/pop to change
// the rules temporarily

#pragma GCC diagnostic error "-Wall"
#pragma GCC diagnostic error "-Wextra"

#if __GNUC__ >= 7

#pragma GCC diagnostic error "-Wimplicit-fallthrough"

#endif /* __GNUC__ >= 7 */

#pragma GCC diagnostic error "-Wstrict-prototypes"
#pragma GCC diagnostic error "-Winit-self"

// -Wformat=2 ¡currently! (2017-12) equivalent of the following 4
#pragma GCC diagnostic error "-Wformat"
#pragma GCC diagnostic error "-Wformat-nonliteral"
#pragma GCC diagnostic error "-Wformat-security"
#pragma GCC diagnostic error "-Wformat-y2k"

#pragma GCC diagnostic error "-Wcast-align"
#pragma GCC diagnostic error "-Wpointer-arith"
#pragma GCC diagnostic error "-Wwrite-strings"
#pragma GCC diagnostic error "-Wcast-qual"
#pragma GCC diagnostic error "-Wshadow"
#pragma GCC diagnostic error "-Wmissing-include-dirs"
#pragma GCC diagnostic error "-Wundef"
#pragma GCC diagnostic error "-Wbad-function-cast"
#pragma GCC diagnostic error "-Wlogical-op"
#pragma GCC diagnostic error "-Waggregate-return"
#pragma GCC diagnostic error "-Wold-style-definition"
#pragma GCC diagnostic error "-Wmissing-prototypes"
#pragma GCC diagnostic error "-Wmissing-declarations"
#pragma GCC diagnostic error "-Wredundant-decls"
#pragma GCC diagnostic error "-Wnested-externs"
#pragma GCC diagnostic error "-Winline"
#pragma GCC diagnostic error "-Wvla"
#pragma GCC diagnostic error "-Woverlength-strings"

//ragma GCC diagnostic error "-Wfloat-equal"
//ragma GCC diagnostic error "-Werror"
//ragma GCC diagnostic error "-Wconversion"

#endif /* defined (__GNUC__) */

#define _DEFAULT_SOURCE
#define _GNU_SOURCE

#define socket socket_hidden

#include <unistd.h>
#include <stdlib.h>

#include <dlfcn.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <errno.h>

#include <endian.h> //for older compilers not defining __BYTE_ORDER__ & friends

#undef socket

// gcc -dM -E -x c /dev/null | grep BYTE

//#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#if __BYTE_ORDER == __BIG_ENDIAN // from endian.h

#define IADDR(a,b,c,d) ((in_addr_t)((a << 24) + (b << 16) + (c << 8) + d))
#define IPORT(v) ((in_port_t)(v))
#define A256(a) ((a)<<8)

//#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#elif __BYTE_ORDER == __LITTLE_ENDIAN // from endian.h

#define IADDR(a,b,c,d) ((in_addr_t)(a + (b << 8) + (c << 16) + (d << 24)))
#define IPORT(v) ((in_port_t)(((v) >> 8) | ((v) << 8)))
#define A256(a) (a)

#else
#error unknown ENDIAN
#endif

// clang -dM -E - </dev/null | grep __GNUC__  outputs '#define __GNUC__ 4'
#if (__GNUC__ >= 4)
#define ATTRIBUTE(x) __attribute__(x)
#else
#define ATTRIBUTE(x)
#endif

// (variable) block begin/end -- explicit liveness...
#define BB {
#define BE }

#define null ((void*)0)

static void * dlsym_next(const char * symbol)
{
    void * sym = dlsym(RTLD_NEXT, symbol);
    char * str = dlerror();

    if (str != null)
	exit(77);
	//die("finding symbol '%s' failed: %s", symbol, str);

    return sym;
}

// Macros FTW! -- use gcc -E to examine expansion

#define _deffn(_rt, _fn, _args) \
_rt _fn _args; \
_rt _fn _args { \
    static _rt (*_fn##_next) _args = null; \
    if (! _fn##_next ) *(void**) (&_fn##_next) = dlsym_next(#_fn);

#if 0
#define cprintf(...) fprintf(stderr, __VA_ARGS__)
#else
#define cprintf(...) do {} while (0)
#endif

_deffn ( int, socket, (int domain, int type, int protocol) )
#if 0
{
#endif
    int sd = socket_next(domain, type, protocol);
    if (sd < 0) return sd;
    // we know (strace(1) told us) that mosh-client uses just this socket //
    const char * sport;
    if (domain != AF_INET || type != SOCK_DGRAM
	|| (sport = getenv("MXTX_MOSH_PORT")) == null)
	return sd;
    int iport = atoi(sport);
    if (iport < 1024 || iport > 65536)
	return sd;
    int hip = ((iport - 1022) / 254) + 1;
    int lop = ((iport - 1022) % 254) + 1;
    struct sockaddr_in iaddr = {
	.sin_family = AF_INET,
	.sin_addr.s_addr = IADDR(127,0,hip,lop),
	.sin_port = 0
    };
    if (bind(sd, (struct sockaddr *)&iaddr, sizeof iaddr) < 0) {
	int e = errno;
	close(sd);
	errno = e;
	return -1;
    }
    return sd;
}
