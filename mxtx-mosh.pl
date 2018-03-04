#!/usr/bin/perl
# -*- mode: cperl; cperl-indent-level: 4 -*-
# $ mxtx-mosh.pl $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2018 Tomi Ollila
#	    All rights reserved
#
# Created: Sat 03 Feb 2018 19:31:00 EET too
# Last modified: Sun 04 Mar 2018 19:52:10 +0200 too

use 5.8.1;
use strict;
use warnings;

use IO::Socket;

# the first version w/o much options

die "Usage: $0 link [command [args]]\n" unless @ARGV;

# mxtx-mosh(1) differs from mosh(1) when executing mosh-client as:
# - escape key is set to '~' (works w/ non-us keyboards, and like openssh)
# - terminal window doesn't get [mosh] prefix
# - doesn't "init" terminal (--no-init in mosh(1) options)

sub get_dgt_port($)
{
    socket my $s, AF_UNIX, SOCK_DGRAM, 0 or die 'socket: ', $!;
    # currently only linux abstract socket space -- portability TBD
    my $to = pack('Sxa*', AF_UNIX, "/tmp/user-1000/mxtx-dgt,$_[0]");
    setsockopt $s, SOL_SOCKET, SO_REUSEADDR, 1;
    # autobind (see man 7 unix)
    bind $s, pack('S', AF_UNIX) or die 'bind: ', $!;
    unless (send $s, "\0\0", 0, $to) {
	return 0 if $!{ECONNREFUSED};
	die 'send: ', $!;
    }
    alarm 2;
    my $buf; my $who = recv $s, $buf, 4, 0;
    alarm 0;
    my ($ver, $port) = unpack('nn', $buf);
    #print "$ver, $port\n";
    return $port;
}

my $mxtxdir = $ENV{HOME} . '/.local/share/mxtx';

my $dgt_port = get_dgt_port $ARGV[0];
if ($dgt_port == 0) {
    system $mxtxdir . '/mxtx-dgramtunneld', $ARGV[0];
    $dgt_port = get_dgt_port $ARGV[0];
    die if $dgt_port == 0
}

my $term_colors = qx/mosh-client -c/; die unless defined $term_colors;
chomp $term_colors;

# adapted from mosh(1)
my @lc_opts;
foreach (qw/LANG LANGUAGE LC_CTYPE LC_NUMERIC LC_TIME LC_COLLATE LC_MONETARY
	    LC_MESSAGES LC_PAPER LC_NAME LC_ADDRESS LC_TELEPHONE LC_MEASUREMENT
	    LC_IDENTIFICATION LC_ALL/) {
    my $v = $ENV{$_} || next;
    push @lc_opts, '-l', $_ . '=' . $v;
}

my $link = $ARGV[0]; if (@ARGV > 1) { $ARGV[0] = '--'; } else { shift; }

open P, '-|', qw/mxtx-rsh -t/, $link,
  qw/. mosh-server new -c/, $term_colors, qw/-i 127.0.0.1/, @lc_opts, @ARGV
  or die $!;

my ($port, $key);
while (<P>) {
    next if /^\s*$/;
    die "Unexpected mosh-server reply '$_'\n" unless /MOSH CONNECT (\d+) (\S+)/;
    ($port, $key) = ($1, $2);
    last;
}
0 while (sysread (P, $_, 512) > 0); # strace -ff -o x ... told 341 bytes read
die "No reply from mosh-server (no mosh-server?)\n" unless defined $port;
close P or die $!;

$ENV{ 'MOSH_KEY' } = $key;
#$ENV{ 'MOSH_PREDICTION_DISPLAY' } = $predict;
$ENV{ 'MOSH_NO_TERM_INIT' } = '1';
$ENV{ 'MOSH_ESCAPE_KEY' } = '~';
$ENV{ 'MOSH_TITLE_NOPREFIX' } = 't';

$ENV{ 'MXTX_MOSH_PORT' } = $port;

my $ldpl = $ENV{LD_PRELOAD};
if ($ldpl) {
    $ENV{LD_PRELOAD} = $mxtxdir . '/ldpreload-moshclienthax.so:' . $ldpl;
}
else {
    $ENV{LD_PRELOAD} = $mxtxdir . '/ldpreload-moshclienthax.so';
}

exec qw/mosh-client 127.0.0.1/, $dgt_port;
