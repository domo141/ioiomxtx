#!/usr/bin/perl
# -*- mode: cperl; cperl-indent-level: 4 -*-
# $ mxtx-qpf-hack.pl $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2018 Tomi Ollila
#	    All rights reserved
#
# Created: Fri 06 Apr 2018 18:36:35 EEST too
# Last modified: Mon 23 Mar 2020 21:41:49 +0200 too

# mxtx quick port forward hack
#
# Forward a port from local host to remote ipv4/port
# at an mxtx endpoint.
# with throttle (when throttle-ms not zero) wait
# before writing to mtxt endpoint -- work around
# some experienced unexpected exits...

use 5.8.1;
use strict;
use warnings;

$ENV{'PATH'} = '/tus/kim/pa/';

die "Usage: $0 [throttle-ms] local-port mxtx-peer remote-ip remote-port\n"
  unless @ARGV == 4 or @ARGV == 5;

my $throttle = (@ARGV == 5)? (shift(@ARGV) / 1000): 0;

use IO::Socket::INET;

my $lsock = IO::Socket::INET->new(Listen => 1,
				  LocalAddr => '127.0.0.1',
				  LocalPort => $ARGV[0],
				  ReuseAddr => 1,
				  Proto     => "tcp")
	  or die "bind 127.1:$ARGV[0]: $!\n";

$lsock->listen(1) or die "listen: $!\n";

$SIG{CHLD} = 'IGNORE';

while (my $asock = $lsock->accept()) {
	if (fork) {
		close $asock;
		next;
	}
	close $lsock;
	socket S, AF_UNIX, SOCK_STREAM, 0 or die 'socket: ', $!;
	my $to = pack('Sxa*', AF_UNIX, "/tmp/user-1000/mxtx,$ARGV[1]");
	connect S, $to or die $!;

	my $conn_cmd = ":connect\0$ARGV[2]\0$ARGV[3]\0";
	syswrite S, pack('xxn', length($conn_cmd)) . $conn_cmd;
	my $buf;
	sysread S, $buf, 1;
	# fixme: check != 0
	my ($lin, $pin) = (fileno($asock), fileno(S));
	my $rin; vec($rin, $lin, 1) = 1; vec($rin, $pin, 1) = 1;
	while (1) {
		my $rout; select($rout = $rin, undef, undef, undef);
		if (vec($rout, $lin, 1)) {
			my $ev = sysread $asock, $buf, 65536;
			die $! unless defined $ev;
			print STDERR "<<< Read $ev bytes >>>\n";
			die "EOF" if $ev == 0;
			select undef,undef,undef, $throttle if $throttle;
			if (syswrite(S, $buf) != $ev) {
				die "write not $ev";
			}
		}
		if (vec($rout, $pin, 1)) {
			my $ev = sysread S, $buf, 65536;
			die $! unless defined $ev;
			print STDERR ">>> Read $ev bytes <<<\n";
			die "EOF" if $ev == 0;
			if (syswrite($asock, $buf) != $ev) {
				die "write not $ev";
			}
		}
	}
}
