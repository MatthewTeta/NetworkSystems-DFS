CFLAGS = -Wall -Wextra -O3 -c
BINDIR = bin
INCLUDEDIR = include

all: mkdirs md5_

mkdirs:
	mkdir -p $(BINDIR) $(INCLUDEDIR)

md5_:
	cp md5/md5.h $(INCLUDEDIR)/md5.h
	$(CC) $(CFLAGS) -o $(BINDIR)/md5.o md5/md5.c

clean:
	$(RM) -rf $(BINDIR) $(INCLUDEDIR)

