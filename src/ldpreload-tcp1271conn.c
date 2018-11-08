#if 0 /* -*- mode: c; c-file-style: "stroustrup"; tab-width: 8; -*-
 set -euf; uname=`exec uname`
 case $uname in *WIN*) echo $0: does not build on $uname; exit 0 ;; esac
 case ${1-} in '') set x -O2; shift; esac
 #case ${1-} in '') set x -ggdb; shift; esac
 x_exec () { printf %s\\n "$*" >&2; exec "$@"; }
 case ${1-} in dbg) sfx=-dbg DEFS=-DDBG; shift ;; *) sfx= DEFS=-DDBG=0 ;; esac
 trg=${0##*''/}; trg=${trg%.c}$sfx.so; test ! -e "$trg" || rm "$trg"
 x_exec ${CC:-gcc} -std=c99 -shared -fPIC -o "$trg" "$0" $DEFS $@ -ldl
 exit $? #
 */
#endif
/*
 * $ ldpreload-tcp1271conn.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *      Copyright (c) 2018 Tomi Ollila
 *          All rights reserved
 *
 * Created: Thu 01 Nov 2018 21:07:27 +0200 too
 * Last modified: Thu 08 Nov 2018 22:35:30 +0200 too
 */

/* This preload library forwards requested localhost connections to a
 * localhost port on mxtx endpoint. The environment variable TCP1271_MAP
 * specifies which ports are to be redirected.
 * Format is: port/dest[/dport][:port/dest[/dport][:...]]
 * An example: LD_PRELOAD=... TCP1271_MAP=5901/w/5900 vncviewer :1
 * (without /dport (/5900 in example) connect to 'port' (5901 in example)).
 * connect(2) will fail (with EIO) if TCP1271_MAP is not defined and (with
 * EINVAL) if parse/port value error is encountered before match.
 */

#define VERDATE "1.0 (2018-11-08)"

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
#pragma GCC diagnostic warning "-Wformat-nonliteral" // XXX ...
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
#ifndef __clang__
#pragma GCC diagnostic error "-Wlogical-op" // XXX ...
#endif
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

#if defined(__linux__) && __linux__
#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <sys/uio.h> // for iovec
#include <dlfcn.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/types.h>
// consts in sockaddr types problematic ...
#define connect(a,b,c) xconnect(a,b,c)
#define setsockopt(a,b,c,d,e) xsetsockopt(a,b,c,d,e)
#include <sys/socket.h>
#undef connect
#undef setsockopt
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/poll.h>

#define null ((void*)0)

#include <netinet/in.h>
#include <netinet/tcp.h> // for SOL_TCP
#include <arpa/inet.h>

#if defined(__linux__) && __linux__
#include <endian.h> //for older compilers not defining __BYTE_ORDER__ & friends
#else
#include <sys/endian.h> //ditto
#endif

#ifndef __BYTE_ORDER
#define __BYTE_ORDER _BYTE_ORDER // opportunistic, picked from freebsd
#define __LITTLE_ENDIAN _LITTLE_ENDIAN
#define __BIG_ENDIAN _BIG_ENDIAN
#endif

// gcc -dM -E -x c /dev/null | grep BYTE

//#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#if __BYTE_ORDER == __BIG_ENDIAN // from endian.h

#define IADDR(a,b,c,d) ((in_addr_t)((a << 24) + (b << 16) + (c << 8) + d))

//#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#elif __BYTE_ORDER == __LITTLE_ENDIAN // from endian.h

#define IADDR(a,b,c,d) ((in_addr_t)(a + (b << 8) + (c << 16) + (d << 24)))

#else
#error unknown ENDIAN
#endif

#if 000
#if DBG
static const char _vers[] = "ldpreload-" " (dbg) " VERDATE;
#else
static const char _vers[] = "ldpreload-"  VERDATE;
#endif
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

#define isizeof (int)sizeof

#if DBG
static char dbuf[4096];
#define dz do { char * dptr = dbuf; di(__LINE__) dc(':') spc
#define da(a) { memcpy(dptr, a, sizeof a - 1); dptr += sizeof a - 1; }
#define ds(s) if (s) { int dbgl = strlen(s); \
	memcpy(dptr, s, dbgl); dptr += dbgl; } else da("(null)")
#define dc(c) *dptr++ = c;
#define spc *dptr++ = ' ';
#define dot *dptr++ = '.';
#define cmm *dptr++ = ','; spc
#define cln *dptr++ = ':'; spc
#define dnl *dptr++ = '\n';
#define du(u) dptr += sprintf(dptr, "%lu", (unsigned long)u);
#define di(i) dptr += sprintf(dptr, "%ld", (long)i);
#define dx(x) dptr += sprintf(dptr, "%lx", (long)x);

#define df da(__func__) dc('(')
#define dfc dc(')')
#define dss(s) da(#s) da(": \"") ds(s) dc('"')
#define dsi(i) da(#i) da(": ") di(i)

#define dw  dnl if (write(2, dbuf, dptr - dbuf)) {} } while (0)
#define dws spc if (write(2, dbuf, dptr - dbuf)) {} } while (0)

#else

#define DBG 0
#define dz do {
#define da(a)
#define ds(s)
#define dc(c)
#define spc
#define dot
#define cmm
#define cln
#define dnl
#define du(u)
#define di(i)
#define dx(x)

#define df
#define dfc
#define dss(s)
#define dsi(i)

#define dw } while (0)
#define dws } while (0)
#endif
#define dw0 } while (0)

static void vwarn(const char * format, va_list ap)
{
    int error = errno;

    vfprintf(stderr, format, ap);
    if (format[strlen(format) - 1] == ':')
	fprintf(stderr, " %s\n", strerror(error));
    else
	fputs("\n", stderr);
    fflush(stderr);
}
#if 0
static void warn(const char * format, ...) ATTRIBUTE ((format (printf, 1, 2)));
static void warn(const char * format, ...)
{
    va_list ap;

    va_start(ap, format);
    vwarn(format, ap);
    va_end(ap);
}
#endif
static void die(const char * format, ...) ATTRIBUTE ((format (printf, 1, 2)));
static void die(const char * format, ...)
{
    va_list ap;

    va_start(ap, format);
    vwarn(format, ap);
    va_end(ap);
    exit(1);
}

static int fill_sockaddr_un(struct sockaddr_un * addr, const char * format,...)
    ATTRIBUTE ((format (printf, 2, 3)));
static int fill_sockaddr_un(struct sockaddr_un * addr, const char * format,...)
{
    va_list ap;
    memset(addr, 0, sizeof *addr);
    va_start(ap, format);
    // we lose one char. perhaps that is tolerable
    int pathlen = vsnprintf(addr->sun_path, sizeof addr->sun_path, format, ap);
    va_end(ap);
    if ((unsigned)pathlen >= sizeof addr->sun_path)
	die("socket path length %d too big", pathlen);
    addr->sun_family = AF_UNIX;
    return pathlen + offsetof(struct sockaddr_un, sun_path);
}

static void * dlsym_next(const char * symbol)
{
    void * sym = dlsym(RTLD_NEXT, symbol);
    char * str = dlerror();

    if (str != null)
	die("finding symbol '%s' failed: %s", symbol, str);

    return sym;
}
#define set_next(name) *(void**)(&name##_next) = dlsym_next(#name)


// Macros FTW! -- use gcc -E to examine expansion

#define _deffn(_rt, _fn, _args) \
_rt _fn _args; \
_rt _fn _args { \
    static _rt (*_fn##_next) _args = null; \
    if (! _fn##_next ) *(void**) (&_fn##_next) = dlsym_next(#_fn);


_deffn ( int, setsockopt, (int sockfd, int level, int optname,
			   const void *optval, socklen_t optlen) )
#if 0
{
#endif
    int rv = setsockopt_next(sockfd, level, optname, optval, optlen);
    if (level == SOL_TCP) return 0;
    return rv;
}

_deffn ( int, connect, (int sd,
			const struct sockaddr * addr, socklen_t addrlen) )
#if 0
{
#endif
#if DBG
    if (addr->sa_family == AF_INET) {
	const unsigned char * s =
	    (const unsigned char *)&(((struct sockaddr_in *)addr)->sin_addr);
	dz da("IP: ") du(s[0]) dot du(s[1]) dot du(s[2]) dot du(s[3])
	   da(", PORT: ") du(ntohs(((struct sockaddr_in*)addr)->sin_port)) dw;
    }
#endif

#define errno_return(e) do { errno = e; return -1; } while (0)

    const char * tmap = getenv("TCP1271_MAP");
    if (tmap == null) errno_return(EIO);

    /* currently no AF_INET6 support ... */
    if (addr->sa_family != AF_INET)
	return connect_next(sd, addr, addrlen);

    int socktype = -1;
    socklen_t typelen = sizeof socktype;
    (void)getsockopt(sd, SOL_SOCKET, SO_TYPE, &socktype, &typelen);

    /* currently no AF_INET6 support ... */
    if (socktype != SOCK_STREAM)
	return connect_next(sd, addr, addrlen);

    if (((const struct sockaddr_in*)addr)->sin_addr.s_addr != IADDR(127,0,0,1))
	return connect_next(sd, addr, addrlen);

    in_port_t dport = ntohs(((const struct sockaddr_in*)addr)->sin_port);

    const char * pp, *dp;
    int pl, dl;
    while (1) {
	char * p;
	int port = strtol(tmap, &p, 10);
	dz da("m1: ") di(port) dw;
	if (port < 1 || port > 65535) errno_return(EINVAL);
	dz da("m2: ") ds(p) dw;
	if (*p != '/') errno_return(EINVAL);
	if (port != dport) {
	    tmap = strchr(p, ':');
	    if (tmap == null) return connect_next(sd, addr, addrlen);
	    tmap++;
	    continue;
	}
	pp = tmap;
	pl = p - tmap;
	dp = p + 1;
	dl = strcspn(dp, ":/ \t\n");
	dz da("m3: ") di(dl) spc ds(dp) dw;
	const char * xp = dp + dl;
	if (xp[0] == ':' || xp[0] == '\0') break;
	if (xp[0] != '/') errno_return(EINVAL);
	xp++;
	int xport = strtol(xp, &p, 10);
	dz da("m4: ") di(port) dw;
	if (*p != ':' && *p != '\0') errno_return(EINVAL);
	if (xport < 1 || xport > 65535) errno_return(EINVAL);
	pp = xp;
	pl = p - xp;
	break;
    }
    dz da("m5: ") ds(pp) dw;
    while (*pp == '0') { pp++; pl--; }

#define return_ECONNREFUSED do { errno = ECONNREFUSED; return -1; } while (0)

    BB;
    long sdflags_orig = fcntl(sd, F_GETFL);
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s < 0)
	return_ECONNREFUSED;
    dup2(s, sd);
    close(s);

    long sdflags_new = fcntl(sd, F_GETFL);
    dz da("fcntl: ") dx(sdflags_new) dw;

    // nonblock or not. does not matter here //
    if (sdflags_new != sdflags_orig)
	(void)fcntl(sd, F_SETFL, sdflags_orig);

#if 0	/* not needed now (or in a decade), for possible future reference */
	if (flarg & O_ASYNC) {
	    dz da("async") dw;
	    fpid = fcntl(sd, F_GETOWN);
	}
#endif
    BE;
    BB;
    struct sockaddr_un uaddr;

    addrlen = fill_sockaddr_un(&uaddr, "%c/tmp/user-%d/mxtx,%.*s",
			       0, getuid(), dl, dp);

    // dz da("inet to unix port '") ds(uaddr + 1) dw;

    int rv = connect_next(sd, (struct sockaddr *)&uaddr, addrlen);
    if (rv < 0) return rv;
    BE;
    char buf[32];
    int bl = snprintf(buf + 4, sizeof buf - 4,
		      ":connect" "%c" "127.0.0.1" "%c" "%.*s" "%c",
		      0, 0, pl, pp, 0);
    if (bl > isizeof buf - 4) die("unlikely internal error");
    *((uint32_t*)&buf) = htonl(bl);
    write(sd, buf, bl + 4);
    BB;
    struct pollfd pfd = { .fd = sd, .events = POLLIN };
    if (poll(&pfd, 1, 5000) <= 0) errno_return(EIO);
    BE;

    char b[1];
    if (read(sd, b, 1) != 1) return -1;
    if (b[0] != 0) errno_return(EIO);
    return 0;
}
