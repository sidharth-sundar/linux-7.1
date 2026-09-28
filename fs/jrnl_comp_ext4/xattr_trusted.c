// SPDX-License-Identifier: GPL-2.0
/*
 * linux/fs/jrnl_comp_ext4/xattr_trusted.c
 * Handler for trusted extended attributes.
 *
 * Copyright (C) 2003 by Andreas Gruenbacher, <a.gruenbacher@computer.org>
 */

#include <linux/string.h>
#include <linux/capability.h>
#include <linux/fs.h>
#include "jrnl_comp_ext4_jrnl_comp_jbd2.h"
#include "jrnl_comp_ext4.h"
#include "xattr.h"

static bool
jrnl_comp_ext4_xattr_trusted_list(struct dentry *dentry)
{
	return capable(CAP_SYS_ADMIN);
}

static int
jrnl_comp_ext4_xattr_trusted_get(const struct xattr_handler *handler,
		       struct dentry *unused, struct inode *inode,
		       const char *name, void *buffer, size_t size)
{
	return jrnl_comp_ext4_xattr_get(inode, JRNL_COMP_EXT4_XATTR_INDEX_TRUSTED,
			      name, buffer, size);
}

static int
jrnl_comp_ext4_xattr_trusted_set(const struct xattr_handler *handler,
		       struct mnt_idmap *idmap,
		       struct dentry *unused, struct inode *inode,
		       const char *name, const void *value,
		       size_t size, int flags)
{
	return jrnl_comp_ext4_xattr_set(inode, JRNL_COMP_EXT4_XATTR_INDEX_TRUSTED,
			      name, value, size, flags);
}

const struct xattr_handler jrnl_comp_ext4_xattr_trusted_handler = {
	.prefix	= XATTR_TRUSTED_PREFIX,
	.list	= jrnl_comp_ext4_xattr_trusted_list,
	.get	= jrnl_comp_ext4_xattr_trusted_get,
	.set	= jrnl_comp_ext4_xattr_trusted_set,
};
