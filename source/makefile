all:disk_init MyFS
disk_init: disk_init.c
	gcc util.c disk_init.c -o disk_init
MyFS: MyFS.c
	gcc -Wall util.c MyFS.c `pkg-config fuse3 --cflags --libs` -o MyFS

.PHONY : all
clean :
	rm -f MyFS disk_init
