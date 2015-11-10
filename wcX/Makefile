BIN = wcX
WAV = $(BIN).wav

SRC = wcX.c modular.c common.c container.c kctl.c seq.c
HDR = modular.h common.h container.h kctl.h seq.h

CFLAGS = -g -O2
LIBS = -lm

$(BIN): $(SRC) $(HDR)
	gcc $(CFLAGS) -o $@ $(SRC) $(LIBS)

clean:
	rm -f $(BIN)

$(WAV): $(BIN)
	$(BIN) | ffmpeg -t 10 -ar 48000 -ac 2 -f s16le -i /dev/stdin -y $(WAV)
play: $(WAV)
	aplay $(WAV)
