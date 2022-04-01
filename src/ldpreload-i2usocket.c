#if 0 /* -*- mode: c; c-file-style: "stroustrup"; tab-width: 8; -*-
 set -euf; uname=`exec uname`
 case $uname in *WIN*) echo $0: does not build on $uname; exit 0 ;; esac
 WARN="-Wall -Wstrict-prototypes -Winit-self -Wformat=2" # -pedantic
 WARN="$WARN -Wcast-align -Wpointer-arith " # -Wfloat-equal #-Werror
 WARN="$WARN -Wextra -Wwrite-strings -Wcast-qual -Wshadow" # -Wconversion
 WARN="$WARN -Wmissing-include-dirs -Wundef -Wbad-function-cast -Wlogical-op"
 WARN="$WARN -Waggregate-return -Wold-style-definition"
 WARN="$WARN -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls"
 WARN="$WARN -Wnested-externs -Winline -Wvla -Woverlength-strings -Wpadded"
 case ${1-} in '') set x -O2; shift; esac
 #case ${1-} in '') set x -ggdb; shift; esac
 x () { printf %s\\n "$*" >&2; "$@"; }
 c () {
	trg=ldpreload-i2u''$1''$sfx.so; shift
	x ${CC:-gcc} -std=c99 -shared -fPIC -o "$trg" "$0" $WARN $DEFS $@ -ldl
 }
 case ${1-} in dbg) sfx=-dbg DEFS=-DDBG; shift ;; *) sfx= DEFS=-DDBG=0 ;; esac
 c bind -DBIND "$@"
 c connect5 -DCONNECT "$@"
 exit $?
 */
#endif
/*
 * $ ldpreload-i2usocket.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *      Copyright (c) 2015 Tomi Ollila
 *          All rights reserved
 *
 * Created: Tue 22 Nov 2011 16:55:43 +0200 too
 * For ttt: Fri 03 Oct 2014 19:21:19 +0300 too
 * For mxtx: Sat 26 Aug 2017 21:24:28 +0300 too
 * Last modified: Sat 02 Apr 2022 00:22:18 +0300 too
 */

#define VERDATE "1.1 (2017-08-31)"

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
#define bind(a,b,c) xbind(a,b,c)
#define connect(a,b,c) xconnect(a,b,c)
#include <sys/socket.h>
#undef bind
#undef connect
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/un.h>

#define null ((void*)0)

#include <netinet/in.h>
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
#define du(u) dptr += sprintf(dptr, "%lu", (unsigned long)i);
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

#if DBG
static void dwritebytes(const char * info, char * p, int l) ATTRIBUTE((unused));
static void dwritebytes(const char * info, char * p, int l)
{
    char buf[3];
    int err = errno;

    write(2, info, strlen(info));

    buf[0] = ' ';
    while (l--) {
	if (0 && *p > 32 && *p < 127) {
	    buf[1] = *p++;
	    write(2, buf, 2);
	    continue;
	}
	buf[1] = hexchar[(*p>>4) & 0xf];
	buf[2] = hexchar[*p++ & 0xf];
	write(2, buf, 3);
    }
    write(2, "\n", 1);
    errno = err;
}
#else
#define dwritebytes(i, p, l) do {} while (0)
#endif


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


#ifdef BIND

int bind(int sd, const struct sockaddr * addr, socklen_t addrlen);
int bind(int sd, const struct sockaddr * addr, socklen_t addrlen)
{
    static int (*bind_next)(int, const struct sockaddr *, socklen_t) = null;
    if (! bind_next)
	set_next(bind);

    if (addr->sa_family != AF_INET)
	goto _next;

    int socktype = -1;
    socklen_t typelen = sizeof socktype;
    (void)getsockopt(sd, SOL_SOCKET, SO_TYPE, &socktype, &typelen);
    if (socktype != SOCK_STREAM && socktype != SOCK_DGRAM)
	goto _next;

    in_port_t port = IPORT(((const struct sockaddr_in*)addr)->sin_port);

    /* fcntl flags (like non-blockingness...), probably no need to handle (?)*/
    int s = socket(AF_UNIX, socktype, 0);
    if (s < 0)
	return -1;
    (void)dup2(s, sd);
    (void)close(s);

    s = 1; setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &s, sizeof s);

    struct sockaddr_un uaddr;
    // path format like in mxtx-socksproxy.c // xxx abstract ns now only...
    char c = (socktype == SOCK_STREAM)? 's': 'd';
    addrlen = fill_sockaddr_un(&uaddr,"%c/tmp/user-%d/un-%c-%d",
				   0, getuid(), c, port);

    return bind_next(sd, (struct sockaddr *)&uaddr, addrlen);
_next:
    return bind_next(sd, addr, addrlen);
}

#endif /* BIND */

#ifdef CONNECT

/* wait up to 5000 ms ... */
static int pollthis(int fd, int event)
{
    struct pollfd pfd;

    pfd.fd = fd;
    pfd.events = event;

    if (poll(&pfd, 1, 500) <= 0) /* Some EINTR protection */
	if (poll(NULL, 0, 200), poll(&pfd, 1, 4300) <= 0) {
	    dz da("poll errno: ") di(errno) dw;
	    errno = EIO;
	    return -1;
	}
    return 1;
}

int connect(int sd, const struct sockaddr * addr, socklen_t addrlen);
int connect(int sd, const struct sockaddr * addr, socklen_t addrlen)
{
    static int (*connect_next)(int, const struct sockaddr *, socklen_t) = null;
    if (! connect_next)
	set_next(connect);

#if DBG
    if (addr->sa_family == AF_INET) {
	const unsigned char * s =
	    (unsigned char *)&(((struct sockaddr_in *)addr)->sin_addr);
	dz da("IP: ") du(s[0]) dot du(s[1]) dot du(s[2]) dot du(s[3])
	   da(", PORT: ") du(IPORT(((struct sockaddr_in*)addr)->sin_port)) dw;
    }
#endif
    if (addr->sa_family != AF_INET) {
	if (addr->sa_family == AF_INET6) return -1; // FIXME
	return connect_next(sd, addr, addrlen);
    }
    int socktype = -1;
    socklen_t typelen = sizeof socktype;
    (void)getsockopt(sd, SOL_SOCKET, SO_TYPE, &socktype, &typelen);
    if (socktype != SOCK_STREAM && socktype != SOCK_DGRAM )
	//return connect_next(sd, addr, addrlen);
	return -1; // FIXME

    /* currently no AF_INET6 support ... */
    int lh =
	((const struct sockaddr_in*)addr)->sin_addr.s_addr == IADDR(127,0,0,1);

    BB;
    long sdflags_orig = fcntl(sd, F_GETFL);
    int s = socket(AF_UNIX, lh? socktype: SOCK_STREAM, 0);
    if (s < 0)
	return -1;
    dup2(s, sd);
    close(s);

    long sdflags_new = fcntl(sd, F_GETFL);
    dz da("fcntl: ") dx(sdflags_curr) dw;

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
    in_port_t port = IPORT(((const struct sockaddr_in*)addr)->sin_port);

    char c = (! lh || socktype == SOCK_STREAM)? 's': 'd';
    addrlen = fill_sockaddr_un(&uaddr,"%c/tmp/user-%d/un-%c-%d",
			       0, getuid(), c, port);

    // dz da("inet to unix port '") ds(uaddr + 1) dw;

    int rv = connect_next(sd, (struct sockaddr *)&uaddr, addrlen);
    if (lh || rv < 0)
	return rv;
    BE;

    /* else non-127.0.0.1 destination: socks5 communication */

    (void)!write(sd, "\005\001", 3);
    char buf[272];
    if (pollthis(sd, POLLIN) < 0)
	goto _nogo;
    if (read(sd, buf, sizeof buf) != 2)
	goto _nogo;
    if (buf[0] != 5 || buf[1] != 0) // 0: no authentication //
	goto _nogo;
    buf[0] = 5; buf[1] = (socktype == SOCK_DGRAM)? 0x03: 0x01; buf[2] = 0;
    buf[3] = 1; // < ATYP: ipv4 addr
    /* ip. 4 bytes. network byte order */
    memcpy(&buf[4], &((const struct sockaddr_in*)addr)->sin_addr.s_addr, 4);
    /* port. 2 bytes. network byte order */
    memcpy(&buf[8], &((const struct sockaddr_in*)addr)->sin_port, 2);
    dwritebytes("socks5 ->", buf, 10);
    if (pollthis(sd, POLLIN) < 0)
	goto _nogo;
    if (read(sd, buf, 5) != 5)
	goto _nogo;
    if (buf[0] != 5 || buf[1] != 0) // 0: access granted //
	goto _nogo;
    if (buf[4] == 0x01) { // ipv4 address
	if (read(sd, buf, 5) != 5)
	    goto _nogo;
    }
    else if (buf[4] == 0x04) { // ipv6 address
	if (read(sd, buf, 17) != 17)
	    goto _nogo;
    }
    else if (buf[4] == 0x03) { // domain name
	int len = (uint8_t)buf[5] + 2;
	if (read(sd, buf, len) != len)
	    goto _nogo;
    }
    else
	goto _nogo;

    return 0;

_nogo:
    shutdown(sd, 2);
    errno = EIO;
    return -1;
}

#endif /* CONNECT */
