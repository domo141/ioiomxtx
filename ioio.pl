#!/usr/bin/env perl
# -*- mode: cperl; cperl-indent-level: 4 -*-
# $ ioio.pl $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2017 Tomi Ollila
#	    All rights reserved
#
# Created: Thu 10 Aug 2017 23:07:28 EEST too
# Last modified: Tue 12 Sep 2017 23:31:04 +0300 too

use 5.8.1;
use strict;
use warnings;

use Socket;

my ($sep, $ep);
if (@ARGV && $ARGV[0] =~ /(^|\/)[.]{0,2}$/) {
    $sep = shift @ARGV; $ep = 'other ';
} else {
    $sep = '///'; $ep = '';
}

die "\nUsage: $0 [<sep>] command1 [args] <sep> command2 [args]\n
<sep>: by default '///', buf if first argument ends with '/', '/.', '/..'
or is '..', '.' or '', it is used as a separator. this alternate separator
cannot point to a file (is directory or empty string). it can be used to
e.g. nest ioio's (also) in the first command.\n\n" unless @ARGV;

my (@cmd1, @cmd2);
{
    my $ac = 0;
    foreach (@ARGV) {
	if ($_ eq $sep) {
	    die "'$sep' also as second arg\n" if $ac == 0;
	    die "'$sep' as last arg\n" if $ac == $#ARGV;
	    last;
	}
	$ac++
    }
    die "Cannot find $ep'$sep' on command line\n" if $ac == @ARGV;
    @cmd1 = splice @ARGV, 0, $ac;
    shift;
    @cmd2 = @ARGV;
}
#print "1: @cmd1\n"; print "2: @cmd2\n"; exit;

socketpair(C, P, AF_UNIX, SOCK_STREAM, PF_UNSPEC) or die "socketpair: $!";

my $pid;
if ($pid = fork) {
    close C;
    open STDIN, "<&P" or die;
    open STDOUT, "<&P" or die;
    close P;
    exec @cmd1;
    die "exec: $!\n"
}
close P;
{
    my $data;
    die "fail!?" unless defined recv C, $data, 1, MSG_PEEK;
    die "eof?" unless length $data;
}
open STDIN, "<&C" or die;
open STDOUT, "<&C" or die;
close C;
# exec w/o fork to drop this perl -- this may result some mess in cli
# after 1st process exits (and shell prompt appears) and 2nd process
# writes something there (e.g Connection reset by peer).
exec @cmd2;
die "exec: $!\n"
