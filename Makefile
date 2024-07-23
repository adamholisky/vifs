CC = gcc
OPTS = -g -O0 -Wno-error -I./include
TARGET_OPTS =

all: vfs.o afs.o rfs.o vifs.o
	gcc $(OPTS) $(TARGET_OPTS) rfs.o afs.o vfs.o vifs.o -o vifs

obj_only_for_vi: TARGET_OPTS = -DVIFS_OS
obj_only_for_vi: vfs.o afs.o rfs.o

obj_only_stage2: vfs.o afs.o rfs.o

run: all 
	./vifs

%.o: src/%.c
	gcc $(OPTS) $(TARGET_OPTS) -c $< -o $@

vfs.o: src/vfs.c
afs.o: src/afs.c
rfs.o: src/rfs.c
vifs.o: src/vifs.c

clean:
	rm -f vifs
	rm -f *.o

#  dd if=/dev/zero of=./afs.img bs=4k iflag=fullblock,count_bytes count=200M