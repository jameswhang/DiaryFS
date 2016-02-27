# DiaryFS
A lightweight, kernel-level versioning filesystem for the Linux kernel

# This is still a work in progress!


## What is this?
DiaryFS saves your diffs over past 5 days and keeps them all. 


## Why do I need this?
You don't need this if you don't like doing stupid things or don't like an additional degree of robustness to your file system. I would recommend it if you like doing weird stuff or surf the web in a dangerous manner :) 


## Use
### As a kernel module:
```
git clone https://github.com/jameswhang/diaryfs
make
(sudo) insmod diaryfs
(sudo) mount -t diaryfs (/dev/sda2) (/temp/dir2)
```

### Using it with another fs: (RECOMMENDED)
```
git clone https://github.com/jameswhang/diaryfs
make
(sudo) insmod diaryfs
(sudo) mount -t ext4  (/dev/sda2) (/temp/dir1)
(sudo) mount -t diaryfs (/dev/sda2) (/temp/dir2)
```

### Fork of Linux kernel build with DiaryFS:
```
git clone https://github.com/jameswhang/linux
make menuconfig
select "filesystems" 
select "miscellaneous filesystems"
select "DiaryFS"
```

