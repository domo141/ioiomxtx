#!/usr/bin/perl
# -*- mode: cperl; cperl-indent-level: 4 -*-
# $ mxtx-gitxxdiff.pl $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2017 Tomi Ollila
#	    All rights reserved
#
# Created: Thu 07 Sep 2017 16:52:39 EEST too
# Last modified: Wed 27 Sep 2017 07:00:23 +0300 too

### code-too-remote ###
use 5.8.1;
use strict;
use warnings;

my $rorl;
### end ###
$rorl = 'local';

use IO::Socket::UNIX;

my $xxdiff_opts = (@ARGV && ord $ARGV[0] == 45)? shift @ARGV: '';

unless (@ARGV) {
    my $n = (ord $0 == 47)? ( ($0 =~ /([^\/]+)$/), $1 ): $0;

    die "\nUsage: [env] [XXDIFF_OPTS=...] $n link:gitpath [git diff opts]\n\n";
}

my ($link, $path) = split /:/, (shift @ARGV), 2;
$path = '' unless defined $path;


my $s = IO::Socket::UNIX->new(Type => SOCK_STREAM,
			      Peer => "\0/tmp/user-" . $< . "/mxtx," . $link);
unless ($s) {
    die "Cannot connect to $link: $!\n";
}

syswrite $s, "\0\0\0\005perl\0";
#syswrite $s, "\0\0\0\014strace\0perl\0";

# send code to remote perl #
open I, '<', $0 or die "opening '$0':";
while (<I>) {
    if (/code[-]too[-]remote/) {
	print $s '# line ', $. + 1, "\n";
	while (<I>) {
	    last if /^### end ###\s/;
	    print $s $_;
	}
    }
}
close I;
print $s "\$rorl = '$link';\n";
print $s "remote_main\n";
print $s "__END__\n";

sysread $s, $_, 128;
die "error in sending code to $link\n" unless $_ eq 'code received.';

syswrite $s, "-\n-\n::path:: $path\na:" . join("\na:", @ARGV) . "\n\n";

sysread $s, $_, 1; sysread($s, $_, 128), die "$_" unless $_ eq '0';

my @lines;
while (<$s>) {
    last unless /^:/;
    chomp;
    push @lines, $_;
}

unless ($_ eq "0\n") {
    warn $_;
    alarm(2);
    warn $_ while (<$s>);
    exit 1;
}

die "$_" unless $_ eq "0\n";

sub read_file($)
{
    my $buf;
    sysread $s, $buf, 8;
    die "$buf\n" unless $buf =~ /^size/;
    my $size = unpack 'x4N', $buf;

    open O, '>', $_[0] or die;
    while ($size > 0) {
	my $l = sysread $s, $buf, $size;
	die $! if ($l <= 0);
	syswrite O, $buf;
	$size -= $l;
    }
    close O;
}

# to be done something smarter with these...
eval "END { unlink 'tmp1', 'tmp2' }";

sub run_xxdiff($$)
{
    my @xo = (defined $ENV{XXDIFF_OPTS})? (split / /, $ENV{XXDIFF_OPTS}): ();
    my ($fa, $fb);

    push @xo, '--indicate-input-processed', '--title1';
    if ($_[0] eq '/dev/null') {
	   $fa = '/dev/null'; push @xo, 'added'; }
    else { $fa = 'tmp1'; push @xo, $_[0]; }

     push @xo, '--title2';

    if ($_[1] eq '/dev/null') {
	   $fb = '/dev/null'; push @xo, 'deleted'; }
    else { $fb = 'tmp2'; push @xo, $_[1]; }
    open P, '-|', 'xxdiff', @xo, $fa, $fb;
    while (<P>) { unlink 'tmp1', 'tmp2'; }
    close P;
    exit 0 if $? == 2; # ctrl-c (to xxdiff) makes it exit w/ 2
}

foreach (@lines) {
    /:\d+(\d\d\d)\s\d+(\d\d\d)\s(\w+)[.]+\s(\w+)[.]+\s(\w)\s+(.*)/ or next;
    print "$1  $2  $3  $4  $5  $6\n";
    my $buf;
    if ($5 eq 'M') {
	$buf = $3 . ' ' . (($4 eq '000000000000')? "./$6": $4);
    }
    elsif ($5 eq 'A') {
	$buf = ($4 eq '000000000000')? "./$6": $4;
    }
    elsif ($5 eq 'D') {
	$buf = $3;
    }
    else { print "skip\n"; next; }

    syswrite $s, $buf . "\n";
    read_file (($5 ne 'A')? 'tmp1': 'tmp2');
    if ($5 eq 'M') {
	read_file 'tmp2';
	run_xxdiff "a:$1:$6", "b:$2:$6";
	next;
    }
    if ($5 eq 'A') {
	run_xxdiff '/dev/null', "b:$2:$6";
	next;
    }
    # 'D'
    run_xxdiff "a:$1:$6", '/dev/null';
}

__END__

### code-too-remote ###
# note: these messages go to `mxtx -s` stderr
sub pdie (@) {
    syswrite(STDERR, "$rorl: $!\n"), exit(0) unless @_;
    syswrite(STDERR, "$rorl: @_ $!\n"), exit(0) if $_[$#_] =~ /:$/;
    syswrite(STDERR, "$rorl: @_\n");
    exit;
}

sub too_large($$)
{
    my $msg = "$_[0]: $_[1]: larger than 524288 bytes";
    syswrite STDOUT, pack('A4N', 'size', length $msg) . $msg;
}

sub write_content($$)
{
    my $size = $_[0];
    my $buf;
    my $len = sysread F, $buf, 8192;
    pdie if ($len <= 0);
    if (index($buf, "\0") >= 0) {
	my $msg = "$_[1]: binary content";
	syswrite STDOUT, pack('A4N', 'size', length $msg) . $msg;
	return;
    }
    syswrite STDOUT, pack('A4N', 'size', $size);
    syswrite STDOUT, $buf;
    $size -= $len;
    while ($size > 0) {
	$len = sysread F, $buf, ($size > 65536)? 65536: $size;
	pdie if ($len <= 0);
	syswrite STDOUT, $buf;
	$size -= $len;
    }
}

sub write_empty() { syswrite STDOUT, pack('A4N', 'size', 0); }

sub remote_main ()
{
    $| = 1;
    syswrite STDOUT, 'code received.';
    while (<STDIN>) {
	if (/::path:: (.*)/) {
	    if ($1 ne '.' and $1 ne '') {
		unless (chdir $1) {
		    syswrite STDOUT, "failed to cd $1: $!\n";
		    exit
		}
	    }
	    last
	}
    }
    syswrite STDOUT, '0';
    my @args;
    while (<STDIN>) {
	#warn $_;
	next if /^a:$/; # lazy in syswrite around line 65
	last unless s/^a://;
	chomp;
	push @args, $_;
    }
    open(OLD, ">&", \*STDERR) or die "Can't dup STDOUT: $!";
    open(STDERR, ">&", \*STDOUT) or die "Can't dup STDERR: $!";
    open P, '-|', qw/git --no-pager diff --raw --abbrev=12/, @args;
    open(STDERR, ">&", \*OLD) or die "Can't dup OLDOUT: $!";
    close OLD;
    undef @args;
    my @lines; push @lines, $_ while (<P>); # warn @lines;
    close P;
    exit if $?;
    push @lines, "0\n";
    syswrite STDOUT, join('', @lines);
    undef @lines;
    while (<STDIN>) {
	my @blobs = split ' ';
	foreach (@blobs) {
	    #warn "-- $_";
	    if ($_ =~ /^[.]\//) {
		my $size = -s $_;
		pdie unless defined $size;
		too_large($_, $size), next if $size > 524288;
		write_empty, next if $size == 0;
		open F, '<', $_ or pdie;
		write_content $size, $_;
		close F;
	    }
	    else {
		#write_empty, next if $_ =~ /^000000000000/;
		my $size = `git cat-file -s $_`; chomp $size;
		too_large($_, $size), next if $size > 524288;
		write_empty, next if $size == 0;
		open F, '-|', qw/git cat-file -p/, $_;
		write_content $size, $_;
		close F;
	    }
	}
    }
}
### end ###
