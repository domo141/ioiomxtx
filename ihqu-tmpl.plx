<!doctype html>
<html><head><title>index html quite useful</title>
<!--
  copy this template as index.html, replace your links at the end,
  perhaps edit this part before ...<body>, and then do $ perl -x index.html
-->
<style type="text/css">
body { background-color: #000; }
ul,li { margin-left: 0.5em; padding: 0; }
table { border-spacing: 30px 5px; }
td { font-size: 400%; }
a:link { color: #88f; }
a:visited { color: #88a; }
</style>
</head><body>
<table><tr>

<!-- to be replaced by the content at the end of this file -->

</tr></table>
<!--
>
#!perl
#line 22
use 5.10.1; use strict; use warnings;

open I, '<', $0 or die;
open O, '>', 'index.wip' or die;

while (<I>) { print O $_; last if /<\/head/; }
while (<I>) { last if /^#!perl/; }

my @tail; while (<I>) { push @tail, $_; last if /__E[N]D__/; }

read I, my $rest, 65536; $_ = $rest; s/\s*-[-]>.*//s; s/\s+//;
close I;
my @rows = split /\n\n+/;

print O "<table><tr>\n";
sub row($_) {
    foreach (split /\n/, $_[0]) {
	/^\s*(\S+)\s+(\S.*)/ or next;
	print O qq'<td><a href="$1" target=_blank>$2</a></td>\n';
    }
}
row (shift @rows);
foreach (@rows) { print O "</tr></table><table><tr>\n"; row $_; }

print O "</tr></table>\n", '<!', "--\n>\n#!perl\n", @tail, $rest;
close O;

link 'index.html.1', 'index.html.2';
link 'index.html',   'index.html.1';
rename 'index.wip', 'index.html';

__END__

http://row1col1.example.com/ row1 col1
http://row1col2.example.com/ row1 col2
http://row1col3.example.com/ row1 col3

http://row2col1.example.com/ row2 col1
http://row2col2.example.com/ row2 col2

http://row3col1.example.com/ row3 col1

-->
</body></html>
