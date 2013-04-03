#include <global.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <Config.h>
#include <ConnectionManager.h>
#include <HThread.h>
#include <Multiplexer.h>
#include <ThreadManager.h>
#include <connection_handler.h>
#include <helper_thread.h>
#include <listener.h>
#include <log.h>
#include <output.h>
#include <util.h>
#include <xmalloc.h>

static uint32_t listen_address = 0;
static int control_socket = -1, data_socket = -1, listen_port = -1;

void ch_init()
{
	uint32_t source_address;
	int port;
	char *t;

	if((t = config_value("global", "listen_address")) != NULL) {
		source_address = inet_network(t);
		xfree(t); 
	} else 
		source_address = INADDR_ANY;

	if((port = config_int_value("global", "port")) < 0)
		port = HL_DEFAULT_PORT;

	if(listen_address != source_address || listen_port != port) {
		listen_address = source_address;
		listen_port = port;

		if(control_socket != -1) {
			multiplexer_remove(control_socket, MPLX_RD);
			close(control_socket);
		}

		if(data_socket != -1) {
			multiplexer_remove(data_socket, MPLX_RD);
			close(data_socket);
		}

		if((control_socket = listener_create(source_address, port)) < 0) {
			log("!!! Warning: Couldn't bind control socket to %s:%d", t = xinet_ntoa(source_address), port);
			xfree(t);

			fatal("!!! Couldn't create a listener socket - fix your configuration and restart!");
		} else {
			multiplexer_add(control_socket, MPLX_RD);
			log("*** Listening on %s:%d", t = xinet_ntoa(source_address), port);
			xfree(t);
		}

		if((data_socket = listener_create(source_address, port + 1)) < 0) {
			log("!!! Warning: Couldn't bind data socket to %s:%d", t = xinet_ntoa(source_address), port);
			xfree(t);

			fatal("!!! Couldn't create a listener socket - fix your configuration and restart!");
		} else
			multiplexer_add(data_socket, MPLX_RD);
	}
}

static void ch_accept_control_connection(int fd)
{
	int cfd;
	uint32_t ip;
	char *s;

	if((cfd = listener_accept(fd, &ip)) < 0)
		return;

	cm_add(cfd, ip);

	log("+++ Connect: %s on control port (%d)", (s = xinet_ntoa(ip)), cfd);
	xfree(s);

	mqueue_send_int(hthread_queue(tm_get_thread()), HT_CONTROL_CONNECT, cfd);
}

static void ch_accept_data_connection(int fd)
{
	ConnectionInfo c;
	char *s;

	c = NEW(ConnectionInfo);
	
	if((c->fd = listener_accept(fd, &c->ip)) < 0) {
		xfree(c);
		return;
	}

	log("+++ Connect: %s on data port (%d)", (s = xinet_ntoa(c->ip)), c->fd);
	xfree(s);

	mqueue_send_ptr(hthread_queue(tm_get_thread()), HT_DATA_CONNECT, c);
}

static void ch_handle_transaction(int fd)
{
	mqueue_send_int(hthread_queue(tm_get_thread()), HT_TRANSACTION, fd);
}

void ch_main()
{
	int fd;

	for(;;) {
		if(multiplexer_poll(&fd, NULL)) {
#ifdef DEBUG
			debug("*** connection handler: notifier activated");
#endif
			continue;
		}

		if(fd == control_socket) {
			ch_accept_control_connection(fd);
			continue;
		}

		if(fd == data_socket) {
			ch_accept_data_connection(fd);
			continue;
		}

#ifdef DEBUG
		debug("*** Transaction received");
#endif
		multiplexer_remove(fd, MPLX_RD);
		ch_handle_transaction(fd);
	}
}
