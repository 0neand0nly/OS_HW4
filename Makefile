CC = gcc
CFLAGS = -pthread
DEBUG = -DDEBUG
all: findeq

findeq: findeq.c
	$(CC) $(CFLAGS) findeq.c -o findeq
	$(CC) $(CFLAGS) $(DEBUG) findeq.c -o findeqD

clean:
	rm -f findeq
	rm -f findeqD
