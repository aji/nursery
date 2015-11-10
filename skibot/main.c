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
#include "child.h"

char *argv0;
time_t started;
enum { NOTHING, IRC, RESTORE } action = NOTHING;

void update(char *errbuf, size_t errlen)
{
	char save_name[32] = "/tmp/ski.XXXXXX";
	int save_fd, err;
	FILE *save;
	struct stat buf;

	if (action != IRC) {
		snprintf(errbuf, errlen, "Not currently in IRC mode");
		return;
	}

	if (stat(argv0, &buf) < 0) {
		snprintf(errbuf, errlen, "Could not stat %s", argv0);
		return;
	}

	if (!(buf.st_mode & S_IXUSR)) {
		snprintf(errbuf, errlen, "%s doesn't look executable", argv0);
		return;
	}

	save_fd = mkstemp(save_name);
	if (save_fd < 0) {
		snprintf(errbuf, errlen, "Couldn't get temporary file");
		return;
	}

	fprintf(stderr, "Saving state data to %s...\n", save_name);
	save = fdopen(save_fd, "w");
	err = irc_save(save);
	fclose(save);

	if (irc_save(save) < 0) {
		snprintf(errbuf, errlen, "Saving IRC state data failed");
		unlink(save_name);
		return;
	}

	execl(argv0, argv0, "--state", save_name, (char*)NULL);

	err = errno;
	snprintf(errbuf, errlen, "exec failed: %s", strerror(err));
}

int restore(const char *save_name)
{
	FILE *save = fopen(save_name, "r");

	fprintf(stderr, "Restoring state data from %s...\n", save_name);

	if (save == NULL)
		return -1;

	if (irc_restore(save) < 0)
		return -1;

	return 0;
}

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
	fprintf(file, "  --irc=HOST[:PORT][/NICKSUFFIX]\n");
	fprintf(file, "              Starts an IRC bot on the specified server. The\n");
	fprintf(file, "              bot will use the nickname SKI-<NICKSUFFIX>.\n");
	fprintf(file, "              PORT defaults to 6667, NICKSUFFIX to GEN01.\n");
	fprintf(file, "  --state=FILE, -sFILE\n");
	fprintf(file, "              For internal use only. Used by the bot during\n");
	fprintf(file, "              self-exec to reload state data.\n");
	fprintf(file, "  --help, -h  Displays this help\n");
	fprintf(file, "\n");
	fprintf(file, "At least ONE action (--irc) is required\n");
}

int main(int argc, char *argv[])
{
	int c, idx;
	char buf[256];

	argv0 = argv[0];

	while ((c = getopt_long(argc, argv, "hs:", longopts, &idx)) != -1) {
		switch (c) {
		case 'i':
			action = IRC;
			snprintf(buf, 256, "%s", optarg);
			break;

		case 's':
			action = RESTORE;
			if (restore(optarg) < 0) {
				fprintf(stderr, "Update failed");
				return 1;
			}
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

		/* fallthrough */
	case RESTORE:
		action = IRC;
		io_poll();

		break;

	default:
		fprintf(stderr, "%s: no action specified\n", argv[0]);
		fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
		return 1;
	}

	return 0;
}
