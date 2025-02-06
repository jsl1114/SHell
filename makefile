CC=gcc
CFLAGS=-g -pedantic -std=gnu17 -Wall

main: main.o
	$(CC) -o main main.o

.PHONY: clean
clean: 
			rm -rf *.o main *.txt