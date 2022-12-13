#!/usr/bin/perl
# -*- mode: cperl; cperl-indent-level: 4 -*-

# copy this file as index-src.pl, replace your links below in $USER_LINKS
# (perhaps edit $HEAD and $TAIL), and then do $ perl index-src.pl

use 5.8.1;
use strict;
use warnings;

# note: all caps just to emphasize the parts most useful to be edited

my $USER_LINKS = <<'EOF';

http://row1col1.example.com/ row1 col1
http://row1col2.example.com/ row1 col2
http://row1col3.example.com/ row1 col3

http://row2col1.example.com/ row2 col1
http://row2col2.example.com/ row2 col2

http://row3col1.example.com/ row3 col1

EOF

my $HEAD = <<"EOF";
<!doctype html>
<html><head><title>index html quite useful</title>
<!--
	See $0 for editing content
-->
<style type="text/css">
body { background-color: #000; color #888; }
ul,li { margin-left: 0.5em; padding: 0; }
table { border-spacing: 30px 5px; }
td { font-size: 400%; }
a:link { color: #88f; }
a:visited { color: #88a; }
</style>
</head><body>
<table><tr>
EOF

my $TAIL = <<"EOF";
</tr></table>
</body></html>
EOF


open O, '>', 'index.wip' or die;

print O $HEAD;

$USER_LINKS =~ s/^\s*(.*?)\s*$/$1/s;

my @rows = split /\n\n+/, $USER_LINKS;

sub row($_) {
    foreach (split /\n/, $_[0]) {
	next if /^\s*#/;
	/^\s*(\S+)\s+(\S.*)/ or next;
	print O qq'<td><a href="$1" target=_blank>$2</a></td>\n';
    }
}
row (shift @rows);
foreach (@rows) { print O "</tr></table><table><tr>\n"; row $_; }

print O $TAIL;
close O;

if (-f 'index.html') {
   system qw/cmp -s index.wip index.html/;
   unlink('index.wip'), exit unless $?;

   my $d = 1;
   my $p = '';
   if (-s 'index.ar') {
       open A, '<', 'index.ar' or die $!;
       read A, $p, 64;
       # should check "!<arch>\n"
       $p =~ /index-(\d+)[.]html/;
       $d = $1 + 1;
   }
   print "Storing previous 'index.html' to an ar(5) archive\n";
   print ": use; ar -vt index.ar ;: to see contents there\n";
   open O, '>', 'index.ar.wip' or die $!;
   print O "!<arch>\n";
   my @s = stat 'index.html'; # $s[9] = mtime, $s[7] = size
   printf O "index-%04d.html/%-11d 0     0     100644  %-10d\140\012",
     $d, $s[9], $s[7];
   open I, '<', 'index.html' or die $!;
   my $l = read I, $_, $s[7];
   die unless $l == $s[7];
   close I;
   print O $_;
   print O "\n" if $s[7] & 1;
   if ($p) {
       print O substr($p, 8);
       print O $p while (read(A, $p, 1024*1024) > 0);
       close A
   }
   close O;
   rename 'index.ar.wip', 'index.ar';
}

# old backup code, one "level" left...
rename 'index.html',   'index.prev';
rename 'index.wip',    'index.html';
