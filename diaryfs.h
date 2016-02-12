/*
 * Copyright (c) 2016 James Whang
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation
 *
 * This code 
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

/* useful for tracking code reachability */
#define UDBG printk(KERN_DEFAULT "DBG:%s:%s:%d\n", __FILE__, __func__, __LINE__)

/* operations vectors defined in specific files */
extern const struct file_operations diaryfs_main_fops;
extern const struct file_operations diaryfs_dir_fops;
extern const struct inode_operations diaryfs_main_iops;
extern const struct inode_operations diaryfs_dir_iops;
extern const struct inode_operations diaryfs_symlink_iops;
extern const struct super_operations diaryfs_sops;
extern const struct dentry_operations diaryfs_dops;
extern const struct address_space_operations diaryfs_aops, diaryfs_dummy_aops;
extern const struct vm_operations_struct diaryfs_vm_ops;

extern int diaryfs_init_inode_cache(void);
extern void diaryfs_destroy_inode_cache(void);
extern int diaryfs_init_dentry_cache(void);
extern void diaryfs_destroy_dentry_cache(void);
extern int new_dentry_private_data(struct dentry *dentry);
extern void free_dentry_private_data(struct dentry *dentry);
extern struct dentry *diaryfs_lookup(struct inode *dir, struct dentry *dentry,
		unsigned int flags);
extern struct inode *diaryfs_iget(struct super-block *sb,
		struct inode *lower_inode);
extern int diaryfs_interpose(struct dentry *dentry, struct super_block 8sb, struct path *lower_path);

/* file private data */
struct diaryfs_file_info {
	struct file * lower_file;
	const struct vm_operations_struct * lower_vm_ops;
};

/* diaryfs inode data in memory */
struct diaryfs_inode_info {
	struct inode *lower_inode;
	struct inode vfs_inode;
};

struct diaryfs_sb_info {
	struct super_block *lower_sb;
};

/* 
 * inode to private data
 *
 * Since we use containers and the struct inode is _inside_ the 
 * diaryfs_inode_info structure, DIARYFS_I will always (given a non-NULL
 * inode pointer), return a valid non-NULL pointer.
 */
static inline struct diaryfs_inode_info *DIARYFS_I(const struct inode *inode){
	diaryfs_inode_info * i = container_of(inode, struct(diaryfs_inode_info, vfs_inode);
	return container_of(inode, struct(wrapfs_inode_info, vfs_inode);
}

/* dentry to private data */
#define DIARYFS_D(dent) ((struct diaryfs_dentry_info*)(dent)->d_fsdata)

/* superblock to private data */
#define DIARYFS_SB(super) ((struct diaryfs_sb_info*)(super)->s_fs_info)

/* file to private data */
#define DIARYFS_F(file) ((struct diaryfs_file_info*)((file)->private_data))

/* file to lower file */
static inline struct file *diaryfs_lower_file(const struct file *f) {
	return DIARYFS_F(f)->lower_file;
}

static inline void diaryfs_set_lower_file(struct file *f, struct file *val) {
	DIARYFS_F(f)->lower_file = val;
}

/* inode to lower inode */
static inline struct inode *diaryfs_lower_inode(const struct inode *i) {
	return DIARYFS_I(i)->lower_inode;
}

static inline void diaryfs_set_lower_inode(struct inode *i, struct inode *val) {
	DIARYFS_I(i)->lower_inode = val;
}

/* superblock to lower superblock */
static inline struct super_block *diaryfs_lower_super(
		const struct super_block *sb) {
	return DIARYFS_SB(sb)->lower_sb;
}

static inline void diaryfs_set_lower_super(struct super_block *sb,
		struct super_block *val) {
	DIARYFS_SB(sb)->lower_sb = val;
}

/* path based (dentry/mnt) macros */
static inline void pathcpy(struct path *dst, const struct path *src) {
	dst->dentry = src->dentry;
	dst->mnt = src->mnt;
}

/* Returns struct path. Caller must path_put it */
static inline void diaryfs_get_lower_path(const struct dentry *dent,
		struct path *lower_path) {
	spin_lock(&DIARYFS_D(dent)->lock);
	pathcpy(lower_path, &DIARYFS_D(dent)->lower_path);
	path_get(lower_path);
	spin_unlock(&DIARYFS_D(dent)->lock);
	return;
}

static inline void diaryfs_put_lower_path(const struct dentry *dent,
			struct path *lower_path) {
	path_put(lower_path);
	return;
}

static inline void diaryfs_set_lower_path(const struct dentry *dent,
			struct path *lower_path) {
	spin_lock(&DIARYFS_D(dent)->lock);
	pathcpy(&DIARYFS_D(dent)->lower_path, lower_path);
	spin_unlock(&DIARYFS_D(dent)->lock);
	return;
}

static inline void diaryfs_reset_lower_path(const struct dentry *dent) {
	spin_lock(&DIARYFS_D(dent)->lock);
	DIARYFS_D(dent)->lower_path.dentry = NULL;
	DIARYFS_D(dent)->lower_path.mnt = NULL;
	spin_unlock(&DIARYFS_D(dent)->lock);
	return;
}

static inline void diaryfs_put_reset_lower_path(const struct dentry *dent) {
	struct path lower_path;
	spin_lock(&DIARYFS_D(dent)->lock);
	pathcpy(&lower_path, &DIARYFS_D(dent)->lower_path);
	DIARYFS_D(dent)->lower_path.dentry = NULL;
	DIARYFS_D(dent)->lower_path.mnt = NULL;
	spin_unlock(&DIARYFS_D(dent)->lock);
	path_put(&lower_path);
	return;
}


/* locking helpers */
static inline struct dentry *lock_parent (struct dentry *dentry) {
	struct dentry *dir = dget_parent(dentry);
	mutex_lock_nested(&d_inode(dir)->i_mutex, I_MUTEX_PARENT);
	return dir;
}

static inline void unlock_dir(struct dentry *dir) {
	mutex_unlock(&d_inode(dir)->i_mutex);
	dput(dir);
}

#endif
