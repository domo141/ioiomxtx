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
# Last modified: Fri 29 Dec 2017 23:28:37 +0200 too

### code-too-remote ###
use 5.8.1;
use strict;
use warnings;

my $rorl;
### end ###
$rorl = 'local';

use IO::Socket::UNIX;

unless (@ARGV) {
    my $n = (ord $0 == 47)? ( ($0 =~ /([^\/]+)$/), $1 ): $0;

    die "\nUsage: $n [xxdiff options and --] link:gitpath [git diff opts]\n\n";
}

my @xxdiff_opts;
if (ord $ARGV[0] == 45) { # -
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
print $s "\$rorl = '$link';\n";
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

    #push @xo, '--indicate-input-processed', '--title1';
    push @xo, '--title1';
    if ($_[0] eq '/dev/null') { push @xo, 'added'; } else { push @xo, $_[1]; }
    push @xo, '--title2';
    if ($_[2] eq '/dev/null') { push @xo, 'deleted'; } else { push @xo, $_[3]; }

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

foreach (@lines) {
    /:\d+(\d\d\d)\s\d+(\d\d\d)\s(\w+)[.]+\s(\w+)[.]+\s(\w)\s+(.*)/ or next;
    print "$1  $2  $3  $4  $5  $6\n";
    my $lf = '';
    my $rf = '';
    if ($5 eq 'M') {
	$lf = $3;
	$rf = $4 if $4 ne '000000000000';
    }
    elsif ($5 eq 'A') {
	$rf = $4 if $4 ne '000000000000';
    }
    elsif ($5 eq 'D') {
	$lf = $3;
    }
    else { print "skip\n"; next; }

    syswrite $s, "$lf $rf\n";
    my $t; sysread $s, $t, 1;
    die unless $t;
    if ($5 eq 'M') {
	$lf = $lp . ".git/tmp-$rpid.$lf";
	if ($rf) { $lf = $lp . ".git/tmp-$rpid.$rf"; }
	else { $rf = $lp . $6; }
	run_xxdiff $lf, "a:$1:$6", $rf, "b:$2:$6";
	next;
    }
    if ($5 eq 'A') {
	if ($rf) { $lf = $lp . ".git/tmp-$rpid.$rf"; }
	else { $rf = $lp . $6; }
	run_xxdiff '/dev/null', '/dev/null', $rf, "b:$2:$6";
	next;
    }
    # 'D'
    $lf = $lp . ".git/tmp-$rpid.$lf";
    run_xxdiff $lf, "a:$1:$6", '/dev/null', '/dev/null';
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

my @tmps;
END { unlink @tmps if @tmps; }
sub openO($)
{
    my $f = ".git/tmp-$$.$_[0]";
    push @tmps, $f;
    open O, '>', $f or pdie;
}

sub too_large($$)
{
    openO $_[0];
    print O "$_[0]: $_[1]: larger than 524288 bytes";
    close O;
}

sub write_content($$)
{
    openO $_[0];
    my $size = $_[1];
    my $buf;
    my $len = sysread F, $buf, 8192;
    pdie if ($len <= 0);
    if (index($buf, "\0") >= 0) {
	print O "$_[1]: binary content";
	close O;
	return;
    }
    syswrite O, $buf;
    $size -= $len;
    while ($size > 0) {
	$len = sysread F, $buf, ($size > 65536)? 65536: $size;
	pdie if ($len <= 0);
	syswrite O, $buf;
	$size -= $len;
    }
    close O;
}

sub write_empty($)
{
    openO $_[0];
    close O;
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
	unlink @tmps if @tmps;
	my @blobs = split ' ';
	foreach (@blobs) {
	    #warn "-- $_";
	    my $size = `git cat-file -s $_`; chomp $size;
	    too_large($_, $size), next if $size > 524288;
	    write_empty($_), next if $size == 0;
	    open F, '-|', qw/git cat-file -p/, $_;
	    write_content($_, $size);
	    close F;
	}
	syswrite STDOUT, 1;
    }
}
### end ###
