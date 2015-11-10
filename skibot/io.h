#ifndef __INC_IO_H__
#define __INC_IO_H__

#include <stdbool.h>
#include <unistd.h>

#define IOFD_INUSE  1
#define IOFD_DORECV 2
#define IOFD_DOSEND 4

struct iofd {
	int fd;

	unsigned flags;

	void (*recv)(struct iofd*);
	void (*send)(struct iofd*);
	void (*post)(struct iofd*);

	void *priv;
};

extern struct iofd *iofd_alloc(int fd, void *priv);
extern void iofd_delete(struct iofd*);

extern void io_poll_once(void);
extern void io_poll_break(void);
extern void io_poll(void);

#define LINEBUF_SIZE (16<<10)

#define LINEBUF_END_OF_STREAM 1
#define LINEBUF_CLEANUP 2
#define LINEBUF_FULL 3

struct linebuf;

typedef void linebuf_line_cb_t(struct linebuf*, char*, size_t);
typedef void linebuf_event_cb_t(struct linebuf*, int);

struct linebuf {
	struct iofd *fd;
	bool closing;

	char inbuf[LINEBUF_SIZE];
	ssize_t inlen;

	char outbuf[LINEBUF_SIZE];
	ssize_t outlen;

	linebuf_line_cb_t *cb;
	linebuf_event_cb_t *event;
	void *priv;
};

extern struct linebuf *linebuf_wrap(int fd, linebuf_line_cb_t *cb,
                                    linebuf_event_cb_t *event, void *priv);
extern void linebuf_destroy(struct linebuf*);
extern void linebuf_shutdown(struct linebuf*);

extern void println(struct linebuf*, const char*, ...);


typedef void pokeq_cb_t(void*, int data);

struct pokeq {
	struct iofd *r;
	int w;
	pokeq_cb_t *cb;
	void *priv;
};

extern struct pokeq *pokeq_new(pokeq_cb_t *cb, void *priv);
extern void pokeq_delete(struct pokeq *q);

extern void poke(struct pokeq *q, int data);


#endif
