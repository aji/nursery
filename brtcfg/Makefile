# I'm really using this mostly as a way to keep build rules in one place
brtcfg: brtcfg.c
	gcc -o brtcfg brtcfg.c -lm

install: brtcfg
	cp brtcfg /usr/local/bin
	chmod +s /usr/local/bin/brtcfg
