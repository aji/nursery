#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/select.h>
#include <stdbool.h>
#include "io.h"

#define MAXFD 64

static struct iofd fds[MAXFD];

struct iofd *iofd_alloc(int fd, void *priv)
{
	int i;

	for (i=0; i<MAXFD; i++) {
		if (fds[i].flags & IOFD_INUSE)
			continue;
		fds[i].fd = fd;
		fds[i].flags = IOFD_INUSE;
		fds[i].recv = fds[i].send = fds[i].post = NULL;
		fds[i].priv = priv;
		return fds + i;
	}

	return NULL;
}

void iofd_delete(struct iofd *iofd)
{
	iofd->flags = 0;
}

void io_poll_once(void)
{
	int i, nfds;
	fd_set r, w;

	FD_ZERO(&r);
	FD_ZERO(&w);

	nfds = 0;
	for (i=0; i<MAXFD; i++) {
		if (!(fds[i].flags & IOFD_INUSE))
			continue;

		if (fds[i].flags & IOFD_DORECV)
			FD_SET(fds[i].fd, &r);
		if (fds[i].flags & IOFD_DOSEND)
			FD_SET(fds[i].fd, &w);

		if (fds[i].fd > nfds)
			nfds = fds[i].fd;
	}

	if (select(nfds + 1, &r, &w, NULL, NULL) < 0)
		return;

	for (i=0; i<MAXFD; i++) {
		if (!(fds[i].flags & IOFD_INUSE))
			continue;

		if (FD_ISSET(fds[i].fd, &r) && fds[i].recv)
			fds[i].recv(fds + i);
		if (FD_ISSET(fds[i].fd, &w) && fds[i].send)
			fds[i].send(fds + i);
	}

	for (i=0; i<MAXFD; i++) {
		if (!(fds[i].flags & IOFD_INUSE) || !fds[i].post)
			continue;
		fds[i].post(fds + i);
	}
}

static int polling = 0;

void io_poll_break(void)
{
	polling = 0;
}

void io_poll(void)
{
	for (polling=1; polling; )
		io_poll_once();
}

void linebuf_destroy(struct linebuf *line)
{
	fprintf(stderr, "destroy linebuf %p\n", line->priv);
	if (line->event)
		line->event(line, LINEBUF_CLEANUP);
	iofd_delete(line->fd);
	free(line);
}

void linebuf_recv(struct iofd *fd)
{
	char buf[LINEBUF_SIZE];
	char *p, *s;
	struct linebuf *line = fd->priv;
	ssize_t sz, len;

	if (line->closing)
		return;

	if (line->inlen >= LINEBUF_SIZE) {
		if (line->event)
			line->event(line, LINEBUF_FULL);
		linebuf_shutdown(line);
		return;
	}

	sz = read(fd->fd, line->inbuf + line->inlen,
	          LINEBUF_SIZE - line->inlen - 1);

	if (sz <= 0) {
		if (sz < 0) {
			perror("linebuf_recv");
			linebuf_destroy(line);
		} else {
			if (line->event)
				line->event(line, LINEBUF_END_OF_STREAM);
			linebuf_shutdown(line);
		}
		return;
	}

	line->inlen += sz;

	while (line->inlen != 0) {
		p = memchr(line->inbuf, '\n', line->inlen);
		s = memchr(line->inbuf, '\r', line->inlen);
		if (!s || (p && p < s))
			s = p;
		if (s == NULL)
			break;

		len = sz = s - line->inbuf;

		while (*s && (*s == '\r' || *s == '\n'))
			*s++ = '\0';

		memcpy(buf, line->inbuf, sz);
		buf[sz] = '\0';
		sz = s - line->inbuf;
		memmove(line->inbuf, s, LINEBUF_SIZE - sz);
		line->inlen -= sz;

		line->cb(line, buf, len);
	}
}

void linebuf_send(struct iofd *fd)
{
	struct linebuf *line = fd->priv;
	ssize_t sz;

	sz = write(fd->fd, line->outbuf, line->outlen);

	if (sz < 0) {
		perror("linebuf_send");
		linebuf_destroy(line);
		return;
	}

	memmove(line->outbuf, line->outbuf + sz, LINEBUF_SIZE - sz);
	line->outlen -= sz;
}

void linebuf_post(struct iofd *fd)
{
	struct linebuf *line = fd->priv;

	fd->flags &= ~(IOFD_DORECV | IOFD_DOSEND);

	if (!line->closing)
		fd->flags |= IOFD_DORECV;
	if (line->outlen > 0)
		fd->flags |= IOFD_DOSEND;

	if (line->closing && line->outlen == 0)
		linebuf_destroy(line);
}

struct linebuf *linebuf_wrap(int fildes, linebuf_line_cb_t *cb,
                             linebuf_event_cb_t *event, void *priv)
{
	struct linebuf *line;
	struct iofd *fd;

	if (cb == NULL)
		return NULL;

	line = malloc(sizeof(*line));
	if (line == NULL)
		return NULL;

	fd = iofd_alloc(fildes, line);
	if (fd == NULL) {
		free(line);
		return NULL;
	}

	line->fd = fd;
	line->closing = false;

	line->inlen = 0;
	line->outlen = 0;

	line->cb = cb;
	line->event = event;
	line->priv = priv;

	fd->flags |= IOFD_DORECV;

	fd->recv = linebuf_recv;
	fd->send = linebuf_send;
	fd->post = linebuf_post;

	return line;
}

void linebuf_shutdown(struct linebuf *line)
{
	fprintf(stderr, "shutdown linebuf %p (%p)\n", line, line->priv);
	line->closing = true;
}

void println(struct linebuf *line, const char *fmt, ...)
{
	char *s, buf[65536];
	va_list va;
	int sz;

	va_start(va, fmt);
	sz = vsnprintf(buf, 65536, fmt, va);
	va_end(va);

	fprintf(stderr, "<- %s$\n", buf);

	if (sz > LINEBUF_SIZE - line->outlen - 2)
		sz = LINEBUF_SIZE - line->outlen - 2;

	memcpy(line->outbuf + line->outlen, buf, sz);
	line->outlen += sz + 2;
	s = line->outbuf + line->outlen - 2;
	*s++ = '\r';
	*s++ = '\n';
}


static void pokeq_recv(struct iofd *fd)
{
	struct pokeq *q = fd->priv;
	int data;

	read(fd->fd, &data, sizeof(data));
	q->cb(q->priv, data);
}

struct pokeq *pokeq_new(pokeq_cb_t *cb, void *priv)
{
	struct pokeq *q = NULL;
	int fds[2];

	if (pipe(fds) < 0)
		return NULL;

	q = malloc(sizeof(*q));
	if (q == NULL)
		goto fail;

	q->r = iofd_alloc(fds[0], q);
	if (q->r == NULL)
		goto fail;
	q->r->recv = pokeq_recv;
	q->r->flags |= IOFD_DORECV;

	q->w = fds[1];

	q->cb = cb;
	q->priv = priv;

	return q;

fail:
	if (q != NULL)
		free(q);
	close(fds[0]);
	close(fds[1]);
	return NULL;
}

void pokeq_delete(struct pokeq *q)
{
	close(q->r->fd);
	close(q->w);
	iofd_delete(q->r);
	free(q);
}

void poke(struct pokeq *q, int data)
{
	write(q->w, &data, sizeof(data));
}
