CC = gcc
CFLAGS = -Wall -Wextra -g -O0
OBJDIR = obj
SRCDIR = src

all: clean dfc dfs

dfc: $(SRCDIR)/dfc.c $(SRCDIR)/transfer.c
	$(CC) $(CFLAGS) -o $@ $^

dfs: $(SRCDIR)/dfs.c $(SRCDIR)/transfer.c
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $(OBJDIR) $@

.PHONY: clean

clean:
	rm -f dfc dfs $(OBJDIR)/*.o

