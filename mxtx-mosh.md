
How mosh manages its client-server connection
=============================================

Normally
--------

- mosh(1) perl program calls `mosh-client -c` to figure out how many colors
  does current terminal supports

- mosh(1) uses ssh to execute `mosh-server new -c <colors> (and many other
  options)` to start server component. mosh-server bind inet[6] udp
  socket and print that along with 128bit symmetric key to stdout. mosh(1)
  collects that information

- mosh(1) execve(2)s `mosh-client` to start sending udp datagrams to the same
  remote host that was used with ssh command, to the port mosh-server printed
  out.

- when mosh-server receives datagrams, it uses the source address of those
  datagrams to send replies to. The source address may change -- mosh-server
  always replies to the address every incoming message is coming from.


Through mxtx tunnel
-------------------

- mxts-mosh(1) perl program sends datagram to `mxtx-dgramtunneld` unix domain
  socket to ask which inet udp port is has its communication socket bound. If
  `mxtx-dgramtunneld` (to the particular remote in question) is not running,
  it attempt to start it, and then ask again.

- when `mxtx-dgramtunneld` is started, it connects to the `mxtx` unix domain
  stream socket of the particular remote in question and requests `mxtx` to
  start `mxtx-dgramtunneld` at the endpoint. Both endpoints send their ident
  messages through the `mxtx` tunnel and when these messages are accepted,
  local `mxtx-dgramtunneld` forks into background and parent exits with zero
  value.

- mxtx-mosh(1) executes `mosh-client -c` to get colors like in "normal"
  operation

- mxtx-mosh(1) uses mxtx-rsh(1) to execute `mosh-server new -c <colors>...`
  pretty much like in "normal" operation.

- mxtx-mosh(1) sets/adds `ldpreload-moshclienthax.so` to `LD_PRELOAD`
  environment variable. ldpreload-moshclienthax.so "wraps" socket(2) system
  call so that after it has called `socket` it will bind the just-created
  socket to ipv4 address `127.0.<hip>.<lop>` -- <hip>.<lop> contains the port
  number mosh-server returned encoded in format that can be decoded by
  `mxtx-dgramtunneld`. The socket wrapper uses `MXTX_MOSH_PORT` environment
  variable to know the remote mosh-server port number.

- mxtx-mosh(1) execve's `mosh-client 127.0.0.1 <dgramtunneld-port>`.
  `mxtx-dgramtunneld` receives mosh-client datagrams, decodes mosh-server
  port number from source ip address and relays the datagram to remote
  `mxtx-dgramtunneld`, which in turn sends the datagram to `mosh-server`.
  mosh-server replies to remote mxtx-dgramtunneld, which relays reply
  datagram to local mxtx-dgramtunneld. local mxtx-dgramtunneld uses
  remote:local ports mapping table to send reply datagram to final
  `mosh-client` udp socket.

See tl;rd; in README.md for quick mxtx-mosh test.

To see on more detail how mosh (and mxtx-mosh) executes commands, enter

    $ strace -s256 -f -o log -e trace=execve,read mosh rhost
