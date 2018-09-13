#if 0 /* -*- mode: c; c-file-style: "stroustrup"; tab-width: 8; -*-
 set -euf; trg=${0##*''/}; trg=${trg%.c}; test ! -e "$trg" || rm "$trg"
 if test -x version.sh
 then DVERSION='-DVERSION="'`exec ./version.sh`'"'
 else DVERSION='-DVERSION="'`exec date +%Y-%m-%d`'"'
 fi
 WARN="-Wall -Wstrict-prototypes -Winit-self -Wformat=2" # -pedantic
 WARN="$WARN -Wcast-align -Wpointer-arith " # -Wfloat-equal #-Werror
 WARN="$WARN -Wextra -Wwrite-strings -Wcast-qual -Wshadow" # -Wconversion
 WARN="$WARN -Wmissing-include-dirs -Wundef -Wbad-function-cast -Wlogical-op"
 WARN="$WARN -Waggregate-return -Wold-style-definition"
 WARN="$WARN -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls"
 WARN="$WARN -Wnested-externs -Winline -Wvla -Woverlength-strings -Wpadded"
 case ${1-} in '') set x -O2; shift; esac
 #case ${1-} in '') set x -ggdb; shift; esac
 CFLAGS="$WARN -DSERVER -DCLIENT $DVERSION"
 set -x
 exec ${CC:-gcc} -std=c99 $CFLAGS "$@" -o "$trg" "$0" -L. -lmxtx # -flto
 exit $?
 */
#endif
/*
 * $ mxtx.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *	Copyright (c) 2017 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Tue 05 Feb 2013 21:01:50 EET too (tx11ssh.c)
 * Created: Sun 13 Aug 2017 20:42:46 EEST too
 * Last modified: Tue 13 Sep 2018 22:22:27 +0300 too
 */

/* LICENSE: 2-clause BSD license ("Simplified BSD License"):

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
   THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#if defined(__linux__) && __linux__ || defined(__CYGWIN__) && __CYGWIN__
#define _DEFAULT_SOURCE // glibc >= 2.19
#define _POSIX_C_SOURCE 200112L // for setenv when  glibc < 2.19
#define _POSIX_SOURCE // for nocldwait when glibc < 2.19
#endif

#include <unistd.h>
#include <stdio.h>
//#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h> // offsetof
#include <signal.h>
#include <time.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h> // for htons/ntohs
#include <errno.h>

#include <sys/cdefs.h>
#define XSTR(x) __STRING(x)

#define NO_DIEWARN_PROTOS
#include "mxtx-lib.h"

#define null ((void*)0)

#define BB {
#define BE }

// explicit flags to debug mxtx.c implementation problems...
#define DEBUG_CMD_STRING 0

#define BUFFER_SIZE 16384

// for some reason without this linking fails even
// libmxtx.a vwarn() should not be referenced
const char * _prg_ident = null;

const char server_protocol_ident[4] = { 'm','x','t','1' };
const char client_protocol_ident[4] = { 'm','x','t','0' };

struct {
    const char * component_ident;
    int component_identlen;
    unsigned int loglevel; // sizeof comparison complained 'plain' int
    char loglevels[8];
    union {
	struct {
	    struct sockaddr_un uaddr;
	} c;
	struct {
	    char * local_path; // server only...
	    char * local_path_end;
	} s;
    } u;
    struct pollfd pfds[256];
    unsigned char chnl2pfd[256];
    unsigned char chnlcntr[256];
    int nfds;
    int alarm_scheduled;
} G;

static void set_ident(const char * ident)
{
    G.component_ident = ident;
    G.component_identlen = strlen(G.component_ident);
}

static void init_G(const char * ident)
{
    memset(&G, 0, sizeof G);

    set_ident(ident);
    G.loglevel = 2;  // for loop below (perhaps?) if this ever changed...
    G.loglevels[0] = G.loglevels[1] = G.loglevels[2] = 1;
}

#if 0
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual" // works with (char *)str ...
static void vout(int fd, const char * format, va_list ap)
{
    int error = errno;

    char timestr[32];
    char msg[1024];
    time_t t = time(0);
    struct tm * tm = localtime(&t);
    struct iovec iov[7];
    int iocnt;

    strftime(timestr, sizeof timestr, "%d/%H:%M:%S ", tm);

    iov[0].iov_base = timestr;
    iov[0].iov_len = strlen(timestr);

    iov[1].iov_base = (char *)G.component_ident; // cast & ignore cast-qual...
    iov[1].iov_len = G.component_identlen;

    iov[2].iov_base = (char *)": ";   // cast & ignore cast-qual...
    iov[2].iov_len = 2;

    vsnprintf(msg, sizeof msg, format, ap);

    iov[3].iov_base = msg;
    iov[3].iov_len = strlen(msg);

    if (format[strlen(format) - 1] == ':') {
	iov[4].iov_base = (char *)" ";   // cast & ignore cast-qual...
	iov[4].iov_len = 1;
	iov[5].iov_base = strerror(error);
	iov[5].iov_len = strlen(iov[5].iov_base);
	iov[6].iov_base = (char *)"\n";  // cast & ignore cast-qual...
	iov[6].iov_len = 1;
	iocnt = 7;
    }
    else {
	iov[4].iov_base = (char *)"\n";  // cast & ignore cast-qual...
	iov[4].iov_len = 1;
	iocnt = 5;
    }
    // This write is "opportunistic", so it's okay to ignore the
    // result. If this fails or produces a short write there won't
    // be any simple way to inform the user anyway. The probability
    // this happening without any other (visible) problems in system
    // is infinitesimally small..
    (void)writev(fd, iov, iocnt); // writev() does not modify *iov

    // XXX noticed that when 2 processes were simultaneously writev'ing
    // XXX the outputs of separate iovs got intermixed. replacement below.
}
#pragma GCC diagnostic pop
#else /* 0 */
static void vout(int fd, const char * format, va_list ap)
{
    int error = errno;

    char msg[1024];
    time_t t = time(0);
    struct tm * tm = localtime(&t);

    size_t l = strftime(msg, 32, "%d/%H:%M:%S ", tm);
    char * p = msg + l;
    memcpy(p, G.component_ident, G.component_identlen);
    p += G.component_identlen;
    *p++ = ':'; *p++ = ' ';
    size_t ll = sizeof msg - (p - msg) - 1;
    l = vsnprintf(p, ll, format, ap);
    if (l < ll - 10) {
	p += l;
	if (p[-1] == ':') {
	    *p++ = ' ';
	    ll = sizeof msg - (p - msg) - 1;
	    char * ep = strerror(error);
	    size_t el = strlen(ep);
	    if (el < ll) {
		memcpy(p, ep, el);
		p += el;
	    }
	}
    }
    else {
	p += (l > ll)? ll: l;
    }
    *p++ = '\n';
    // This write is "opportunistic", so it's okay to ignore the
    // result. If this fails or produces a short write there won't
    // be any simple way to inform the user anyway. The probability
    // this happening without any other (visible) problems in system
    // is infinitesimally small...
    if (write(fd, msg, p - msg)) { /* ignored */ }
}
#endif /* 0 */

/*
 * define these: some preliminaries:
 *
 * warn() if it is software (syscall) problem (die() when unrecoverable)
 * log1() if there is (parse) error in input
 * ...
 * log[234]() various levels, expect low(ish) traffic
 * ...
 * log5() for logging all traffic (not there yet)
 *
 * but no stress -improve logging based on usage experience
 */

#define log1(...) if (G.loglevels[1]) warn(__VA_ARGS__)
#define log2(...) if (G.loglevels[2]) warn(__VA_ARGS__)
#define log3(...) if (G.loglevels[3]) warn(__VA_ARGS__)
#define log4(...) if (G.loglevels[4]) warn(__VA_ARGS__)
// XXX move all data io to log5() -- i.e. high-traffic
#define log5(...) if (G.loglevels[5]) warn(__VA_ARGS__)

#define tdbg(format, ...) warn(XSTR(__LINE__) ": " format, __VA_ARGS__)

// non-static so library functions use these
void ATTRIBUTE ((format (printf, 1, 2)))
warn(const char * format, ...); void
warn(const char * format, ...)
{
    va_list ap;

    va_start(ap, format);
    vout(2, format, ap);
    va_end(ap);
}

// non-static so library functions use these
void ATTRIBUTE ((format (printf, 1, 2))) ATTRIBUTE ((noreturn))
die(const char * format, ...); void
die(const char * format, ...)
{
    va_list ap;

    close(0); close(1); close(3); close(4);

    va_start(ap, format);
    vout(2, format, ap);
    va_end(ap);
    exit(1);
}

static void sleep100ms(int c, int m)
{
    log2("sleep 100 msec (0.1 sec) (%d/%d)", c, m);
    poll(0, 0, 100);
}

static void init_comm(void)
{
    const int n = sizeof G.pfds / sizeof G.pfds[0];

    for (int fd = 0; fd < n; fd++)
	G.pfds[fd].events = POLLIN;

    for (int fd = 5; fd < 256; fd++)
	(void)close(fd);

    sigact(SIGPIPE, SIG_IGN, 0);
}

static void wait_peer_protocol_ident(int fd, const char *ident)
{
    char buf[4] = {0,0,0,0};
    // it is extremely unlikely that 1-3 bytes is returned by the call below
    if (read(fd, buf, 4) != 4)
	die("short read while waiting peer ident:...");
    if (memcmp(buf, ident, 4) != 0)
	die("ident mismatch: '%4.4s' <> '%4.4s'", buf, ident);
}

static int readfully(int fd, void * buf, ssize_t len)
{
    int tl = 0;

    if (len <= 0) {
	warn("No data to be read from %d", fd);
	return false;
    }
    while (1) {
	int l = read(fd, buf, len);

	if (l == len) return true;

	if (l <= 0) {
	    if (l == 0) {
		log1("EOF from %d", fd);
		return false;
	    }
	    if (errno == EINTR) {
		if (G.alarm_scheduled) return false;
		else continue;
	    }
	    else {
		log1("read(%d, ...) failed:", fd);
		return false;
	    }
	}
	tl += l;
	buf = (char *)buf + l;
	len -= l;
    }
}

static bool to_socket(int sd, void * data, size_t datalen)
{
    ssize_t wlen = write(sd, data, datalen);
    if (wlen == (ssize_t)datalen) {
	log4("Channel %d:%d %zd bytes of data written",sd,G.chnlcntr[sd],wlen);
	return true;
    }
    // XXX did we write any data -- and if so, do we care (e.g. EPIPE anyway?)
    if (errno == EPIPE) {
	log3("Channel %d:%d disappeared (epipe)", sd,G.chnlcntr[sd]);
	return false;
    }
    char * buf = (char *)data;
    int tries = 1;
    ssize_t dlen = (ssize_t)datalen;
    do {
	// here if socket buffer full (typically 100kb of data unread)
	// give peer 1/10 of a second to read it and retry.
	// POLLOUT might inform that there is room for new data but
	// write may still fail if the data doesn't fully fit ???
	sleep100ms(tries, 100);

	if (wlen < 0)
	    wlen = 0;

	buf += wlen;
	dlen -= wlen;

	wlen = write(sd, buf, dlen);
#if EAGAIN != EWOULDBLOCK
	if (errno != EAGAIN && errno != EWOULDBLOCK) {
#else
	if (errno != EWOULDBLOCK) {
#endif
	    warn("Channel %d fd gone ...:", sd);
	    return false;
	}
#if 0
	}
#endif
	if (wlen == dlen) {
	    log2("Writing %lu bytes of data to %d took %d tries",
		 datalen, sd, tries);
	    return true;
	}
	log4("%d: wlen %d (of %u):", sd, (int)wlen, (unsigned int)datalen);
    } while (tries++ < 100); // 100 times makes that 10 sec total.

    log1("Peer #%d too slow to read traffic. Dropping", sd);
    return false;
}

static void mux_eof_to_iopipe(int fd, int chnl)
{
    unsigned char hdr[4];
    hdr[0] = chnl;
    hdr[1] = G.chnlcntr[chnl];
    hdr[2] = 0;
    hdr[3] = 0;

    log4("Channel %d:%d mux eof (to fd %d)", chnl, hdr[1], fd);

    if (write(fd, hdr, 4) != 4)
	die("write() to iopipe failed:");
}

//#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual" // works with (char *)str ...
static void mux_to_iopipe(int fd, int chnl, const void * data, size_t datalen)
{
    uint16_t hdr[2];
    struct iovec iov[2];
    ((unsigned char *)hdr)[0] = chnl;
    ((unsigned char *)hdr)[1] = G.chnlcntr[chnl];
    hdr[1] = htons(datalen);
    iov[0].iov_base = hdr;
    iov[0].iov_len = 4;
    iov[1].iov_base = (char *)data; // cast & ignore cast-qual...
    iov[1].iov_len = datalen;

    log4("Channel %d:%d mux %zd bytes (to fd %d)",
	 chnl, G.chnlcntr[chnl], datalen, fd);

    if (writev(fd, iov, 2) != (ssize_t)datalen + 4)
	die("writev() to iopipe failed:");  // \\ writev() does not modify *iov
}
//#pragma GCC diagnostic pop

static int from_iopipe(int fd, uint8_t * chnl, uint8_t * cntr, char * data)
{
    uint16_t hdr[2];

    xreadfully(fd, hdr, 4);
    *chnl = ((unsigned char *)hdr)[0];
    *cntr = ((unsigned char *)hdr)[1];
    int len = ntohs(hdr[1]);
    if (len > BUFFER_SIZE)
	die("Protocol error: mx input message: chnl %d:%d len %d too long\n",
	    *chnl, *cntr, len);

    log4("Mx input: channel %d:%d expect %d bytes", *chnl, *cntr, len);

    if (len)
	xreadfully(fd, data, len);
    log4("Mx input: channel %d:%d demuxed %d bytes (from fd %d)",
	 *chnl, *cntr, len, fd);
    return len;
}

static void close_socket_and_remap(int sd)
{
    close(sd);
    G.nfds--;
    int o = G.chnl2pfd[sd];
    G.chnl2pfd[sd] = 0;

    log3("Closing channel %d:%d", sd, G.chnlcntr[sd]);
    G.chnlcntr[sd]++;

    log4("remap: sd %d, o %d, nfds %d, cntr %d",sd, o, G.nfds, G.chnlcntr[sd]);

    if (o == G.nfds)
	return;

    G.pfds[o].fd = G.pfds[G.nfds].fd;
    G.pfds[o].revents = G.pfds[G.nfds].revents;

    G.chnl2pfd[G.pfds[o].fd] = o;

#if 0
    if (G.loglevels[4]) {
	for (int i = 0; i < G.nfds; i++) {
	    int fd = G.pfds[i].fd;
	    warn("fdmap: i: %d -> fd: %d (%d)", i, fd, G.chnl2pfd[fd]);
	}
    }
#endif
}

static int from_socket_to_iopipe(int pfdi, int iofd)
{
    char buf[BUFFER_SIZE];
    int fd = G.pfds[pfdi].fd;
    int len = read(fd, buf, sizeof buf);

    // unify
    log4("Channel %d:%d read %d bytes", fd, G.chnlcntr[fd], len);
    log4("%d %d %d -- read %d", fd, pfdi, G.chnl2pfd[fd], len);

    if (len <= 0) {
	if (len < 0) {
	    log1("Read from %d failed. Closing:", fd);
	} else
	    log3("EOF: channel %d:%d (local). Closing.", fd, G.chnlcntr[fd]);

	mux_eof_to_iopipe(iofd, fd);
	//if (G.pfds[pfdi].events != POLLIN)
	//    G.pfds[pfdi].events = POLLIN;
	close_socket_and_remap(fd);
	return false;
    }
    mux_to_iopipe(iofd, fd, buf, len);
    return true;
}

// POLLHUP, POLLERR, and POLLNVAL, if ever
static int x_from_socket_to_iopipe(int pfdi, int iofd)
{
    int e = from_socket_to_iopipe(pfdi, iofd);
    if (! e) {
	if (G.pfds[pfdi].events != POLLIN)
	    G.pfds[pfdi].events = POLLIN;
    }
    return e;
}

#if CLIENT

static void unlink_socket_file_atexit(void)
{
    unlink(G.u.c.uaddr.sun_path);
}

static int send_new_conn_cmd_to_iopipe(int iofd, int chnl)
{
    char buf[BUFFER_SIZE - 128];
    int rv;
    alarm(2);
    rv = readfully(chnl, buf, 4);
    alarm(0);
    if (G.alarm_scheduled) {
	warn("Timeout reading command array length");
	G.alarm_scheduled = 0;
	return false;
    }
    if (! rv) return false;
    if (buf[0] != '\0' || buf[1] != '\0') {
	warn("Unexpected protocol version %02x%02x (expected 0x0000)",
	     buf[1], buf[0]);
	return false;
    }
    int len = (unsigned char)buf[2] * 256 + (unsigned char)buf[3];
    log4("command [args] length %d", len);
    if ((size_t)len > sizeof buf) {
	warn("Command length too long (%d > %lu bytes", len, sizeof buf);
	return false;
    }
    alarm(2);
    rv = readfully(chnl, buf, len);
    alarm(0);
    if (G.alarm_scheduled) {
	warn("Timeout reading command");
	G.alarm_scheduled = 0;
	return false;
    }
    if (! rv) return false;
    if (buf[len - 1] != '\0') {
	warn("command does not end with '\\0'");
	return false;
    }
#if DEBUG_CMD_STRING // debug position: command & args
    //if (G.loglevels[4])
    { write(2, "dbg c: ", 7); write(2, buf, len); }
#endif
    mux_to_iopipe(iofd, chnl, buf, len);
    return true;
}

static void client_handle_server_message(void)
{
    char buf[BUFFER_SIZE];
    unsigned char chnl, cntr;

    int len = from_iopipe(0, &chnl, &cntr, buf);

    log4("chnl %d:%d, len %d (%d)", chnl, cntr, len, G.nfds);

    if (cntr != G.chnlcntr[chnl]) {
	if (len > 0)
	    log3("Data to EOF'd channel %d:%d (!= %d) (%d bytes). Dropped",
		 chnl, cntr, G.chnlcntr[chnl], len);
	return;
    }
    if (len == 0) {
	log3("EOF for channel %d:%d (remote). Closing.", chnl, cntr);
	close_socket_and_remap(chnl);
	return;
    }
    if (! to_socket(chnl, buf, len)) {
	mux_eof_to_iopipe(1, chnl);
	close_socket_and_remap(chnl);
    }
}

static void client_loop(void) ATTRIBUTE ((noreturn));
static void client_loop(void)
{
    while (1)
    {
	//log4("before poll: nfds = %d", G.nfds);
	int n;
	if ((n = poll(G.pfds, G.nfds, -1)) <= 0)
	    die("poll:");

	if (G.pfds[0].revents) {
	    client_handle_server_message();
	    if (n == 1)
		continue;
	}
	for (int i = 2; i < G.nfds; i++)
	    if (G.pfds[i].revents && ! from_socket_to_iopipe(i, 1))
		i--;

	// last add to fd table
	if (G.pfds[1].revents) { /* XXX should check POLLIN */
	    int sd = accept(3, null, 0);
	    log4("New connection: %d = accept(ss, ...)", sd);
	    if (! checkpeerid(sd))
		close(sd);
	    else if (sd > 255) {
		//if (G.nfds == sizeof G.pfds / sizeof G.pfds[0]) {
		warn("Connection limit reached: closing %d", sd);
		close(sd);
	    }
	    else {
		log3("New connection. Channel %d:%d", sd, G.chnlcntr[sd]);
		if (send_new_conn_cmd_to_iopipe(1, sd)) {
		    G.pfds[G.nfds].fd = sd;
		    G.chnl2pfd[sd] = G.nfds++;
		    set_nonblock(sd); // for write to not block
		    log4("New chnl %d, nfds %d", sd, G.nfds);
		}
		else {
		    log3("Closing %d", sd);
		    close(sd);
		}
	    }
	}
    }
}


static void start_client(char * socket_path)
{
    set_ident("c");

    fill_mxtx_socket_path(&G.u.c.uaddr,
			  socket_path[0] == '\0'? "0": socket_path, "");

#if defined(__linux__) && __linux__
    if (socket_path[0] == '@' && strchr(socket_path, '/') != null)
	G.u.c.uaddr.sun_path[0] = 0;
#endif
    if (G.u.c.uaddr.sun_path[0] != 0)
	atexit(unlink_socket_file_atexit);

    (void)close(3);
    int sd = xbind_listen_unix_socket(&G.u.c.uaddr, SOCK_STREAM);
    if (sd != 3)
	die("Unexpected fd '%d' (not 3) for client socket", sd);

    if (write(1, client_protocol_ident, sizeof client_protocol_ident)
	!= sizeof client_protocol_ident)
	die("Could not send client protocol ident:");

    wait_peer_protocol_ident(0, server_protocol_ident);
    write(1, "", 1);
    char c; xreadfully(0, &c, 1);

    init_comm();

    G.pfds[0].fd = 0;
    G.pfds[1].fd = 3;
    G.nfds = 2;

    log1("Initialization done (v. " VERSION ")");
}

#endif // CLIENT

#if SERVER

static int server_handle_internal_command(char ** args, int argc)
{
    if (strcmp(args[0], ":connect")) {
	log1("%s: unknown internal command", args[0]);
	return -1;
    }
    if (argc != 3) {
	log1("connect: 2 args: host and port");
	return -1;
    }

    // basic ipv4 style for the time being...
    in_addr_t iaddr = inet_addr(args[1]);
    if (iaddr == INADDR_NONE) {
	log1("%s: invalid address", args[1]);
	return -1;
    }
    int iport = atoi(args[2]);
    if (iport < 1 || iport > 65535) {
	log1("%s: invalid port", args[2]);
	return -1;
    }
    int sd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in ia = {
	.sin_family = AF_INET,
	.sin_port = htons(iport),
	.sin_addr = { .s_addr = iaddr }
    };
    set_nonblock(sd);
    if (connect(sd, (struct sockaddr *)&ia, sizeof ia) < 0) {
	if (errno != EINPROGRESS) {
	    log1("connecting to %s:%s failed:", args[1], args[2]);
	    close(sd);
	    return -1;
	}
    }
    //tdbg("connect(%d) to %s:%d in progress", sd, args[1], iport);
    log4("set pollout %d nfds %d", sd, G.nfds);
    G.pfds[G.nfds].events = POLLOUT;
    return sd;
}

static int server_handle_connect_completed(int pfdi)
{
    int fd = G.pfds[pfdi].fd;
    log4("pollout received: %d ndfs %d", fd, G.nfds);
    G.pfds[pfdi].events = POLLIN;
    int error;
    socklen_t errsize = sizeof error;
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &errsize)) {
	error = errno;
	warn("getsockopt failed:");
    }
    if (error != 0) {
	errno = error;
	warn("connect failed:");
    }
    if (error) {
	unsigned char c = (unsigned char)error;
	// in case of server, iopipe is fd 1
	mux_to_iopipe(1, fd, &c, 1);
	mux_eof_to_iopipe(1, fd);
	close_socket_and_remap(fd);
	return false;
    }
    // success, write one byte, which is '\0'
    mux_to_iopipe(1, fd, "", 1);
    return true;
}

static int server_execute_command_buf(char * buf, int len)
{
#define MAX_ARGS 64
    char *args[MAX_ARGS];
    int argc = 0;
    len--;
    if (buf[len] != 0) {
	log1("Command buffer does not end with '\\0'");
	return -1;
    }
#if DEBUG_CMD_STRING
    { write(2, "dbg s: ", 7); write(2, buf, len); }
#endif
    int l, cmdlen = l = strlen(buf);
    do {
	log3("New command: args[%d] = '%s' (%d bytes)", argc, buf, l);
	args[argc++] = buf;
	// log4("%d %d", len, l);
	if (l >= len) {
	    args[argc] = null;
	    break;
	}
	buf += (l + 1);
	len -= (l + 1);
	l = strlen(buf);
    } while (argc < MAX_ARGS);
    if (argc == MAX_ARGS) {
	log1("Command '%s' has too many args", args[0]);
	return -1;
    }
#undef MAX_ARGS

    char * cmd = args[0];

    if (cmd[0] == ':')
	return server_handle_internal_command(args, argc);

    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
	warn("socketpair:");
	return -1;
    }

    pid_t pid = fork();
    if (pid == 0) {
	// child //
	close(sv[0]);
	xdup2(sv[1], 0);
	xdup2(sv[1], 1);
	close(sv[1]);
	if (cmd[0] == '~') {
	    if (chdir(G.u.s.local_path) < 0)
		die("chdir('%s'):", G.u.s.local_path);
	    args[0]++;
	    cmd++;
	}
	if (memcmp(cmd, "mxtx-", 5) == 0) {
	    if (cmdlen <= 60) {
		strcpy(G.u.s.local_path_end, cmd);
		if (access(G.u.s.local_path, X_OK) == 0)
		    cmd = G.u.s.local_path;
	    }
	}
	// reset signals handlers that were ignore to their defaults //
	signal(SIGPIPE, SIG_DFL);
	signal(SIGCHLD, SIG_DFL);
	execvp(cmd, args);
	// XXX errno = ENOENT did not work -- there is other place same done...
	die("execvp: %s ...: %s", cmd, strerror(ENOENT));
    }
    if (pid < 0) {
	close(sv[0]);
	close(sv[1]);
	warn("fork:");
	return -1;
    }
    // parent //
    close(sv[1]);
    fcntl(sv[0], F_SETFD, FD_CLOEXEC);
    return sv[0];
}

static void server_handle_client_message(void)
{
    char buf[BUFFER_SIZE];
    unsigned char chnl, cntr;

    int len = from_iopipe(0, &chnl, &cntr, buf);

    int pfdi = G.chnl2pfd[chnl];

    log4("chnl %d:%d, len %d, pfdi %d(/%d)", chnl, cntr, len, pfdi, G.nfds);

    if (cntr != G.chnlcntr[chnl]) {
	if (len > 0)
	    log2("Data to EOF'd channel %d (cntr %d != %d) (%d bytes)."
		  " Dropped. ", chnl, cntr, G.chnlcntr[chnl], len);
	return;
    }
    if (pfdi == 0) {
	if (chnl < 4) {
	    log1("New connection %d:%d less than 4. Drop it", chnl, cntr);
	    return;
	}
	if (len == 0) {
	    log1("New connection %d:%d without command (or eof)", chnl, cntr);
	    return;
	}
	log3("New connection. Channel %d:%d", chnl, cntr);
	int fd = server_execute_command_buf(buf, len);
	if (fd != chnl) {
	    if (fd < 0) {
		log1("Failed to handle command %s", buf);
		mux_eof_to_iopipe(1, chnl);
		G.chnlcntr[chnl]++;
		return;
	    }
	    xdup2(fd, chnl);
	    if (fd < chnl)
		xdup2(3, fd); // /dev/null to plug (stale or slow) fd
	    else
		close(fd);
	}
	set_nonblock(chnl); // for write to not block
	G.pfds[G.nfds].fd = chnl; // G.pfds[G.nfds].events may have been set...
	G.chnl2pfd[chnl] = G.nfds++;
	log4("New chnl %d, nfds %d", fd, G.nfds);
	return;
    }
    if (len == 0) {
	log3("EOF for channel %d:%d (remote). Closing.", chnl, cntr);
	close_socket_and_remap(chnl);
	return;
    }

    if (! to_socket(chnl, buf, len)) {
	mux_eof_to_iopipe(1, chnl);
	close_socket_and_remap(chnl);
    }
}

static void server_loop(void) ATTRIBUTE ((noreturn));
static void server_loop(void)
{
    while (1)
    {
	//log4("before poll: nfds = %d", G.nfds);
	int n;
	if ((n = poll(G.pfds, G.nfds, -1)) <= 0)
	    die("poll:");

	if (G.pfds[0].revents) { /* XXX should check POLLIN */
	    server_handle_client_message();
	    if (n == 1)
		continue;
	}

	for (int i = 1; i < G.nfds; i++) {
	    int e;
	    if ((e = G.pfds[i].revents) != 0) {
		if (e & POLLIN)
		    e = from_socket_to_iopipe(i, 1);
		else if (e & POLLOUT)
		    e = server_handle_connect_completed(i);
		else
		    e = x_from_socket_to_iopipe(i, 1);
		if (! e)
		    i--;
	    }
	}
    }
}

#define LOCAL_MXTX_DIR ".local/share/mxtx/"

static void start_server(const char * cwd_path)
{
    set_ident("s");

    const char * home = getenv("HOME");
    if (home == null) die("Cannot find HOME in environment");
    if (cwd_path[0] == '~') {
	// XXX fixme: add xchdir() -- also other paths than ~... (if ever...)
	if (chdir(home) == 0 && cwd_path[1] == '/' && cwd_path[2] != '\0')
	    (void)chdir(cwd_path + 2);
    }
    int homelen = strlen(home);
    G.u.s.local_path = malloc(homelen + sizeof(LOCAL_MXTX_DIR) + 64);
    if (G.u.s.local_path == null) die("Out of Memory!");
    memcpy(G.u.s.local_path, home, homelen);
    char * p = G.u.s.local_path + homelen;
    *p++ = '/';
    memcpy(p, LOCAL_MXTX_DIR, sizeof LOCAL_MXTX_DIR);
    G.u.s.local_path_end = p + sizeof LOCAL_MXTX_DIR - 1;
    BB;
    char pwd[1024];
    if (getcwd(pwd, sizeof pwd) != NULL)
	(void)setenv("MXTX_PWD", pwd, 1);
    BE;
    close(3);
    int nullfd = open("/dev/null", O_RDWR, 0);
    if (nullfd < 0) die("open('/dev/null'):");
    if (nullfd != 3) die("Unexpected fd %d (not 3) for nullfd", nullfd);
    fcntl(3, F_SETFD, FD_CLOEXEC);

    if (write(1, server_protocol_ident, sizeof server_protocol_ident)
	!= sizeof server_protocol_ident)
	die("Could not send client ident:");

    wait_peer_protocol_ident(0, client_protocol_ident);
    write(1, "", 1);
    char c; xreadfully(0, &c, 1);

    init_comm();

    G.pfds[0].fd = 0;
    G.nfds = 1;

    log1("Initialization done (v. " VERSION ")");
}

#endif // SERVER

static void exit_sig(int sig) { exit(sig); }

static void set_alarm_scheduled(int sig) {
    (void)sig;
    G.alarm_scheduled = 1;
}

int main(int argc, char ** argv)
{
    signal(SIGHUP, exit_sig);
    signal(SIGINT, exit_sig);
    signal(SIGTERM, exit_sig);
    sigact(SIGALRM, set_alarm_scheduled, 0);

    BB;
    char * progname = argv[0];
    init_G(progname);

    // smarter opt parsing, e.g. getopt
    while (argc > 1 && memcmp(argv[1], "-v", 2) == 0) {
	for (int i = 1; argv[1][i] == 'v'; i++) {
	    G.loglevel++;
	    if (G.loglevel < sizeof G.loglevels / sizeof G.loglevels[0])
		G.loglevels[G.loglevel] = 1;
	}
	argc--; argv++;
    }
    if (argc < 2) {
	set_ident("mxtx " VERSION);
#define NL "\n"
	die("\n\nUsage: %s [-v*] -(s[~]|c[socketname])"
	    NL
	    NL "  -c: act as client"
	    NL "  -s: act as server"
	    NL "  -v: add verbosity (from level 2, effectively up to 5)"
	    NL
	    NL "  socketname: used by client for socket path. if path does not"
	    NL "              contain '/'s, it resolves to common prefix, and"
	    NL "              given path is suffixed to it. default is '0'."
	    NL
	    NL "  ~: used in -s to change server current directory to $HOME."
#if defined(__linux__) && __linux__
	    NL
	    NL "  on linux, abstract socket namespace is used in common prefix."
	    NL "  if user-given socketname starts with '@' (and contains '/'s)"
	    NL "  then abstract namespace is used with these paths, too."
#endif
	    NL, progname);
    }
    BE;

    if (memcmp(argv[1], "-s", 2) == 0) {
	start_server(argv[1] + 2);
	sigact(SIGCHLD, SIG_IGN, SA_NOCLDWAIT);
	server_loop();
	die("not reached");
    }
    if (memcmp(argv[1], "-c", 2) == 0) {
	start_client(argv[1] + 2);
	client_loop();
	die("not reached");
    }
    die("'%s': unknown argument", argv[1]);
}
