#!/bin/sh

# mxtx acute program usage

case ~ in '~') echo "'~' does not expand. old /bin/sh?" >&2; exit 1; esac

case ${BASH_VERSION-} in *.*) set -o posix; shopt -s xpg_echo; esac
case ${ZSH_VERSION-} in *.*) emulate ksh; esac

set -euf
#set -x # or enter /bin/sh -x ... on command line.

# LANG=C LC_ALL=C; export LANG LC_ALL
# PATH='/sbin:/usr/sbin:/bin:/usr/bin'; export PATH

warn () { printf '%s\n' "$*"; } >&2
die () { exec >&2; printf '%s\n' "$*"; exit 1; }

x () { printf '+ %s\n' "$*" >&2; "$@"; }
x_env () { printf '+ %s\n' "$*" >&2; env "$@"; }
x_eval () { printf '+ %s\n' "$*" >&2; eval "$*"; }
x_exec () { printf '+ %s\n' "$*" >&2; exec "$@"; die "exec '$*' failed"; }

saved_IFS=$IFS; readonly saved_IFS

usage () {
	printf '\nUsage: %s %s %s\n\n' "$0" "$cmd" "$1"
	test $# = 1 || { shift; printf '  %s\n' "$@"; echo; }
	exit 1;
} >&2

#gdbrun='gdb -ex run --args'

sfs_dirs=/usr/libexec/openssh:/usr/lib/openssh:/usr/lib/ssh:/usr/libexec
sfs_dirs=$sfs_dirs:/usr/sbin:/usr/bin # these 2 for cygwin support

if test "${MXTX_APU_WRAPPER-}"
then
	#echo $# "$@" >&2
	if test "$MXTX_APU_WRAPPER" = sftp
	then
		eval r='$'$(($# - 1))
		x_exec mxtx-io "$r" env PATH=$sfs_dirs sftp-server
		exit not reached
	fi
	if test "$MXTX_APU_WRAPPER" = xpra
	then	exec mxtx-io "$2" /bin/sh -c "exec $3"
		exit not reached
	fi
	die "'$MXTX_APU_WRAPPER': unknown wrapper command"
fi

set_chromie () {
	set -- chromium chromium-freeworld chromium-browser google-chrome
	for chromie
	do	command -v $chromie >/dev/null || continue
		case $chromie
		 in *chrome*)	cbcnfdir=g-chrome-mxtx
		 ;; *)		cbcnfdir=g-chromium-mxtx
		esac ; return
	done
	die 'Cannot find chrome / chromium browser on $PATH'
}

case $0 in */*/*) mxtxdir=$HOME/.local/share/mxtx
	;; ./*)	mxtxdir=$PWD
	;; *)	mxtxdir=$HOME/.local/share/mxtx
esac

_mxtxpath () {
	case $1
	in "$MXTX_PWD"/*) p=${1#"$MXTX_PWD"/}
	;; "$MXTX_PWD") p=
	;; *) p=$1
	esac
	printf '%s\n' "$MXTX_LINK:$p"
}

cmd_path () # show file/dir paths -- most useful in mxtx-rsh shell
{
	if test $# = 0
	then
		printf 'pwd\n%s\n' "$PWD"
		rwd=`exec readlink -f "$PWD"`
		test "$rwd" = "$PWD" || printf '%s\n' "$rwd"
		test "${MXTX_PWD-}" && test "${MXTX_LINK-}" || exit 0
		_mxtxpath "$rwd"
		echo
		exit
	fi
	#else
	for arg
	do
		if test -f "$arg"; then echo f
		elif test -d "$arg"; then echo d
		else echo -
		fi
		arg=`readlink -f "$arg"`
		printf '%s\n' "$arg"
		test "${MXTX_PWD-}" && test "${MXTX_LINK-}" || continue
		_mxtxpath "$arg"
	done
	echo
}

cmd_sshmxtx () # create default forward tunnel using ssh
{
	test $# -ge 2 || usage 'link [user@]host [env=val [env...]' \
			"'env's, if any, will be set in host"
	link=$1 remote=$2 shift 2;
	for arg; do
		# perhaps too tight here...
		case $arg in *['(`\']*) ;; *=*) set -- "$@" \""$arg"\"; esac
		shift
	done
	x_exec ioio.pl / .mxtx -c"$link" / ssh "$remote" env $* .mxtx -s
}

set_i2u_ldpra ()
{
	ldpra=$mxtxdir/ldpreload-i2uconnect5.so
	test -f "$ldpra" || die "'$ldpra' does not exist"
	export "LD_PRELOAD=$ldpra${LD_PRELOAD:+:$LD_PRELOAD}"
	export LD_PRELOAD

}

cmd_chromie () # start chromium / chrome browser w/ mxtx socks5 tunneling
{
	case ${1-} in h) ic=--incognito
		   ;; v) ic=
		   ;; *) usage "('h'|'v') [link|url] ..." \
			"'h' -- incognito" "'v' -- not"
	esac
	shift
	set_chromie

	# shortcut to http://index-{link}.html (when $1 ~ /^[a-z0-9]{1,3}$/)
	case ${1-} in [a-z0-9] | [a-z0-9][a-z0-9] | [a-z0-9][a-z0-9][a-z0-9] )
		l=$1; shift; set -- http://index-$l.html "$@"
	esac
	set_i2u_ldpra
	x_exec "$chromie" --user-data-dir=$HOME/.config/$cbcnfdir \
			--host-resolver-rules='MAP * 0.0.0.0 , EXCLUDE 127.1' \
			$ic --proxy-server=socks5://127.1:1080 "$@"
}

cmd_ffox () # start firefox browser w/ mxtx socks5 tunneling
{
	case ${1-} in h) prv=--private
		   ;; v) prv=
		   ;; *) usage "('h'|'v') [link|url] ..." \
			"'h' -- private" "'v' -- not"
	esac
	shift
	case $HOME in *["$IFS"]*) die "Whitespace in '$HOME'"; esac
	profile_name=ff-mxtx
	profile_dir=$HOME/.config/$profile_name

	# shortcut to http://index-{link}.html (when $1 ~ /^[a-z0-9]{1,3}$/)
	case ${1-} in [a-z0-9] | [a-z0-9][a-z0-9] | [a-z0-9][a-z0-9][a-z0-9] )
		l=$1; shift; set -- http://index-$l.html "$@"
	esac
	set_i2u_ldpra
	test -d $profile_dir || {
		firefox -CreateProfile "$profile_name $profile_dir" -no-remote
		printf %s\\n > $profile_dir/user.js \
			'user_pref("network.proxy.socks", "127.0.0.1");' \
			'user_pref("network.proxy.socks_port", 1080);' \
			'user_pref("network.proxy.socks_remote_dns", true);' \
			'user_pref("network.proxy.type", 1);'
	}
	x_exec firefox -profile $profile_dir $prv -no-remote "$@" &
}

find_sftp_server () {
	IFS=:
	for d in $sfs_dirs
	do	test -x $d/sftp-server || continue
		IFS=$saved_IFS
		sftp_server=$d/sftp-server
		return
	done
	die "Cannot find sftp-server on local host"
}

cmd_sshfs () # sshfs mount using mxtx tunnel
{
	# versatility of sshlessfs options cannot be achieved w/ just sh code
	# in addition to sshfs options read-only sftp-server option would be ni
	test $# -ge 2 || usage 'path mountpoint [sshfs options]' \
		"either 'path' or 'mountpoint' is to have ':' but not both" \
		"'reverse' mount (i.e. ':' in mountpoint) may have some security implications"
	case $1 in *:*) case $2 in *:*) die "2 link:... paths!"; esac; esac
	p=$1 m=$2; shift 2
	case $m in *:*)
		eval la=\$$# # last arg
		# drop last arg
		a=$1; shift; for arg; do set -- "$@" "$a"; a=$arg; shift; done
		mp=${m#*:}; test "$mp" || mp=.
		case $la
		in '!') find_sftp_server
		 # quite a bit of trial&error (in gitrepos.sh) to get there.
		 x export FAKECHROOT_EXCLUDE_PATH=${sftp_server%/*}:/dev:/etc
		 x_exec mxtx-io /// fakechroot \
			/usr/bin/env /usr/sbin/chroot "$p" $sftp_server /// \
			"${m%%:*}": sshfs "r:$p" "$mp" -o slave "$@"
		;; '!!')find_sftp_server
		 x_exec mxtx-io /// $sftp_server /// \
			"${m%%:*}": sshfs "r:$p" "$mp" -o slave "$@"
		;; *)	exec >&2; echo
		 echo Due to potential security implications to do reverse
		 echo mount, enter either '!' or "'!!'" at the end of the
		 echo command line. "With ! fakechroot(1) attempts to chroot"
		 echo sftp-server to the mounted directory. With "'!!'" such
		 echo fake chrooting is not done -- peer may ptrace '(gdb!)'
		 echo sshfs program to cd outside of the mounted path.
		 echo; exit 1
		esac
	esac
	case $p in *:*) ;; *) die "neither paths link:...!"; esac
	# forward mount
	command -v sshfs >/dev/null || die "'sshfs': no such command"
	x_exec mxtx-io /// sshfs "$p" "$m" -o slave "$@" /// \
		"${p%%:*}": env PATH=$sfs_dirs sftp-server
}

cmd_sftp () # like sftp, but via mxtx tunnel
{
	MXTX_APU_WRAPPER=sftp x_exec sftp -S "$0" "$@"
}

cmd_tcp1271c () # tunnel localhost connection to mxtx endpoint
{
	test $# -gt 1 || usage 'mapping command [args]' \
		'mapping format: port/dest[/dport][:port/dest[/dport][:...]]' \
		'example: mxtx-apu.sh tcp1271c 5901/w/5900 vncwiever :1'

	so=$HOME/.local/share/mxtx/ldpreload-tcp1271conn.so
	export LD_PRELOAD=$so${LD_PRELOAD:+:$LD_PRELOAD}
	export TCP1271_MAP=$1
	shift
	exec "$@"
	exit not reached
}


cmd_emacs () # emacs, with loaded mxtx.el -- a bit faster if first arg is '!'
{
	test "${1-}" != '!' || { shift; set -- --eval '(mxtx!)' "$@"; }
	x_exec emacs -l $HOME/.local/share/mxtx/mxtx.el "$@"
}

# remove leading '_' for xpra experiments
_cmd_xpra () # xpra support (wip)
{
	test $# != 0 || usage 'command [link:disp] [options]'
	case $1 in initenv | showconfig | list ) x_exec xpra "$@" ; esac
	test $# -gt 1 || die "$0 xpra $1 needs display arg"
	xcmd=$1 disp=$2; shift 2
	MXTX_APU_WRAPPER=xpra x_exec xpra --ssh="$0" "$xcmd" ssh:"$disp" "$@"
}

cmd_vsfa () # wrap open() and stat() syscalls for opportunistic remote access
{
	test $# != 0 || usage 'command [args]'
	so=$HOME/.local/share/mxtx/ldpreload-vsfa.so
	export LD_PRELOAD=$so${LD_PRELOAD:+:$LD_PRELOAD}
	exec "$@"
}

cmd_ping () # 'ping' (including time to execute `date` on destination)
{
	x export TIME='elapsed: %e s'
	x date
	x_exec /usr/bin/time mxtx-io "$1" date
}

cmd_exit () # exit all .mxtx processes running on this system (dry-run w/o '!')
{
	#pgrep -a '[.]mxtx'   # -a option not in older pgreps
	ps x | grep '[.]mxtx' # if we also had -e, this would be unnecessary

	case ${1-} in	'!')	x_exec pkill '[.]mxtx'
		;;	[!.]*)	x_exec pkill -f "[.]mxtx.*-c$1"
	esac

	echo "Add '!' to the command line to exit the processes shown above"
}

cmd_hints () # hints of some more acute ways to utilize mxtx tools
{
	printf '%s\n' '' \
  "Use mxtx-rsh as proxy:" \
  " GIT_SSH_COMMAND='mxtx-rsh {link} . ssh' git clone git@ror:{repo}" \
  " GIT_SSH_COMMAND='mxtx-rsh {link} . ssh' git pull --rebase --autostash" '' \
  "Use mxtx-rsh as replacement for ssh:" \
  " GIT_SSH_COMMAND='mxtx-rsh' git push {link}:{repopath} HEAD:new-master" \
  "  (and on {link}: git update-ref refs/remotes/origin/master new-master)" ''
}

cmd_source () # check source of given '$0' command
{
	set +x
	test "${1-}" || usage 'cmd-prefix'
	echo
	exec sed -n -e "/^\(cmd_\)\?$1.*(/,/^}/p" "$0"
}


#case ${1-} in --sudo) shift; exec sudo "$0" "$@"; die 'not reached'; esac

case ${1-} in -x) setx=true; shift ;; *) setx=false ;; esac

case ${1-} in -e) cmd=$2; readonly cmd; shift 2; cmd_"$cmd" "$@"; exit ;; esac
#test "${1-}" != -e || { cmd=$2; readonly cmd; shift 2; cmd_"$cmd" "$@"; exit;}

# ---

case ${1-} in '')
	echo mxtx acute program use
	echo
	echo Usage: $0 '<command> [args]'
	echo
	echo ${0##*/} commands available:
	echo
	sed -n '/^cmd_[a-z0-9_]/ { s/cmd_/ /; s/ () [ #]*/                   /
		s/$0/'"${0##*/}"'/g; s/\(.\{11\}\) */\1/p; }' "$0"
	echo
	echo Command can be abbreviated to any unambiguous prefix.
	echo
	exit 0
esac

cm=$1; shift

# case $cm in
# 	d) cm=diff ;;
# esac

cc= cp=
for m in `LC_ALL=C exec sed -n 's/^cmd_\([a-z0-9_]*\) (.*/\1/p' "$0"`
do
	case $m in
		$cm) cp= cc=1 cmd=$cm; break ;;
		$cm*) cp=$cc; cc="$m $cc"; cmd=$m ;;
	esac
done

test   "$cc" || { echo $0: $cm -- command not found.; exit 1; }
test ! "$cp" || { echo $0: $cm -- ambiguous command: matches $cc; exit 1; }
unset cc cp cm
readonly cmd
$setx && { unset setx; set -x; } || unset setx
cmd_$cmd "$@"

# Local variables:
# mode: shell-script
# sh-basic-offset: 8
# sh-indentation: 8
# tab-width: 8
# End:
# vi: set sw=8 ts=8
