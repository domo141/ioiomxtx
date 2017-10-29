#!/bin/sh
#
# $ termtower-tmpl.sh $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
# This file has been put into the public domain.
#
# Created: Tue 05 Sep 2017 22:44:11 EEST too
# Last modified: Thu 07 Sep 2017 07:43:24 +0300 too

# move current graphical terminal window to a position and open
# two more terminal windows below that. copy and add features.

case ${BASH_VERSION-} in *.*) set -o posix; shopt -s xpg_echo; esac
case ${ZSH_VERSION-} in *.*) emulate ksh; esac

set -euf
#set -x

decor_height=20
font_height=15
rows=20

distance=$(( decor_height + rows * font_height ))

saved_IFS=$IFS; readonly saved_IFS

warn () { printf '%s\n' "$*"; } >&2
die () { printf '%s\n' "$*"; exit 1; } >&2

x () { printf '+ %s\n' "$*" >&2; "$@"; }
x_env () { printf '+ %s\n' "$*" >&2; env "$@"; }
x_eval () { printf '+ %s\n' "$*" >&2; eval "$*"; }
x_exec () { printf '+ %s\n' "$*" >&2; exec "$@"; die "exec '$*' failed"; }

find_term () {
        for term; do command -v $term >/dev/null && return 0 || :; done
        die 'No known X terminal program (add yours ?)'
}

if test "${DISPLAY-}"
then
        find_term urxvt xterm
        newterm () { x=$1 y=$2; shift 2; $term -g 80x$rows+$x+$y "$@" & }

elif case `exec uname` in *CYGWIN*) true ;; *) false ;; esac
then
        command -v mintty >/dev/null ||
        die "'mintty' not available (add yours?)"
        term=mintty
        newterm () { x=$1 y=$2; shift 2; $term -s 80,$rows -p $x,$y "$@" & }
else
        die 'No known graphical environment (add yours ?)'
fi

printf "\033[8;$rows;80t" # resize to 80x$rows
printf '\033[3;77;20t' # move to position 77x20

newterm 77 $(( 20 + distance ))
newterm 77 $(( 20 + distance * 2 ))

# Local variables:
# mode: shell-script
# sh-basic-offset: 8
# sh-indentation: 8
# tab-width: 8
# End:
# vi: set sw=8 ts=8
