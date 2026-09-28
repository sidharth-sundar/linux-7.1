// SPDX-License-Identifier: GPL-2.0
/*
 * linux/fs/jrnl_comp_ext4/xattr_hurd.c
 * Handler for extended gnu attributes for the Hurd.
 *
 * Copyright (C) 2001 by Andreas Gruenbacher, <a.gruenbacher@computer.org>
 * Copyright (C) 2020 by Jan (janneke) Nieuwenhuizen, <janneke@gnu.org>
 */

#include <linux/init.h>
#include <linux/string.h>
#include "jrnl_comp_ext4.h"
#include "xattr.h"

static bool
jrnl_comp_ext4_xattr_hurd_list(struct dentry *dentry)
{
	return test_opt(dentry->d_sb, XATTR_USER);
}

static int
jrnl_comp_ext4_xattr_hurd_get(const struct xattr_handler *handler,
		    struct dentry *unused, struct inode *inode,
		    const char *name, void *buffer, size_t size)
{
	if (!test_opt(inode->i_sb, XATTR_USER))
		return -EOPNOTSUPP;

	return jrnl_comp_ext4_xattr_get(inode, JRNL_COMP_EXT4_XATTR_INDEX_HURD,
			      name, buffer, size);
}

static int
jrnl_comp_ext4_xattr_hurd_set(const struct xattr_handler *handler,
		    struct mnt_idmap *idmap,
		    struct dentry *unused, struct inode *inode,
		    const char *name, const void *value,
		    size_t size, int flags)
{
	if (!test_opt(inode->i_sb, XATTR_USER))
		return -EOPNOTSUPP;

	return jrnl_comp_ext4_xattr_set(inode, JRNL_COMP_EXT4_XATTR_INDEX_HURD,
			      name, value, size, flags);
}

const struct xattr_handler jrnl_comp_ext4_xattr_hurd_handler = {
	.prefix	= XATTR_HURD_PREFIX,
	.list	= jrnl_comp_ext4_xattr_hurd_list,
	.get	= jrnl_comp_ext4_xattr_hurd_get,
	.set	= jrnl_comp_ext4_xattr_hurd_set,
};
