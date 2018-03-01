#if 0 /* -*- mode: c; c-file-style: "stroustrup"; tab-width: 8; -*-
 set -euf; trg=${0##*''/}; trg=${trg%.c}; test ! -e "$trg" || rm "$trg"
 case ${1-} in '') set x -O2; shift; esac
 #case ${1-} in '') set x -ggdb; shift; esac
 set -x; exec ${CC:-gcc} -std=c99 "$@" -o "$trg" "$0" -L. -lmxtx # -flto
 exit $?
 */
#endif
/*
 * $ mxtx-dgramtunnel.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *      Copyright (c) 2017 Tomi Ollila
 *          All rights reserved
 *
 * Created: Mon 05 Feb 2018 04:49:13 -0800 too
 * Last modified: Thu 01 Mar 2018 20:49:35 +0200 too
 */

// hint: strace -f [-o of] ... and strace -p ... are useful when inspecting...

#include "more-warnings.h"

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>

#include <endian.h> //for older compilers not defining __BYTE_ORDER__ & friends

#include "mxtx-lib.h"

#define null ((void*)0)

#define isizeof (int)sizeof

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

#define DBG 0
#if DBG
#define dprintf(...) fprintf(stderr, __VA_ARGS__)
#define dprintf0(...) do { } while (0)
#else
#define dprintf(...) do { } while (0)
#define dprintf0(...) do { } while (0)
#endif


#define LPKTREAD_DATA_BUFFER_SIZE 4097 // seen 1272-sized udp datagrams...
#define LPKTREAD_HAVE_PEEK 1
#include "lpktread.ch"

const char * _prg_ident = "mxtx-dgramtunneld: ";

static void client(const char * lnk) ATTRIBUTE((noreturn));
static void server(void) ATTRIBUTE((noreturn));

int main(int argc, char * argv[])
{
    switch (argc) {
    case 1: server();
    case 2: client(argv[1]);
    default:
        die("Usage: %s [link]", argv[0]);
    }
}

// we could use port zero (0) in bind, to let system choose
// port, but this gives some more control and flexibility
static int bind_dgram_isock_to_fd_3(int sp, int ep)
{
    int sd = xsocket(AF_INET, SOCK_DGRAM);
    int one = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in iaddr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = IADDR(127,0,0,1)
    };
    for (int i = sp; i < ep; i++) {
        iaddr.sin_port = IPORT(i);
        if (bind(sd, (struct sockaddr *)&iaddr, sizeof iaddr) == 0) {
            xmovefd(sd, 3);
            return i;
        }
        if (errno == ECONNREFUSED)
            continue;
        die("bind:");
    }
    die("Cannot find free port");
}


static bool initial_protosync(int fd, const char * sm, const char * em)
{
    write(fd, sm, 8);
    char buf[8];
    if (read(fd, buf, 8) != 8) {
        warn("No response from remote");
        return false;
    }
    if (memcmp(buf, em, 8) != 0) {
        warn("Ident mismatch, expected '%s'", em);
        return false;
    }
    return true;
}

static void bind_dgram_usock_to_fd_4(const char * lnk)
{
    struct sockaddr_un uaddr;
    int sd = xbind_listen_unix_socket(
        fill_mxtx_socket_path(&uaddr, lnk, "-dgt"), SOCK_DGRAM);
    xmovefd(sd, 4);
}

static void send_to_iport(uint32_t addr, in_port_t port,
                          const unsigned char * data, int datalen)
{
    struct sockaddr_in iaddr = {
        .sin_family = AF_INET,
        .sin_port = port,
        .sin_addr.s_addr = addr
    };
    /* XXX */ sendto(3, data, datalen, 0,
                     (struct sockaddr *)&iaddr, sizeof iaddr);
}

static bool connect_to_server_fd_5(const char * lnk)
{
    int sd = connect_unix_stream_mxtx_socket(lnk, "");
    if (sd < 0) return false;
    xmovefd(sd, 5);

    //char cmd[] = "....strace\0./mxtx-dgramtunneld";
    char cmd[] = "....mxtx-dgramtunneld";
    cmd[0] = cmd[1] = cmd[2] = 0; cmd[3] = sizeof cmd - 4;
    write(5, cmd, sizeof cmd);

    // in case of failure no need to close fd 5, will be dup2()d over next time
    return initial_protosync(5, "mxtxdtc1", "mxtxdts1");
}

static void client(const char * lnk)
{
    bind_dgram_usock_to_fd_4(lnk); // checkaa vielä portit XXX

    if (! connect_to_server_fd_5(lnk))
        exit(1);

    int iport = bind_dgram_isock_to_fd_3(40001, 40404);

    // strace -f can be used to not let fork() detach when debugging...
    switch (fork()) {
    case -1: die("fork failed:");
    case 0: break;
    default: exit(0);
    }
    /* child */
    close(0); close(1); close(2); setsid(); // fd 2 to log (or caller does)?

    struct pollfd pfds[3] = {
        { .fd = 5, .events = POLLIN },
        { .fd = 3, .events = POLLIN },
        { .fd = 4, .events = POLLIN }
    };
    struct { unsigned short sport; unsigned short cport; } portmap[1024];
    memset(portmap, 0, sizeof portmap);
    time_t next_live_try = 0;
    time_t next_ping_try = time(null);
    int sent_after_recv = 0;
    LPktRead pr;
    lpktread_init(&pr, 5);

    while (1) {
        (void)poll(pfds, 3, -1);
        if (pfds[0].revents) { // msg from tunnel
            unsigned char * datap;
            int len;
            sent_after_recv = 0;
            while ( (len = lpktread(&pr, &datap)) != 0) {
                if (len < 3) { // XXX
                    if (len == 2) goto next1; // pong received
                    pfds[0].fd = -1;
                    close(5);
                    next_live_try = time(null);
                    goto next1;
                }
                int sport = datap[0] * 256 + datap[1];
                int cport = 0;
                for (size_t i = 0; i < sizeof portmap/sizeof portmap[0]; i++) {
                    if (portmap[i].sport == sport) {
                        cport = portmap[i].cport;
                        break;
                    }
                    if (portmap[i].sport == 0)
                        break;
                }
                if (cport == 0) {
                    warn("cannot find port mapping for port %d", sport);
                    goto next1;
                }
                int hip = ((sport - 1022) / 254) + 1;
                int lop = ((sport - 1022) % 254) + 1;
                send_to_iport(IADDR(127,0,hip,lop), IPORT(cport),
                              datap + 2, len - 2);
            }
        }
        unsigned char sbuf[2048];
    next1:
        if (pfds[1].revents) { // msg from mosh kilent
            struct sockaddr_in iaddr;
            socklen_t addrlen = sizeof iaddr;
            int len = recvfrom(3, sbuf + 4, sizeof sbuf - 4, 0,
                               (struct sockaddr *)&iaddr, &addrlen);
            if (len < 0 || len > isizeof sbuf - 4) {
                die("XXX: len %d;", len);
            }
            if (next_live_try != 0) {
                // disconnected...
                time_t ct = time(null);
                if (ct - next_live_try < 0) goto next2;
                if (! connect_to_server_fd_5(lnk)) {
                    next_live_try = ct + 10;
                    goto next2;
                }
                next_live_try = 0;
                sent_after_recv = 0;
                pfds[0].fd = 5;
            }
            // else here and any optimization in next block unnecessary
            if (sent_after_recv > 20) {
                time_t ct = time(null);
                if (ct - next_ping_try > 0) {
                    next_ping_try = ct + 10;
                    int snt = send(5, "\000\002\000", 4, MSG_DONTWAIT); // ping
                    if (snt != 4 || ++sent_after_recv > 20 + 60) {
                        pfds[0].fd = -1;
                        close(5);
                        next_live_try = time(null);
                    }
                }
                goto next2;
            }
            unsigned char i1 = ((unsigned char *)&iaddr.sin_addr.s_addr)[0];
            unsigned char i2 = ((unsigned char *)&iaddr.sin_addr.s_addr)[1];
            unsigned char i3 = ((unsigned char *)&iaddr.sin_addr.s_addr)[2];
            unsigned char i4 = ((unsigned char *)&iaddr.sin_addr.s_addr)[3];
            /**/ if (i1 != 127)
                warn("ip address octet 1 (%d) != 127", i1);
            else if (i2 != 0)
                warn("ip address octet 2 (%d) != 0", i2);
            else if (i3 == 0 || i3 == 255)
                warn("ip address octet 3 (%d) not in [1,254]", i3);
            else if (i4 == 0 || i4 == 255)
                warn("ip address octet 4 (%d) not in [1,254]", i4);
            else {
                int sport = (i3 - 1) * 254 + (i4 - 1) + 1022;
                int cport = IPORT(iaddr.sin_port); // IPORT works both ways...
                for (size_t i = 0; i < sizeof portmap/sizeof portmap[0]; i++) {
                    if (portmap[i].sport == sport) {
                        if (cport != portmap[i].cport)
                            portmap[i].cport = cport;
                        break;
                    }
                    if (portmap[i].cport == cport) {
                        if (sport != portmap[i].sport)
                            portmap[i].sport = sport;
                        break;
                    }
                    if (portmap[i].cport == 0) {
                        portmap[i].cport = cport;
                        portmap[i].sport = sport;
                        break;
                    }
                }
                // XXX portmap table could in rare cases be too small
                sbuf[0] = (len + 2) / 256; sbuf[1] = (len + 2) % 256;
                sbuf[2] = sport / 256; sbuf[3] = sport % 256;
                /* XXX CHK */ write(5, sbuf, len + 4);
                sent_after_recv++;
            }
        }
    next2:
        if (pfds[2].revents) {
            // service port number requested
            struct sockaddr_un uaddr;
            socklen_t addrlen = sizeof uaddr;
            int len = recvfrom(4, sbuf, 8, 0,
                               (struct sockaddr*)&uaddr, &addrlen);
            if (len != 2) {
                // might liveloop if not exit (probably eintr to be checked)
                if (len < 0) die("unexpected read failure...");
                sbuf[0] = sbuf[1] = sbuf[2] = sbuf[3] = 0;
                goto reply;
            }
            if (sbuf[0] != 0 || sbuf[1] != 0) { // unexpected "version"
                sbuf[2] = sbuf[3] = 0;
                goto reply;
            }
            sbuf[2] = iport / 256; sbuf[3] = iport % 256;
        reply:
            (void)sendto(4, sbuf, 4, 0, (struct sockaddr*)&uaddr, addrlen);
        }
    }
    exit(0);
}

static void server(void)
{
    if (! initial_protosync(1, "mxtxdts1", "mxtxdtc1"))
        exit(1);

    (void)bind_dgram_isock_to_fd_3(40501, 40904);

    struct pollfd pfds[2] = {
        { .fd = 1, .events = POLLIN },
        { .fd = 3, .events = POLLIN }
    };
    LPktRead pr;
    lpktread_init(&pr, 1);
    while (1) {
        (void)poll(pfds, 2, -1);
        if (pfds[0].revents) {
            unsigned char * datap;
            int len;
            while ( (len = lpktread(&pr, &datap)) > 0) {
                if (len < 3) {
                    if (len == 2) { // ping
                        const unsigned char * pp = _lpktread_peek(&pr, 4);
                        // collapse consecutive pings into one //
                        if (!pp || memcmp(pp, "\000\002\000", 4) != 0)
                            write(1, "\000\002\000", 4); // pong
                        goto next1;
                    }
                    die("tunnel data len %d < 2!", len);
                }
                int port = (datap[0] << 8) + datap[1];
                send_to_iport(IADDR(127,0,0,1), IPORT(port),
                              datap + 2, len - 2);
                // XXX w/ connrefused, howto info kilent ???
                // XXX to be seen, improbable, user can RET ~ .
            }
            if (len < 0) {
                if (datap != null)
                    warn("read from mxtx failed:");
                else
                    warn("EOF from mxtx");
                exit(7);
            }
        }
    next1:
        if (pfds[1].revents) {
            char buf[2048];
            struct sockaddr_in iaddr;
            unsigned int addrlen = sizeof iaddr;
            int l = recvfrom(3, buf + 4, sizeof buf - 4, 0,
                             (struct sockaddr *)&iaddr, &addrlen);
            // XXX check addrlen (here & everywhere ?)
            if (l <= 0)
                die("XXX l = %d", l);
            int port = IPORT(iaddr.sin_port);
            buf[2] = port >> 8; buf[3] = port & 0xff;
            l += 2;
            buf[0] = l >> 8; buf[1] = l & 0xff;
            l += 2;
            if (write(1, buf, l) != l)
                // improve when shown needed
                die("short or failed write:");
        }
    }
    exit(0);
}
