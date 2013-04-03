#!/bin/sh

echo "#ifndef MACHDEP_H"
echo "#define MACHDEP_H"
echo

case "`uname -p`" in
powerpc)
	echo "#define HOST_BIGENDIAN 1"
	;;
sparc)
	echo "#define HOST_BIGENDIAN 1"
	;;
SUN*)
	echo "#define HOST_BIGENDIAN 1"
	;;
*)
	echo "/* #undef HOST_BIGENDIAN */"
	;;
esac

case "`uname`" in
Darwin)
	echo "/* #undef HAVE_SIGSET */"
	echo "#define HAVE_VASPRINTF 1"
	;;
SunOS)
	echo "#define HAVE_SIGSET 1"
	echo "/* #undef HAVE_VASPRINTF */"
	echo "#define _POSIX_PTHREAD_SEMANTICS"
	echo "#define _REENTRANT"
	;;
Linux)
	echo "/* #undef HAVE_SIGSET */"
	echo "#define HAVE_VASPRINTF 1"
	echo "#define _GNU_SOURCE 1"
	;;
FreeBSD)
	echo "/* #undef HAVE_SIGSET */"
	echo "#define HAVE_VASPRINTF 1"
	;;
OpenBSD)
	echo "/* #undef HAVE_SIGSET */"
	echo "#define HAVE_VASPRINTF 1"
	;;
NetBSD)
	echo "/* #undef HAVE_SIGSET */"
	echo "#define HAVE_VASPRINTF 1"
	;;
*)
	echo "/* #undef HAVE_SIGSET */"
	echo "#define HAVE_VASPRINTF 1"
	;;
esac

echo
echo "#endif"

exit 0
