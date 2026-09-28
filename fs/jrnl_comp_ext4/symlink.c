// SPDX-License-Identifier: GPL-2.0
/*
 *  linux/fs/jrnl_comp_ext4/symlink.c
 *
 * Only fast symlinks left here - the rest is done by generic code. AV, 1999
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/fs/minix/symlink.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  jrnl_comp_ext4 symlink handling code
 */

#include <linux/fs.h>
#include <linux/namei.h>
#include "jrnl_comp_ext4.h"
#include "xattr.h"

static const char *jrnl_comp_ext4_encrypted_get_link(struct dentry *dentry,
					   struct inode *inode,
					   struct delayed_call *done)
{
	struct buffer_head *bh = NULL;
	const void *caddr;
	unsigned int max_size;
	const char *paddr;

	if (!dentry)
		return ERR_PTR(-ECHILD);

	if (jrnl_comp_ext4_inode_is_fast_symlink(inode)) {
		caddr = JRNL_COMP_EXT4_I(inode)->i_data;
		max_size = sizeof(JRNL_COMP_EXT4_I(inode)->i_data);
	} else {
		bh = jrnl_comp_ext4_bread(NULL, inode, 0, 0);
		if (IS_ERR(bh))
			return ERR_CAST(bh);
		if (!bh) {
			JRNL_COMP_EXT4_ERROR_INODE(inode, "bad symlink.");
			return ERR_PTR(-EFSCORRUPTED);
		}
		caddr = bh->b_data;
		max_size = inode->i_sb->s_blocksize;
	}

	paddr = fscrypt_get_symlink(inode, caddr, max_size, done);
	brelse(bh);
	return paddr;
}

static int jrnl_comp_ext4_encrypted_symlink_getattr(struct mnt_idmap *idmap,
					  const struct path *path,
					  struct kstat *stat, u32 request_mask,
					  unsigned int query_flags)
{
	jrnl_comp_ext4_getattr(idmap, path, stat, request_mask, query_flags);

	return fscrypt_symlink_getattr(path, stat);
}

static void jrnl_comp_ext4_free_link(void *bh)
{
	brelse(bh);
}

static const char *jrnl_comp_ext4_get_link(struct dentry *dentry, struct inode *inode,
				 struct delayed_call *callback)
{
	struct buffer_head *bh;
	char *inline_link;

	/*
	 * Create a new inlined symlink is not supported, just provide a
	 * method to read the leftovers.
	 */
	if (jrnl_comp_ext4_has_inline_data(inode)) {
		if (!dentry)
			return ERR_PTR(-ECHILD);

		inline_link = jrnl_comp_ext4_read_inline_link(inode);
		if (!IS_ERR(inline_link))
			set_delayed_call(callback, kfree_link, inline_link);
		return inline_link;
	}

	if (!dentry) {
		bh = jrnl_comp_ext4_getblk(NULL, inode, 0, JRNL_COMP_EXT4_GET_BLOCKS_CACHED_NOWAIT);
		if (IS_ERR_OR_NULL(bh))
			return ERR_PTR(-ECHILD);
		if (!jrnl_comp_ext4_buffer_uptodate(bh)) {
			brelse(bh);
			return ERR_PTR(-ECHILD);
		}
	} else {
		bh = jrnl_comp_ext4_bread(NULL, inode, 0, 0);
		if (IS_ERR(bh))
			return ERR_CAST(bh);
		if (!bh) {
			JRNL_COMP_EXT4_ERROR_INODE(inode, "bad symlink.");
			return ERR_PTR(-EFSCORRUPTED);
		}
	}

	set_delayed_call(callback, jrnl_comp_ext4_free_link, bh);
	nd_terminate_link(bh->b_data, inode->i_size,
			  inode->i_sb->s_blocksize - 1);
	return bh->b_data;
}

const struct inode_operations jrnl_comp_ext4_encrypted_symlink_inode_operations = {
	.get_link	= jrnl_comp_ext4_encrypted_get_link,
	.setattr	= jrnl_comp_ext4_setattr,
	.getattr	= jrnl_comp_ext4_encrypted_symlink_getattr,
	.listxattr	= jrnl_comp_ext4_listxattr,
};

const struct inode_operations jrnl_comp_ext4_symlink_inode_operations = {
	.get_link	= jrnl_comp_ext4_get_link,
	.setattr	= jrnl_comp_ext4_setattr,
	.getattr	= jrnl_comp_ext4_getattr,
	.listxattr	= jrnl_comp_ext4_listxattr,
};

const struct inode_operations jrnl_comp_ext4_fast_symlink_inode_operations = {
	.get_link	= simple_get_link,
	.setattr	= jrnl_comp_ext4_setattr,
	.getattr	= jrnl_comp_ext4_getattr,
	.listxattr	= jrnl_comp_ext4_listxattr,
};
