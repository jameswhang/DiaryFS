DIARYFS_VERSION="0.1"

EXTRA_CFLAGS += -DDIARYFS_VERSION=\"$(WRAPFS_VERSION)\"

obj-m += diaryfs.o

diaryfs-y := dentry.o file.o inode.o main.o super.o lookup.o mmap.o
