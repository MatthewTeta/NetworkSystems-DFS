CC = gcc
CFLAGS = -Wall -Wextra -g -O0
OBJDIR = obj
SRCDIR = src
INCLUDE = ./libraries/include
BIN = ./libraries/bin

all: clean libraries dfc dfs

libraries:
	make -C libraries

dfc: $(SRCDIR)/dfc.c $(SRCDIR)/transfer.c $(BIN)/md5.o
	$(CC) $(CFLAGS) -I$(INCLUDE) -B$(BIN) -o $@ $^

dfs: $(SRCDIR)/dfs.c $(SRCDIR)/transfer.c
	$(CC) $(CFLAGS) -I$(INCLUDE) -B$(BIN) -o $@ $^

$(BIN)/md5.o:
	make -C libraries

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $(OBJDIR) $@

.PHONY: clean

clean:
	rm -f dfc dfs $(OBJDIR)/*.o

