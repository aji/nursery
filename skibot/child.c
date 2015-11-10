#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/wait.h>
#include "io.h"
#include "child.h"

#define MAXCHILDREN 32

struct child children[MAXCHILDREN];

struct child *child_find(pid_t pid)
{
	int i;
	for (i=0; i<MAXCHILDREN; i++) {
		if (children[i].pid == pid)
			return &children[i];
	}
	return NULL;
}

struct child *child_alloc(void)
{
	return child_find(0);
}

void child_free(struct child *child)
{
	child->pid = 0;
	child->defunct = true;
}

static bool have_handler = false;

void on_sigchld(int sig)
{
	struct child *child;
	pid_t pid;
	int status;

	while ((pid = waitpid(0, &status, WNOHANG)) > 0) {
		child = child_find(pid);
		if (child == NULL)
			continue;
		poke(child->poke, status);
	}
}

void child_line(struct linebuf *line, char *buf, size_t sz)
{
	struct child *child = line->priv;

	if (child == NULL || child->line == NULL || child->defunct)
		return;

	buf[sz] = '\0';
	child->line(child, buf);
}

void child_shutdown(struct child *child)
{
	if (child->defunct)
		return;
	fprintf(stderr, "child_shutdown\n");
	if (child->in)
		linebuf_shutdown(child->in);
	child->out = 2;
	child->in = NULL;
	child->defunct = true;
}

void child_destroy(struct child *child)
{
	child_shutdown(child);
	fprintf(stderr, "child_destroy\n");
	if (child->poke)
		pokeq_delete(child->poke);
	child->poke = NULL;
	child->priv = NULL;
	child_free(child);
}

void child_event(struct linebuf *line, int event)
{
	struct child *child = line->priv;

	fprintf(stderr, "child_event: %d\n", event);

	if (child == NULL)
		return;

	switch (event) {
	case LINEBUF_CLEANUP:
	case LINEBUF_END_OF_STREAM:
	case LINEBUF_FULL:
		child_shutdown(child);
		break;
	}
}

void child_poke(void *priv, int data)
{
	struct child *child = priv;
	int stat = data;

	if (child == NULL)
		return;

	fprintf(stderr, "POKED! %d\n", data);

	if (WIFEXITED(stat)) {
		if (child->event)
			child->event(child, CHILD_EXITED, WEXITSTATUS(stat));
		child_destroy(child);
	} else if (WIFSIGNALED(stat)) {
		if (child->event)
			child->event(child, CHILD_SIGNALED, WTERMSIG(stat));
		child_destroy(child);
	}
}

struct child *child_spawn(const char *path, char *const argv[],
		child_line_cb_t *line, child_event_cb_t *event, void *priv)
{
	struct child *child;
	int fd_to[2], fd_fr[2];

	if (have_handler == false) {
		signal(SIGCHLD, on_sigchld);
		have_handler = true;
	}

	fd_to[0] = fd_to[1] = -1;
	fd_fr[0] = fd_fr[1] = -1;

	child = child_alloc();
	if (child == NULL)
		return NULL;
	child->defunct = false;
	child->in = NULL;
	child->poke = NULL;
	child->line = line;
	child->event = event;
	child->priv = priv;

	if (pipe(fd_to) < 0 || pipe(fd_fr) < 0)
		goto err;

	child->in = linebuf_wrap(fd_fr[0], child_line, child_event, child);
	if (child->in == NULL)
		goto err;

	child->out = fd_to[1];

	child->poke = pokeq_new(child_poke, child);
	if (child->poke == NULL)
		goto err;

	child->pid = fork();

	if (child->pid < 0)
		goto err;

	if (child->pid == 0) {
		/* TODO: close other fds? */
		close(0);
		close(1);

		fcntl(fd_to[0], F_DUPFD, 0);
		fcntl(fd_fr[1], F_DUPFD, 1);

		execvp(path, argv);

		fprintf(stderr, "Failed to spawn %s\n", path);
		exit(1);
	}

	fprintf(stderr, "Spawned with PID %d\n", child->pid);

	close(fd_to[0]);
	close(fd_fr[1]);

	return child;

err:
	if (child->in != NULL)
		linebuf_shutdown(child->in);
	if (child->poke != NULL)
		pokeq_delete(child->poke);

	if (child != NULL)
		child_free(child);

	if (fd_to[0] >= 0) {
		close(fd_to[0]);
		close(fd_to[1]);
	}
	if (fd_fr[0] >= 0) {
		close(fd_fr[0]);
		close(fd_fr[1]);
	}

	return NULL;
}

void childln(struct child *child, const char *fmt, ...)
{
	char buf[65536];
	va_list va;
	size_t sz;

	if (child->defunct)
		return;

	va_start(va, fmt);
	sz = vsnprintf(buf, 65536, fmt, va);
	va_end(va);

	write(child->out, buf, sz);
}

void child_kill(struct child *child, int signal)
{
	if (child == NULL || !child->pid)
		return;
	kill(child->pid, signal);
}
