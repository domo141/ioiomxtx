#if 0 /* -*- mode: c; c-file-style: "stroustrup"; tab-width: 8; -*-
 set -euf; trg=${0##*''/}; trg=${trg%.c}; test ! -e "$trg" || rm "$trg"
 uname=`exec uname`
 case $uname in *WIN*) echo $0: does not work on $uname; exit 0 ;; esac
 WARN="-Wall -Wstrict-prototypes -Winit-self -Wformat=2" # -pedantic
 WARN="$WARN -Wcast-align -Wpointer-arith " # -Wfloat-equal #-Werror
 WARN="$WARN -Wextra -Wwrite-strings -Wcast-qual -Wshadow" # -Wconversion
 WARN="$WARN -Wmissing-include-dirs -Wundef -Wbad-function-cast -Wlogical-op"
 WARN="$WARN -Waggregate-return -Wold-style-definition"
 WARN="$WARN -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls"
 WARN="$WARN -Wnested-externs -Winline -Wvla -Woverlength-strings -Wpadded"
 case ${1-} in '') set x -O2; shift; esac
 #case ${1-} in '') set x -ggdb; shift; esac
 set -x; exec ${CC:-gcc} -std=c99 $WARN "$@" -o "$trg" "$0" -L. -lmxtx # -flto
 exit $?
 */
#endif
/*
 * $ mxtx-socksproxy.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *      Copyright (c) 2017 Tomi Ollila
 *          All rights reserved
 *
 * Created: Sun 20 Aug 2017 22:07:17 EEST too
 * Last modified: Sun 01 Apr 2018 19:47:31 +0300 too
 */

#if defined(__linux__) && __linux__ || defined(__CYGWIN__) && __CYGWIN__
#define _DEFAULT_SOURCE // for newer linux environments
#define _POSIX_SOURCE // for some older linux environments
#endif

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <setjmp.h>
#include <errno.h>

// for xinet_connect()
#include <netinet/in.h>
#include <arpa/inet.h>

#include "mxtx-lib.h"

#define null ((void*)0)

#define DBG 1
#if DBG
#define dprintf(...) fprintf(stderr, __VA_ARGS__)
#define dprintf0(...) do { } while (0)
#else
#define dprintf(...) do { } while (0)
#define dprintf0(...) do { } while (0)
#endif

const char * _prg_ident = "mxtx-socksproxy: ";

static struct {
    char * network;
    char pidbuf[16];
    pid_t ppid; // -Wpadded
    struct timeval stv;
    sigjmp_buf restart_env;
} G;

#define LINEREAD_DATA_BUFFER_SIZE 4096
#include "lineread.ch"

static int find_dest(const char * host, int hostlen,
                     const char ** net, const char ** addr)
{
    const char * p = G.network;
    //write(1, p, 96); exit(0);

    const unsigned char * s = (const unsigned char *)p;
    p = (const char *)s + 1;
    dprintf0("link: '%s'\n", p);
    s += (int)s[0] + 2;
    while (1) {
        int len1 = s[0];
        int len2 = s[1];
        if (len1 == 0) {
            if (len2 == 0) break;
            p = (const char *)s + 2;
            dprintf0("link: '%s'\n", p);
            s += len2 + 3;
            continue;
        }
        s += 2;
        dprintf0("n: %s -- %d %d %s %s\n", p, len1, len2, s, s + len1 + 1);
        if (memcmp(host, s, hostlen + 1) == 0) {
            *net = p;
            *addr = (const char *)(s + len1 + 1);
            return len2;
        }
        s += len1 + len2 + 2;
    }
    return 0;
}

static void init(int argc, char * argv[])
{
    if (argc < 2)
        die("\nUsage: %s link [link...]\n\n"
            "  '0' is good default link\n\n", argv[0]);
#define INITIAL_ALLOC_SIZE 262144 // expect mmap, let's see what realloc does
    G.network = malloc(INITIAL_ALLOC_SIZE);
    if (G.network == null) die("out of memory");
    char * p = G.network;
    for (int i = 1; argv[i]; i++) {
        if (p - G.network > INITIAL_ALLOC_SIZE - 1024) {
            warn("too much input data"); // make better msg if really happens
            break;
        }
        int sd;
        if (argv[i][0] == '\0' || (argv[i][1] == '\0' &&
                                   (argv[i][0] == '/' || argv[i][0] == '.'))) {
            const char * home = getenv("HOME");
            if (home == null) continue; // unlikely
            char fn[4096];
            if ((unsigned)snprintf(fn, sizeof fn,
                                   "%s/.local/share/mxtx/hosts-to-proxy",
                                   home) >= sizeof fn)
                continue; // unlikely
            sd = open(fn, O_RDONLY);
            if (sd < 0) {
                warn("Cannot open %s:", fn);
                continue;
            }
            if (argv[i][0] != '\0') argv[i][0] = '\0';
        }
        else {
            sd = connect_unix_stream_mxtx_socket(argv[i], "");
            if (sd < 0) continue;
            write(sd, "\0\0\0\024~cat\0hosts-to-proxy\0", 24);
        }
        // we know argv[i] maxlen is less than 108
        p += snprintf(p, 256, "%c%s", (int)strlen(argv[i]), argv[i]) + 1;
        fprintf(stderr, "link: '%s'\n", argv[i]);
        LineRead lr;
        lineread_init(&lr, sd);
        int ln = 0;
        while (1) {
            char * dp; // put outside loop if cared read() return values...
            int ll = lineread(&lr, &dp);
            if (ll <= 0) { if (ll == 0) continue; else break; }
            ln++;
            while (isspace(*dp)) dp++;
            char * ip = strpbrk(dp, " \t");
            if (ip == null) {
                warn("%s:%d: no space separator\n", argv[i], ln);
                continue;
            }
            if (dp == ip) {
                warn("%s:%d: hostname empty\n", argv[i], ln);
                continue;
            }
            int hnlen = ip - dp;
            *ip = '\0';
            if (hnlen > 250) {
                warn("%s:%d: hostname '%s' too long\n", argv[i], ln, dp);
                continue;
            }
            while (isspace(*++ip)) {};
            char * ep = ip;
            // fix multiple spaces, laita pituus talteen
            while (!isspace(*ep) && *ep != '\0') ep++;
            if (ip == ep) {
                warn("%s:%d: ip address empty\n", argv[i], ln);
                continue;
            }
            int iplen = ep - ip;
            *ep = '\0';
            if (iplen > 42) {
                warn("%s%d: ip address '%s' too long\n", argv[i], ln, ip);
                continue;
            }
            if (! isalnum(dp[0])) // check "outcomments"
                continue;
            p += snprintf(p, 512, "%c%c%s%c%s", hnlen, iplen, dp, 0, ip) + 1;
            fprintf(stderr, " %s: %s\n", dp, ip);
        }
        *p++ = '\0'; // end link with zero-length field (no second)
        close(sd);
    }
    fprintf(stderr, "\n");
    *p++ = '\0'; // end list w/ double \0's
    G.network = realloc(G.network, p - G.network);
    if (G.network == null) die("Out of memory"); // let system clear it...

#if DBG // test scan...
    (void)find_dest("", 0, null, null);
#endif

    struct sockaddr_un saddr;
#if defined(__linux__) && __linux__
    fill_sockaddr_un(&saddr, "%c/tmp/user-%d/un-s-1080", 0, getuid());
#else
    char * xdgrd = getenv("XDG_RUNTIME_DIR");
    if (xdgrd)
        fill_sockaddr_un(&saddr, "%s/un-s-1080", xdgrd);
    else {
        uid_t uid = getuid();
        char dir[64];
        (void)snprintf(dir, sizeof dir, "/tmp/user-%d", uid);
        (void)mkdir(dir, 0700);
        fill_sockaddr_un(&saddr, "/tmp/user-%d/un-s-1080", uid);
    }
#endif
    int sd = xbind_listen_unix_socket(&saddr, SOCK_STREAM);
    xmovefd(sd, 3);
}

static void restart_sighandler(int sig)
{
    (void)sig;
    if (getpid() != G.ppid) return;
    close(3);
    warn("Restarting...");
    siglongjmp(G.restart_env, 1);
    die("not reached");
}

// echo 50 | tr 50 '\005\000' | nc -l 1080 | od -tx1

static void start(void);
static void io(int ifd, int ofd);

extern char ** environ;
int main(int argc, char * argv[])
{
    seterr_linebuf();
    init(argc, argv);
    sigact(SIGCHLD, SIG_IGN, SA_NOCLDWAIT);

    G.ppid = getpid();
    if (sigsetjmp(G.restart_env, 1)) {
        execve(argv[0], argv, environ);
        die("not reached");
    }
    sigact(SIGUSR1, restart_sighandler, 0);

    while (1) {
        int sd = accept(3, null, 0);
        if (! checkpeerid(sd)) {
            close(sd);
            continue;
        }
        if (fork()) {
            close(sd);
            continue;
        }
        // child of fork()
        xmovefd(sd, 3);
        break;
    }
    // child of fork() continues
    start();

    struct pollfd pfds[2];
    pfds[0].fd = 3; pfds[1].fd = 4;
    pfds[0].events = pfds[1].events = POLLIN;

    while (1) {
        if (poll(pfds, 2, -1) <= 0) {
            warn("unexpected poll() return without any input available.");
            sleep(1);
        }
        if (pfds[0].revents) io(3, 4);
        if (pfds[1].revents) io(4, 3);
    }
    return 0;
}

static float elapsed(void) {
    struct timeval tv;
    gettimeofday(&tv, null);
    if (tv.tv_usec < G.stv.tv_usec) {
        tv.tv_sec--;
        tv.tv_usec += 1000000;
    }
    return (tv.tv_sec - G.stv.tv_sec) +
        (float)(tv.tv_usec - G.stv.tv_usec) / 1000000;
}

static int xinet_connect(const char * addr, int iport /*, bool nonblock */)
{
    // copy from mxtx.c // (gener|lib)ify some day (w/ v6)...
    // in the future name resolving (using 9.9.9.9 perhaps) perhaps
    // basic ipv4 style for the time being...
    in_addr_t iaddr = inet_addr(addr);
    if (iaddr == INADDR_NONE) {
        die("%s: invalid address", addr);
    }
    if (iport < 1 || iport > 65535) {
        die("%d: invalid port", iport);
    }
    int sd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in ia = {
        .sin_family = AF_INET,
        .sin_port = htons(iport),
        .sin_addr = { .s_addr = iaddr }
    };
    if (connect(sd, (struct sockaddr *)&ia, sizeof ia) < 0) {
        close(sd);
        die("Connection to %s:%d failed:", addr, iport);
    }
    return sd;
}

static void may_serve_index_file_request(const char * host, char * rbuf);

static void start(void)
{
    char buf[512];
    char * p;
    unsigned char * ubuf = (unsigned char *)buf;
    int port;
    gettimeofday(&G.stv, null);
    snprintf(G.pidbuf, sizeof G.pidbuf, "sp-%d: ", getpid());
    _prg_ident = G.pidbuf;
    /*
     * In this program we can do things simple and expect system to return
     * enough data -- and if not, then drop it (we'll see how well this flies)
     */
    alarm(10);
    int len = read(3, buf, 260);
    if (len < 3)
        die("Could not read min 3 octets of data (read returned %d)", len);

    const char * net = null;

    // https://www.ietf.org/rfc/rfc1928.txt
    // https://en.wikipedia.org/wiki/SOCKS

    if (buf[0] == 5) { // socks5 first //
        if (len != ubuf[1] + 2)
            die("Unexpected number of octets of input data(%d != %d)",
                len, ubuf[1] + 2);
        write(3, "\005", 2); // reply 05 00 (no authentication required)

        // we trust such a small amout of data can be read on one call...
        len = read(3, buf, 5 + 255 + 2); // VER CMD RV ATYP DST.ADDR DST.PORT
        if (len < 10)
            die("Minimum socks5 request has 10 octets (got %d)", len);
        if (buf[1] != 0x01)
            die("Request not CONNECT (0x01) (was %02x)", ubuf[1]);

        strcpy(buf + 304, ":connect");
        p = buf + 313;
        if (buf[3] == 0x03) { // domain name
            int hlen = ubuf[4];
            int rest = hlen + 7;
            if (len != rest)
                die("Name socks5 request not %d octets (was %d octets)",
                    rest, len);
            port = ubuf[rest - 2] * 256 + ubuf[rest - 1];
            ubuf[rest - 2] = '\0';
            // the following function may not return (see more in definition)
            may_serve_index_file_request(buf + 5, p);
            const char * addr;
            int al = find_dest(buf + 5, hlen, &net, &addr);
            if (al) p += sprintf(p, "%s", addr) + 1; // p was buf + 313
            else p += sprintf(p, "%s", buf + 5) + 1; // for warn() below
        }
        else if (buf[3] == 0x01) {  // ipv4 address
            if (len != 10)
                die("IPv4 socks5 request not 10 octets (was %d octets)", len);
            port = ubuf[8] * 256 + ubuf[9];
            p += sprintf(p, "%u.%u.%u.%u", ubuf[4],ubuf[5],ubuf[6],ubuf[7]) + 1;
        }
        else if (buf[3] == 0x04) {  // ipv6 address
            if (len != 22)
                die("IPv6 socks5 request not 22 octets (was %d octets)", len);
            port = ubuf[20] * 256 + ubuf[21];
            p += sprintf(p, "%x:%x:%x:%x:%x:%x:%x:%x",
                         ubuf[4] * 256 + ubuf[5], ubuf[6] * 256 + ubuf[7],
                         ubuf[8] * 256 + ubuf[9], ubuf[10] * 256 + ubuf[11],
                         ubuf[12] * 256 + ubuf[13], ubuf[14] * 256 + ubuf[15],
                         ubuf[16] * 256 + ubuf[17], ubuf[18] * 256 + ubuf[19])
                + 1;
        }
        else die("Unknown socks5 request %02x", buf[3]);
    }
    else if (buf[0] == 4) {
        die("Socks4 support to be implemented");
    }
    else die ("Unknown socks version (%d) request", buf[0]);
    alarm(0);
    if (net == null) {
        warn("'%s'(%d) not in any configuration", buf + 313, port);
        write(3, "\005\004\0\001" "\0\0\0\0" "\0", 10);
        exit(0);
    }
    if (buf[0] == 5 && buf[3] == 0x03) {
        warn("Connecting %s %s:%d via %s", buf + 5, buf + 313, port, net);
    }
    else
        warn("Connecting %s:%d via %s", buf + 313, port, net); // XXX loglevel //

    int sd;
    if (net[0] == '\0') {
        sd = xinet_connect(buf + 313, port);
    }
    else {
        sd = connect_unix_stream_mxtx_socket(net, "");
        if (sd < 0)
            die("Cannot connect to mxtx socket '%s':", net);

        len = p - buf - 304 + sprintf(p, "%d", port) + 1;
        ubuf[300] = ubuf[301] = 0; // protocol version
        ubuf[302] = len / 256; ubuf[303] = len % 256; // msg length
        write(sd, buf + 300, len + 4);
        if (read(sd, buf, 1) != 1)
            die("Did not get reply from mxtx socket '%s' (%s:%d):",
                net, buf + 313, port);
        if (buf[0] != 0) {
            warn("Nonzero (%d) reply code for '%s:%d'", buf[0],buf + 313,port);
            switch (ubuf[0]) {
            case ECONNREFUSED: // check matching errno's on every OS
                write(3, "\005\005\0\001" "\0\0\0\0" "\0", 10); break;
            default:
                write(3, "\005\001\0\001" "\0\0\0\0" "\0", 10); break;
            }
            exit(0);
        }
    }
    // request granted, ipv4 address 10.0.0.7, port 16128
    write(3, "\005\000\0\001" "\012\0\0\007" "\077", 10);

    // examine possibility to fdpass socket fd ... //
    xmovefd(sd, 4);
}

static void io(int ifd, int ofd)
{
    char buf[8192];
    int l = read(ifd, buf, sizeof buf);
    if (l <= 0) {
        // hmm, econnreset...
        if (l == 0 || errno == ECONNRESET) {
            float tr = elapsed();
            warn("EOF from %d (%.3f s). Exiting.", ifd, tr);
            exit(0);
        }
        // do we need EINTR handling. possibly not, (as not blocking) //
        die("read from %d failed:", ifd);
    }
    int wl = write(ofd, buf, l);
    if (wl != l) {
        if (wl < 0)
            die("write to %d failed:", ofd);
        else
            die("short write to %d (%d < %d)", ofd, wl, l);
    }
}

// this function breaks the event loop idea in the main() function,
// later this whole thing may be refactored, but initially this serves
// the purpose
static void may_serve_index_file_request(const char * host, char * rbuf)
{
    char c;
    if (sscanf(host, "index-%32[^./].htm%1[l]%1s", rbuf, &c, &c) != 2)
        return;
    // add code to fetch index.html from remove and spiff some output
    // not the nicest way to handle this -- refactoring may be required
    // when more features are added to this program...
    alarm(0);

    warn("Loading index.html from %s", rbuf);

    // some duplicate code!
    int sd = connect_unix_stream_mxtx_socket(rbuf, "");
    if (sd < 0)
        // xxx create better (i.e. 'a') reply page perhaps
        die("Cannot connect to mxtx socket '%s':", rbuf);

    write(sd, "\0\0\0\020~cat\0index.html\0", 20);

    // XXX hardcoded socks5 reply
    // request granted, ipv4 address 10.0.0.7, port 16128 (^^^ duplicated ^^^)
    write(3, "\005\000\0\001" "\012\0\0\007" "\077", 10);
    alarm(10);
    char buf[4096];
    read(3, buf, sizeof buf); // and discard it //
#define reply ("HTTP/1.1 200 OK\r\n" "Connection: close\r\n" "\r\n")
    write(3, reply, sizeof reply - 1);
    int len;
    while ((len = read(sd, buf, sizeof buf)) > 0)
        write(3, buf, len);
    close(3);
    close(sd);
    exit(0);
}
