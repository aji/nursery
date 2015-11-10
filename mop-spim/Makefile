CC=gcc
LEX=flex
YACC=bison
CFLAGS=-Wall -g -O0
LDFLAGS=-Wall

SFILES= \
	pascal.tab.c \
	pascal.yy.c \
	container.c \
	dump.c \
	semantic.c \
	type.c \
	error.c \
	cfg.c \
	cfg_vnum.c \
	cfg_ert.c \
	cg_common.c \
	cg_mips.c \
	main.c
HFILES= \
	cfg.h \
	cg.h \
	container.h \
	dump.h \
	error.h \
	shared.h \
	type.h

SRCDIR=./src
OBJDIR=./obj
SRC=$(addprefix $(SRCDIR)/,$(SFILES))
HDR=$(addprefix $(SRCDIR)/,$(HFILES))
OBJ=$(patsubst $(SRCDIR)%.c,$(OBJDIR)%.o,$(SRC))
BIN=a.out

all: $(OBJ)
	$(CC) $(LDFLAGS) -o $(BIN) $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(HDR)
	$(CC) $(CFLAGS) -c -o $@ $<

%.yy.c: %.l %.y
	$(LEX) -o$@ $<
%.tab.c: %.y
	$(YACC) --defines=src/y.tab.h -o $@ $<

$(OBJ): | $(OBJDIR)

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(BIN)

new: clean all

testall:
	cd test; make

testclean:
	cd test; make clean

testnew: testclean testall
