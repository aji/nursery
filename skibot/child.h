#ifndef __INC_CHILD_H__
#define __INC_CHILD_H__

#include <unistd.h>
#include "io.h"

#define CHILD_FORGETTING 0
#define CHILD_EXITED 1
#define CHILD_SIGNALED 2

struct child;

typedef void child_line_cb_t(struct child*, char*);
typedef void child_event_cb_t(struct child*, int, int);

struct child {
	pid_t pid;
	bool defunct;

	struct linebuf *in;
	int out;
	struct pokeq *poke;

	child_line_cb_t *line;
	child_event_cb_t *event;
	void *priv;
};

/* execvp */
extern struct child *child_spawn(const char *path, char *const argv[],
		child_line_cb_t *line, child_event_cb_t *event, void *priv);
extern void childln(struct child *child, const char *fmt, ...);
extern void child_kill(struct child *child, int signal);

#endif
