#
# $ GNUmakefile $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2017 Tomi Ollila
#	    All rights reserved
#
# Created: Wed 16 Aug 2017 21:09:05 EEST too
# Last modified: Fri 29 Dec 2017 19:46:53 +0200 too

SHELL = /bin/sh

BIN := mxtx mxtx-io mxtx-rshd mxtx-rsh mxtx-socksproxy
BIN += ldpreload-i2ubind.so ldpreload-i2uconnect5.so ldpreload-vsfa.so

# Note: all source file dependencies may not be listed (usually not a problem)
#       do `make clean all` at the end of development session...

all: $(BIN)

mxtx: src/mxtx.c libmxtx.a $(wildcard .git/index)
	sh $<

mxtx-lib.o: src/mxtx-lib.c src/mxtx-lib.h
	sh $<

libmxtx.a: src/mxtx-lib.c src/mxtx-lib.h
	perl -x $<

mxtx-io: src/mxtx-io.c libmxtx.a
	sh $<

mxtx-socksproxy: src/mxtx-socksproxy.c libmxtx.a
	sh $<

mxtx-rshd: src/mxtx-rshd.c src/lpktread.ch libmxtx.a
	sh $<

mxtx-rsh: src/mxtx-rsh.c src/lpktread.ch libmxtx.a
	sh $<


ldpreload-i2ubind.so ldpreload-i2uconnect5.so: src/ldpreload-i2usocket.c
	sh $<

ldpreload-vsfa.so: src/ldpreload-vsfa.c
	sh $<


YES ?= NO
install: $(BIN)
	sed '1,/^$@.sh:/d;/^#.#eos/q' GNUmakefile | /bin/sh -s YES=$(YES) $(BIN)

install.sh:
	test -n "$1" || exit 1 # embedded shell script; not to be made directly
	die () { exit 1; }
	set -euf
	export LC_ALL=C LANG=C
	echo
	if test "$1" = YES=YES
	then	mmkdir () { test -d "$1" || mkdir -vp "$1"; }
		mmkdir $HOME/bin/
		mmkdir $HOME/.local/share/mxtx/
		shift
		yesyes=true
		echo strip "$@" >&2; strip "$@"
		xcp () { test ! -d "$3" || set -- "$1" 2 "${3%/}/${1##*/}"
			 cmp "$1" "$3" >/dev/null 2>&1 &&
				echo "$1" and "$3" identical ||
				cp -f -v "$1" "$3"; }
		echo Copying:
	else
		yesyes=false
		xcp () { echo '' "$@"; }
		echo Would install:
	fi
	xcp mxtx as $HOME/bin/.mxtx
	xcp ioio.pl to $HOME/bin/
	xcp mxtx.el to $HOME/.local/share/mxtx/
	xcp addhost.sh to $HOME/.local/share/mxtx/
	xcp mxtx-rshd to $HOME/.local/share/mxtx/
	xcp mxtx-io to $HOME/bin/
	xcp mxtx-rsh to $HOME/bin/
	xcp mxtx-cp.sh as $HOME/bin/mxtx-cp
	xcp mxtx-apu.sh to $HOME/bin/
	xcp mxtx-socksproxy as $HOME/.local/share/mxtx/socksproxy
	xcp ldpreload-i2ubind.so to $HOME/.local/share/mxtx/
	xcp ldpreload-i2uconnect5.so to $HOME/.local/share/mxtx/
	xcp ldpreload-vsfa.so to $HOME/.local/share/mxtx/
	echo
	$yesyes || {
		echo Enter '' make install YES=YES '' to do so.
		echo; exit
	}
#	#eos
	exit 1 # not reached

# possibly partial uninstall
unin:
	cd "$$HOME" && exec rm -f \
		.local/share/mxtx/mxtx[.-]* \
		.local/share/mxtx/ldpreload-* \
		.local/share/mxtx/addhost.sh \
		.local/share/mxtx/socksproxy \
		./bin/.mxtx bin/mxtx-* bin/ioio.pl

.PHONY: git-head
git-head:
	sed '1,/^$@.sh:/d;/^#.#eos/q' GNUmakefile | /bin/sh -s $@

git-head.sh:
	test -n "$1" || exit 1 # embedded shell script; not to be made directly
	die () { printf %s\\n "$*" >&2; exit 1; }
	set -euf
#	#test -z "`exec git status --porcelain -uno`" || die changes in working copy
	fmt='format:commit %H  %h%ntree   %T  %t%n'
	fmt=$fmt'Author:     %an <%ae>%nAuthorDate: %aD%n'
	fmt=$fmt'Commit:     %cn <%ce>%nCommitDate: %cD%n%n% B'
	git log --pretty="$fmt" --name-status -1 > "$1"
#	#eos
	exit 1 # not reached

.PHONY: a
a:	#git-head
	@test -z "`exec git status --porcelain -uno`" || \
		echo Note: not taking changes from \'dirty\' working tree!
	@set -euf; set x `exec git log -1 --pretty='%h %ci'`; s=$$3-g$$2; \
	set -x; exec git archive --prefix=mxtx-$$s/ -o mxtx-$$s.tar.gz HEAD

# convenience helper to update mxtx over an mxtx link
.PHONY: remote-update
remote-update:
	sed '1,/^$@.sh:/d;/^#.#eos/q' GNUmakefile | /bin/sh -s "$(MAKE)" "$D"

remote-update.sh:
	test -n "$1" || exit 1 # embedded shell script; not to be made directly
	die () { printf %s\\n "$*" >&2; exit 1; }
	set -euf
	export LC_ALL=C LANG=C
	test -n "$2" || die Usage':' $1 install D=dest
	make=$1 D=$2
	set x `exec git log -1 --pretty='%h %ci'`; s=$3-g$2;
	die () { exit 1; }
	set -x
	test -f mxtx-$s.tar.gz || die mxtx-$s.tar.gz not made "($make a)"
	mxtx-rsh -n "$D" . /bin/true
	set +e
	mxtx-rsh    "$D" . tar zxvf - < mxtx-$s.tar.gz
	mxtx-rsh -n "$D" cd mxtx-$s '&&' make install YES=YES
	mxtx-rsh -n "$D" . rm -rf mxtx-$s
#	#eos
	exit 1 # not reached

clean:
	rm -rf $(BIN) mxtx-lib.o libmxtx.a git-head *~ src/*~ _tmp

.SUFFIXES:
MAKEFLAGS += --no-builtin-rules --warn-undefined-variables

# Local variables:
# mode: makefile
# End:
