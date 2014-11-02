CFLAGS=-std=c++11 -fPIC -m32
CC=g++

all:
	$(CC) $(CFLAGS) -shared -o main.so main.cpp

clean:
	rm main.so
