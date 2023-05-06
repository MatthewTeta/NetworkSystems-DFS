$(CC) = gcc
$(CFLAGS) = -Wall -Wextra -g -O0
$(OBJDIR) = obj

all: clean dfc dfs

dfc: dfc.c
	$(CC) $(CFLAGS) -o $@ $^

dfs: dfs.c
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $(OBJDIR)$@

.PHONY: clean

clean:
	rm -f dfc dfs $(OBJDIR)/*.o
