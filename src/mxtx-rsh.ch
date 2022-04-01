/* -*- mode: c; c-file-style: "stroustrup"; tab-width: 8; -*-
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *      Copyright (c) 2017 Tomi Ollila
 *          All rights reserved
 *
 * Created: Sat 02 Sep 2017 18:24:44 EEST too
 * Last modified: Sat 26 Feb 2022 22:19:31 +0200 too
 */

// some common parts of mxtx-rsh and mxtx-rshd -- contains generated code

#define MXTX_RSHC_IDENT "mxtxrsh2"
const char mxtx_rshc_ident[8] = { 'm','x','t','x','r','s','h','2' };
const char mxtx_rshd_ident[8] = { 'm','x','t','x','r','s','h','3' };


// signal delivery.  // expect ack // perhaps later
#define RSH_SIGHUP     1
#define RSH_SIGINT     2
#define RSH_SIGQUIT    3
#define RSH_SIGKILL    9 // self-delivered, if ever
#define RSH_SIGUSR1   10
#define RSH_SIGUSR2   12
#define RSH_SIGPIPE   13
#define RSH_SIGTERM   15
#define RSH_SIGCONT   18 // self-delivered, if ever
#define RSH_SIGSTOP   19 // ditto
#define RSH_SIGWINCH  28
