#!/usr/bin/env perl
# -*- mode: cperl; cperl-indent-level: 4 -*-
# $ mxtx-gitxxdiff.pl $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2017 Tomi Ollila
#	    All rights reserved
#
# Created: Thu 07 Sep 2017 16:52:39 EEST too
# Last modified: Sat 30 Dec 2017 17:40:48 +0200 too

### code-too-remote ###
use 5.8.1;
use strict;
use warnings;
### end ###

use IO::Socket::UNIX;

unless (@ARGV) {
    my $n = (ord $0 == 47)? ( ($0 =~ /([^\/]+)$/), $1 ): $0;

    die "\nUsage: $n [xxdiff options and --] link:gitpath [git diff opts]\n\n";
}

my @xxdiff_opts;
if (ord $ARGV[0] == 45) { # '-'
    do {
	$_ = shift @ARGV;
	push @xxdiff_opts, $_;
    } while ($_ ne '--' and @ARGV);
    pop @xxdiff_opts;
}
die "Trailing '--' after xxdiff options missing.\n" unless @ARGV;

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
print $s "remote_main\n";
print $s "__END__\n";

sysread $s, $_, 128;
die "error in sending code to $link\n" unless /^code received: pid (\d+)$/;
my $rpid = $1;

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

sub run_xxdiff($$$$)
{
    #print join(' -- ', @_), "\n";

    my @xo = @xxdiff_opts;

    #push @xo, '--indicate-input-processed';
    push @xo, '--title1', $_[1], '--title2', $_[3];

    system 'xxdiff', @xo, '--', $_[0], $_[2];
    #open P, '-|', 'xxdiff', @xo, '--', $_[0], $_[2];
    #while (<P>) { unlink 'tmp1', 'tmp2'; }
    #close P;
    exit 0 if $? == 2; # ctrl-c (to xxdiff) makes it exit w/ 2
}

my $ldpl = $ENV{LD_PRELOAD};
if ($ldpl) {
    $ENV{LD_PRELOAD} = $ENV{HOME} . '/.local/share/mxtx/ldpreload-vsfa.so:' . $ldpl;
}
else {
    $ENV{LD_PRELOAD} = $ENV{HOME} . '/.local/share/mxtx/ldpreload-vsfa.so'
}

my $lp = $path? "$link:$path/": "$link:";

my $pnl = '';
foreach (@lines) {
    unless (/:\d+(\d\d\d)\s\d+(\d\d\d)\s(\w+)[.]+\s(\w+)[.]+\s(\w)\s+(.*)/) {
	print '.'; $pnl = "\n"; next;
    }
    print "$pnl$1  $2  $3  $4  $5  $6\n"; $pnl = '';

    if ($5 ne 'M' and $5 ne 'A' and $5 ne 'D') { print "skip\n"; next; }
    my $lf = $3;
    my $rf = ($5 eq 'D' or $4 ne '000000000000')? $4: './'.$6;

    syswrite $s, "$lf $rf\n";
    $_ = <$s>;
    die unless defined $_;
    chomp;
    ($lf, $rf) = split ' ', $_, 2;
    $lf = $lp . $lf unless ord $lf == 47; # '/'dev/null
    $rf = $lp . $rf unless ord $rf == 47; # '/'dev/null
    if ($5 eq 'M') {
	run_xxdiff $lf, "a:$1:$6", $rf, "b:$2:$6";
	next;
    }
    if ($5 eq 'A') {
	run_xxdiff '/dev/null', 'added', $rf, "b:$2:$6";
	next;
    }
    # 'D'
    run_xxdiff $lf, "a:$1:$6", '/dev/null', 'deleted';
}

__END__

### code-too-remote ###

# note: `die` output goes to mxtx -s stderr

my @tmps;
END { unlink @tmps if @tmps; }
sub openO($)
{
    my $f = ".git/tmp-$$.$_[0]";
    push @tmps, $f;
    open O, '>', $f or die $!;
    return $f;
}

sub write_message($$)
{
    my $f = openO $_[0];
    print O "$_[0]: $_[1]";
    close O;
    $_[0] = $f;
}
sub too_large($$)
{
    write_message $_[0], "$_[1]: larger than 524288 bytes";
}
sub binary_file($)
{
    write_message $_[0], "binary content";
}

sub write_content($$)
{
    my $buf;
    my $len = sysread F, $buf, 8192;
    die $! if ($len <= 0);
    if (index($buf, "\0") >= 0) {
	binary_file $_[0];
	return;
    }
    my $f = openO $_[0];
    syswrite O, $buf;
    my $size = $_[1] - $len;
    while ($size > 0) {
	$len = sysread F, $buf, ($size > 65536)? 65536: $size;
	die $! if ($len <= 0);
	syswrite O, $buf;
	$size -= $len;
    }
    close O;
    $_[0] = $f;
}

sub remote_main ()
{
    $| = 1;
    syswrite STDOUT, "code received: pid $$";
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
	next if /^a:$/; # lazy in syswrite around line 71
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
    my @lines; push @lines, $_ while (<P>); #warn @lines;
    close P;
    exit if $?;
    push @lines, "0\n";
    syswrite STDOUT, join('', @lines);
    undef @lines;
    while (<STDIN>) {
	unlink(@tmps), @tmps = () if @tmps;
	chomp;
	my @blobs = split ' ', $_, 2;
	foreach (@blobs) {
	    #warn "-- $_";
	    if ($_ =~ s|^[.]\/||) {
		my $size = -s $_;
		die $! unless defined $size;
		$_ = '/dev/null', next if $size == 0;
		too_large($_, $size), next if $size > 524288;
		binary_file($_), next if -B $_;
	    }
	    else {
		$_ = '/dev/null', next if /^000000000000/;
		my $size = `git cat-file -s $_`; chomp $size;
		too_large($_, $size), next if $size > 524288;
		$_ = '/dev/null', next if $size == 0;
		open F, '-|', qw/git cat-file -p/, $_;
		write_content($_, $size);
		close F;
	    }
	}
	# assignments to $_ replaces @blobs list items
	syswrite STDOUT, "@blobs\n";
    }
}
### end ###
