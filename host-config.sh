#!/bin/sh

DARWIN_CC="gcc"
DARWIN_CFLAGS="-Wall -O2"
DARWIN_LD="gcc"
DARWIN_LDFLAGS="-lpthread"

SOLARIS_CC="cc"
SOLARIS_CFLAGS="-xO2"
SOLARIS_LD="cc"
SOLARIS_LDFLAGS="-lpthread -lsocket -lnsl -lcrypt"

LINUX_CC="gcc"
LINUX_CFLAGS="-Wall -O2"
LINUX_LD="gcc"
LINUX_LDFLAGS="-lpthread -lcrypt"

FREEBSD_CC="gcc"
FREEBSD_CFLAGS="-Wall -O2"
FREEBSD_LD="gcc"
FREEBSD_LDFLAGS="-pthread -lcrypt"

OPENBSD_CC="gcc"
OPENBSD_CFLAGS="-Wall -O2"
OPENBSD_LD="gcc"
OPENBSD_LDFLAGS="-pthread -lcrypt"

NETBSD_CC="gcc"
NETBSD_CFLAGS="-Wall -O2"
NETBSD_LD="gcc"
NETBSD_LDFLAGS="-pthread -lcrypt"

DEFAULT_CC="cc"
DEFAULT_CFLAGS=""
DEFAULT_LD="cc"
DEFAULT_LDFLAGS="-pthread -lcrypt"

case "$1" in
CC)
	if [ "$CC" != "" ]; then
		echo $CC
		exit 0
	fi

	case "`uname`" in
	Darwin)
		echo $DARWIN_CC
		;;
	SunOS)
		echo $SOLARIS_CC
		;;
	Linux)
		echo $LINUX_CC
		;;
	FreeBSD)
		echo $FREEBSD_CC
		;;
	OpenBSD)
		echo $OPENBSD_CC
		;;
	NetBSD)
		echo $NETBSD_CC
		;;
	*)
		echo $DEFAULT_CC
		;;
	esac
	;;
CFLAGS)
	if [ "$CFLAGS" != "" ]; then
		echo $CFLAGS
		exit 0
	fi

	case "`uname`" in
	Darwin)
		echo $DARWIN_CFLAGS
		;;
	SunOS)
		echo $SOLARIS_CFLAGS
		;;
	Linux)
		echo $LINUX_CFLAGS
		;;
	FreeBSD)
		echo $FREEBSD_CFLAGS
		;;
	OpenBSD)
		echo $OPENBSD_CFLAGS
		;;
	NetBSD)
		echo $NETBSD_CFLAGS
		;;
	*)	
		echo $DEFAULT_CFLAGS
		;;
	esac
	;;
LD)
	if [ "$LD" != "" ]; then
		echo $LD
		exit 0
	fi

	case "`uname`" in
	Darwin)
		echo $DARWIN_LD
		;;
	SunOS)
		echo $SOLARIS_LD
		;;
	Linux)
		echo $LINUX_LD
		;;
	FreeBSD)
		echo $FREEBSD_LD
		;;
	OpenBSD)
		echo $OPENBSD_LD
		;;
	NetBSD)
		echo $NETBSD_LD
		;;
	*)
		echo $DEFAULT_LD
		;;
	esac
	;;
LDFLAGS)
	if [ "$LDFLAGS" != "" ]; then
		echo $LDFLAGS
		exit 0
	fi

	case "`uname`" in
	Darwin)
		echo $DARWIN_LDFLAGS
		;;
	SunOS)
		echo $SOLARIS_LDFLAGS
		;;
	Linux)
		echo $LINUX_LDFLAGS
		;;
	FreeBSD)
		echo $FREEBSD_LDFLAGS
		;;
	OpenBSD)
		echo $OPENBSD_LDFLAGS
		;;
	NetBSD)
		echo $NETBSD_LDFLAGS
		;;
	*)
		echo $DEFAULT_LDFLAGS
		;;
	esac
	;;
esac

exit 0
