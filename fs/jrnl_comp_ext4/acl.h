// SPDX-License-Identifier: GPL-2.0
/*
  File: fs/jrnl_comp_ext4/acl.h

  (C) 2001 Andreas Gruenbacher, <a.gruenbacher@computer.org>
*/

#include <linux/posix_acl_xattr.h>

#define JRNL_COMP_EXT4_ACL_VERSION	0x0001

typedef struct {
	__le16		e_tag;
	__le16		e_perm;
	__le32		e_id;
} jrnl_comp_ext4_acl_entry;

typedef struct {
	__le16		e_tag;
	__le16		e_perm;
} jrnl_comp_ext4_acl_entry_short;

typedef struct {
	__le32		a_version;
} jrnl_comp_ext4_acl_header;

static inline size_t jrnl_comp_ext4_acl_size(int count)
{
	if (count <= 4) {
		return sizeof(jrnl_comp_ext4_acl_header) +
		       count * sizeof(jrnl_comp_ext4_acl_entry_short);
	} else {
		return sizeof(jrnl_comp_ext4_acl_header) +
		       4 * sizeof(jrnl_comp_ext4_acl_entry_short) +
		       (count - 4) * sizeof(jrnl_comp_ext4_acl_entry);
	}
}

static inline int jrnl_comp_ext4_acl_count(size_t size)
{
	ssize_t s;
	size -= sizeof(jrnl_comp_ext4_acl_header);
	s = size - 4 * sizeof(jrnl_comp_ext4_acl_entry_short);
	if (s < 0) {
		if (size % sizeof(jrnl_comp_ext4_acl_entry_short))
			return -1;
		return size / sizeof(jrnl_comp_ext4_acl_entry_short);
	} else {
		if (s % sizeof(jrnl_comp_ext4_acl_entry))
			return -1;
		return s / sizeof(jrnl_comp_ext4_acl_entry) + 4;
	}
}

#ifdef CONFIG_JRNL_COMP_EXT4_FS_POSIX_ACL

/* acl.c */
struct posix_acl *jrnl_comp_ext4_get_acl(struct inode *inode, int type, bool rcu);
int jrnl_comp_ext4_set_acl(struct mnt_idmap *idmap, struct dentry *dentry,
		 struct posix_acl *acl, int type);
extern int jrnl_comp_ext4_init_acl(jrnl_comp_handle_t *, struct inode *, struct inode *);

#else  /* CONFIG_JRNL_COMP_EXT4_FS_POSIX_ACL */
#include <linux/sched.h>
#define jrnl_comp_ext4_get_acl NULL
#define jrnl_comp_ext4_set_acl NULL

static inline int
jrnl_comp_ext4_init_acl(jrnl_comp_handle_t *handle, struct inode *inode, struct inode *dir)
{
	return 0;
}
#endif  /* CONFIG_JRNL_COMP_EXT4_FS_POSIX_ACL */

