CC = gcc

all: vfs.o afs.o rfs.o 
	gcc -g -O0 rfs.o afs.o vfs.o -o vifs

run: all 
	./vifs

%.o: %.c
	gcc -g -Wno-error -O0 -c $< -o $@

vfs.o: vfs.c vfs.h
afs.o: afs.c afs.h vfs.h
rfs.o: rfs.c rfs.h vfs.h

clean:
	rm -f vifs
	rm -f *.o
