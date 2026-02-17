
# Makefile


CC = gcc
CFLAGS = -std=c99 -g -Werror -Wall

all: server subscriber


server: server.c
	$(CC) $(CFLAGS) server.c -o server -lm


subscriber: subscriber.c
	$(CC) $(CFLAGS) subscriber.c -o subscriber

.PHONY: clean run_server run_subscriber


run_server:
	./server


run_subscriber:
	./subscriber

clean:
	rm -f server subscriber