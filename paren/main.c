// Copyright (c) 2015 Alex Iadicicco <alex@ajitek.net>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "paren.h"

static char *memdup(char *buf, size_t sz) {
	char *n = malloc(sz);
	memcpy(n, buf, sz);
	return n;
}

static void interactive(ReReadContext *rd, EvalContext *ev) {
	Node *n;
	char *line;
	char buf[65536];
	int len;

	int deficit = 0;
	int allocs, frees;

	for (;;) {
		line = readline(pa_reread_empty(rd) ? "Pa> " : " .. ");
		if (line == NULL)
			break;

		if (*line == '.') {
			if (!strcmp(line, ".deficit"))
				deficit = !deficit;
			continue;
		}

		len = snprintf(buf, 65536, "%s\n", line);
		pa_reread_put(rd, strdup(buf), len);
		free(line);

		allocs = pa_allocs;
		frees = pa_frees;
		while ((n = pa_reread(rd)) != NULL) {
			pa_print(pa_eval(ev, n));
			printf("\n");
		}

		if (deficit)
			printf("deficit: %d\n", allocs - frees);
	}

	printf("\n");
}

static void script(ReReadContext *rd, EvalContext *ev, int fd) {
	Node *n;
	char buf[65536];
	ssize_t sz;

	while (!pa_reread_finished(rd)) {
		sz = read(fd, buf, 65536);
		if (sz == 0) {
			pa_reread_eof(rd);
		} else if (sz < 0) {
			perror("read");
			exit(1);
		} else {
			pa_reread_put(rd, memdup(buf, sz), sz);
		}

		while ((n = pa_reread(rd)) != NULL) {
			n = pa_eval(ev, n);
			if (n->tag == T_EXCEPTION) {
				printf("exception: %.*s\n",
				       n->exc.msg->len, n->exc.msg->buf);
				return;
			}
			DECREF(n);
		}
	}
}

int main(int argc, char *argv[]) {
	ReReadContext rd;
	EvalContext ev;
	char *filename = NULL;
	bool iact = false;
	int c;

	while ((c = getopt(argc, argv, "f:i")) != -1) switch (c) {
	case 'f':
		filename = optarg;
		break;

	case 'i':
		iact = true;
		break;

	default:
		return 1;
	}

	pa_reread_init(&rd);
	pa_eval_init(&ev);

	pa_set_args(argc - optind + 1, argv + optind - 1);

	pa_add_module(&ev, pa_mod_core(), "core");
	pa_add_module(&ev, pa_mod_io(),   "io");
	pa_add_module(&ev, pa_mod_net(),  "net");

	if (filename != NULL) {
		int fd = open(filename, O_RDONLY);
		script(&rd, &ev, fd);
		close(fd);
	}

	pa_reread_init(&rd);

	if (iact || filename == NULL)
		interactive(&rd, &ev);

	return 0;
}
