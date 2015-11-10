PROG = alicorn
SRCS = config.c \
	log.c \
	main.c \
	module.c \
	account.c \
	network.c \
	client.c \
	route.c \
	cap.c \
	command.c \
	user.c \
	db.c \
	core.c \
	cmdsock.c

DISTCLEAN = autoconf.h

include ../buildsys.mk
include ../extra.mk

CFLAGS += $(libunicorn_CFLAGS) -g
LIBS += $(libunicorn_LIBS) -rdynamic

build: all
