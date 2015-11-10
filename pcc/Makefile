CC = gcc

pcc: parse.o lexer.o main.o sem.o symtab.o
	$(CC) -g -o $@ $^
clean:
	rm -f pcc *.o
%.o: %.c
	$(CC) -g -c -o $@ $<
