DIARYFS_VERSION="0.1"

EXTRA_CFLAGS += -DWRAPFS_VERSION=\"$(WRAPFS_VERSION)\"

obj-m += diaryfs.o

diaryfs-y := dentry.o file.o inode.o main.o super.o lookup.o mmap.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
