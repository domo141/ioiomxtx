#!/bin/sh
# -*- mode: shell-script; sh-basic-offset: 8; tab-width: 8 -*-
# $ test-strace-wrapper.sh $

# copy this to ~/.local/share/mxtx/ to a name of a program you
# want to strace (before that rename the prg w/ -st suffix).
# the ':::' as first argument (look code below) does that, too...

# add options at will

set -euf

# installer ( e.g. enter ./test-strace-wrapper.sh ':::' mxtx-rshd )
if test "${1-}" = ':::'
then
    set -x
    mv "$HOME"/.local/share/mxtx/"$2" "$HOME"/.local/share/mxtx/"$2"-st
    exec cp "$0" "$HOME"/.local/share/mxtx/"$2"
fi

bn=${0##*/}
set -x
cd "$HOME"/.local/share/mxtx/ ### XXX changes cwd XXX ###
# first of the uncommented commands below is active
#exec strace -f ./"$bn"-st "$@"
exec strace -s 256 ./"$bn"-st "$@"
exec ./"$bn"-st "$@"
