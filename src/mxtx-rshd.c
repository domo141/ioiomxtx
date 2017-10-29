#if 0 /* -*- mode: c; c-file-style: "stroustrup"; tab-width: 8; -*-
 set -euf; trg=${0##*''/}; trg=${trg%.c}; test ! -e "$trg" || rm "$trg"
 WARN="-Wall -Wstrict-prototypes -Winit-self -Wformat=2" # -pedantic
 WARN="$WARN -Wcast-align -Wpointer-arith " # -Wfloat-equal #-Werror
 WARN="$WARN -Wextra -Wwrite-strings -Wcast-qual -Wshadow" # -Wconversion
 WARN="$WARN -Wmissing-include-dirs -Wundef -Wbad-function-cast -Wlogical-op"
 WARN="$WARN -Waggregate-return -Wold-style-definition"
 WARN="$WARN -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls"
 WARN="$WARN -Wnested-externs -Winline -Wvla -Woverlength-strings -Wpadded"
 case ${1-} in '') set x -O2; shift; esac
 #case ${1-} in '') set x -ggdb; shift; esac
 set -x; exec ${CC:-gcc} -std=c99 $WARN "$@" -o "$trg" "$0" -L. -lmxtx -lutil
 exit $?
 */
#endif
/*
 * $ mxtx-rshd.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *      Copyright (c) 2017 Tomi Ollila
 *          All rights reserved
 *
 * Created: Sat 02 Sep 2017 18:42:37 EEST too
 * Last modified: Sat 28 Oct 2017 11:39:51 +0300 too
 */

#define execvp(f,a) __xexecvp(f,a)  // cheat constness...

#define _DEFAULT_SOURCE
#define _GNU_SOURCE // for some older linux environments

#include <unistd.h>
#include <stdio.h>
//#include <string.h>
#include <stdlib.h>
//#include <stdbool.h>
//#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <pty.h>
#include <termio.h>
#include <utmp.h> // login_tty()
//#include <.h>
#include <poll.h>

#undef execvp
void execvp(const char * file, const char ** argv);

#include "mxtx-lib.h"

#include "mxtx-rsh.ch"
#define LPKTREAD_DATA_BUFFER_SIZE 16384
#include "lpktread.ch"

#define null ((void*)0)

#define BB {
#define BE }

const char * _prg_ident = "rshd: ";

struct {
    pid_t pid;
    int cmd_fds;
} G;

static void exec_cmd(unsigned int havetty, const char* file, const char** argv)
{
    union {
        struct {
            int tty, pty;
        } t;
        struct {
            int sv[2], pv[2];
        } p;
    } u;

    // warn("havetty: %08x", havetty);
    if (havetty) {
        struct winsize ws = {
            .ws_col = havetty >> 16, .ws_row = havetty & 0xffff
        };
        //struct termios tio;
        //if (openpty(&pty, &tty, null, &tio, &ws) < 0)
        if (openpty(&u.t.pty, &u.t.tty, null, null, &ws) < 0)
            die("openpty:");
    }
    else {
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, u.p.sv) < 0)
            die("socketpair:");
        if (pipe(u.p.pv) < 0)
            die("pipe:");
    }
    if ( (G.pid = fork()) == 0) {
        // child //
        if (havetty) {
            close(u.t.pty);
            if (login_tty(u.t.tty) < 0) die("login_tty:");
        }
        else {
            close(u.p.sv[0]); close(u.p.pv[0]);
            xmovefd(u.p.sv[1], 0); xdup2(0, 1); xmovefd(u.p.pv[1], 2);
        }
        execvp(file, argv);
        die("execvp %s:", file);
    }
    // parent //
    if (havetty) {
        close(u.t.tty);
        xmovefd(u.t.pty, 3);
    }
    else {
        close(u.p.sv[1]); close(u.p.pv[1]);
        xmovefd(u.p.sv[0], 3); xmovefd(u.p.pv[0], 4);
    }
}


static void wait_exit(void) ATTRIBUTE ((noreturn));
static void wait_exit(void)
{
    for (int i = 0; i < 100; i++) {
        int status;
        // xxx should read input from 0, if any ... //
        pid_t pid = waitpid(G.pid, &status, 0);
        if (pid == G.pid) {
            // XXX encode to something peer can decode (more portable)
            // or check if this is just compatible...
            char msgbuf[5] = { 0, 3, 'e', status / 256 & 0xff, status & 0xff };
            xwritefully(1, msgbuf, 5);
            break;
        }
    }
    exit(0);
}


static inline void pio(int fd, struct pollfd * pfd, char * buf, size_t buflen)
{
    int chnl = fd + '0' - 2;
    int len = read(fd, buf + 3, buflen - 3);
    if (len <= 0) {
        if (len == 0
            || errno == EIO /* tty :O */) {
            // send eof, channel 1 or 2 //
            char msgbuf[4] = { 0, 2, 'x', chnl };
            xwritefully(1, msgbuf, 4);
            if (--G.cmd_fds <= 0)
                wait_exit();
            pfd->fd = -pfd->fd;
        }
        else die("XXX read(%d):", fd);
    }
    else {
        len += 1;
        buf[0] = len / 256; buf[1] = len % 256; buf[2] = chnl;
        xwritefully(1, buf, len + 2);
    }
}

static void socket_io(LPktRead * pr);

int main(void)
//int main(int argc, char * argv[]) // later perhaps debug options...
{
    write(1, mxtx_rshd_ident, sizeof mxtx_rshd_ident);
    LPktRead pr;
    xreadfully(0, pr.data, sizeof mxtx_rshc_ident);
    if (memcmp(pr.data, mxtx_rshc_ident, sizeof mxtx_rshc_ident) != 0)
        die("client ident mismatch");
    unsigned int tty = 0;
    BB;
    char xenv[1024];
    unsigned int xepos = 0;
    // xxx look man ssh for ideas what env vars set by me //
    lpktread_init(&pr, 0);
    while (1) {
        int len;
        unsigned char * data;
        while ((len = lpktread(&pr, &data)) == 0) {};
        if (len < 0) // change to silent exit
            die("EOF (or fatal error)");
        // data[len] = '\0'; warn("read len: %d: %s", len, data); // XXX debug
        switch (data[0]) {
        case 'e':
            if (xepos + len >= sizeof xenv - 2) // -2 inaccurate but safe
                warn("env buffer full; skipping adding new"); // unlikely
            // there is no point disallowing any env vars as long as
            // environment setting is not restricted by executed command line
            memcpy(xenv + xepos, data + 1, len - 1);
            xenv[xepos + len - 1] = '\0';
            putenv(xenv + xepos);
            xepos += len + 1 - 1;
            break;
        case 't': // tty, if size > 0
            if (len == 5) {
                tty = data[1] << 24 | data[2] << 16 | data[3] << 8 | data[4];
                if (tty == 0) tty = 0x00500018; // 80x24
            }
            break;
        case 'c': {
            if (len < 3)
                die("No command!");
#define MAX_ARGS 64 // copied from mxtx.c...
            const char * argv[MAX_ARGS];
            int argc = 0;
            len -= 2;
            data++;
            if (data[len] != 0)
                die("Command buffer does not end with '\\0'");
            int l = strlen((const char *)data);
            do {
                //warn("New command: argv[%d] = '%s' (%d bytes)", argc, data, l);
                argv[argc++] = (const char *)data;
                // warn("%d %d", len, l);
                if (l >= len) {
                    argv[argc] = null;
                    break;
                }
                data += (l + 1);
                len -= (l + 1);
                l = strlen((const char *)data);
            } while (argc < MAX_ARGS);
            if (argc == MAX_ARGS)
                die("Command '%s' has too many args", argv[0]);
#undef MAX_ARGS
            exec_cmd(tty, argv[0], argv);
            }
            goto _cont;
        case 's': {
            const char * shell = getenv("SHELL");
            const char * argv[4];

            if (shell == null) shell = "/bin/sh";
            char sbuf[12]; // with '8' gcc7 complained about truncation...
            if (len == 1) {
                char * p = strrchr(shell, '/');
                // hax for shell launching wrappers which cannot handle
                // modified argv[0]: use as e.g. SHELL=/usr/local/bin//zsh
                if (p && p != shell && p[-1] == '/') {
                    argv[0] = p+1;
                    argv[1] = "-il";
                    argv[2] = null;
                }
                else {
                    snprintf(sbuf, sizeof sbuf, "-%s", p? p+1: shell);
                    argv[0] = sbuf;
                    argv[1] = null;
                }
            }
            else {
                argv[0] = shell;
                argv[1] = "-c"; argv[2] = (char *)data + 1; argv[3] = null;
            }
            exec_cmd(tty, shell, argv);
            }
            goto _cont;
        default:
            warn("skipping unknown data %02x", data[0]);
        }
    }
_cont:
    //signal(SIGCHLD, SIG_DFL); // seemed to inherit parent NOCLDWAIT ? //
    write(1, "\0\001" "a", 3);
    BE;
    char ibuf[8192];
    if (tty) {
        struct pollfd pfds[2];
        pfds[0].fd = 0; pfds[0].events = POLLIN;
        pfds[1].fd = 3; pfds[1].events = POLLIN;
        G.cmd_fds = 1;
        while (1) {
            int n = poll(pfds, 2, -1);
            (void)n;
            if (pfds[0].revents) socket_io(&pr);
            if (pfds[1].revents) pio(3, &pfds[1], ibuf, sizeof ibuf);
        }
    } else { // not tty
        struct pollfd pfds[3];
        pfds[0].fd = 0; pfds[0].events = POLLIN;
        pfds[1].fd = 3; pfds[1].events = POLLIN;
        pfds[2].fd = 4; pfds[2].events = POLLIN;
        G.cmd_fds = 2;
        while (1) {
            int n = poll(pfds, 3, -1);
            (void)n;
            if (pfds[0].revents) socket_io(&pr);
            if (pfds[1].revents) pio(3, &pfds[1], ibuf, sizeof ibuf);
            if (pfds[2].revents) pio(4, &pfds[2], ibuf, sizeof ibuf);
        }
    }
    return 0;
}

static void socket_io(LPktRead * pr)
{
    unsigned char * datap;
    int len;
    while ( (len = lpktread(pr, &datap)) > 0) {
        if (datap[0] == '0') {
            xwritefully(3, (char *)datap + 1, len - 1);
            continue;
        }
        if (datap[0] == 'w') {
            if (len == 5) {
                struct winsize ws = {
                    .ws_col = datap[1] * 256 + datap[2],
                    .ws_row = datap[3] * 256 + datap[4]
                };
                if (ioctl(3, TIOCSWINSZ, &ws) < 0)
                    warn("WINCH ioctl (%dx%x) failed:", ws.ws_col, ws.ws_row);
            }
            xwritefully(1, "\0\001" "a", 3); // ack, as requested
            continue;
        }
        if (datap[0] == 's') {
            if (len == 2) {
                int sig = 0;
#define CASE(name) case RSH_ ## name: sig = name; break
                switch (datap[1]) {
                    CASE (SIGHUP);
                    CASE (SIGINT);
                    CASE (SIGQUIT);
                    CASE (SIGKILL);
                    CASE (SIGUSR1);
                    CASE (SIGUSR2);
                    CASE (SIGPIPE);
                    CASE (SIGTERM);
                    CASE (SIGCONT);
                    CASE (SIGSTOP);
                    //CASE (SIGWINCH);
                }
#undef CASE
                if (sig) kill(G.pid, sig);
            }
            xwritefully(1, "\0\001" "a", 3); // ack, as requested
            continue;
        }
        if (datap[0] == 'x')
            goto _eof;

        warn("drop unknown cmd 0x%02x", datap[0]);
    }

    if (len == 0)
        return /*0*/;

    if (datap != null)
        warn("read from rsh client failed:");
_eof:
    // error or eof //
    if (shutdown(3, SHUT_WR) < 0)
        close(3);
    return /*1*/;
}
