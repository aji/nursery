#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include "io.h"
#include "irc.h"

char *argv0;
time_t started;
enum { NOTHING, IRC, RESTORE } action = NOTHING;

struct option longopts[] = {
	{ "irc", required_argument, NULL, 'i' },
	{ "state", required_argument, NULL, 's' },
	{ "help", no_argument, NULL, 'h' },
	{ NULL }
};

void usage(FILE *file)
{
	fprintf(file, "Usage: %s OPTIONS\n", argv0);
	fprintf(file, "\n");
	fprintf(file, "Options:\n");
	fprintf(file, "  --irc=HOST[:PORT][/NICK]\n");
	fprintf(file, "              Starts an IRC bot on the specified server. The\n");
	fprintf(file, "              bot will use given nick. Port defaults to 6667.\n");
	fprintf(file, "  --help, -h  Displays this help\n");
	fprintf(file, "\n");
	fprintf(file, "At least ONE action (--irc) is required\n");
}

int main(int argc, char *argv[])
{
	int c, idx;
	char buf[256];

	argv0 = argv[0];

	while ((c = getopt_long(argc, argv, "h:", longopts, &idx)) != -1) {
		switch (c) {
		case 'i':
			action = IRC;
			snprintf(buf, 256, "%s", optarg);
			break;

		case 'h':
			usage(stdout);
			return 0;

		default:
			fprintf(stderr, "Try '%s --help' for more information.\n",
			        argv[0]);
			return 1;
		}
	}

	started = time(NULL);

	switch (action) {
	case IRC:
		if (irc_start(buf) < 0) {
			fprintf(stderr, "Connection failed :(\n");
			return 1;
		}

		io_poll();

		break;

	default:
		fprintf(stderr, "%s: no action specified\n", argv[0]);
		fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
		return 1;
	}

	return 0;
}
