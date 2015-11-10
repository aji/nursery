CC=gcc
CFLAGS=-g -O0 -Wall
BIN=skibot
OBJS=io.o child.o irc.o main.o version.o

PKG_NAME=skibot
PKG_VERSION=0.3
PKG_BUILT=$(shell date '+%x %X %z')
PKG_VARS += -D'PKG_NAME="$(PKG_NAME)"'
PKG_VARS += -D'PKG_VERSION="$(PKG_VERSION)"'
PKG_VARS += -D'PKG_BUILT="$(PKG_BUILT)"'

$(BIN): $(OBJS)
	$(CC) -o $(BIN) $(OBJS)
.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<
version.o: version.c Makefile
	$(CC) -c $(CFLAGS) $(PKG_VARS) -o $@ $<
clean:
	rm -f $(BIN)
	rm -f $(OBJS)
