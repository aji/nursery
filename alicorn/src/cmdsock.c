/*
   Alicorn -- Alex's IRC bouncer
   Copyright (C) 2012 Alex Iadicicco

   This file is part of Alicorn and is protected under the terms contained
   in the COPYING file in the project root.
 */

#include <mowgli.h>
#include <unicorn.h>

#include "alicorn.h"

/* TODO: This flag needs to be replaced with a real piece of configuration
   from somewhere. The default (and only) behavior is to just act like
   we're on UNIX */
#define SOME_UNIX_FLAG

#ifdef SOME_UNIX_FLAG

#include <sys/un.h>

#define ALICORN_CMDSOCK_PATH "/tmp/alicorn.sock"
#define CONN_BUFSIZE 32768

struct cs_conn {
	int fd;
	mowgli_eventloop_pollable_t *poll;
	char buf[CONN_BUFSIZE];
	size_t n;
};

struct cs_listener {
	int fd;
	mowgli_eventloop_pollable_t *poll;
};

static struct cs_listener *cs_listener = NULL;

static void conn_user_out(struct a_user *user, char *fmt, ...)
{
	struct cs_conn *conn = user->priv;
	va_list va;

	va_start(va, fmt);
	conn->n += vsnprintf(conn->buf + conn->n, CONN_BUFSIZE - conn->n, fmt, va);
	va_end(va);
	conn->buf[conn->n++] = '\n';
}

static struct a_user_ops conn_user_ops = {
	.out = conn_user_out,
};

static void conn_call(struct cs_conn *conn)
{
	struct a_user *user;
	char *command;
	int parc;
	char *parv[128];

	user = a_user_create(&conn_user_ops, conn);
	command = mowgli_strdup(conn->buf);
	conn->n = 0;

	parc = 0;
	parv[parc++] = strtok(command, " ");
	for (;;) {
		parv[parc] = strtok(NULL, " ");
		if (parv[parc] == NULL)
			break;
		parc++;
	}

	a_command_dispatch(user, NULL, alicorn.cmds, parc, (const char**)parv);

	a_user_destroy(user);
}

static void conn_stop(struct cs_conn *conn)
{
	if (!conn)
		return;
	if (conn->poll)
		mowgli_pollable_destroy(alicorn.eventloop, conn->poll);
	if (!(conn->fd < 0))
		close(conn->fd);
	mowgli_free(conn);
}

static void conn_start_write(mowgli_eventloop_t *eventloop, struct cs_conn *conn);

static void conn_write(mowgli_eventloop_t *eventloop, mowgli_eventloop_io_t *io,
		mowgli_eventloop_io_dir_t dir, void *userdata)
{
	struct cs_conn *conn = userdata;
	ssize_t s;

	s = send(conn->fd, conn->buf, conn->n, 0);
	if (s < 0) {
		a_log(LERROR, "send() returned error, stopping");
		conn_stop(conn);
		return;
	}

	conn->n -= s;

	if (conn->n == 0) {
		conn_stop(conn);
		return;
	}

	memmove(conn->buf, conn->buf + s, conn->n);
}

static void conn_read(mowgli_eventloop_t *eventloop, mowgli_eventloop_io_t *io,
		mowgli_eventloop_io_dir_t dir, void *userdata)
{
	struct cs_conn *conn = userdata;
	ssize_t s;

	s = recv(conn->fd, conn->buf + conn->n, CONN_BUFSIZE - conn->n, 0);

	if (s < 0) {
		a_log(LERROR, "recv() returned error, stopping");
		conn_stop(conn);
		return;
	}

	if (s == 0) {
		conn->buf[conn->n] = '\0';
		conn_call(conn);
		conn_start_write(eventloop, conn);
		return;
	}

	conn->n += s;
}

static void conn_start_write(mowgli_eventloop_t *eventloop, struct cs_conn *conn)
{
	mowgli_pollable_setselect(eventloop, conn->poll, MOWGLI_EVENTLOOP_IO_READ, NULL);
	mowgli_pollable_setselect(eventloop, conn->poll, MOWGLI_EVENTLOOP_IO_WRITE, conn_write);
}

static void listener_stop(struct cs_listener *listener)
{
	if (!listener)
		return;
	if (listener->poll)
		mowgli_pollable_destroy(alicorn.eventloop, listener->poll);
	if (!(listener->fd < 0)) {
		unlink(ALICORN_CMDSOCK_PATH);
		close(listener->fd);
	}
	mowgli_free(listener);
	return;
}

static void listener_read(mowgli_eventloop_t *eventloop, mowgli_eventloop_io_t *io,
		mowgli_eventloop_io_dir_t dir, void *userdata)
{
	struct cs_listener *listener = userdata;
	struct cs_conn *conn;
	int fd;

	conn = mowgli_alloc(sizeof(*conn));
	conn->n = 0;

	conn->fd = accept(listener->fd, NULL, NULL);
	if (conn->fd < 0) {
		a_log(LERROR, "accept() failed, dropping CLI");
		listener_stop(listener);
		return;
	}

	conn->poll = mowgli_pollable_create(eventloop, conn->fd, conn);
	mowgli_pollable_setselect(eventloop, conn->poll, MOWGLI_EVENTLOOP_IO_READ, conn_read);
	mowgli_pollable_setselect(eventloop, conn->poll, MOWGLI_EVENTLOOP_IO_WRITE, NULL);
}

bool a_cmdsock_init(void)
{
	struct sockaddr_un un;
	char errbuf[256];

	cs_listener = mowgli_alloc(sizeof(*cs_listener));

	cs_listener->fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (cs_listener->fd < 0) {
		a_log(LERROR, "Could not create UNIX socket!");
		mowgli_free(cs_listener);
		return false;
	}

	/* 108 was in unix(7) ... */
	un.sun_family = AF_UNIX;
	snprintf(un.sun_path, 108, "%s", ALICORN_CMDSOCK_PATH);

	if (unlink(ALICORN_CMDSOCK_PATH) < 0) {
		switch (errno) {
		case EACCES:
		case EPERM:
			a_log(LERROR, "No permission to unlink Alicorn cmdsock... what?!");
			break;
		case ENOTDIR:
		case ENOENT:
			a_log(LDEBUG, "Unlink failed, but bind should succeed");
			break;
		default:
			strerror_r(errno, errbuf, 512);
			a_log(LERROR, "Unknown error ocurred when unlinking: %s", errbuf);
			break;
		}
	}

	if (bind(cs_listener->fd, (const struct sockaddr*)&un, sizeof(un)) < 0) {
		a_log(LERROR, "Could not bind to %s!", ALICORN_CMDSOCK_PATH);
		goto fail_sock;
	}

	if (chmod(ALICORN_CMDSOCK_PATH, 0600) < 0) {
		a_log(LERROR, "Could not chmod 600 %s!", ALICORN_CMDSOCK_PATH);
		goto fail_sock;
	}

	if (listen(cs_listener->fd, 5) < 0) {
		a_log(LERROR, "Could not listen!");
		goto fail_sock;
	}

	cs_listener->poll = mowgli_pollable_create(alicorn.eventloop, cs_listener->fd, cs_listener);

	if (cs_listener->poll == NULL) {
		a_log(LERROR, "Could not create pollable");
		goto fail_sock;
	}

	mowgli_pollable_setselect(alicorn.eventloop, cs_listener->poll, MOWGLI_EVENTLOOP_IO_READ, listener_read);
	mowgli_pollable_setselect(alicorn.eventloop, cs_listener->poll, MOWGLI_EVENTLOOP_IO_WRITE, NULL);

	a_log(LDEBUG, "Alicorn cmdsock listening on %s", ALICORN_CMDSOCK_PATH);

	return true;

fail_sock:
	close(cs_listener->fd);
	mowgli_free(cs_listener);
	return false;
}

void a_cmdsock_deinit(void)
{
	listener_stop(cs_listener);
}

/* These next two functions serve as the primary command line interface
   for sending commands to the command socket */

static void irc_to_ansi(char *buf, size_t n)
{
	char ansibuf[32768];
	bool embolden = true;
	size_t s, tn;

	s = tn = 0;
	while (tn < n) {
		switch (buf[tn]) {
		case '\r':
		case 1:
		case 3: /* don't know how to deal with color yet... */
			break;
		case 2:
			ansibuf[s++] = '\x1b';
			ansibuf[s++] = '[';
			ansibuf[s++] = embolden ? '1' : '0';
			ansibuf[s++] = 'm';
			embolden = !embolden;
			break;
		default:
			ansibuf[s++] = buf[tn];
		}

		tn++;
	}

	write(STDOUT_FILENO, ansibuf, s);
}

int a_cmdsock_send(int parc, char *parv[])
{
	int fd;
	char buf[32768];
	size_t n, tn;
	ssize_t s;
	struct sockaddr_un un;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		fprintf(stderr, "Failed to create a UNIX socket\n");
		return 1;
	}

	/* 108 is what was in unix(7) ... */
	un.sun_family = AF_UNIX;
	snprintf(un.sun_path, 108, "%s", ALICORN_CMDSOCK_PATH);

	if (connect(fd, (const struct sockaddr*)&un, sizeof(un)) < 0) {
		fprintf(stderr, "Failed to connect to %s\n", ALICORN_CMDSOCK_PATH);
		fprintf(stderr, "Is Alicorn running?\n");
		close(fd);
		return 1;
	}

	tn = 0;
	for (n=0; n<parc; n++) {
		tn += snprintf(buf+tn, 32768-tn, "%s", parv[n]);
		if (n+1<parc)
			tn += snprintf(buf+tn, 32768-tn, " ");
	}

	n = 0;
	do {
		s = send(fd, buf + n, tn - n, 0);
		if (s < 0) {
			fprintf(stderr, "Error on send (tn=%d n=%d s=%d)\n", tn, n, s);
			close(fd);
			return 1;
		}
		n += s;
	} while (n < tn);

	shutdown(fd, SHUT_WR);

	do {
		n = recv(fd, buf, 32768, 0);
		if (n < 0) {
			fprintf(stderr, "Error on recv\n");
			close(fd);
			return 1;
		}
		irc_to_ansi(buf, n);
	} while (n > 0);

	close(fd);

	return 0;
}

#else /* #ifdef SOME_UNIX_FLAG */

bool a_cmdsock_init(void)
{
	a_log(LWARN, "No CLI interface, it will be considerably more difficult to manage the database this way");
	return true;
}

void a_cmdsock_deinit(void)
{
	/* nothing */
}

int a_cmdsock_send(int parc, char *parv[])
{
	fprintf(stderr, "No CLI interface for Alicorn, sorry :(\n");
	return 1;
}

#endif /* SOME_UNIX_FLAG */
