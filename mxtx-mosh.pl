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
# Last modified: Tue 27 Feb 2018 21:45:57 +0200 too

use 5.8.1;
use strict;
use warnings;

use IO::Socket;

# the first version w/o much options

die "Usage: $0 link\n" unless @ARGV == 1;

# document differences to mosh(1) defaults

sub get_dgt_port($) {
    socket my $s, AF_UNIX, SOCK_DGRAM, 0 or die 'socket: ', $!;
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

my $dgt_port = get_dgt_port $ARGV[0];
if ($dgt_port == 0) {
    system $ENV{HOME} . '/.local/share/mxtx/mxtx-dgramtunneld', $ARGV[0];
    $dgt_port = get_dgt_port $ARGV[0];
    die if $dgt_port == 0
}

my $term_colors = qx/mosh-client -c/; die unless defined $term_colors;
chomp $term_colors;
#print $term_colors, "\n";

# adapted from mosh(1)
my @lc_opts;
foreach (qw/LANG LANGUAGE LC_CTYPE LC_NUMERIC LC_TIME LC_COLLATE LC_MONETARY
	    LC_MESSAGES LC_PAPER LC_NAME LC_ADDRESS LC_TELEPHONE LC_MEASUREMENT
	    LC_IDENTIFICATION LC_ALL/) {
    my $v = $ENV{$_} || next;
    push @lc_opts, '-l', $_ . '=' . $v;
}
#print " @lc_opts\n";

# remote from here and down to m/^__E[N]D__/ for local mosh testing...

socket S, AF_UNIX, SOCK_STREAM, 0 or die 'socket: ', $!;
my $to = pack('Sxa*', AF_UNIX, "/tmp/user-1000/mxtx,$ARGV[0]");
connect S, $to or die $!;

my $srv_cmd = join "\0", ( qw/mosh-server new -c/, $term_colors,
			   qw/-i 127.0.0.1/, @lc_opts, "" );
syswrite S, pack('xxn', length($srv_cmd)) . $srv_cmd;

my ($port, $key);
while (<S>) {
    next if /^\s*$/;
    die "Unexpected mosh-server reply '$_'\n" unless /MOSH CONNECT (\d+) (\S+)/;
    ($port, $key) = ($1, $2);
    last;
}
die "No reply from mosh-server (no mosh-server?)\n" unless defined $port;
print $port, $key, "\n";
close S or die $!;

$ENV{ 'MOSH_KEY' } = $key;
#$ENV{ 'MOSH_PREDICTION_DISPLAY' } = $predict;
$ENV{ 'MOSH_NO_TERM_INIT' } = '1';
$ENV{ 'MOSH_ESCAPE_KEY' } = '~';
$ENV{ 'MOSH_TITLE_NOPREFIX' } = 't';

$ENV{ 'MXTX_MOSH_PORT' } = $port;

my $ldpl = $ENV{LD_PRELOAD};
if ($ldpl) {
    $ENV{LD_PRELOAD} = $ENV{HOME} . '/.local/share/mxtx/ldpreload-moshclienthax.so:' . $ldpl;
}
else {
    $ENV{LD_PRELOAD} = $ENV{HOME} . '/.local/share/mxtx/ldpreload-moshclienthax.so'
}

exec qw/mosh-client 127.0.0.1/, $dgt_port;

__END__

open P, '-|', qw/strace -f -o trace-s mosh-server new -c/, $term_colors,
  qw/-i 127.0.0.1/, @lc_opts;
my ($port, $key);
while (<P>) {
    ($port, $key) = ($1, $2), last if /MOSH CONNECT *(\d+) (\S+)/;
}
print $port, $key, "\n";
#close P or die $!;

$ENV{ 'MOSH_KEY' } = $key;
#$ENV{ 'MOSH_PREDICTION_DISPLAY' } = $predict;
$ENV{ 'MOSH_NO_TERM_INIT' } = '1';
$ENV{ 'MOSH_ESCAPE_KEY' } = '~';
$ENV{ 'MOSH_TITLE_NOPREFIX' } = 't';

$ENV{ 'MXTX_MOSH_PORT' } = $port;
$ENV{ 'LD_PRELOAD' } = './ldpreload-moshclienthax.so';

exec qw/strace -o trace-c mosh-client 127.0.0.1/, $port;
