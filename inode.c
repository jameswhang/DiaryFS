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

static int diaryfs_create(struct inode *dir, struct dentry *dentry,
							umode_t mode, bool want_excl) {

	int err;
	struct dentry * lower_dentry;
	struct dentry * lower_parent_dentry = NULL;
	struct path lower_path;

	diaryfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_parent_dentry = lock_parent(lower_dentry);

	err = vfs_create(lower_parent_dentry->d_inode, lower_dentry, mode, want_excl);
	if (err) 
		goto out;
	err = diaryfs_interpose(dentry, dir->i_sb, &lower_path);
	if (err)
		goto out;
	fsstack_copy_attr_times(dir, diaryfs_lower_inode(dir));
	fsstack_copy_inode_size(dir, lower_parent_dentry->d_inode);

out:
	unlock_dir(lower_parent_dentry);
	diaryfs_put_lower_path(dentry, &lower_path);
	return err;
}

static int diaryfs_link(struct dentry * old_dentry, struct inode *dir,
						struct dentry * new_dentry) {
	struct dentry * lower_old_dentry;
	struct dentry * lower_new_dentry;
	struct dentry * lower_dir_dentry;

	u64 file_size_save;
	int err;
	struct path lower_old_path, lower_new_path;

	file_size_save = i_size_read(old_dentry->d_inode);

	diaryfs_get_lower_path(old_dentry, &lower_old_path);
	diaryfs_get_lower_path(new_dentry, &lower_new_path);

	lower_old_dentry = lower_old_path.dentry;
	lower_new_dentry = lower_new_path.dentry;
	lower_dir_dentry = lock_parent(lower_new_dentry);

	err = vfs_link(lower_old_dentry, lower_dir_dentry->d_inode, 
				   lower_new_dentry, NULL);
	if (err || !lower_new_dentry->d_inode)
		goto out;

	err = diaryfs_interpose(new_dentry, dir->i_sb, &lower_new_path);
	if (err) 
		goto out;

	fsstack_copy_attr_times(dir, lower_new_dentry->d_inode);
	fsstack_copy_inode_size(dir, lower_new_dentry->d_inode);

	set_nlink(old_dentry->d_inode,
		(	diaryfs_lower_inode(old_dentry->d_inode))->i_nlink);
	i_size_write(new_dentry->d_inode, file_size_save);
out:
	unlock_dir(lower_dir_dentry);
	diaryfs_put_lower_path(old_dentry, &lower_old_path);
	diaryfs_put_lower_path(new_dentry, &lower_new_path);
	return err;
}

static int diaryfs_unlink(struct inode *dir, struct dentry *dentry) {
	int err;

	struct dentry * lower_dentry;
	struct inode * lower_dir_inode = diaryfs_lower_inode(dir);
	struct dentry * lower_dir_dentry;
	struct path lower_path;

	diaryfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	dget(lower_dentry);
	lower_dir_dentry = lock_parent(lower_dentry);

	err = vfs_unlink(lower_dir_inode, lower_dentry, NULL);

	/*
	 * Note: unlinking on top of NFS can cause silly-renamed files
	 * Trying to delete such files results in EBUSY from NFS 
	 * below. Silly-renamed files will get deleted by NFS later on, 
	 * so we just need to detect them here and treat EBUSY errors
	 * as if the upper file was successfully deleted.
	 */

	if (err == -EBUSY && lower_dentry->d_flags & DCACHE_NFSFS_RENAMED)
		err = 0;
	if (err)
		goto out;

	fsstack_copy_attr_times(dir, lower_dir_inode);
	fsstack_copy_inode_size(dir, lower_dir_inode);
	set_nlink(dentry->d_inode, diaryfs_lower_inode(dentry->d_inode)->i_nlink);
	dentry->d_inode->i_ctime = dir->i_ctime;
	d_drop(dentry); /* this is needed, else LTP fails */
out:
	unlock_dir(lower_dir_dentry);
	dput(lower_dentry);
	diaryfs_put_lower_path(dentry, &lower_path);
	return err;
}

static int diaryfs_symlink(struct inode *dir, struct dentry *dentry, const char * symname) {
	int err;
	struct dentry *lower_dentry;
	struct dentry *lower_parent_dentry = NULL;
	struct path lower_path; 

	diaryfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_parent_dentry = lock_parent(lower_dentry);

	err = vfs_symlink(lower_parent_dentry->d_inode, lower_dentry, symname);
	if (err)
		goto out;
	err = diaryfs_interpose(dentry, dir->i_sb, &lower_path);
	if (err)
		goto out;
	fsstack_copy_attr_times(dir, diaryfs_lower_inode(dir));
	fsstack_copy_inode_size(dir, lower_parent_dentry->d_inode);

out:
	unlock_dir(lower_parent_dentry);
	diaryfs_put_lower_path(dentry, &lower_path);
	return err;
}

static int diaryfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode) {
	int err;
	struct dentry * lower_dentry;
	struct dentry * lower_parent_dentry = NULL;
	struct path lower_path;

	diaryfs_get_lower_path(dentry, &lower_path); 
	lower_dentry = lower_path.dentry;
	lower_parent_dentry = lock_parent(lower_dentry);

	err = vfs_mkdir(lower_parent_dentry->d_inode, lower_dentry, mode);
	if (err)
		goto out;
	err = diaryfs_interpose(dentry, dir->i_sb, &lower_path);
	if (err) 
		goto out;

	fsstack_copy_attr_times(dir, diaryfs_lower_inode(dir));
	fsstack_copy_inode_size(dir, lower_parent_dentry->d_inode);

	/* update the # of links on parent directory */
	set_nlink(dir, diaryfs_lower_inode(dir)->i_nlink);

out:
	unlock_dir(lower_parent_dentry);
	diaryfs_put_lower_path(dentry, &lower_path);
	return err;
}

static int diaryfs_rmdir(struct inode * dir, struct dentry * dentry) {
	int err;
	struct dentry * lower_dentry;
	struct dentry * lower_dir_dentry;
	struct path lower_path;

	diaryfs_get_lower_path(dentry, &lower_path); 
	lower_dentry = lower_path.dentry; 
	lower_dir_dentry = lock_parent(lower_dentry);

	err = vfs_rmdir(lower_dir_dentry->d_inode, lower_dentry);
	if (err)
		goto out; 

	d_drop(dentry);

	if (dentry->d_inode)
		clear_nlink(dentry->d_inode); 

	fsstack_copy_attr_times(dir, lower_dir_dentry->d_inode); 
	fsstack_copy_inode_size(dir, lower_dir_dentry->d_inode); 
	set_nlink(dir, lower_dir_dentry->d_inode->i_nlink); 

out:
	unlock_dir(lower_dir_dentry);
	diaryfs_put_lower_path(dentry, &lower_path); 
	return err;
}

static int diaryfs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode, dev_t dev) {
	int err;
	struct dentry * lower_dentry;
	struct dentry * lower_parent_dentry = NULL;
	struct path lower_path; 

	diaryfs_get_lower_path(dentry, &lower_path); 
	lower_dentry = lower_path.dentry;
	lower_parent_dentry = lock_parent(lower_dentry); 

	err = vfs_mknod(lower_parent_dentry->d_inode, lower_dentry, mode, dev);
	if (err) 
		goto out;

	err = diaryfs_interpose(dentry, dir->i_sb, &lower_path);
	if (err) 
		goto out; 

	fsstack_copy_attr_times(dir, diaryfs_lower_inode(dir)); 
	fsstack_copy_inode_size(dir, lower_parent_dentry->d_inode); 

out:
	unlock_dir(lower_parent_dentry);
	diaryfs_put_lower_path(dentry, &lower_path);
	return err;
}

/*
 * The locking rules in diaryfs_rename are complex. We could use a simpler
 * superblock level namespace lock for renames and copy-ups 
 */
static int diaryfs_rename(struct inode * old_dir, struct dentry * old_dentry,
			struct inode * new_dir, struct dentry * new_dentry) {

	int err = 0; 
	struct dentry * lower_old_dentry = NULL;
	struct dentry * lower_new_dentry = NULL;
	struct dentry * lower_old_dir_dentry = NULL; 
	struct dentry * lower_new_dir_dentry = NULL; 
	struct dentry * trap = NULL;
	struct path lower_old_path, lower_new_path; 

	diaryfs_get_lower_path(old_dentry, &lower_old_path); 
	diaryfs_get_lower_path(new_dentry, &lower_new_path); 
	lower_old_dentry = lower_old_path.dentry;
	lower_new_dentry = lower_new_path.dentry; 
	lower_old_dir_dentry = dget_parent(lower_old_dentry);
	lower_new_dir_dentry = dget_parent(lower_new_dentry); 

	trap = lock_rename(lower_old_dir_dentry, lower_new_dir_dentry);
	/* source should not be ancestor of the target */ 
	if (trap == lower_old_dentry) {
		err = -EINVAL;
		goto out;
	}
	/* target should not be ancestor of source */
	if (trap == lower_new_dentry) {
		err = -EINVAL;
		goto out; 
	} err = vfs_rename(lower_old_dir_dentry->d_inode, lower_old_dentry, lower_new_dir_dentry->d_inode, lower_new_dentry, NULL, 0);

	if (err)
		goto out;

	fsstack_copy_attr_all(new_dir, lower_new_dir_dentry->d_inode);
	fsstack_copy_inode_size(new_dir, lower_new_dir_dentry->d_inode);
	if (new_dir != old_dir) {
		fsstack_copy_attr_all(old_dir, 
				lower_old_dir_dentry->d_inode);
		fsstack_copy_inode_size(old_dir,
				lower_old_dir_dentry->d_inode);
	}
out:
	unlock_rename(lower_old_dir_dentry, lower_new_dir_dentry);
	dput(lower_old_dir_dentry);
	dput(lower_new_dir_dentry);
	diaryfs_put_lower_path(old_dentry, &lower_old_path);
	diaryfs_put_lower_path(new_dentry, &lower_new_path);
	return err;
}

static int diaryfs_readlink(struct dentry * dentry, char __user *buf, int bufsize) {
	int err;
	struct dentry * lower_dentry;
	struct path lower_path;

	diaryfs_get_lower_path(dentry, &lower_path); 
	lower_dentry = lower_path.dentry;
	if (!lower_dentry->d_inode->i_op ||
			!lower_dentry->d_inode->i_op->readlink) {
		err = -EINVAL;
		goto out;
	}

	/*
	err = lower_dentry->d_inode->i_op->readilnk(lower_dentry, buf, bufsize);
	if (err < 0)
		goto out; /FIXME
		*/

	fsstack_copy_attr_atime(dentry->d_inode, lower_dentry->d_inode);

out:
	diaryfs_put_lower_path(dentry, &lower_path);
	return err;	
}

static const char * diaryfs_follow_link(struct dentry *dentry, void **cookie) {
	char * buf;
	int len = PAGE_SIZE, err; 
	mm_segment_t old_fs; 

	/* This is freed by the put_link method assuming a successful call */
	buf = kmalloc(len, GFP_KERNEL);
	if (!buf) {
		buf = ERR_PTR(-ENOMEM);
		return buf;
	}

	/* read the symlink, and then we will follow it */
	old_fs = get_fs(); 
	set_fs(KERNEL_DS);
	err = diaryfs_readlink(dentry, buf, len); 
	set_fs(old_fs); 
	if (err < 0) {
		kfree(buf);
		buf = ERR_PTR(err);
	} else {
		buf[err] = '\0';
	}
	return *cookie = buf;
}

static int diaryfs_permission(struct inode *inode, int mask) {
	struct inode * lower_inode;
	int err;

	lower_inode = diaryfs_lower_inode(inode);
	err = inode_permission(lower_inode, mask);
	return err;
}

static int diaryfs_setattr(struct dentry * dentry, struct iattr * attr) {
	int err;
	struct dentry * lower_dentry;
	struct inode * inode;
	struct inode * lower_inode; 
	struct path lower_path; 
	struct iattr lower_attr;

	inode = dentry->d_inode;

	/*
	 * Check if the user has permission to change the inode. 
	 * No check if this user can change the lower inode, that should
	 * happen when calling notify_change on the lower node
	 */
	err = inode_change_ok(inode, attr);
	if (err) 
		goto out_err;

	diaryfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_inode = diaryfs_lower_inode(inode); 

	/* prepare the lower struct iattr with the lower file */
	memcpy(&lower_attr, attr, sizeof(lower_attr));
	if (attr->ia_valid & ATTR_FILE) 
		lower_attr.ia_file = diaryfs_lower_file(attr->ia_file);


	/*
	 * If shrinking, first truncate upper level to cancel writing dirty
	 * pages beyond the new eof. Also, if its maxbytes is more
	 * limiting. There is no need to vmtruncate the upper level
	 * afterwards in the other cases
	 */

	if (attr->ia_valid & ATTR_SIZE) {
		err = inode_newsize_ok(inode, attr->ia_size);
		if (err)
			goto out;
		truncate_setsize(inode, attr->ia_size);
	}

	/*
	 * Mode changes for clearing setuid/setgid bits. Allow lower fs
	 * to interpret this in its own way.
	 */
	if (lower_attr.ia_valid & (ATTR_KILL_SUID | ATTR_KILL_SGID))
		lower_attr.ia_valid &= ~ATTR_MODE;

	/*
	 * Notify the lower inode
	 * We used d_inode(lower_dentry) because lower_inode may be unlinked
	 */
	mutex_lock(&lower_dentry->d_inode->i_mutex);
	err = notify_change(lower_dentry, &lower_attr, NULL);
	mutex_unlock(&lower_dentry->d_inode->i_mutex);

	if (err)
		goto out;

	/* get attrs from the lower inode */
	fsstack_copy_attr_all(inode, lower_inode);

out:
	diaryfs_put_lower_path(dentry, &lower_path);
out_err:
	return err;
}

static int diaryfs_getattr(struct vfsmount *mnt, struct dentry * dentry, struct kstat *stat) {
	int err;
	struct kstat lower_stat;
	struct path lower_path;

	diaryfs_get_lower_path(dentry, &lower_path); 
	err = vfs_getattr(&lower_path, &lower_stat);
	if (err) 
		goto out;
	fsstack_copy_attr_all(dentry->d_inode, lower_path.dentry->d_inode);
	generic_fillattr(dentry->d_inode, stat);
	stat->blocks = lower_stat.blocks;
out:
	diaryfs_put_lower_path(dentry, &lower_path);
	return err;
}

static int diaryfs_setxattr(struct dentry * dentry, const char * name, const void * value, size_t size, int flags) {
	int err;
	struct dentry * lower_dentry;
	struct path lower_path;

	diaryfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	if (!lower_dentry->d_inode->i_op->setxattr) {
		err = -EOPNOTSUPP;
		goto out;
	}
	err = vfs_setxattr(lower_dentry, name, value, size, flags);
	if (err)
		goto out;
	fsstack_copy_attr_all(dentry->d_inode, lower_path.dentry->d_inode);
out:
	diaryfs_put_lower_path(dentry, &lower_path); 
	return err;
}

static ssize_t diaryfs_getxattr(struct dentry *dentry, const char * name, void *buffer, size_t size) {
	int err;
	struct dentry * lower_dentry;
	struct path lower_path; 

	diaryfs_get_lower_path(dentry, &lower_path); 
	lower_dentry = lower_path.dentry;
	if (!lower_dentry->d_inode->i_op->getxattr) {
		err = -EOPNOTSUPP;
		goto out;
	}
	err = vfs_getxattr(lower_dentry, name, buffer, size);
	if (err)
		goto out;
	fsstack_copy_attr_atime(dentry->d_inode, lower_path.dentry->d_inode);

out:
	diaryfs_put_lower_path(dentry, &lower_path);
	return err;
}

static ssize_t diaryfs_listxattr(struct dentry * dentry, char * buffer, size_t buffer_size) {
	int err;
	struct dentry * lower_dentry;
	struct path lower_path; 

	diaryfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	if (!lower_dentry->d_inode->i_op->listxattr) {
		err = -EOPNOTSUPP;
		goto out;
	}
	err = vfs_listxattr(lower_dentry, buffer, buffer_size);
	if (err)
		goto out;
	fsstack_copy_attr_atime(dentry->d_inode, lower_path.dentry->d_inode);
out:
	diaryfs_put_lower_path(dentry, &lower_path);
	return err;
}

static int diaryfs_removexattr(struct dentry * dentry, const char * name) {
	int err;
	struct dentry * lower_dentry;
	struct path lower_path;
	diaryfs_get_lower_path(dentry, &lower_path); 
	lower_dentry = lower_path.dentry;
	if (!lower_dentry->d_inode->i_op ||
		!lower_dentry->d_inode->i_op->removexattr) {
		err = -EINVAL;
		goto out;
	}
	err = vfs_removexattr(lower_dentry, name);
	if (err) 
		goto out;
	fsstack_copy_attr_all(dentry->d_inode, lower_path.dentry->d_inode);
out:
	diaryfs_put_lower_path(dentry, &lower_path);
	return err;
}

const struct inode_operations diaryfs_symlink_iops = {
	.readlink 		= diaryfs_readlink,
	.permission 	= diaryfs_permission,
   	.follow_link 	= diaryfs_follow_link,
	.setattr		= diaryfs_setattr,
	.getattr 		= diaryfs_getattr,
	.listxattr 		= diaryfs_listxattr,
	.removexattr	= diaryfs_removexattr,
};

const struct inode_operations diaryfs_dir_iops = {
	.create 		= diaryfs_create,
	.lookup			= diaryfs_lookup,
	.link			= diaryfs_link,
	.unlink			= diaryfs_unlink,
	.symlink		= diaryfs_symlink,
	.mkdir			= diaryfs_mkdir,
	.rmdir			= diaryfs_rmdir,
	.mknod 			= diaryfs_mknod,
	.rename			= diaryfs_rename, 
	.permission		= diaryfs_permission,
	.setattr		= diaryfs_setattr,
	.getattr		= diaryfs_getattr,
	.setxattr		= diaryfs_setxattr,
	.getxattr		= diaryfs_getxattr,
	.listxattr		= diaryfs_listxattr,
	.removexattr	= diaryfs_removexattr,
};

const struct inode_operations diaryfs_main_iops = {
	.permission 	= diaryfs_permission,
	.setattr		= diaryfs_setattr,
	.getattr		= diaryfs_getattr,
	.setxattr		= diaryfs_setxattr,
	.getxattr		= diaryfs_getxattr,
	.listxattr		= diaryfs_listxattr,
	.removexattr	= diaryfs_removexattr,
};
