#if 0 /* -*- mode: c; c-file-style: "stroustrup"; tab-width: 8; -*-
 set -euf; trg=${0##*''/}; trg=${trg%.c}; test ! -e "$trg" || rm "$trg"
 case ${1-} in '') set x -O2; shift; esac
 #case ${1-} in '') set x -ggdb; shift; esac
 set -x; exec ${CC:-gcc} -std=c99 "$@" -o "$trg" "$0" -L. -lmxtx
 exit $?
 */
#endif
/*
 * $ mxtx-rsh.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *      Copyright (c) 2017 Tomi Ollila
 *          All rights reserved
 *
 * Created: Sun 03 Sep 2017 21:45:01 EEST too
 * Last modified: Sat 02 Apr 2022 00:22:18 +0300 too
 */

#include "more-warnings.h"

// for linux to compile w/ -std=c99
//#define _DEFAULT_SOURCE // Since glibc 2.19: -- defined in more-warnings.h
//#define _BSD_SOURCE // Glibc 2.19 and earlier: -- ditto

#include <unistd.h>
#include <stdio.h>
//#include <string.h>
#include <stdlib.h>
//#include <stdbool.h>
//#include <fcntl.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <signal.h>
#if defined(__linux__) && __linux__ || defined(__CYGWIN__) && __CYGWIN__
#include <pty.h>
#else
#include <termios.h>
#endif
//#include <.h>

#include "mxtx-lib.h"

#include "mxtx-rsh.ch"
#define LPKTREAD_DATA_BUFFER_SIZE (65532 - 3)
#include "lpktread.ch"

#define null ((void*)0)

#define BB {
#define BE }

#define isizeof (int)sizeof

#define df(...) fprintf(stderr, __VA_ARGS__)

const char * _prg_ident = "mxtx-rsh: ";

struct {
    char waitsigack;
    char mayhavesig;
    char sighup;
    char sigint;
    char sigquit;
    char sigterm;
    char tty;
    char nostdin;
    char * link;
    char env_link[128];
    struct termios saved_tio;
#if defined (__LP64__) && __LP64__
    int pad2;
#endif
} G;

static void reset_tio(void)
{
    tcsetattr(0, TCSAFLUSH, &G.saved_tio);
}

static void sighandler(int sig);

static void hook_exit(void);
static void may_run_hook(bool entry)
{
    char buf[4096];
    const char * home = getenv("HOME");
    if (home == null) return; // unlikely !
    size_t l = (size_t)snprintf(buf, sizeof buf,
                                "%s/.local/share/mxtx/rsh-hook", home);
    if (l >= sizeof buf) return; // unlikely !
    if (access(buf, F_OK | X_OK) != 0) {
        if (errno != ENOENT) warn("%s:", buf);
        return; // if dir: user error //
    }
    pid_t pid = fork();
    if (pid < 0) die("fork():");
    if (pid > 0) {
        if (entry) atexit(hook_exit);
        waitpid(pid, NULL, 0);
        return;
    }
    // child //
    if (entry) setenv("MXTX_RSH_ENTRY", "t", 1);
    else       setenv("MXTX_RSH_ENTRY", "", 1);
    char * args[3];
    args[0] = buf;
    args[1] = G.link;
    args[2] = null;
    xexecvp(buf, args);
}
static void hook_exit(void) {
    may_run_hook(false);
}

static void opts(int * argcp, char *** argvp) {
    while (*argcp > 0 && (*argvp)[0][0] == '-') {
        /**/ if ((*argvp)[0][1] == 't' && (*argvp)[0][2] == '\0')
            G.tty = 1;
        else if ((*argvp)[0][1] == 'n' && (*argvp)[0][2] == '\0')
            G.nostdin = 1;
        else if ((*argvp)[0][1] == 'q' && (*argvp)[0][2] == '\0')
            ; // drop -q (quiet)...
        else if ((*argvp)[0][1] == 'e' && (*argvp)[0][2] == '\0') {
            // drop -e <escape-char> (for now)
            (*argcp)--; (*argvp)++;
        }
        else
            die("'%s': unknown option", (*argvp)[0]);
        (*argcp)--; (*argvp)++;
        continue;
    }
}


int main(int argc, char * argv[])
{
    int caa = 0;
    const char * prgname = argv[0];
    argc--; argv++;
    opts(&argc, &argv);

#define USAGEFMT "Usage: %s [-t] [-n] remote [.] [command] [args]"
    if (argc <= 0) die(USAGEFMT, prgname);

    G.link = argv[0];
    argc--; argv++;
    opts(&argc, &argv);

    if (argc < 0) die(USAGEFMT, prgname);
#undef USAGEFMT

    if (argc == 0 && G.nostdin == 0)
        G.tty = 1;
    else if (argc > 0 && argv[0][0] == '.' && argv[0][1] == '\0') {
        if (argc == 1)
            die("After '.' command is required");
        caa = 1;
    }

    if (G.tty && tcgetattr(0, &G.saved_tio) < 0) {
        warn("Cannot read tty parameters. force remote tty mode");
        G.tty = -1; // tristate //
    }

    if (argc == 0)
        may_run_hook(true);
    BB;
    int sd = connect_unix_stream_mxtx_socket(G.link, "");
    if (sd < 0) exit(1);
    xmovefd(sd, 3);
    BE;
    (void)!write(3, "\0\0\0\012" "mxtx-rshd\0" MXTX_RSHC_IDENT,
                 4 + 10 + sizeof mxtx_rshc_ident);
    LPktRead pr;
    BB;
    unsigned char * p = (unsigned char *)pr.data;
    if (G.tty) {
         struct winsize ws;
         if (G.tty < 0) {
             ws.ws_col = 80, ws.ws_row = 24;
         }
         else if (ioctl(0, TIOCGWINSZ, &ws) < 0) {
             warn("Getting window size failed (using 80x24):");
             ws.ws_col = 80, ws.ws_row = 24;
         }
         *p++ = '\0'; *p++ = '\005'; *p++ = 't';
         *p++ = ws.ws_col / 256; *p++ = ws.ws_col % 256;
         *p++ = ws.ws_row / 256; *p++ = ws.ws_row % 256;
    }
    snprintf(G.env_link, sizeof G.env_link, "MXTX_LINK=%s", G.link);
    putenv(G.env_link);
    const char * pe[] = { "TERM","LANG","LANGUAGE","LC_CTYPE","LC_NUMERIC",
                          "LC_TIME","LC_COLLATE","LC_MONETARY","LC_MESSAGES",
                          "LC_PAPER","LC_NAME","LC_ADDRESS","LC_TELEPHONE",
                          "LC_MEASUREMENT","LC_IDENTIFICATION","LC_ALL",
                          "MXTX_LINK" };
    for (unsigned int i = 0; i < sizeof pe / sizeof (char *); i++) {
        // xxx could go through ** environ, perhaps some day ?
        char * e = getenv(pe[i]);
        if (e) {
            int l = snprintf((char *)p + 2, sizeof pr.data - (p - pr.data) - 4,
                             "e%s=%s", pe[i], e);
            p[0] = l / 256; p[1] = l % 256;
            p += l + 2;
            if ((p - pr.data) > isizeof pr.data - 100) // unlikely!
                die("Environment variables takes too much space");
        }
    }
    if (argc == 0) {
        *p++ = '\0'; *p++ = '\001'; *p++ = 's'; // interactive shell
    }
    else {
        unsigned char * s = p; p += 3;
        for (int i = caa? 1: 0; i < argc; i++) {
            int l = strlen(argv[i]);
            if (p - pr.data >= isizeof pr.data - l - 4)
                die("Command line and env. variables take too much space");
            memcpy(p, argv[i], l);
            p += l; *p++ = caa? '\0': ' ';
        }
        if (caa) s[2] = 'c';
        else { p--; s[2] = 's'; }
        s[0] = (p - s - 2) / 256; s[1] = (p - s - 2) % 256;
    }
    // write whole command structure in one sweep
    (void)!write(3, pr.data, p - pr.data);
    BE;
    BB;
    size_t l = read(3, pr.data, sizeof mxtx_rshd_ident + 3);
    if (l < 8)
        die("could not read rshd ident");
    if (memcmp(pr.data, mxtx_rshd_ident, sizeof mxtx_rshd_ident) != 0)
        die("server ident mismatch");
    if (l < sizeof mxtx_rshd_ident + 3)
        l = read(3, pr.data + l, sizeof mxtx_rshd_ident + 3 - l);
    if (memcmp(pr.data + sizeof mxtx_rshd_ident, "\0\001" "a", 3) != 0)
        die("initial ack not received");
    BE;
    if (G.tty > 0) {
        struct termios tio = G.saved_tio;
        // see ttydefaults.h and then compare w/ what other sw does here
        cfmakeraw(&tio);
        tio.c_cc[VMIN] = 1;
        tio.c_cc[VTIME] = 0;
        atexit(reset_tio);
        tcsetattr(0, TCSANOW, &tio);
        sigact(SIGWINCH, sighandler, 0);
    }
    else {
        sigact(SIGHUP, sighandler, 0);
        sigact(SIGINT, sighandler, 0);
        sigact(SIGQUIT, sighandler, 0);
        sigact(SIGTERM, sighandler, 0);
    }

    lpktread_init(&pr, 3);

    struct pollfd pfds[2];
    pfds[0].fd = 3; pfds[0].events = POLLIN;
    pfds[1].fd = 0; pfds[1].events = POLLIN;
    int nfds = G.nostdin? 1: 2;
    while (1) {
        char ibuf[8192];
        int n = poll(pfds, nfds, -1);
        (void)n;
        if (pfds[0].revents) {
            unsigned char * datap;
            int len;
            while ( (len = lpktread(&pr, &datap)) > 0) {
                if (datap[0] == '1') {
                    xwritefully(1, (char *)datap + 1, len - 1);
                    continue;
                }
                if (datap[0] == '2') {
                    xwritefully(2, (char *)datap + 1, len - 1);
                    continue;
                }
                if (datap[0] == 'a') {
                    G.waitsigack = 0;
                    G.mayhavesig = 1;
                    continue;
                }
                if (datap[0] == 'x') {
                    /**/ if (datap[1] == '1') { if (!isatty(1)) close(1); }
                    else if (datap[1] == '2') { if (!isatty(2)) close(2); }
                    continue;
                }
                if (datap[0] == 'e') {
                    exit(datap[1]); // XXX handle exit code //
                }
                warn("drop unknown input %02x", datap[0]);
                continue;
            }
            if (len < 0) {
                if (datap != null)
                    warn("read from mxtx failed:");
                else
                    warn("EOF from mxtx");
                exit(7);
            }
        }
        if (pfds[1].revents) {
            int len = read(0, ibuf + 3, sizeof ibuf - 3);
            if (len <= 0) {
                if (len == 0) {
                    // send eof //
                    char msgbuf[3] = { 0, 1, 'x' };
                    xwritefully(3, msgbuf, 3);
                    // pfds[1].fd = -1; // not needed anymore
                    nfds = 1;
                }
                else die("read from stdin failed:");
            }
            else {
                //for (int i=0; i<len; i++) df(" %d",ibuf[i+3]); //df("\r\n");
                len += 1;
                ibuf[0] = len / 256; ibuf[1] = len % 256; ibuf[2] = '0';
                xwritefully(3, ibuf, len + 2);
            }
        }
        if (G.mayhavesig) {
            G.mayhavesig = 0;
            unsigned char * p = pr.data;
            if (G.waitsigack) continue;
            if (G.tty > 0) {
                if (G.sighup) {
                    G.sighup = 0;
                    struct winsize ws;
                    if (ioctl(0, TIOCGWINSZ, &ws) < 0) {
                        warn("getting window size failed");
                    }
                    else {
                        *p++ = '\0'; *p++ = '\005'; *p++ = 'w';
                        *p++ = ws.ws_col / 256; *p++ = ws.ws_col % 256;
                        *p++ = ws.ws_row / 256; *p++ = ws.ws_row % 256;
                    }
                }
            }
            else {
                if (G.sighup) { G.sighup = 0;
                    *p++ = '\0'; *p++ = '\002'; *p++ = 's'; *p++ = RSH_SIGHUP;
                }
                if (G.sigint) { G.sigint = 0;
                    *p++ = '\0'; *p++ = '\002'; *p++ = 's'; *p++ = RSH_SIGINT;
                }
                if (G.sigquit) { G.sigquit = 0;
                    *p++ = '\0'; *p++ = '\002'; *p++ = 's'; *p++ = RSH_SIGQUIT;
                }
                if (G.sigterm) { G.sigterm = 0;
                    *p++ = '\0'; *p++ = '\002'; *p++ = 's'; *p++ = RSH_SIGTERM;
                }
            }
            if (p != pr.data) {
                xwritefully(3, pr.data, p - pr.data);
                G.waitsigack = 1;
            }
        }
    }
    return 0;
}

static void sighandler(int sig) {
    G.mayhavesig = 1;
    switch (sig) {
    case SIGHUP: case SIGWINCH: G.sighup = 1; break;
    case SIGINT: G.sigint = 1; break;
    case SIGTERM: G.sigterm = 1; break;
    case SIGQUIT: G.sigquit = 1; break;
    }
}
