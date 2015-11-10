/*
   Alicorn -- Alex's IRC bouncer
   Copyright (C) 2012 Alex Iadicicco
  
   This file is part of Alicorn and is protected under the terms contained
   in the COPYING file in the project root
 */

#include <mowgli.h>
#include <unicorn.h>
#include "alicorn.h"

struct alicorn alicorn;

static void alicorn_create(void)
{
	memset(&alicorn, 0, sizeof(alicorn));

	alicorn.eventloop = mowgli_eventloop_create();
	alicorn.dns = mowgli_dns_create(alicorn.eventloop, MOWGLI_DNS_TYPE_ASYNC);

	alicorn.h_outgoing = irc_hook_table_create();
	alicorn.h_incoming = irc_hook_table_create();
	alicorn.h_db = irc_hook_table_create();

	alicorn.cmds = a_commands_create();
}

static void alicorn_destroy(void)
{
	mowgli_eventloop_destroy(alicorn.eventloop);
	/* mowgli_dns_destroy(alicorn.dns); TODO */

	irc_hook_table_destroy(alicorn.h_outgoing);
	irc_hook_table_destroy(alicorn.h_incoming);
	irc_hook_table_destroy(alicorn.h_db);

	a_commands_destroy(alicorn.cmds);
}


static bool everything_init(void)
{
	bool ok = true;

	if (ok) ok = a_module_init();
	if (ok) ok = a_account_init();
	if (ok) ok = a_network_init();
	if (ok) ok = a_client_init();
	if (ok) ok = a_cap_init();
	if (ok) ok = a_core_init();
	if (ok) ok = a_cmdsock_init();

	return ok;
}

static void everything_deinit(void)
{
	a_module_deinit();
	a_account_deinit();
	a_network_deinit();
	a_client_deinit();
	a_cap_deinit();
	a_core_deinit();
	a_cmdsock_deinit();
}


static void usage(int argc, char *argv[])
{
	fprintf(stderr, "Usage: %s [-fhv]\n", argv[0]);
	fprintf(stderr, "Usage: %s BNC <command>\n\n", argv[0]);

	fprintf(stderr, "-c <file>  Select alternate configuration file\n");
	fprintf(stderr, "-d <dir>   Select alternate data directory\n");
	fprintf(stderr, "-f         Do not fork to background/stay in foreground\n");
	fprintf(stderr, "-h         Show this help\n");
	fprintf(stderr, "-l <file>  Select alternate log file\n");
	fprintf(stderr, "-v         Increase logging level\n\n");

	fprintf(stderr, "Commands may be sent to a running instance of Alicorn by\n");
	fprintf(stderr, "using the BNC command line argument. Note that this must\n");
	fprintf(stderr, "be the only command line option\n");
}

int main(int argc, char *argv[])
{
	bool foreground = false;
	char *config_fname = "etc/alicorn.conf";
	char *log_fname = "log/alicorn.log";
	char *datadir = PREFIX;
	int c;

	if (argc > 1 && !strcasecmp(argv[1], "BNC")) {
		if (argc < 3) {
			usage(argc, argv);
			return 1;
		}

		a_cmdsock_send(argc - 2, argv + 2);
		return 0;
	}

	while ((c = getopt(argc, argv, "c:d:fhl:v")) > 0) {
		switch (c) {
		case 'c':
			config_fname = mowgli_strdup(optarg);
			break;
		case 'd':
			datadir = mowgli_strdup(optarg);
			break;
		case 'f':
			foreground = true;
			break;
		case 'h':
			usage(argc, argv);
			return 0;
		case 'l':
			log_fname = mowgli_strdup(optarg);
			break;
		case 'v':
			a_log_set_level(LDEBUG);
			break;

		default:
		case '?':
			usage(argc, argv);
			return 1;
		}
	}

	if (chdir(datadir) < 0) {
		a_log(LFATAL, "Failed to chdir() to datadir (%s), exiting", datadir);
		return 1;
	}

	mowgli_log_set_cb(a_log_write);
	a_log_open(log_fname);

	if (!foreground) {
		a_log(LNOTICE, "Forking into background");
		a_log_set_echo(false);
	}

	if (foreground || fork() == 0) {
		alicorn_create();
		if (!everything_init()) {
			a_log(LERROR, "Everything failed");
			return 1;
		}

		if (a_config_hash(config_fname) < 0) {
			a_log(LERROR, "Config hash failed");
			return 1;
		}

		a_db_load("var/alicorn.db");

		a_log(LDEBUG, "Running main event loop");
		mowgli_eventloop_run(alicorn.eventloop);

		a_db_save("var/alicorn.db");

		everything_deinit();
		alicorn_destroy();

		a_log(LNOTICE, "Goodbye");
	}

	return 0;
}
