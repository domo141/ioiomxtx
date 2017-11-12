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
 set -x; exec ${CC:-gcc} -std=c99 $WARN "$@" -o "$trg" "$0" -L. -lmxtx #-flto
 exit $?
 */
#endif
/*
 * $ mxtx-io.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *      Copyright (c) 2017 Tomi Ollila
 *          All rights reserved
 *
 * Created: Wed 16 Aug 2017 20:24:06 EEST too
 * Last modified: Sun 12 Nov 2017 12:18:33 +0200 too
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/un.h>
#include <errno.h>

#include "mxtx-lib.h"

#define null ((void*)0)

const char * _prg_ident = "mxtx-io: ";

static void io(int ifd, int ofd)
{
    char buf[8192];
    int l = read(ifd, buf, sizeof buf);
    if (l <= 0) {
        // hmm, econnreset...
        if (l == 0 || errno == ECONNRESET) {
            //warn("EOF from %d. Exiting.", ifd);
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

static int rexec(char * lnk, char ** rcmdv)
{
    unsigned char rcmdline[1024];
    unsigned char * p = rcmdline + 4;
    char *rcmd = rcmdv[0];
    for (; *rcmdv; ++rcmdv) {
        size_t len = strlen(*rcmdv);
        if (len >= sizeof rcmdline - (p - rcmdline) - 1)
            die("command line %s... too long", rcmd);
        strcpy((char *)p, *rcmdv);
        p += len + 1;
    }
    rcmdline[0] = 0;
    rcmdline[1] = 0;
    rcmdline[2] = (p - rcmdline - 4) / 256;
    rcmdline[3] = (p - rcmdline - 4) & 255;

    int sd = connect_to_mxtx(default_mxtx_socket_path(lnk));
    if (sd < 0) exit(1);
    write(sd, rcmdline, p - rcmdline);
    return sd;
}

// optignore: for cases this used as 'ssh' command...
static void optignore(int * argcp, char *** argvp) {
    while (*argcp > 0) {
        // warn("%d %s", *argcp, (*argvp)[0]);
        if ((*argvp)[0][0] != '-')
            break;
        if (strcmp((*argvp)[0], "-e") == 0) {
            *argcp -= 2; *argvp += 2;
        }
        else
            break;
    }
}

static int is_link(char * arg)
{
    if (arg[0] == ':' && arg[1] == '\0') return 0;
    char * p = strrchr(arg, ':');
    if (p && p[1] == '\0') {
        p[0] = '\0';
        return 1;
    }
    return 0;
}

int main(int argc, char ** argv)
{
    char * prgname = argv[0];
    char * sep = null;

    argc--; argv++;

    // check first arg being separator, e.g ''  '.'  '..'  '*/'  '*/.'  '*/..'
    if (argc > 0) {
        char * p = strrchr(argv[0], '/');
        if (p == null) p = argv[0]; else p++;
        if (p[0] == '\0') sep = argv[0];
        else if (p[0] == '.') {
            if (p[1] == '\0' || (p[1] == '.' && p[2] == '\0'))
                sep = argv[1];
        }
    }
    if (sep) { argc--; argv++; }

    optignore(&argc, &argv);

    if (argc < 1) {
        fprintf(stderr, "\nUsage: %s [sep] [link] command [args] "
                "[<sep> [link:] command [args]]\n\n\
'by default' %s runs command on linked system and communicates via stdio.\n\
\n\
when first arg matches '', '.', '..', '*/', '*/.', '*/..', it is considered\n\
as separator string for 2 command lines given on command line. in this case\n\
if next arg (in both command lines) end with ':', then that command is run\n\
on linked system (otherwise directly) and (std)io of both of these commands\n\
are tied together.\n\n", prgname, prgname);
        exit(1);
    }

    char ** rcmdl2 = null;

    if (sep) {
        for (int i = 0; argv[i]; i++) {
            if (strcmp(argv[i], sep) == 0) {
                if (argv[i + 1] == null)
                    die("Second command line missing after '%s'"
                        " on command line", sep);
                rcmdl2 = &argv[i + 1];
                argv[i] = null;
            }
        }
        if (rcmdl2 == null)
            die("Second '%s' missing on command line", sep);
    }
    else if (argc < 2)
        die("Command missing");

    /// XXX unify above and below (if you care) ///

    int sd1, sd2, sdo;
    if (!sep) {
        is_link(argv[0]);
        char * lnk = argv[0];
        argc--; argv++;
        optignore(&argc, &argv);
        if (argc < 1) die("Command missing");
        sd1 = rexec(lnk, argv);
        sd2 = 0;
        sdo = 1;
        // continue to io loop //
    }
    else {
        int r1 = is_link(argv[0]);
        int r2 = is_link(rcmdl2[0]);

        /**/ if (r1 && ! r2) {
            close(0);
            sd1 = rexec(argv[0], argv + 1);
            xmovefd(sd1, 0); // expect it to be 0, though
            xdup2(0, 1);
            xexecvp(rcmdl2[0], rcmdl2);
            // not reached //
        }
        else if (r2 && ! r1) {
            close(0);
            sd2 = rexec(rcmdl2[0], rcmdl2 + 1);
            xmovefd(sd2, 0); // expect it to be 0, though
            xdup2(0, 1);
            xexecvp(argv[0], argv);
            // not reached //
        }
        else if (! r1) { // both local processes (much like ioio.pl)
            int sv[2];
            if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0)
                die("socketpair");

            pid_t pid = fork();
            if (pid == 0) {
                // child //
                close(sv[0]);
                xdup2(sv[1], 0);
                xdup2(sv[1], 1);
                close(sv[1]);
                xexecvp(*argv, argv);
                // not reached //
            }
            if (pid < 0) die("fork:");
            // parent //
            close(sv[1]);
            xdup2(sv[0], 0);
            xdup2(sv[0], 1);
            close(sv[0]);
            xexecvp(*rcmdl2, rcmdl2);
            // not reached //
        }
        else {
            sd1 = rexec(argv[0], argv + 1);
            sd2 = rexec(rcmdl2[0], rcmdl2 + 1);
            sdo = sd2;
            // continue to io loop //
        }
    }

    struct pollfd pfds[2];
    pfds[0].fd = sd1; pfds[1].fd = sd2;
    pfds[0].events = pfds[1].events = POLLIN;

    while (1) {
        if (poll(pfds, 2, -1) <= 0) {
            warn("unexpected poll() return without any input available.");
            sleep(1);
        }
        if (pfds[0].revents) io(sd1, sdo);
        if (pfds[1].revents) io(sd2, sd1);
    }
    return 0;
}
