/*
 * Copyright (c) 2016 James Whang
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation
 *
 * THANKSTO: 	
 * The wrapfs team @ Stony Brook University
 *  - Erez Zadok
 * 	- Shrikar Archak
 */ 

#include "diaryfs.h"

/*
 * The inode cache is used with alloc_inode for both our inode info and the
 * vfs inode.
 */

static struct kmem_cache *diaryfs_inode_cachep;

/* final actions when unmounting a file system */
static void diaryfs_put_super(struct super_block *sb) {
	struct diaryfs_sb_info *spd;
	struct super_block *s;

	spd = DIARYFS_SB(sb);
	if (!spd) {
		return;
	}

	/* decrement lower super references */
	s = diaryfs_lower_super(sb);
	diaryfs_set_lower_super(sb, NULL);
	atomic_dec(&s->s_active);

	kfree(spd);
	sb->s_fs_info = NULL;
}

static int diaryfs_statfs(struct dentry *dentry, struct kstatfs *buf) {
	int err;
	struct path lower_path;

	diaryfs_get_lower_path(dentry, &lower_path);
	err = vfs_statfs(&lower_path, buf);
	diaryfs_put_lower_path(dentry, &lower_path);

	/* set return buf to our f/s to avoid confusing user-level utils */
	buf->f_type = DIARYFS_SUPER_MAGIC;

	return err;
}

/*
 * @flags: numeric mount options
 * @options: mount options string
 */
static int diaryfs_remount_fs(struct super_block *sb, int *flags, char *options) {
	int err = 0;
	/* The VFS takes care of "ro" and "rw" flags among others. 
	 * We can accept a few flags (RDONLY, MANDLOCK) and honor
	 * SILENT, but anything else is an error.
	 */
	if ((*flags & ~(MS_RDONLY | MS_MANDLOCK | MS_SILENT)) != 0) {
		printk(KERN_ERR
				"wrapfs: remount flags 0x%x unsupported\n", *flags);
		err = -EINVAL;
	}
	return err;
}

/* 
 * Called by iput() when the inode reference count reached zero and 
 * the inode is not hashed anywhere. Used to clear anything that 
 * needs to be, before the inode is completely destroyed and put
 * on the inode free list.
 */
static void diaryfs_evict_inode(struct inode *inode) {
	struct inode *lower_inode;

	truncate_inode_pages(&inode->i_data, 0);
	clear_inode(inode);

	/* Decrement a refernece to a lower_inode, which was incremented by 
	 * the read_inode when it was created initially
	 */
	lower_inode = diaryfs_lower_inode(inode);
	diaryfs_set_lower_inode(inode, NULL);
	iput(lower_inode);
}

static struct inode *diaryfs_alloc_inode(struct super_block *sb) {
	struct diaryfs_inode_info * inode;
	inode = kmem_cache_alloc(diaryfs_inode_cachep, GFP_KERNEL);
	if (!inode)
		return NULL; 

	/* memset everything upto the inode to 0  */
	memset(inode, 0, offsetof(struct diaryfs_inode_info, vfs_inode));

	inode->vfs_inode.i_version = 1; 
	return &inode->vfs_inode;
}

static void diaryfs_destroy_inode(struct inode *inode) {
	kmem_cache_free(diaryfs_inode_cachep, DIARYFS_I(inode));
}

/* diaryfs inode cache constructor */
static void init_once(void *obj) {
	struct diaryfs_inode_info *i = (struct diaryfs_inode_info*)obj;

	inode_init_once(&i->vfs_inode);
}

int diaryfs_init_inode_cache(void) {
	int err = 0;
	diaryfs_inode_cachep = 
		kmem_cache_create("diaryfs_inode_cache",
						  sizeof(struct diaryfs_inode_info), 
						  0,
						  SLAB_RECLAIM_ACCOUNT,
						  init_once);

	if (!diaryfs_inode_cachep)
		err = -ENOMEM;
	return err;
}

void diaryfs_destroy_inode_cache(void) {
	if (diaryfs_inode_cachep) 
		kmem_cache_destroy(diaryfs_inode_cachep);
}

/*
 * Used only in nfs, to kill any pending RPC tasks, so that subsequent
 * code can actually succeed and won't leave tasks that need handling.
 */
static void diaryfs_umount_begin(struct super_block *sb) {
	struct super_block * lower_sb;

	lower_sb = diaryfs_lower_super(sb);
	if (lower_sb && lower_sb->s_op && lower_sb->s_op->umount_begin) 
		lower_sb->s_op->umount_begin(lower_sb);
}

const struct super_operations diaryfs_sops = {
	.put_super	    = diaryfs_put_super,
	.statfs			= diaryfs_statfs,
	.remount_fs		= diaryfs_remount_fs,
	.evict_inode	= diaryfs_evict_inode,
	.umount_begin	= diaryfs_umount_begin,
	.show_options	= generic_show_options,
	.alloc_inode	= diaryfs_alloc_inode,
	.destroy_inode	= diaryfs_destroy_inode,
	.drop_inode		= generic_delete_inode,
};



























































