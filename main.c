#include <signal.h>

#include <AccountManager.h>
#include <Config.h>
#include <ConnectionManager.h>
#include <Multiplexer.h>
#include <ThreadManager.h>
#include <TransferManager.h>
#include <connection_handler.h>
#include <fileops.h>
#include <global.h>
#include <log.h>
#include <output.h>
#include <tracker.h>
#include <transaction_handler.h>

#define CONFIG_FILE	"hotwired.conf"
#if 0
static const char *config_search_path[] = { 
	"/etc/hotwired.conf", 
	"/usr/local/etc/hotwired.conf",
	"/usr/local/hotwired/hotwired.conf",
	"hotwired.conf",
	NULL
};
#endif

/* Small wrapper for config_init, handles paths */
static int load_config()
{
	/* Check the current directory */
	if(config_init(CONFIG_FILE) < 0)
		return -1;

	return 0;
}

/* Initialization and handler for SIGHUP */ 
static void server_init()
{
#ifdef DEBUG
	debug("load_config()");
#endif
	/* (re)load configuration */
	if(load_config() < 0) 
		log("!!! Couldn't find a configuration file, using defaults");

#ifdef DEBUG
	debug("tm_init()");
#endif
	/* Initialize the thread manager */
	if(tm_init() < 0)
		fatal("!!! Fatal error: Couldn't initialize thread manager");

#ifdef DEBUG
	debug("am_init()");
#endif
	/* Initialize the account manager */
	am_init();

#ifdef DEBUG
	debug("ch_init()");
#endif
	/* Initialize the connection handler */
	ch_init();

#ifdef DEBUG
	debug("f_init()");
#endif
	/* Initialize file operations */
	f_init();

#ifdef DEBUG
	debug("tfm_init()");
#endif
	tfm_init();

#ifdef DEBUG
	debug("tracker_init()");
#endif
	/* Initialize tracker thread */
	tracker_init();
}

int main()
{
	/* Initialize logfiles */
	log_init();

	/* Initialize multiplexer */
	multiplexer_create();

	/* Initialize the transaction handler */
	th_init();

	/* Initialize the connection manager */
	cm_init();
	
	/* Call initializers which get re-called with SIGHUP */
	server_init();

	/* Set server_init to be the handler for SIGHUP */
	signal(SIGHUP, (void (*)(int))server_init);

	/* Ignore SIGPIPE */
	signal(SIGPIPE, SIG_IGN);
	
	/* Begin the main loop */
	ch_main();
	
	return 0;
}
