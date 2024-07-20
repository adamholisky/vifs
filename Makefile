CC = gcc
OPTS = -g -O0 -Wno-error
TARGET_OPTS =

all: vfs.o afs.o rfs.o 
	gcc $(OPTS) $(TARGET_OPTS) rfs.o afs.o vfs.o -o vifs

obj_only_for_vi: TARGET_OPTS = -DVIFS_OS
obj_only_for_vi: vfs.o afs.o rfs.o

obj_only_stage2: vfs.o afs.o rfs.o

run: all 
	./vifs

%.o: %.c
	gcc $(OPTS) $(TARGET_OPTS) -c $< -o $@

vfs.o: vfs.c vfs.h
afs.o: afs.c afs.h vfs.h
rfs.o: rfs.c rfs.h vfs.h

clean:
	rm -f vifs
	rm -f *.o
