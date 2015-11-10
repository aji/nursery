CFLAGS=-g
LDFLAGS=-lrt -lpari
MAIN=build/main.o

.PHONY: all
all: build/null build/division build/pm1 build/rho build/pari build/elliptic

${MAIN}: main.c test.h
	gcc -o $@ -c main.c ${CFLAGS}

build/null: ${MAIN} null.c test.h
	gcc -o $@ ${MAIN} null.c ${CFLAGS} ${LDFLAGS}
build/division: ${MAIN} division.c test.h
	gcc -o $@ ${MAIN} division.c ${CFLAGS} ${LDFLAGS}
build/elliptic: ${MAIN} elliptic.c test.h
	gcc -o $@ ${MAIN} elliptic.c ${CFLAGS} ${LDFLAGS}
build/pari: ${MAIN} pari.c test.h
	gcc -o $@ ${MAIN} pari.c ${CFLAGS} ${LDFLAGS}
build/pm1: ${MAIN} pm1.c test.h
	gcc -o $@ ${MAIN} pm1.c ${CFLAGS} ${LDFLAGS}
build/rho: ${MAIN} rho.c test.h
	gcc -o $@ ${MAIN} rho.c ${CFLAGS} ${LDFLAGS}
