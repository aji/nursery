LIBS = -ledit
override CFLAGS += -Ieditline -Wall -g
%.o: %.c paren.h
	cc $(CFLAGS) -o $@ -c $<
paren: paren.o main.o
	cc $(CFLAGS) -o $@ $^ $(LIBS)
clean:
	rm -f paren paren.o main.o
