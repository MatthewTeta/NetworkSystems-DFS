CFLAGS = -g -O0
INCLUDE = ../libraries/include
BIN = ../libraries/bin

all: clean manifest

manifest: manifest.c $(BIN)/md5.o
	$(CC) $(CFLAGS) -I$(INCLUDE) -B$(BIN) -o $@ $< $(BIN)/md5.o

clean:
	rm -f manifest

