#!/bin/sh
# -*- mode: shell-script; sh-basic-offset: 8; tab-width: 8 -*-
# $ version.sh $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#       Copyright (c) 2017 Tomi Ollila
#           All rights reserved
#
# Created: Sun 12 Nov 2017 17:34:40 EET too
# Last modified: Thu 08 Nov 2018 22:44:31 +0200 too

case ${BASH_VERSION-} in *.*) set -o posix; shopt -s xpg_echo; esac
case ${ZSH_VERSION-} in *.*) emulate ksh; esac

set -euf

version_num=2.1

prev_commit=15f7489086b4d639618f87e49d658e3ad3fabd3d

LANG=C LC_ALL=C export LANG LC_ALL; unset LANGUAGE

if test -e .git # || test -e ../.git
then
	s () { set -- `exec git --no-pager log -1 --abbrev=7 --pretty='%h %ci'`
	       ch=$1 cd=$2; }; s
	cn=`git --no-pager log --oneline $prev_commit..HEAD | wc -l`
	test "`exec git status --porcelain -uno`" && m=-mod || m=
	test "$cn" && cn=$((cn - 1)) || cn=9999
	echo $version_num-$cn-g$ch-$cd$m
	exit
fi

IFS=/
set -- $PWD ''
for s
do case $s in mxtx-20[0-9][0-9]-[0-9][0-9]-[0-9][0-9]-g*) break; esac
done

if test "$s"
then
	IFS=-
	set -- $s
	echo $version_num-$5-$2-$3-$4
else
	exec date +$version_num-%Y-%m-%d
fi
