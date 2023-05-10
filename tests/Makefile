CFLAGS = -g -O0
INCLUDE = ../libraries/include
BIN = ../libraries/bin

all: clean manifest parse_conf

manifest: manifest.c $(BIN)/md5.o
	$(CC) $(CFLAGS) -I$(INCLUDE) -B$(BIN) -o $@ $< $(BIN)/md5.o

parse_conf: parse_conf.c
	$(CC) $(CFLAGS) -I$(INCLUDE) -B$(BIN) -o $@ $<

clean:
	rm -f manifest

