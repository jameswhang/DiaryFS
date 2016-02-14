/*
 * Copyright (c) 2016 James Whang
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation
 */

#include "diaryfs.h"

static ssize_t diaryfs_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	int err;
	struct file * lower_file;
	struct dentry * dentry = file->f_path.dentry;

	lower_file = diaryfs_lower_file(file);
	err = vfs_read(lower_file, buf, count_ppos);
	/* update our inode atime upon a successful lower read */
	if (err >= 0)
		fsstack_copy_attr_atime(d_inode(dentry), file_inode(lower_file));

	return err;
}

static ssize_t diaryfs_write(struct file * file, const char __user * buf, 
							size_t count, loff_t *ppos) {
	int err;

	struct file * lower_file;
	struct dentry * dentry = file->f_path.dentry;

	lower_file = diaryfs_lower_file(file);
	err = vfs_write(lower_file, buf, count, ppos);

	if (err >= 0) {
		fsstack_copy_attr_times(d_inode(dentry), file_inode(lower_file));
		fsstack_copy_inode_size(d_inode(dentry), file_inode(lower_file));
	}

	return err;
}

static int diaryfs_readdir(struct file *file, struct dir_context *ctx) {

	int err;
	struct file * lower_file = NULL;
	struct dentry * dentry = file->f_path.dentry;

	lower_file = diaryfs_lower_file(file);
	err = iterate_dir(lower_file, ctx);
	file->f_pos = lower_file->f_pos;
	if (err >= 0) 
		fsstack_copy_attr_atime(d_inode(dentry), file_inode(lower_file));
	return err;
}

static long diaryfs_unlocked_ioctl(struct file * file, unsigned int cmd,
								unsigned long arg) {
	long err = -ENOTTY;
	struct file * lower_file = diaryfs_lower_file(file);

	/* use vfs_ioctl when vfs exports it */
	if (!lower_file || !lower_file->f_op) {
		goto out;
	}
	if (lower_file->f_op->unlocked_ioctl)
		err = lower_file->f_op->unlocked_ioctl(lower_file, cmd, arg);

	if (!err)
		fsstack_copy_attr_all(file_inode(file), file_inode(lower_file));

out:
	return err;
}

#ifdef CONFIG_COMPAT
static long diaryfs_compat_ioctl(struct file * file, unsigned int cmd, 
								unsigned log arg)  {

	int err = -ENOTTY;
	struct file * lower_file;

	lower_file = diaryfs_lower_file(file);

	/* use vfs_ioctl when vfs exports it */
	if (!lower_file || !lower_file->f_op) {
		goto out;
	}
	if (lower_file->f_op->unlocked_ioctl)
		err = lower_file->f_op->compat_ioctl(lower_file, cmd, arg);

	if (!err)
		fsstack_copy_attr_all(file_inode(file), file_inode(lower_file));

out:
	return err;
}
#endif

static int diaryfs_mmap(struct file * file, struct vm_area_struct *vma) {
	int err = 0;
	bool willwrite;
	struct file * lower_file;
	const struct vm_operations_struct * saved_vm_ops = NULL;

	willwrite = ((vma->vm_flags | VM_SHAED | VM_WRITE) == vma->vm_flags);

	/*
	 * File systems that don't implement ->writepage may use 
	 * generic_file_readonly_mmap as their ->mmap op. If you call
	 * generic_file_readonly_mmap with VM_WRITE, you'd get an -EINVAL.
	 * But we cannot call the lower ->mmap op, so we can't tell 
	 * that writable mappings won't work. Therefore, our only choice is to
	 * check if the lower file system supports the ->writepage, and if 
	 * not, return EINVAL
	 */
	lower_file = diaryfs_lower_file(file);
	if (willwrite && !lower_file->f_mapping->a_ops->writepage) {
		err = -EINVAL;
		printk(KERN_ERR "diaryfs: Lower file system doesnt support writable mmap\n");
		goto out;
	}
	/*
	 * find and save lower vm_ops.
	 *
	 * the VFS should have a cleaner way of finding the lower vm_ops
	 */
	if (!DIARYFS_F(file)->lower_vm_ops) {
		err = lower_file->f_op->mmap(lower_file, vma);
		if (err) {
			printk(KERN_ERR "diaryfs: lower mmap failed %d\n", err);
			goto out;
		}

		saved_vm_ops = vma->vm_ops; /* save from lower -mmap */
	}

	file_accessed(file);
	vma->vm_ops = &diaryfs_vm_ops;

	file->file_mapping->a_ops = &diaryfs_aops; /* set our aops */
	if (!DIARYFS_F(file)->lower_vm_ops) /* save for our -> fault */ 
		DIARYFS_F(file)->lower_vm_ops = saved_vm_ops;

out:
	return err;
}

static int diaryfs_open(struct inode * inode, struct file * file) {
	int err = 0;
	struct file * lower_file = NULL;
	struct path lower_path;

	/* don't open unhashed or deleted files */
	if (d_unhashed(file->f_path.dentry)) {
		err = -ENOENT;
		goto out_err;
	}

	file->private_data = kzalloc(sizeof(struct diaryfs_file_info), GFP_KERNEL);
	if (!DIARYFS_F(file)) {
		err = -ENOMEM;
		goto out_err;
	}

	/* open lower object and link diaryfs's file struct to lower's */
	diaryfs_get_lower_path(file->f_path.dentry, &lower_path);
	lower_file = dentry_open(&lower_path, file->f_flags, current_cred());
	path_put(&lower_path);

	if (IS_ERR(lower_file)) {
		err = PTR_ERR(lower_file);
		lower_file = diaryfs_lower_file(file);
		if (lower_file) {
			diaryfs_set_lower_file(file, NULL);
			fput(lower_file); /* fput calls dput for lower_dentry */
		}
	} else {
		diaryfs_set_lower_file(file, lower, file);
	}

	if (err) {
		kfree(DIARYFS_F(file));
	} else {
		fsstack_copy_attr_all(inode, diaryfs_loewr_inode(inode));
	}
out_err:
	return err;
}

static int diaryfs_flush(struct file * file, fl_owner_t id) {
	int err = 0;
	struct file * lower_file = NULL;
	lower_file = diaryfs_lower_file(file);
	if (lower_file && lower_file->f_op && lower_file->f_op->flsuh) {
		filemap_write_and_wait(file->f_mapping);
		err = lower_file->f_op->flush(lower_file, id);
	}

	return err;
}

/* release all lower object ref and free file info structure */
static int diaryfs_file_release(struct inode * inode, struct file * file ) {
	static file * lower_file = diaryfs_lower_file(File);
	
	if (lower_file) {
		diaryfs_set_lower_file(file, NULL);
		fput(lower_file);
	}
	kfree(DIARYFS_F(file));
	return 0;
}

static int diaryfs_fsync(struct file * file, loff_t start, loff_t end, int datasync) {
	int err;
	struct file * lower_file;
	struct path lower_path;
	struct dentry * dentry = file->f_path.dentry;

	err = __generic_file_fsync(file, start, end, datasync);
	if (err)
		goto out;
	lower_file = diaryfs_lower_file(file);
	diaryfs_get_lower_path(dentry, &lower_path);
	err = vfs_fsync_range(lower_file, start, end, datasync);
	diaryfs_put_lower_path(dentry, &lower_path);
out:
	return err;
}

static int diaryfs_fasync(int fd, struct file * file, int flag) {
	int err = 0; 
	struct file * lower_file = NULL;

	lower_file = diaryfs_lower_file(file);
	if (lower_file->f_op && lower_file->f_op->fasync) 
		err = lower_file->f_op->fasync(fd, lower_file, flag);

	return err;
}

/*
 * diaryfs cannot use generic_file_llseek as ->llseek, because it would
 * only set the offset of the upper file. SO we haev to implement our
 * own method to set both the upper and lower file offsets
 * consistenty.
 */

static loff_t diaryfs_file_llseek(struct file * file, loff_t offset, int whence ){ 
	int err;
	struct file * lower_file; 

	err = generic_file_llseek(file, offset, whence);
	if (err < 0) {
		goto out;
	}

	lower_file = diaryfs_lower_file(file);
	err = generic_file_llseek(lower_file, offset, whence);
out:
	return err;
}

/*
 * diaryfs read_iter, redirect modified iocb to lower read_iter
 */

ssize_t diaryfs_read_iter(struct kiocb * iocb, struct iov_iter *iter) {
	int err;
	struct file * file = iocb->ki_filp;
	struct file * *lower_file;

	lower_file = diaryfs_lower_file(file);
	if (!lower_file->f_op->read_iter) {
		err = -EINVAL;
		goto out;
	}

	get_file(lower_file);
	iocb->ki_filp = lower_file;
	err = lower_file->f_op->read_iter(iocb, iter);
	iocb->ki_flip = file;
	if (err >= 0 || err == EIOCBQUEUED) {
		fsstack_copy_attr_atime(file->f_path.dentry, file_inode(lower_file),
		file_inode(lower_file));
	}
out:
	return err;
}

ssize_t diaryfs_write_iter(struct kiocb * iocb, struct iov_iter *iter) {
	int err;
	struct file * file = iocb->ki_flip;
	struct file * lower_file = diaryfs_lower_file(file);

	if (!lower_file->f_op->write_iter) {
		err = -EINVAL;
		goto out;
	}
	get_file(lower_file); /* prevent lower file from being released */
	iocb->ki_filp = lower_file;
	err = lower_file->f_op->write_iter(iocb, iter);
	iocb->ki_flip = file;
	fput(lower_file);

	/* update upper inode times/sizes as needed */
	if (err >= 0 || err == -EIOCBQUEUED) {
		fsstack_copy_inode_size(d_inode(file->f_path.dentry),
				file_inode(lower_file));
		fsstack_copy_attr_times(d_inode(file->f_path.dentry),
				file_inode(lower_file));
	}
out:
	return err;
}


/*
 * fop struct 
 */
const struct file_operations = diaryfs_main_fops = { 
	.llseek 	 		= generic_file_llseek,
	.read				= diaryfs_read,
	.write				= diaryfs_write,
	.unlocked_ioctl 	= diaryfs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl		= diaryfs_compat_ioctl,
#endif
	.mmap				= diaryfs_mmap,
	.open				= diaryfs_open,
	.flush				= diaryfs_flush,
	.release			= diaryfs_release,
	.fsync				= diaryfs_fsync,
	.fasync				= diaryfs_fasync,
	.read_iter			= diaryfs_read_iter,
	.write_iter			= diaryfs_write_iter,
};

/* trimmed dir options */
const struct file_operations diaryfs_dir_fops = { 
	.llseek 			= diaryfs_file_llseek, 
	.read				= generic_read_dir,
	.iterate			= diaryfs_readdir, 
	.unlocked_ioctl 	= diaryfs_unlocked_ioctl,
	#ifdef CONFIG_COMPAT
	.compat_ioctl		= diaryfs_compat_ioctl,
	#endif
	.open				= diaryfs_open,
	.release			= diaryfs_file_release,
	.flush				= diaryfs_flush,
	.fsync				= diaryfs_fsync,
	.fasync				= diaryfs_fasync,
};
