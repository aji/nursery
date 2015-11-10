BIN = alonzo
OBJ = lambda.o alonzo.o alias.o
HEADER = lambda.h alias.h

$(BIN): $(OBJ) $(HEADER)
	gcc -g -o $@ $(OBJ)
%.o: %.c $(HEADER)
	gcc -Wall -O2 -g -c -o $@ $<

clean:
	rm $(OBJ) $(BIN)
