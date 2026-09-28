// SPDX-License-Identifier: GPL-2.0
/*
 * linux/fs/jrnl_comp_ext4/xattr_security.c
 * Handler for storing security labels as extended attributes.
 */

#include <linux/string.h>
#include <linux/fs.h>
#include <linux/security.h>
#include <linux/slab.h>
#include "jrnl_comp_ext4_jrnl_comp_jbd2.h"
#include "jrnl_comp_ext4.h"
#include "xattr.h"

static int
jrnl_comp_ext4_xattr_security_get(const struct xattr_handler *handler,
			struct dentry *unused, struct inode *inode,
			const char *name, void *buffer, size_t size)
{
	return jrnl_comp_ext4_xattr_get(inode, JRNL_COMP_EXT4_XATTR_INDEX_SECURITY,
			      name, buffer, size);
}

static int
jrnl_comp_ext4_xattr_security_set(const struct xattr_handler *handler,
			struct mnt_idmap *idmap,
			struct dentry *unused, struct inode *inode,
			const char *name, const void *value,
			size_t size, int flags)
{
	return jrnl_comp_ext4_xattr_set(inode, JRNL_COMP_EXT4_XATTR_INDEX_SECURITY,
			      name, value, size, flags);
}

static int
jrnl_comp_ext4_initxattrs(struct inode *inode, const struct xattr *xattr_array,
		void *fs_info)
{
	const struct xattr *xattr;
	jrnl_comp_handle_t *handle = fs_info;
	int err = 0;

	for (xattr = xattr_array; xattr->name != NULL; xattr++) {
		err = jrnl_comp_ext4_xattr_set_handle(handle, inode,
					    JRNL_COMP_EXT4_XATTR_INDEX_SECURITY,
					    xattr->name, xattr->value,
					    xattr->value_len, XATTR_CREATE);
		if (err < 0)
			break;
	}
	return err;
}

int
jrnl_comp_ext4_init_security(jrnl_comp_handle_t *handle, struct inode *inode, struct inode *dir,
		   const struct qstr *qstr)
{
	return security_inode_init_security(inode, dir, qstr,
					    &jrnl_comp_ext4_initxattrs, handle);
}

const struct xattr_handler jrnl_comp_ext4_xattr_security_handler = {
	.prefix	= XATTR_SECURITY_PREFIX,
	.get	= jrnl_comp_ext4_xattr_security_get,
	.set	= jrnl_comp_ext4_xattr_security_set,
};
