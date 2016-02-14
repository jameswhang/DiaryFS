DIARYFS_VERSION="0.1"

EXTRA_CFLAGS += -DDIARYFS_VERSION=\"$(WRAPFS_VERSION)\"

obj-m += diaryfs.o

diaryfs-objs := dentry.o file.o inode.o main.o super.o lookup.o mmap.o

CFLAGS_super .o = -DDEBUG

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
