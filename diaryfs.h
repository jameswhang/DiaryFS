/*
 * Copyright (c) 2016 James Whang
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation
 */

#ifndef __DIARYFS_H_
#define __DIARYFS_H_

#include <linux/dcache.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/seq_file.h>
#include <linux/statfs.h>
#include <linux/fs_stack.h>
#include <linux/magic.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/xattr.h>

/* The FS name */
#define DIARYFS_NAME "diaryfs"

/* diaryfs root inode number */
#define DIARYFS_ROOT_INO 1

