#!/bin/sh
#
# $ addhost.sh -- add host to hosts-to-proxy for socksproxy to use $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#       Copyright (c) 2017 Tomi Ollila
#           All rights reserved
#
# Created: Thu 24 Aug 2017 22:13:39 EEST too
# Last modified: Mon 08 Apr 2019 20:03:21 +0300 too

case ${BASH_VERSION-} in *.*) set -o posix; shopt -s xpg_echo; esac
case ${ZSH_VERSION-} in *.*) emulate ksh; esac

set -euf
#set -x

LANG=C LC_ALL=C export LANG LC_ALL; unset LANGUAGE

die () { printf '%s\n' "$*"; exit 1; } >&2

test $# = 1 || die "Usage: $0 host"

cd $HOME/.local/share/mxtx
echo Working directory: $PWD/
if grep "$1" hosts-to-proxy 2>/dev/null
then die "'$1' is already in 'hosts-to-proxy file"
fi
# ipv4 only for now... grab first ipv4 address available
am=[1-9][0-9]*
host=`host -t A "$1" | sed "s/.* \($am\.$am\.$am\.$am\).*/\1/;tx;d;:x;q"`
test "$host" || die "Could not figure address to host '$1'"
echo "$1 $host" >> hosts-to-proxy
echo "Added  $1 $host  to hosts-to-proxy"
echo : Hint'; ' pkill -USR1 socksproxy

# Local variables:
# mode: shell-script
# sh-basic-offset: 8
# sh-indentation: 8
# tab-width: 8
# End:
# vi: set sw=8 ts=8
