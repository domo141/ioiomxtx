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
 * $ ldpreload-vsfa.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *      Copyright (c) 2017 Tomi Ollila
 *          All rights reserved
 *
 * Created: Thu 14 Dec 2017 08:48:34 EET too
 * Last modified: Thu 01 Feb 2018 19:38:11 +0200 too
 */

/* "virtual soft file access" ld_preload library, a hacky module to
 * redirect file open to a file behind mxtx link. intercepts open() and
 * fopen() C library functions (and now stat(2)...). any related (like seek())
 * are not supported. also, "opened" files are not seekable. thus, there are
 * many cases that don't work, but the point here is to focus on the cases
 * that do work.
 */

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

#define _ATFILE_SOURCE

#define open open_hidden
#define open64 open64_hidden
#define openat openat_hidden
#define openat64 openat64_hidden
#define open open_hidden
#define fopen fopen_hidden
#define stat stat_x // struct stat... //
#define lstat stat_hidden
#define __xstat __xstat_hidden
#define __xstat64 __xstat64_hidden
#define __lxstat __lxstat_hidden
#define __lxstat64 __lxstat64_hidden
#define access access_hidden

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h> // offsetof()
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
//#include <.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <dlfcn.h>

#undef open
#undef open64
#undef openat
#undef openat64
#undef fopen
#undef stat
#undef lstat
#undef __xstat
#undef __xstat64
#undef __lxstat
#undef __lxstat64
#undef access

#define null ((void*)0)

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


#if DBG
static const char hexchar[16] = "0123456789abcdef";
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
static void warn(const char * format, ...) ATTRIBUTE ((format (printf, 1, 2)));
static void warn(const char * format, ...)
{
    va_list ap;

    va_start(ap, format);
    vwarn(format, ap);
    va_end(ap);
}
static void die(const char * format, ...) ATTRIBUTE ((format (printf, 1, 2)));
static void die(const char * format, ...)
{
    va_list ap;

    va_start(ap, format);
    vwarn(format, ap);
    va_end(ap);
    exit(1);
}

static void * dlsym_next(const char * symbol)
{
    void * sym = dlsym(RTLD_NEXT, symbol);
    char * str = dlerror();

    if (str != null)
	die("finding symbol '%s' failed: %s", symbol, str);

    return sym;
}


// from mxtx-lib.c...
static bool checkpeerid(int sd)
{
#if defined(__linux__) && __linux__ || defined(__CYGWIN__) && __CYGWIN__
    struct ucred cr;
    socklen_t len = sizeof cr;
    if (getsockopt(sd, SOL_SOCKET, SO_PEERCRED, &cr, &len) < 0) {
        warn("getsockopt SO_PEERCRED on channel %d failed:", sd);
        return false;
    }
    int uid = (int)getuid();
    if ((int)cr.uid != uid) {
        warn("Peer real uid %d not %d on channel %d", (int)cr.uid, uid, sd);
        return false;
    }
    return true;
#elif defined (__XXXunsupp__) && __XXXunsupp__
#error fixme, when known what and how...
    //getpeerucred(...);
    warn("Peer check unimplemented");
    return false;
#else
    // fallback default -- so far not tested anywhere //
    int peuid, pegid;
    if (getpeereid(sd, &peuid, &pegid) < 0) {
        warn("getpeereid on channel %d failed:", sd);
        return false;
    }
    int euid = (int)geteuid();
    if (peuid != euid) {
        warn("Peer effective uid %d not %d on channel %d",(int)peuid,euid, sd);
        return false;
    }
    return true;
#endif
}

// slightly different than connect_to_mxtx(default_mxtx_socket_path(lnk));
// perhaps unify, PIC to be solved first, though (and parallelism in that other
static int connect_to_mxtx_with(const char * lnk)
{
    struct sockaddr_un addr = {
        .sun_family = AF_UNIX
    };
// default_mxtx_socket_path() part
#if defined(__linux__) && __linux__
    uid_t uid = getuid();
    int len = snprintf(addr.sun_path, sizeof addr.sun_path,
		       "@/tmp/user-%d/mxtx,%s", uid, lnk);
    addr.sun_path[0] = '\0';
#else
    char * xdgrd = getenv("XDG_RUNTIME_DIR");
    int len;
    if (xdgrd) {
        len = snprintf(addr.sun_path, sizeof addr.sun_path,
		       "%s/mxtx,%s", xdgrd, lnk);
    }
    else {
	uid_t uid = getuid();
	(void)snprintf(path, sizeof path, "/tmp/user-%d", uid);
	(void)mkdir(path, 0700);
	len = snprintf(path, sizeof path, "/tmp/user-%d/mxtx,%s", uid, lnk);
    }
#endif
    if ((size_t)len >= sizeof addr.sun_path) {
	warn("socket path too long (%d octets)", len);
	return -1;
    }

    int sd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sd < 0) {
        warn("creating socket failed:");
        return -1;
    }
    int one = 1;
    setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof one);

    int pathlen = len + offsetof(struct sockaddr_un, sun_path);
    if (connect(sd, (struct sockaddr *)&addr, pathlen) < 0) {
        if (addr.sun_path[0] == '\0') addr.sun_path[0] = '@';
        warn("connecting to mxtx port %s failed:", addr.sun_path);
        goto closerr;
    }
    if (! checkpeerid(sd))
        goto closerr;

    return sd;
closerr:
    close(sd);
    return -1;
}


#define LNKMAX 32
static unsigned getlink(char * lnk, const char * pathname)
{
    for (int i = 0; pathname[i]; i++) {
	if (pathname[i] == ':') {
	    if (i >= LNKMAX) break;
	    memcpy(lnk, pathname, i);
	    lnk[i] = '\0';
	    return i + 1;
	}
	if (pathname[i] == '/') break;
    }
    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"  // write(2) calls //

static int open_read(const char * lnk, const char * pathname)
{
    // w/ use of gather array (and writev) some copying and memory
    // usage could be avoided, but it works surprisingly non-atomically)
    char buf[4120];
    int plen = strlen(pathname) + 1;
    if (plen > 4096) return -1;

    int sd = connect_to_mxtx_with(lnk);
    int mlen = plen + 4;
    buf[0] = buf[1] = 0; buf[2] = mlen >> 8; buf[3] = mlen & 0xff;
    /*buf[4] = '~';*/ buf[4] = 'c'; buf[5] = 'a'; buf[6] = 't'; buf[7] = '\0';
    memcpy(buf + 8, pathname, plen);
    write(sd, buf, mlen + 4);
    return sd;
}

static const char sh[] = "set -eu"
    "\n" "if test \"$0\" = '+'; then exec 3>>\"$1\"; else exec 3>\"$1\"; fi"
    "\n" "test -z \"$2\" || chmod $2 \"$1\" || :"
    "\n" "exec cat >&3";

static int open_write(const char * lnk, const char * pathname,
		      int append, int mode)
{
    //write(2, sh, sizeof sh);
    char buf[6144];
    int plen = strlen(pathname) + 1;
    if (plen > 4096) return -1;
    int sd = connect_to_mxtx_with(lnk);
    buf[0] = buf[1] = 0; /**/ buf[4] = 's'; buf[5] = 'h'; buf[6] = '\0';
    buf[7] = '-'; buf[8] = 'c'; buf[9] = '\0';
    memcpy(buf + 10, sh, sizeof sh);
    char * p = buf + 10 + sizeof sh;
    *p++ = append? '+': '-'; *p++ = '\0';
    memcpy(p, pathname, plen); p += plen;
    if ((unsigned)mode <= 0777) {
	sprintf(p, "%o", mode);
	p += strlen(p);
    }
    else *p = '\0';
    int mlen = p - buf - 3;
    buf[2] = mlen >> 8; buf[3] = mlen & 0xff;
    write(sd, buf, mlen + 4);
    return sd;
}

static int stat_it(const char * lnk, const char * pathname, struct stat_x * st)
{
    char buf[4120];
    int plen = strlen(pathname) + 1;
    if (plen > 4096) return -1;

    int sd = connect_to_mxtx_with(lnk);

#define STAT_CMD "stat\0-c\0%d %i %f %u %g %s %B %b %X %Y %Z"
    memcpy(buf + 4, STAT_CMD, sizeof STAT_CMD);
    char * p = buf + 4 + sizeof STAT_CMD;
#undef STAT_CMD

    memcpy(p, pathname, plen); p += plen;
    int mlen = p - buf - 4;

    buf[0] = buf[1] = 0; buf[2] = mlen >> 8; buf[3] = mlen & 0xff;
    write(sd, buf, mlen + 4);
    int l = read(sd, buf, 512);
    close(sd);
    if (l < 20) return -1;
    buf[l] = '\0';
    st->st_dev = strtol(buf, &p, 10);
    st->st_ino = strtol(p, &p, 10);
    st->st_mode = strtol(p, &p, 16);
    st->st_nlink = 1;
    st->st_uid = strtol(p, &p, 10);
    st->st_gid = strtol(p, &p, 10);
    st->st_rdev = 0;
    st->st_size = strtol(p, &p, 10);
    st->st_blksize = strtol(p, &p, 10);
    st->st_blocks = strtol(p, &p, 10);
    st->st_atime = strtol(p, &p, 10);
    st->st_mtime = strtol(p, &p, 10);
    st->st_ctime = strtol(p, null, 10);
    return 0;
}
#pragma GCC diagnostic pop

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

_deffn ( int, open, (const char * pathname, int flags, mode_t mode) )
#if 0
{
#endif
    cprintf("*** open(\"%s\", %x, %o)\n", pathname, flags, mode);
    char lnk[LNKMAX];
    int o;
    if ((o = getlink(lnk, pathname)) == 0)
	return open_next(pathname, flags, mode);

    if (flags & O_RDWR) return -1;

    if (flags & O_WRONLY)
	return open_write(lnk, pathname + o, (flags & O_APPEND), mode & 0777);
    else
	return open_read(lnk, pathname + o);
}

_deffn ( int, open64, (const char * pathname, int flags, mode_t mode) )
#if 0
{
#endif
    cprintf("*** open64(\"%s\", %x, %o)\n", pathname, flags, mode);
    char lnk[LNKMAX];
    int o;
    if ((o = getlink(lnk, pathname)) == 0)
	return open64_next(pathname, flags, mode);

    if (flags & O_RDWR) return -1;

    if (flags & O_WRONLY)
	return open_write(lnk, pathname + o, (flags & O_APPEND), mode & 0777);
    else
	return open_read(lnk, pathname + o);
}

_deffn ( int, openat, (int dirfd, const char * pathname, int flags, mode_t mode) )
#if 0
{
#endif
    cprintf("*** openat(%d, \"%s\", %x, %o)\n", dirfd, pathname, flags, mode);
    if (dirfd == AT_FDCWD)
	return open(pathname, flags, mode);
    return openat_next(dirfd, pathname, flags, mode);
}

_deffn ( int, openat64, (int dirfd, const char * pathname, int flags, mode_t mode) )
#if 0
{
#endif
    cprintf("*** openat64(%d, \"%s\", %x, %o)\n", dirfd, pathname, flags, mode);
    if (dirfd == AT_FDCWD)
	return open64(pathname, flags, mode);
    return openat64_next(dirfd, pathname, flags, mode);
}

_deffn ( FILE *, fopen, (const char * pathname, const char * mode) )
#if 0
{
#endif
    cprintf("*** fopen(\"%s\", \"%s\")\n", pathname, mode);
    char lnk[LNKMAX];
    int o;
    if ((o = getlink(lnk, pathname)) == 0)
	return fopen_next(pathname, mode);

    if (strchr(mode, '+') != NULL) return NULL;

    int fd;
    if (mode[0] == 'r')
	fd = open_read(lnk, pathname + o);
    else
	fd = open_write(lnk, pathname + o, !!strchr(mode, 'a'), -1);

    if (fd < 0) return NULL;
    return fdopen(fd, mode);
}

#if 1
_deffn ( int, stat, (const char * pathname, struct stat_x * statbuf) )
#if 0
{
#endif
    cprintf("*** stat(\"%s\", \"%p\")\n", pathname, statbuf);
    char lnk[LNKMAX];
    int o;
    if ((o = getlink(lnk, pathname)) == 0)
	return stat_next(pathname, statbuf);

    return stat_it(lnk, pathname + o, statbuf);
}
_deffn ( int, lstat, (const char * pathname, struct stat_x * statbuf) )
#if 0
{
#endif
    cprintf("*** lstat(\"%s\", \"%p\")\n", pathname, statbuf);
    char lnk[LNKMAX];
    int o;
    if ((o = getlink(lnk, pathname)) == 0)
	return lstat_next(pathname, statbuf);

    return stat_it(lnk, pathname + o, statbuf);
}
#endif

_deffn ( int, __xstat, (int v, const char * pathname, struct stat_x * statbuf) )
#if 0
{
#endif
    cprintf("*** __xstat(%d, \"%s\", \"%p\")\n", v, pathname, statbuf);
    char lnk[LNKMAX];
    int o;
    if ((o = getlink(lnk, pathname)) == 0)
	return __xstat_next(v, pathname, statbuf);

    return stat_it(lnk, pathname + o, statbuf);
}

_deffn ( int, __xstat64, (int v, const char * pathname, struct stat_x * statbuf) )
#if 0
{
#endif
    cprintf("*** __xstat64(%d, \"%s\", \"%p\")\n", v, pathname, statbuf);
    char lnk[LNKMAX];
    int o;
    if ((o = getlink(lnk, pathname)) == 0)
	return __xstat64_next(v, pathname, statbuf);

    return stat_it(lnk, pathname + o, statbuf);
}

// fake lstat*s(), same code as in stat*s()
_deffn ( int, __lxstat, (int v, const char * pathname, struct stat_x * statbuf) )
#if 0
{
#endif
    cprintf("*** __lxstat(%d, \"%s\", \"%p\")\n", v, pathname, statbuf);
    char lnk[LNKMAX];
    int o;
    if ((o = getlink(lnk, pathname)) == 0)
	return __lxstat_next(v, pathname, statbuf);

    return stat_it(lnk, pathname + o, statbuf);
}

_deffn ( int, __lxstat64, (int v, const char * pathname, struct stat_x * statbuf) )
#if 0
{
#endif
    cprintf("*** __lxstat64(%d, \"%s\", \"%p\")\n", v, pathname, statbuf);
    char lnk[LNKMAX];
    int o;
    if ((o = getlink(lnk, pathname)) == 0)
	return __lxstat64_next(v, pathname, statbuf);

    return stat_it(lnk, pathname + o, statbuf);
}

_deffn ( int, access, (const char * pathname, int mode) )
#if 0
{
#endif
    cprintf("*** access(\"%s\", %x)\n", pathname, mode);
    char lnk[LNKMAX];
    int o;
    if ((o = getlink(lnk, pathname)) == 0)
	return access_next(pathname, mode);

    // dumb success...
    return 0;
}
