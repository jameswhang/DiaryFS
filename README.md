# DiaryFS
A journaling filesystem for the Linux kernel



## What is this?
DiaryFS saves your diffs over past 5 days and keeps them all. 


## Why do I need this?
You don't need this if you don't like doing stupid things or don't like an additional degree of robustness to your file system. I would recommend it if you like doing weird stuff or surf the web in a dangerous manner :) 


## Use

```
git clone https://github.com/jameswhang/linux
make menuconfig
select "filesystems" 
select "miscellaneous filesystems"
select "DiaryFS"
```
