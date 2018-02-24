#if 0 /* -*- mode: c; c-file-style: "stroustrup"; tab-width: 8; -*-
 set -euf; trg=${0##*''/}; trg=${trg%.c}.o; test ! -e "$trg" || rm "$trg"
 WARN="-Wall -Wstrict-prototypes -Winit-self -Wformat=2" # -pedantic
 WARN="$WARN -Wcast-align -Wpointer-arith " # -Wfloat-equal #-Werror
 WARN="$WARN -Wextra -Wwrite-strings -Wcast-qual -Wshadow" # -Wconversion
 WARN="$WARN -Wmissing-include-dirs -Wundef -Wbad-function-cast -Wlogical-op"
 WARN="$WARN -Waggregate-return -Wold-style-definition"
 WARN="$WARN -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls"
 WARN="$WARN -Wnested-externs -Winline -Wvla -Woverlength-strings -Wpadded"
 case ${1-} in '') set x -O2; shift; esac
 #case ${1-} in '') set x -ggdb; shift; esac
 set -x; exec ${CC:-gcc} -std=c99 $WARN "$@" -o "$trg" -c "$0" # -flto
 exit $?
 */
#endif
/*
 * $ mxtx-lib.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *      Copyright (c) 2017 Tomi Ollila
 *          All rights reserved
 *
 * Created: Wed 16 Aug 2017 21:06:56 EEST too
 * Last modified: Sat 24 Feb 2018 16:46:14 +0200 too
 */

/* sh mxtx-lib.c will compile single mxtx-lib.o -- good for testing
 * that new code works. perl -x mxtx-lib.c will compile libmxtx.a
 */

#define _GNU_SOURCE 1   // for struct ucred
#define _POSIX_C_SOURCE 199309L // sigaction

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#include "mxtx-lib.h"

extern char * _prg_ident;
void vwarn(const char * format, va_list ap)
{
    int error = errno;

    //fputs(timestr(), stderr);
    fputs(_prg_ident, stderr);
    vfprintf(stderr, format, ap);
    if (format[strlen(format) - 1] == ':')
        fprintf(stderr, " %s\n", strerror(error));
    else
        fputs("\n", stderr);
    fflush(stderr);
}
void warn(const char * format, ...)
{
    va_list ap;

    va_start(ap, format);
    vwarn(format, ap);
    va_end(ap);
}
void die(const char * format, ...)
{
    va_list ap;

    va_start(ap, format);
    vwarn(format, ap);
    va_end(ap);
    exit(1);
}

void seterr_linebuf(void)
{
    static char buf[1024];
    setvbuf(stderr, buf, _IOLBF, sizeof buf);
}

void sigact(int sig, void (*handler)(int), int flags)
{
    struct sigaction action = { .sa_handler = handler,
                                 /* NOCLDSTOP needed if ptraced */
                                .sa_flags = flags|SA_NOCLDSTOP };
    sigemptyset(&action.sa_mask);
    sigaction(sig, &action, NULL);
}

int xsocket(int domain, int type)
{
    int sd = socket(domain, type, 0);
    if (sd < 0) die("socket:");
    return sd;
}

void xdup2(int o, int n)
{
    if (dup2(o, n) < 0)
        die("dup2:");
}

int xfcntl(int fd, int cmd, int arg)
{
    int rv = fcntl(fd, cmd, arg);
    if (rv < 0) die("fcntl:");
    return rv;
}

void xexecvp(const char * file, char *const argv[])
{
    execvp(file, argv);
    die("execvp: %s ...:", file);
}

void set_nonblock(int fd)
{
    xfcntl(fd, F_SETFL, xfcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

void xreadfully(int fd, void * buf, ssize_t len)
{
    if (len <= 0) return;

    while (1) {
        int l = read(fd, buf, len);

        if (l >= len) return;

        if (l <= 0) {
            if (l == 0) die("EOF from %d", fd);
            if (errno == EINTR) continue;
            else die("read(%d, ...) failed:", fd);
        }
        buf = (char *)buf + l;
        len -= l;
    }
}

void xwritefully(int fd, const void * buf, ssize_t len)
{
    if (len <= 0) return;

    while (1) {
        int l = write(fd, buf, len);

        if (l >= len) return;

        if (l <= 0) {
            if (l == 0) continue;
            if (errno == EINTR) continue;
            else die("write(%d, ...) failed:", fd);
        }
        buf = (const char *)buf + l;
        len -= l;
    }
}

bool checkpeerid(int sd)
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
#elif __XXXsolaris__ // fixme, when known what and how...
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

// :sockaddr_un.o
struct sockaddr_un * fill_sockaddr_un(struct sockaddr_un * addr,
                                      const char * format, ...)
{
    va_list ap;
    memset(addr, 0, sizeof *addr);
    va_start(ap, format);
    // we lose one char. perhaps that is tolerable
    int pathlen = vsnprintf(addr->sun_path, sizeof addr->sun_path, format, ap);
    va_end(ap);
    addr->sun_family = pathlen;
    return addr;
}

// :fillpath.o
struct sockaddr_un * fill_mxtx_socket_path(struct sockaddr_un * uaddr,
                                           const char * path, const char * x)
{
    if (strchr(path, '/') != NULL)
        return fill_sockaddr_un(uaddr, "%s%s", x, path);

    // else
#if defined(__linux__) && __linux__
    uid_t uid = getuid();
    return fill_sockaddr_un(uaddr, "%c/tmp/user-%d/mxtx%s,%s",0, uid, x, path);
#else
    char * xdgrd = getenv("XDG_RUNTIME_DIR");
    if (xdgrd)
        return fill_sockaddr_un(uaddr, "%s/mxtx%s,%s", xdgrd, x, path);
    // else
    uid_t uid = getuid();
    (void)snprintf(path, sizeof path, "/tmp/user-%d", uid);
    (void)mkdir(path, 0700);
    return fill_sockaddr_un("/tmp/user-%d/mxtx%s,%s", uid, x,suffix);
#endif
}

// :usconnect.o
int connect_unix_stream_socket(struct sockaddr_un * addr)
{
    int sd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sd < 0) {
        warn("creating socket failed:");
        return -1;
    }
    int one = 1;
    setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof one);

    unsigned int pathlen = addr->sun_family;
    addr->sun_family = AF_UNIX;

    if (pathlen >= sizeof addr->sun_path) {
        warn("socket path too long (%d octets)", pathlen);
        goto closerr;
    }
    pathlen += offsetof(struct sockaddr_un, sun_path);
    if (connect(sd, (struct sockaddr *)addr, pathlen) < 0) {
        if (addr->sun_path[0] == '\0') addr->sun_path[0] = '@';
        warn("connecting %s failed:", addr->sun_path);
        goto closerr;
    }
    if (! checkpeerid(sd))
        goto closerr;

    return sd;
closerr:
    close(sd);
    return -1;
}

// XXX mxtx nimeen jotenki
int connect_unix_stream_mxtx_socket(const char * path, const char * x)
{
    struct sockaddr_un addr;
    return connect_unix_stream_socket(fill_mxtx_socket_path(&addr, path, x));
}


int xbind_listen_unix_socket(struct sockaddr_un * addr, int type)
{
    int sd = xsocket(AF_UNIX, type);
    int one = 1;
#if 0
    close(sd); sd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in * iaddr = (struct sockaddr_in *)addr;
    memset(iaddr, 0, sizeof *iaddr); //INADDR_ANY
    iaddr->sin_family = AF_INET;
    iaddr->sin_port = htons(1080);
#endif
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

    unsigned int pathlen = addr->sun_family;
    addr->sun_family = AF_UNIX;

    if (pathlen >= sizeof addr->sun_path)
        die("socket path too long (%d octets)", pathlen);

    pathlen += offsetof(struct sockaddr_un, sun_path);
    if (bind(sd, (struct sockaddr *)addr, pathlen) < 0) {
        if (errno == EADDRINUSE)
            die("bind: address already in use\n"
                "The socket '%s' exists and may be live\n"
                "Remove the file and try again if the socket is stale",
                addr->sun_path[0]? addr->sun_path: addr->sun_path + 1);
        die("bind:");
    }
    if (listen(sd, 5) < 0)
        die("listen:");

    return sd;
}


#if 0 /*
#!perl
#line 317
#---- 317

use 5.8.1;
use strict;
use warnings;

unlink 'mxtx-lib.a', 'mxtx-lib.wip';
system qw/rm -rf _tmp/ if -d '_tmp';
mkdir '_tmp';
open I, '<', 'src/mxtx-lib.c' or die;
my @lead;
while (<I>) {
    if ( /include.*mxtx-lib.h/) {
        push @lead, qq'#include "../src/mxtx-lib.h"\n';
        last;
    }
    push @lead, $_;
}

chdir '_tmp';

my @llad;
my $name = '';
while (<I>) {
    push @llad, $_;
    $name = $1, next if m|^// :(\S+)[.]o|;
    if (/^[a-z].*?\s(\S+)\(.*\)\s*(\/\/|$)/) {
        $name = $1 unless $name;
        open O, '>', $name . '.c';
        print O @lead, "\n";
        my $l = $. - @llad;
        print O "#line $l\n";
        print O @llad, "\n";
        @llad = ();
        while (<I>) {
            print O $_;
            last if /^}/;
        }
        close O;
        system qw/sh/, "$name.c";
        die if $?;
        $name = '';
    }
}

chdir '..';
system qw/ar qvs libmxtx.wip/, <_tmp/?*.o>;
rename 'libmxtx.wip', 'libmxtx.a';
print "created 'libmxtx.a'\n";
system qw/rm -rf _tmp/;
__END__
 */
#endif
