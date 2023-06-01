CC=gcc

all: findeq

cimin:
	$(CC) findeq.c -pthread -DDEBUG

clean:


