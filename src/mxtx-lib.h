#if 0 /* -*- mode: c; c-file-style: "stroustrup"; tab-width: 8; -*-
 set -x; : test compile :; exec ${CC:-gcc} "$0"
 exit $?
 */
#endif
/*
 * $ mxtx-lib.h $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *      Copyright (c) 2017 Tomi Ollila
 *          All rights reserved
 *
 * Created: Wed 16 Aug 2017 21:07:44 EEST too
 * Last modified: Mon 05 Feb 2018 06:53:06 -0800 too
 */

#ifndef MXTX_LIB_H
#define MXTX_LIB_H

// add just the header files to make cc mxtx-lib.h to compile

#ifndef NO_DIEWARN_PROTOS
#include <stdarg.h>
#endif
#include <stdbool.h>

// clang -dM -E - </dev/null | grep __GNUC__  outputs '#define __GNUC__ 4'
#if (__GNUC__ >= 4)
#define ATTRIBUTE(x) __attribute__(x)
#else
#define ATTRIBUTE(x)
#endif

// w/ this define defining _DEFAULT_SOURCE or _BSD_SOURCE can be avoided...
// but, seterr_linebuf...
//#define setlinebuf(stream) setvbuf((stream), NULL, _IOLBF, 0)

#ifndef NO_DIEWARN_PROTOS
void vwarn(const char * format, va_list ap);
void warn(const char * format, ...) ATTRIBUTE ((format (printf, 1, 2)));
void die(const char * format, ...)
    ATTRIBUTE ((format (printf, 1, 2))) ATTRIBUTE ((noreturn));
#endif

void seterr_linebuf(void);

// be specific on system where to check/set SA_NOCLDWAIT
#if defined (__CYGWIN__) && __CYGWIN__
//#ifndef SA_NOCLDWAIT
#define SA_NOCLDWAIT 0 // seems to be #defined to 2 on linux //
//#endif
#endif

void sigact(int sig, void (*handler)(int), int flags);

int xsocket(int domain, int type);
void xdup2(int o, int n);
int xfcntl(int fd, int cmd, int arg);
void xexecvp(const char * file, char *const argv[]) ATTRIBUTE ((noreturn));
void set_nonblock(int fd);

void xreadfully(int fd, void * buf, ssize_t len);
void xwritefully(int fd, const void * buf, ssize_t len);

struct sockaddr_un;
int fill_sockaddr_un(struct sockaddr_un * addr, const char * format, ...)
        ATTRIBUTE ((format (printf, 2, 3)));
char * default_mxtx_socket_path(const char * suffix);

bool checkpeerid(int sd);
int connect_to_mxtx(const char * path);
int xbind_unix_port(struct sockaddr_un * saddr, int pathlen);

static inline void xmovefd(int ofd, int nfd) {
    if (ofd == nfd) return;
    xdup2(ofd, nfd); close(ofd);
}

#endif /* MXTX_LIB_H */
