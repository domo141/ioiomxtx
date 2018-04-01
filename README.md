
ioiomxtx
========

The *ioiomxtx* software set provides `ioio.pl`, simple tool to connect 2
programs together via their stdio (1,0,1,0) file descriptors. This feature
yields many very powerful possibilities.

The rest of the files here implements software components for multiplexing
data through a software tunnel; this tunnel can be created using `ioio.pl`
to connect one `mxtx` endpoint to another, often utilizing **ssh** on one
(or both!) endpoints.

In order to use `mxtx` this software needs to be installed on every tunnel
endpoints.


tl;dr;
------

```
$ git pull --rebase --autostash
$ make install YES=YES
$ rm -f mxtx-20[12]*.tar.gz
$ make a
$ scp mxtx-20[12]*.tar.gz rhost:
$ ssh rhost 'tar zxf mxtx-20[12]*.tar.gz'
$ ssh rhost 'cd mxtx-20[12]* && exec make install YES=YES'
$ ssh rhost 'rm -rf mxtx-20[12]*'
$ mxtx-apu.sh sshmxtx lnk rhost
$ : on another terminal :
$ mxtx-mosh lnk || mxtx-rsh lnk
```


ioio
----

`ioio.pl` runs two (2) commands and connects their stdio streams to each other.
The implementation is very simple, effectively just around 40 lines of code.

The command line usage looks like:

```
  $ ioio.pl command1 [args] '///' command2 [args]
```

The string `///`  is used to separate the command lines. Note that
`command1` needs to write something into stdout before `command2` is
executed; this is due to that the commands may ask e.g. passwords from
tty before continuing -- this way the requests don't intermix (usually).

With such a simple program some pretty neat things can be achieved;
usually in companion with `ssh(1)`.

As an alternative to `///` separator, "specially" formatted first argument
can also be used as a separator (`ioio.pl` contains built-in help for more
information). The following example use this alternative ('.' as a separator):

```
  $ ioio.pl . ssh rhost sshfs r: mnt/rhost -o slave . /path/to/sftp-server -R
```

The above makes reverse sshfs mount -- local current directory is mounted
as `mnt/rhost` on remote system, in read-only mode. Note that sftp-server
has capability to access parent directories of the initial path if such
thing would be an issue (e.g. using ptrace or gdb to attach running sshfs...).

And:

```
  $ ioio.pl ssh host1 .mxtx -c /// ssh host2 .mxtx -s
```

Accesses *host2* from *host1* using mxtx (installed as `$HOME/bin/.mxtx`) via
this current host working as intermediate gateway. After ssh negotiation with
*host1* is completed, the handshake message from mxtx (client) is received,
and then connection to *host2* will be initiated. In this case, the order
of commands could be reversed, as the mxtx server would start the handshake
the same way...

The above example created mxtx link `0` on client host (`host1`) for other
mxtx-* commands to use. Running `.mxtx` without parameters will give good
usage information for other options (better to document there than here).


mxtx
----

`mxtx` is software component which creates tunnel between 2 endpoints and
multiplexes traffic between these. Maximum of 250 simultaneous "connections"
can be handled by this software. On one end `mxtx` is started in `client`
mode, and in `server` mode on another.

`mxtx` client binds unix domain socket where mxtx-aware client software can
connect. Via this socket client software requests a command to be executed on
server. `mxtx` client creates new "connection" and sends request to server,
and bidirectional stdio between mxtx client and the program executed by server
is then transferred (until EOF is received on either end).

`mxtx` does not handle any flow control -- usually the socketpairs it
uses to communicate with programs can take all the data coming from network
socket fast enough, and if not, it waits a while to get data delivered.
If endpoint is still too slow to read data (e.g. startup time took too much
time), the connection will be dropped. In the case flow control is desired,
the endpoints must have protocol to communicate that (so far I've managed
fine without it).

(That said, simple `mxtx-rsh {link} git -C {path/to/repo} log | less` can
 be used to trigger this "feature" (when log long enough...).)

Typically `ioio.pl` is used to start `mxtx` endpoints (for the time being)...

### build c source and install

```
  $ make
  $ make install
  $ make install YES=YES
```

`mxtx` is installed as `$HOME/bin/.mxtx`, to move it away from e.g. tab
completion (otoh, of course, .m&lt;TAB> completes it). It is basically a
daemon program, launched by user (once) so this feels like a good name for it.

Rest of user-executable programs are installed with `$HOME/bin/.mxtx-` prefix
and other accombanied files at `$HOME/.local/share/mxtx/`. The command
`make unin` removes most of the installed files (should remove all in
`$HOME/bin/` but leaves `$HOME/.local/share/mxtx/` around).

Like mentioned before `mxtx` need to be installed on all endpoints tunnels
are to be created. `make a` can be used to (git-)archive sources for offline
copying.

### naming and tab completion

Naming is hard, and so is tab completion. After trying a few naming options
(to make tab completion easier), I resorted back to (initial) `mxtx` but
wanted `mx` prefix always complete to `mxtx-` -- and finally succeeded
on zsh using old-style *compctl* interface.

```
  function comp_cmdxpn {
      case $1'|'$2
        in mx'|') reply=(mxtx- mxtx--)
        ;; *) reply=()
      esac
  }
  compctl -C -K comp_cmdxpn + -c
```

This is good for me, but patches welcome on anything better (or bash support).

### short command introduction

#### mxtx-mosh

'Mobile shell'. Tunnels the UDP traffic between mosh-clients and mosh-servers
through an mxtx channel. Works pretty much like normal mosh application.
Requires Mosh to be installed on mxtx endpoints.

#### mxtx-rsh

'Remote' shell. Requests execution of `mxtx-rshd`. Multiplexes stdout, stderr
and return value. Tracks window size (when with tty) and sends WINCH requests.

#### mxtx-io

Very simple `ioio` -like functionality. No additional multiplexing -- stderr
of executed command is shown (on terminal) where `mxtx`s were started.

#### mxtx-cp

Mxtx copy command. Utilizes `rsync(1)` (subset of rsync options available, some
set on default). There is also `--tar` workaround mode to environments where
rsync(1) is not available (on either communication endpoints). In case there
wasn't even `tar(1)` available, user could resort to `mxtx-io`, `cat(1)` and
shell redirections to copy a file.

#### socksproxy

Special utility to tunnel traffic to designated destinations after socks5
communication is completed. Binds to unix domain socket so an *ldpreload*
library is used to make clients able to connect to it. There is an
`mxtx-apu.sh chromie` command which starts special (incognito)
chrome/chromium instance which uses this socksproxy for connections.

`mxtx-socksproxy` is installed as `$HOME/.local/share/mxtx/socksproxy`
(for now). It takes mxtx *link* names as arguments. When run it reads
($HOME/.local/share/mxtx/socksproxy/)`hosts-to-proxy` files on link
targets to see what hosts are connectable behind every particular link
('', '/' and '.' resolve to local system -- for those connections that can
be made directly by socksproxy). The tool `./addhost.sh` (installed to the
same .local/share/mxtx/ directory) can be used to ease adding hosts to the
`hosts-to-proxy` file.

#### mxtx-apu.sh

Mxtx Acute Program Usage, is a wrapper command to make some ordinary system
commands use mxtx tunnels for their operations. The tool provides self-help
when executed without arguments.

#### mxtx.el

An emacs lisp file mainly to add mxtx tramp support. `mxtx-apu.sh` has `emacs`
command to ease using this file.

#### ihqu-tmpl.plx

Index.Html Quite Useful -- `socksproxy` request to load
`$HOME/.local/share/mxtx/socksproxy/index.html` (behind link) when it is
asked to forward request to `http://index-{link}.html` page.
This file can be used to create such an `index.html` file (look into it).

#### addhost.sh

Resolves ipv4 address of a given hostname and writes results to
`$HOME/.local/share/mxtx/hosts-to-proxy` (used by *socksproxy*).

#### termtower-tmpl.sh

`termtower-tmpl.sh`, as named as template, when executed as is, moves current
graphical terminal to a new location and opens 2 more terminal windows beneath
it (uxrvt, xterm (or mintty)). These terminals can be used e.g. to open `mxtx`
tunnels to several hosts. Copy and edit to suit your needs...
