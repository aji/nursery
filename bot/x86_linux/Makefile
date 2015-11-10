all: foobot loader
foobot: io.S string.S entry.S irc.S
	gcc -g -o $@ $^ -nostdlib
loader: loader.c
	gcc -g -o $@ $^
clean:
	rm -f foobot loader
