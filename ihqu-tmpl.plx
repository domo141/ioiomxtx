#!/usr/bin/perl
# -*- mode: cperl; cperl-indent-level: 4 -*-

# copy this file as index-src.pl, replace your links below in $USER_LINKS
# and tune $TAIL (perhaps edit $HEAD), and then do $ perl index-src.pl

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

my $TAIL = <<"EOF";
</tr></table>

<script>
function klik(n) {
    var upx = document.querySelector('#upx' + n);
    var num = document.querySelector('#imp' + n);
    window.open(upx.innerText.trim() + num.value.trim(), "_blank")
    return false
}
</script>
<table><tr><td>
<hr/>
<form onsubmit="return klik('1')"
  <label id="upx1">http://sample-one/</label>
  <input id="imp1" size="8" required="true" value="" />
</form>
<hr/>
<form onsubmit="return klik('2')"
  <label id="upx2">https://sample-two/browse/</label>
  <input id="imp2" size="10" required="true" value="" />
</form>
<hr/>
</td><tr/></table>

</body></html>
EOF

my $HEAD = <<"EOF";
<!doctype html>
<html><head><title>index html quite useful</title>
<!--
	See $0 for editing content (USER_LINKS, HEAD & TAIL)
-->
<style type="text/css">
body { background-color: #000; color: #888; }
ul,li { margin-left: 0.5em; padding: 0; }
table { border-spacing: 30px 5px; }
td { font-size: 400%; color: #858; }
form { font-size: 40%; }
input { background-color: #000; color: #fff; }
a:link { color: #88f; }
a:visited { color: #88a; }
</style>
</head><body>
<table><tr>
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
   print("No Change\n"), unlink('index.wip'), exit unless $?;

   my $d = 1;
   my $p = '';
   if (-s 'index.ar') {
       open A, '<', 'index.ar' or die $!;
       read A, $p, 64;
       # should check "!<arch>\n"
       $d = ($p =~ /index-0*(\d+)[.]html/)? $1 + 1: 1;
   }
   print "Storing previous 'index.html' (#$d) to an ar(5) archive\n";
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
   print "...and renaming prev index.html as index.prev\n";
   rename 'index.html', 'index.prev';
}

rename 'index.wip', 'index.html';
