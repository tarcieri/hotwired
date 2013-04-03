#!/bin/sh

rm -f Multiplexer.c

case "`uname`" in
Darwin)
	ln -s Multiplexer.select.c Multiplexer.c
	;;
SunOS)
	ln -s Multiplexer.devpoll.c Multiplexer.c
	;;
Linux)
	ln -s Multiplexer.poll.c Multiplexer.c
	;;
FreeBSD)
	ln -s Multiplexer.kqueue.c Multiplexer.c
	;;
OpenBSD)
	ln -s Multiplexer.poll.c Multiplexer.c
	;;
NetBSD)
	ln -s Multiplexer.poll.c Multiplexer.c
	;;
*)
	ln -s Multiplexer.select.c Multiplexer.c
	;;
esac
