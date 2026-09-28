// SPDX-License-Identifier: GPL-2.0
/*
  File: fs/jrnl_comp_ext4/xattr.h

  On-disk format of extended attributes for the jrnl_comp_ext4 filesystem.

  (C) 2001 Andreas Gruenbacher, <a.gruenbacher@computer.org>
*/

#include <linux/xattr.h>

/* Magic value in attribute blocks */
#define JRNL_COMP_EXT4_XATTR_MAGIC		0xEA020000

/* Maximum number of references to one attribute block */
#define JRNL_COMP_EXT4_XATTR_REFCOUNT_MAX		1024

/* Name indexes */
#define JRNL_COMP_EXT4_XATTR_INDEX_USER			1
#define JRNL_COMP_EXT4_XATTR_INDEX_POSIX_ACL_ACCESS	2
#define JRNL_COMP_EXT4_XATTR_INDEX_POSIX_ACL_DEFAULT	3
#define JRNL_COMP_EXT4_XATTR_INDEX_TRUSTED		4
#define	JRNL_COMP_EXT4_XATTR_INDEX_LUSTRE			5
#define JRNL_COMP_EXT4_XATTR_INDEX_SECURITY	        6
#define JRNL_COMP_EXT4_XATTR_INDEX_SYSTEM			7
#define JRNL_COMP_EXT4_XATTR_INDEX_RICHACL		8
#define JRNL_COMP_EXT4_XATTR_INDEX_ENCRYPTION		9
#define JRNL_COMP_EXT4_XATTR_INDEX_HURD			10 /* Reserved for Hurd */

struct jrnl_comp_ext4_xattr_header {
	__le32	h_magic;	/* magic number for identification */
	__le32	h_refcount;	/* reference count */
	__le32	h_blocks;	/* number of disk blocks used */
	__le32	h_hash;		/* hash value of all attributes */
	__le32	h_checksum;	/* crc32c(uuid+blknum+xattrblock) */
	__u32	h_reserved[3];	/* zero right now */
};

struct jrnl_comp_ext4_xattr_ibody_header {
	__le32	h_magic;	/* magic number for identification */
};

struct jrnl_comp_ext4_xattr_entry {
	__u8	e_name_len;	/* length of name */
	__u8	e_name_index;	/* attribute name index */
	__le16	e_value_offs;	/* offset in disk block of value */
	__le32	e_value_inum;	/* inode in which the value is stored */
	__le32	e_value_size;	/* size of attribute value */
	__le32	e_hash;		/* hash value of name and value */
	char	e_name[];	/* attribute name */
};

#define JRNL_COMP_EXT4_XATTR_PAD_BITS		2
#define JRNL_COMP_EXT4_XATTR_PAD		(1<<JRNL_COMP_EXT4_XATTR_PAD_BITS)
#define JRNL_COMP_EXT4_XATTR_ROUND		(JRNL_COMP_EXT4_XATTR_PAD-1)
#define JRNL_COMP_EXT4_XATTR_LEN(name_len) \
	(((name_len) + JRNL_COMP_EXT4_XATTR_ROUND + \
	sizeof(struct jrnl_comp_ext4_xattr_entry)) & ~JRNL_COMP_EXT4_XATTR_ROUND)
#define JRNL_COMP_EXT4_XATTR_NEXT(entry) \
	((struct jrnl_comp_ext4_xattr_entry *)( \
	 (char *)(entry) + JRNL_COMP_EXT4_XATTR_LEN((entry)->e_name_len)))
#define JRNL_COMP_EXT4_XATTR_SIZE(size) \
	(((size) + JRNL_COMP_EXT4_XATTR_ROUND) & ~JRNL_COMP_EXT4_XATTR_ROUND)

#define IHDR(inode, raw_inode) \
	((struct jrnl_comp_ext4_xattr_ibody_header *) \
		((void *)raw_inode + \
		JRNL_COMP_EXT4_GOOD_OLD_INODE_SIZE + \
		JRNL_COMP_EXT4_I(inode)->i_extra_isize))
#define ITAIL(inode, raw_inode) \
	((void *)(raw_inode) + \
	 JRNL_COMP_EXT4_SB((inode)->i_sb)->s_inode_size)
#define IFIRST(hdr) ((struct jrnl_comp_ext4_xattr_entry *)((hdr)+1))

/*
 * XATTR_SIZE_MAX is currently 64k, but for the purposes of checking
 * for file system consistency errors, we use a somewhat bigger value.
 * This allows XATTR_SIZE_MAX to grow in the future, but by using this
 * instead of INT_MAX for certain consistency checks, we don't need to
 * worry about arithmetic overflows.  (Actually XATTR_SIZE_MAX is
 * defined in include/uapi/linux/limits.h, so changing it is going
 * not going to be trivial....)
 */
#define JRNL_COMP_EXT4_XATTR_SIZE_MAX (1 << 24)

/*
 * The minimum size of EA value when you start storing it in an external inode
 * size of block - size of header - size of 1 entry - 4 null bytes
 */
#define JRNL_COMP_EXT4_XATTR_MIN_LARGE_EA_SIZE(b)					\
	((b) - JRNL_COMP_EXT4_XATTR_LEN(3) - sizeof(struct jrnl_comp_ext4_xattr_header) - 4)

#define BHDR(bh) ((struct jrnl_comp_ext4_xattr_header *)((bh)->b_data))
#define ENTRY(ptr) ((struct jrnl_comp_ext4_xattr_entry *)(ptr))
#define BFIRST(bh) ENTRY(BHDR(bh)+1)
#define IS_LAST_ENTRY(entry) (*(__u32 *)(entry) == 0)

#define JRNL_COMP_EXT4_ZERO_XATTR_VALUE ((void *)-1)

/*
 * If we want to add an xattr to the inode, we should make sure that
 * i_extra_isize is not 0 and that the inode size is not less than
 * JRNL_COMP_EXT4_GOOD_OLD_INODE_SIZE + extra_isize + pad.
 *   JRNL_COMP_EXT4_GOOD_OLD_INODE_SIZE   extra_isize header   entry   pad  data
 * |--------------------------|------------|------|---------|---|-------|
 */
#define JRNL_COMP_EXT4_INODE_HAS_XATTR_SPACE(inode)				\
	((JRNL_COMP_EXT4_I(inode)->i_extra_isize != 0) &&				\
	 (JRNL_COMP_EXT4_GOOD_OLD_INODE_SIZE + JRNL_COMP_EXT4_I(inode)->i_extra_isize +	\
	  sizeof(struct jrnl_comp_ext4_xattr_ibody_header) + JRNL_COMP_EXT4_XATTR_PAD <=	\
	  JRNL_COMP_EXT4_INODE_SIZE((inode)->i_sb)))

struct jrnl_comp_ext4_xattr_info {
	const char *name;
	const void *value;
	size_t value_len;
	int name_index;
	int in_inode;
};

struct jrnl_comp_ext4_xattr_search {
	struct jrnl_comp_ext4_xattr_entry *first;
	void *base;
	void *end;
	struct jrnl_comp_ext4_xattr_entry *here;
	int not_found;
};

struct jrnl_comp_ext4_xattr_ibody_find {
	struct jrnl_comp_ext4_xattr_search s;
	struct jrnl_comp_ext4_iloc iloc;
};

struct jrnl_comp_ext4_xattr_inode_array {
	unsigned int count;
	struct inode *inodes[] __counted_by(count);
};

extern const struct xattr_handler jrnl_comp_ext4_xattr_user_handler;
extern const struct xattr_handler jrnl_comp_ext4_xattr_trusted_handler;
extern const struct xattr_handler jrnl_comp_ext4_xattr_security_handler;
extern const struct xattr_handler jrnl_comp_ext4_xattr_hurd_handler;

#define JRNL_COMP_EXT4_XATTR_NAME_ENCRYPTION_CONTEXT "c"

/*
 * The JRNL_COMP_EXT4_STATE_NO_EXPAND is overloaded and used for two purposes.
 * The first is to signal that there the inline xattrs and data are
 * taking up so much space that we might as well not keep trying to
 * expand it.  The second is that xattr_sem is taken for writing, so
 * we shouldn't try to recurse into the inode expansion.  For this
 * second case, we need to make sure that we take save and restore the
 * NO_EXPAND state flag appropriately.
 */
static inline void jrnl_comp_ext4_write_lock_xattr(struct inode *inode, int *save)
{
	down_write(&JRNL_COMP_EXT4_I(inode)->xattr_sem);
	*save = jrnl_comp_ext4_test_inode_state(inode, JRNL_COMP_EXT4_STATE_NO_EXPAND);
	jrnl_comp_ext4_set_inode_state(inode, JRNL_COMP_EXT4_STATE_NO_EXPAND);
}

static inline int jrnl_comp_ext4_write_trylock_xattr(struct inode *inode, int *save)
{
	if (down_write_trylock(&JRNL_COMP_EXT4_I(inode)->xattr_sem) == 0)
		return 0;
	*save = jrnl_comp_ext4_test_inode_state(inode, JRNL_COMP_EXT4_STATE_NO_EXPAND);
	jrnl_comp_ext4_set_inode_state(inode, JRNL_COMP_EXT4_STATE_NO_EXPAND);
	return 1;
}

static inline void jrnl_comp_ext4_write_unlock_xattr(struct inode *inode, int *save)
{
	if (*save == 0)
		jrnl_comp_ext4_clear_inode_state(inode, JRNL_COMP_EXT4_STATE_NO_EXPAND);
	up_write(&JRNL_COMP_EXT4_I(inode)->xattr_sem);
}

extern ssize_t jrnl_comp_ext4_listxattr(struct dentry *, char *, size_t);

extern int jrnl_comp_ext4_xattr_get(struct inode *, int, const char *, void *, size_t);
extern int jrnl_comp_ext4_xattr_set(struct inode *, int, const char *, const void *, size_t, int);
extern int jrnl_comp_ext4_xattr_set_handle(jrnl_comp_handle_t *, struct inode *, int, const char *, const void *, size_t, int);
extern int jrnl_comp_ext4_xattr_set_credits(struct inode *inode, size_t value_len,
				  bool is_create, int *credits);
extern int __jrnl_comp_ext4_xattr_set_credits(struct super_block *sb, struct inode *inode,
				struct buffer_head *block_bh, size_t value_len,
				bool is_create);

extern int jrnl_comp_ext4_xattr_delete_inode(jrnl_comp_handle_t *handle, struct inode *inode,
				   struct jrnl_comp_ext4_xattr_inode_array **array,
				   int extra_credits);
extern void jrnl_comp_ext4_xattr_inode_array_free(struct jrnl_comp_ext4_xattr_inode_array *array);

extern int jrnl_comp_ext4_expand_extra_isize_ea(struct inode *inode, int new_extra_isize,
			    struct jrnl_comp_ext4_inode *raw_inode, jrnl_comp_handle_t *handle);
extern void jrnl_comp_ext4_evict_ea_inode(struct inode *inode);

extern const struct xattr_handler * const jrnl_comp_ext4_xattr_handlers[];

extern int jrnl_comp_ext4_xattr_ibody_find(struct inode *inode, struct jrnl_comp_ext4_xattr_info *i,
				 struct jrnl_comp_ext4_xattr_ibody_find *is);
extern int jrnl_comp_ext4_xattr_ibody_get(struct inode *inode, int name_index,
				const char *name,
				void *buffer, size_t buffer_size);
extern int jrnl_comp_ext4_xattr_ibody_set(jrnl_comp_handle_t *handle, struct inode *inode,
				struct jrnl_comp_ext4_xattr_info *i,
				struct jrnl_comp_ext4_xattr_ibody_find *is);

extern struct mb_cache *jrnl_comp_ext4_xattr_create_cache(void);
extern void jrnl_comp_ext4_xattr_destroy_cache(struct mb_cache *);

extern int
__xattr_check_inode(struct inode *inode, struct jrnl_comp_ext4_xattr_ibody_header *header,
		    void *end, const char *function, unsigned int line);

#define xattr_check_inode(inode, header, end) \
	__xattr_check_inode((inode), (header), (end), __func__, __LINE__)

#ifdef CONFIG_JRNL_COMP_EXT4_FS_SECURITY
extern int jrnl_comp_ext4_init_security(jrnl_comp_handle_t *handle, struct inode *inode,
			      struct inode *dir, const struct qstr *qstr);
#else
static inline int jrnl_comp_ext4_init_security(jrnl_comp_handle_t *handle, struct inode *inode,
				     struct inode *dir, const struct qstr *qstr)
{
	return 0;
}
#endif

#ifdef CONFIG_LOCKDEP
extern void jrnl_comp_ext4_xattr_inode_set_class(struct inode *ea_inode);
#else
static inline void jrnl_comp_ext4_xattr_inode_set_class(struct inode *ea_inode) { }
#endif

extern int jrnl_comp_ext4_get_inode_usage(struct inode *inode, qsize_t *usage);
