DIARYFS_VERSION="0.1"

EXTRA_CFLAGS += -DWRAPFS_VERSION=\"$(WRAPFS_VERSION)\"

obj-$(CONFIG_DIARY_FS) += diaryfs.o

diaryfs-y := dentry.o file.o inode.o main.o super.o lookup.o mmap.o
