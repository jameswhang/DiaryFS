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
 * returns: -ERRNO if error (returned to user)
 *          0: tell VFS to invalidate dentry
 *          1: dentry is valid
 */
static int diaryfs_d_revalidate(struct dentry *dentry, unsigned int flags)
{
	struct path lower_path;
	struct dentry *lower_dentry;
	int err = 1;

	if (flags & LOOKUP_RCU)
		return -ECHILD;

	diaryfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	if (!(lower_dentry->d_flags & DCACHE_OP_REVALIDATE))
		goto out;
	err = lower_dentry->d_op->d_revalidate(lower_dentry, flags);
out:
	diaryfs_put_lower_path(dentry, &lower_path);
	return err;
}

static void diaryfs_d_release(struct dentry *dentry)
{
	/* release and reset the lower paths */
	diaryfs_put_reset_lower_path(dentry);
	free_dentry_private_data(dentry);
	return;
}

const struct dentry_operations diaryfs_dops = {
	.d_revalidate	= diaryfs_d_revalidate,
	.d_release	= diaryfs_d_release,
};
