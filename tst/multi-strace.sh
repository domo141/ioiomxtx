#!/bin/sh
#
# $ multi-strace.sh $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2021 Tomi Ollila
#	    All rights reserved
#
# Created: Thu 27 May 2021 21:36:22 EEST too
# Last modified: Fri 28 May 2021 19:25:00 +0300 too

case ${BASH_VERSION-} in *.*) set -o posix; shopt -s xpg_echo; esac
case ${ZSH_VERSION-} in *.*) emulate ksh; esac

set -euf  # hint: sh -x thisfile [args] to trace execution

LANG=C LC_ALL=C; export LANG LC_ALL; unset LANGUAGE

die () { printf '%s\n' "$@"; exit 1; } >&2

x_bg () { printf '+ %s\n' "$*" >&2; "$@" & }

test $# -gt 0 || die '' "Usage: ${0##*/} names (e.g. '.mxtx' 'rsync')" '' \
	"Notes: 'pgrep' behaves like 'grep -F'. May need root..." ''

for arg
do
	echo
	pgrep -a "$arg" | while read pid rest
	do
		trap wait 0
		echo $pid -- $rest
		x_bg strace -tt -o strace-$arg-$pid -p $pid
	done &
done
sleep 1
echo
wait


# Local variables:
# mode: shell-script
# sh-basic-offset: 8
# tab-width: 8
# End:
# vi: set sw=8 ts=8
