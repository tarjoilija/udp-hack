CFLAGS=-Wall -m32
CC=gcc

all: wrapper.so

wrapper.o: wrapper.c
	$(CC) $(CFLAGS) -c -fPIC wrapper.c

wrapper.so: wrapper.o
	$(CC) $(CFLAGS) -shared -rdynamic -o wrapper.so wrapper.o -ldl -llua -lm

clean:
	rm wrapper.o wrapper.so
