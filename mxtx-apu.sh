#!/bin/sh

# mxtx acute program usage

case ${BASH_VERSION-} in *.*) set -o posix; shopt -s xpg_echo; esac
case ${ZSH_VERSION-} in *.*) emulate ksh; esac

set -euf  # hint: (z|ba|da|'')sh -x thisfile [args] to trace execution

die () { printf '%s\n' '' "$@" ''; exit 1; } >&2

x () { printf '+ %s\n' "$*" >&2; "$@"; }
x_env () { printf '+ %s\n' "$*" >&2; env "$@"; }
x_eval () { printf '+ %s\n' "$*" >&2; eval "$*"; }
x_exec () { printf '+ %s\n' "$*" >&2; exec "$@"; exit not reached; }

saved_IFS=$IFS; readonly saved_IFS

usage () { printf %s\\n '' "Usage: ${0##*/} $cmd $@" ''; exit 1; } >&2

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

cmds=

cmds=$cmds'
cmd_path  show file/dir paths -- most useful in mxtx-rsh shell'
cmd_path ()
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

cmds=$cmds'
cmd_sshmxtx  create default forward mxtx tunnel using ssh'
cmd_sshmxtx ()
{
	test $# -ge 2 || usage 'link [user@]host [env=val [env...]' '' \
			"  'env's, if any, will be set in host"
	link=$1 remote=$2 shift 2;
	for arg; do
		# perhaps too tight here...
		case $arg in *['(`\']*) ;; *=*) set -- "$@" \""$arg"\"; esac
		shift
	done
	x_exec ioio.pl / .mxtx -c"$link" / ssh "$remote" env $* .mxtx -s
}

cmds=$cmds'
cmd_revmxtx  create "reverse" mxtx tunnel'
cmd_revmxtx ()
{
	test $# -ge 2 || usage 'link "back"link'
	x_exec ioio.pl mxtx-rsh "$1" .mxtx -c"'$2'" /// .mxtx -s~
}

set_i2u_ldpra ()
{
	ldpra=$mxtxdir/ldpreload-i2uconnect5.so
	test -f "$ldpra" || die "'$ldpra' does not exist"
	export "LD_PRELOAD=$ldpra${LD_PRELOAD:+:$LD_PRELOAD}"
	export LD_PRELOAD

}

cmds=$cmds'
cmd_chromie  start chromium / chrome browser w/ mxtx socks5 tunneling'
cmd_chromie ()
{
	case ${1-} in h) ic=--incognito
		   ;; v) ic=
		   ;; *) usage "('h'|'v') [link|url] ..." \
			"  'h' -- incognito" "  'v' -- not"
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

cmds=$cmds'
cmd_ffox  start firefox browser w/ mxtx socks5 tunneling'
cmd_ffox ()
{
	case ${1-} in h) prv=--private
		   ;; v) prv=
		   ;; *) usage "('h'|'v') [link|url] ..." \
			"  'h' -- private" "  'v' -- not"
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

cmds=$cmds'
cmd_curl  run curl via socks5 tunneling (mxtx socks5 proxy like 2 above)'
cmd_curl ()
{
	set_i2u_ldpra
	x_exec curl --socks5-hostname 127.1:1080 "$@"
}

cmds=$cmds'
cmd_aps5h  run command, $all_proxy set as socks5h:... (mxtx socks5 proxy)'
cmd_aps5h ()
{
	set_i2u_ldpra
	test $# = 0 && set -- env
	all_proxy=socks5h://127.0.0.1:1080 exec "$@"
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

cmds=$cmds'
cmd_sshfs  sshfs mount using mxtx tunnel'
cmd_sshfs ()
{
	# versatility of sshlessfs options cannot be achieved w/ just sh code
	# in addition to sshfs options read-only sftp-server option would be ni
	test $# -ge 2 || usage 'path mountpoint [sshfs options]' '' \
		"  either 'path' or 'mountpoint' is to have ':' but not both."\
		"  'reverse' mount (i.e. ':' in mountpoint) may have some security implications"
	p=$1 m=$2; shift 2
	case $m in *:*)
		case $p in *:*) die "2 link:... paths!"; esac
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

cmds=$cmds'
cmd_sftp  like sftp, but via mxtx tunnel'
cmd_sftp ()
{
	export MXTX_APU_WRAPPER=sftp
	x_exec sftp -S "$0" "$@"
}

cmds=$cmds"
cmd_wgitsshc  run command with GIT_SSH_COMMAND='mxtx-rsh {link} . ssh'"
cmd_wgitsshc ()
{
	test $# = 0 && usage 'link (git)command [args]'
	l=$1; shift
	GIT_SSH_COMMAND="mxtx-rsh '$l' . ssh" exec "$@"
}

cmds=$cmds'
cmd_tcp1271c  tunnel localhost connection to mxtx endpoint'
cmd_tcp1271c ()
{
	test $# -gt 1 || usage 'mapping command [args]' '' \
		'  mapping format: port/dest[/dport][:port/dest[/dport][:...]]' \
		'  example: mxtx-apu.sh tcp1271c 5901/w/5900 vncwiever :1'

	so=$HOME/.local/share/mxtx/ldpreload-tcp1271conn.so
	export LD_PRELOAD=$so${LD_PRELOAD:+:$LD_PRELOAD}
	export TCP1271_MAP=$1
	shift
	exec "$@"
	exit not reached
}

cmds=$cmds"
cmd_emacs  emacs, with loaded mxtx.el -- a bit faster if first arg is '!'"
cmd_emacs ()
{
	test "${1-}" != '!' || { shift; set -- --eval '(mxtx!)' "$@"; }
	echo fyi: ///mxtx:{r}: >&2
	x_exec emacs -l $HOME/.local/share/mxtx/mxtx.el "$@"
}

# uncomment for xpra experiments
#cmds=$cmds'
#cmd_xpra  xpra support (wip)'
cmd_xpra ()
{
	test $# != 0 || usage 'command [link:disp] [options]'
	case $1 in initenv | showconfig | list ) x_exec xpra "$@" ; esac
	test $# -gt 1 || die "$0 xpra $1 needs display arg"
	xcmd=$1 disp=$2; shift 2
	export MXTX_APU_WRAPPER=xpra
	x_exec xpra --ssh="$0" "$xcmd" ssh:"$disp" "$@"
}

cmds=$cmds'
cmd_vsfa  wrap open() and stat() syscalls for opportunistic remote access'
cmd_vsfa ()
{
	test $# != 0 || usage 'command [args]'
	so=$HOME/.local/share/mxtx/ldpreload-vsfa.so
	export LD_PRELOAD=$so${LD_PRELOAD:+:$LD_PRELOAD}
	exec "$@"
}

cmds=$cmds"
cmd_ping  'ping' "'(including time to execute `date` on destination)'
cmd_ping ()
{
	test $# = 0 && usage 'link'
	export TIME='elapsed: %e s'
	for arg
	do 	x date
		x /usr/bin/time mxtx-io "$arg" date
	done
}

cmds=$cmds"
cmd_exit  exit all .mxtx processes running on this system (dry-run w/o '!')"
cmd_exit ()
{
	#pgrep -a '[.]mxtx'   # -a option not in older pgreps
	ps x | grep '[.]mxtx' # if we also had -e, this would be unnecessary

	case ${1-} in	'!')	x_exec pkill '[.]mxtx'
		;;	[!.]*)	x_exec pkill -f "[.]mxtx.*-c$1"
	esac

	echo "Add '!' to the command line to exit the processes shown above"
}

cmds=$cmds'
cmd_hints  hints of some more acute ways to utilize mxtx tools'
cmd_hints ()
{
	printf '%s\n' '' \
  "GIT_SSH_COMMAND - Use mxtx-rsh as proxy:" \
  " GIT_SSH_COMMAND='mxtx-rsh {link} . ssh' git clone git@ror:{repo}" \
  " GIT_SSH_COMMAND='mxtx-rsh {link} . ssh' git pull --rebase --autostash" '' \
  "GIT_SSH_COMMAND - Use mxtx-rsh as replacement for ssh:" \
  " GIT_SSH_COMMAND='mxtx-rsh' git push {link}:{repopath} HEAD:new-main" \
  "  (and on {link}: git update-ref refs/remotes/origin/main new-main)" \
  "  (\or no remote: git rebase new-main && git branch -d new-main)" \
  "  (slightly related: git update-ref refs/heads/main new-value old-value)" ''\
  'Try X11 forwarding for one (1) client:' \
  " mxtx-io // socat [-x] unix-connect:/tmp/.X11-unix/X0 stdio // mxtx-rsh {link} socat unix-listen:/tmp/.X11-unix/X5 stdio" \
  " (then, on {link} (on separate cli): DISPLAY=:5 xeyes" \
  "  it may take some time to see it -- and ~10x longer with xterm(1))" ''
}

#case ${1-} in --sudo) shift; exec sudo "$0" "$@"; die 'not reached'; esac

case ${1-} in -x) setx=true; shift ;; *) setx=false ;; esac

case ${1-} in -e) cmd=$2; readonly cmd; shift 2; cmd_"$cmd" "$@"; exit ;; esac
#test "${1-}" != -e || { cmd=$2; readonly cmd; shift 2; cmd_"$cmd" "$@"; exit;}

# ---

IFS='
'
test $# = 0 && {
	echo mxtx acute program use
	echo
	echo Usage: $0 '{command} [args]'
	echo
	echo Commands of ${0##*/} "('.' to list, '.. cmd(pfx)' to view source):"
	echo
	set -- $cmds
	rows=$((($# + 4) / 5))
	cols=$((($# + ($rows - 1)) / $rows))
	c=0; while test $c -lt $rows; do eval r$c="'  '"; c=$((c + 1)); done
	c=0; i=0
	for arg
	do arg=${arg%% *}; arg=${arg#cmd_}
	   test $i -lt $(($# - rows)) && {
	      arg=$arg'          '; arg=${arg%${arg#???????????}}; }
	   eval r$c='$r'$c'$arg'
	   i=$((i + 1)); c=$((i % rows))
	done
	c=0; while test $c -lt $rows; do eval echo \$r$c; c=$((c + 1)); done
	echo
	echo Command can be abbreviated to any unambiguous prefix.
	echo
	exit 0
}
cm=$1; shift

case $#/$cm
in 0/.)
	set -- $cmds
	IFS=' '
	echo
	for cmd
	do	set -- $cmd; cmd=${1#cmd_}; shift
		printf ' %-9s  %s\n' $cmd "$*"
	done
	echo
	exit
;; 1/..)
	set +x
	# $1 not sanitized but that should not be too much of a problem...
	exec sed -n "/^cmd_$1/,/^}/p; \${g;p}" "$0"
;; */.) cm=$1; shift
;; */..) cmd=..; usage cmd-prefix

#;; */d) cm=diff
esac

cc= cp=
for m in $cmds
do
	m=${m%% *}; m=${m#cmd_}
	case $m in
		$cm) cp= cc=1 cmd=$cm; break ;;
		$cm*) cp=$cc; cc=$m${cc:+, $cc}; cmd=$m
	esac
done
IFS=$saved_IFS

test "$cc" || die "$0: $cm -- command not found."
test "$cp" && die "$0: $cm -- ambiguous command: matches $cc"

unset cc cp cm
#set -x
cmd'_'$cmd "$@"
exit
