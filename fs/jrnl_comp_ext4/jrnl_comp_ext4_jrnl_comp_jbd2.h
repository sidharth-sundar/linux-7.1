// SPDX-License-Identifier: GPL-2.0+
/*
 * jrnl_comp_ext4_jrnl_comp_jbd2.h
 *
 * Written by Stephen C. Tweedie <sct@redhat.com>, 1999
 *
 * Copyright 1998--1999 Red Hat corp --- All Rights Reserved
 *
 * Ext4-specific journaling extensions.
 */

#ifndef _JRNL_COMP_EXT4_JRNL_COMP_JBD2_H
#define _JRNL_COMP_EXT4_JRNL_COMP_JBD2_H

#include <linux/fs.h>
#include <linux/jrnl_comp_jbd2.h>
#include "jrnl_comp_ext4.h"

#define JRNL_COMP_EXT4_JOURNAL(inode)	(JRNL_COMP_EXT4_SB((inode)->i_sb)->s_journal)

/* Define the number of blocks we need to account to a transaction to
 * modify one block of data.
 *
 * We may have to touch one inode, one bitmap buffer, up to three
 * indirection blocks, the group and superblock summaries, and the data
 * block to complete the transaction.
 *
 * For extents-enabled fs we may have to allocate and modify up to
 * 5 levels of tree, data block (for each of these we need bitmap + group
 * summaries), root which is stored in the inode, sb
 */

#define JRNL_COMP_EXT4_SINGLEDATA_TRANS_BLOCKS(sb)				\
	(jrnl_comp_ext4_has_feature_extents(sb) ? 20U : 8U)

/* Extended attribute operations touch at most two data buffers,
 * two bitmap buffers, and two group summaries, in addition to the inode
 * and the superblock, which are already accounted for. */

#define JRNL_COMP_EXT4_XATTR_TRANS_BLOCKS		6U

/* Define the minimum size for a transaction which modifies data.  This
 * needs to take into account the fact that we may end up modifying two
 * quota files too (one for the group, one for the user quota).  The
 * superblock only gets updated once, of course, so don't bother
 * counting that again for the quota updates. */

#define JRNL_COMP_EXT4_DATA_TRANS_BLOCKS(sb)	(JRNL_COMP_EXT4_SINGLEDATA_TRANS_BLOCKS(sb) + \
					 JRNL_COMP_EXT4_XATTR_TRANS_BLOCKS - 2 + \
					 JRNL_COMP_EXT4_MAXQUOTAS_TRANS_BLOCKS(sb))

/*
 * Define the number of metadata blocks we need to account to modify data.
 *
 * This include super block, inode block, quota blocks and xattr blocks
 */
#define JRNL_COMP_EXT4_META_TRANS_BLOCKS(sb)	(JRNL_COMP_EXT4_XATTR_TRANS_BLOCKS + \
					JRNL_COMP_EXT4_MAXQUOTAS_TRANS_BLOCKS(sb))

/* Define an arbitrary limit for the amount of data we will anticipate
 * writing to any given transaction.  For unbounded transactions such as
 * write(2) and truncate(2) we can write more than this, but we always
 * start off at the maximum transaction size and grow the transaction
 * optimistically as we go. */

#define JRNL_COMP_EXT4_MAX_TRANS_DATA		64U

/* We break up a large truncate or write transaction once the handle's
 * buffer credits gets this low, we need either to extend the
 * transaction or to start a new one.  Reserve enough space here for
 * inode, bitmap, superblock, group and indirection updates for at least
 * one block, plus two quota updates.  Quota allocations are not
 * needed. */

#define JRNL_COMP_EXT4_RESERVE_TRANS_BLOCKS	12U

/*
 * Number of credits needed if we need to insert an entry into a
 * directory.  For each new index block, we need 4 blocks (old index
 * block, new index block, bitmap block, bg summary).  For normal
 * htree directories there are 2 levels; if the largedir feature
 * enabled it's 3 levels.
 */
#define JRNL_COMP_EXT4_INDEX_EXTRA_TRANS_BLOCKS	12U

#ifdef CONFIG_QUOTA
/* Amount of blocks needed for quota update - we know that the structure was
 * allocated so we need to update only data block */
#define JRNL_COMP_EXT4_QUOTA_TRANS_BLOCKS(sb) ((jrnl_comp_ext4_quota_capable(sb)) ? 1 : 0)
/* Amount of blocks needed for quota insert/delete - we do some block writes
 * but inode, sb and group updates are done only once */
#define JRNL_COMP_EXT4_QUOTA_INIT_BLOCKS(sb) ((jrnl_comp_ext4_quota_capable(sb)) ?\
		(DQUOT_INIT_ALLOC*(JRNL_COMP_EXT4_SINGLEDATA_TRANS_BLOCKS(sb)-3)\
		 +3+DQUOT_INIT_REWRITE) : 0)

#define JRNL_COMP_EXT4_QUOTA_DEL_BLOCKS(sb) ((jrnl_comp_ext4_quota_capable(sb)) ?\
		(DQUOT_DEL_ALLOC*(JRNL_COMP_EXT4_SINGLEDATA_TRANS_BLOCKS(sb)-3)\
		 +3+DQUOT_DEL_REWRITE) : 0)
#else
#define JRNL_COMP_EXT4_QUOTA_TRANS_BLOCKS(sb) 0
#define JRNL_COMP_EXT4_QUOTA_INIT_BLOCKS(sb) 0
#define JRNL_COMP_EXT4_QUOTA_DEL_BLOCKS(sb) 0
#endif
#define JRNL_COMP_EXT4_MAXQUOTAS_TRANS_BLOCKS(sb) (JRNL_COMP_EXT4_MAXQUOTAS*JRNL_COMP_EXT4_QUOTA_TRANS_BLOCKS(sb))
#define JRNL_COMP_EXT4_MAXQUOTAS_INIT_BLOCKS(sb) (JRNL_COMP_EXT4_MAXQUOTAS*JRNL_COMP_EXT4_QUOTA_INIT_BLOCKS(sb))
#define JRNL_COMP_EXT4_MAXQUOTAS_DEL_BLOCKS(sb) (JRNL_COMP_EXT4_MAXQUOTAS*JRNL_COMP_EXT4_QUOTA_DEL_BLOCKS(sb))

/*
 * Ext4 handle operation types -- for logging purposes
 */
#define JRNL_COMP_EXT4_HT_MISC             0
#define JRNL_COMP_EXT4_HT_INODE            1
#define JRNL_COMP_EXT4_HT_WRITE_PAGE       2
#define JRNL_COMP_EXT4_HT_MAP_BLOCKS       3
#define JRNL_COMP_EXT4_HT_DIR              4
#define JRNL_COMP_EXT4_HT_TRUNCATE         5
#define JRNL_COMP_EXT4_HT_QUOTA            6
#define JRNL_COMP_EXT4_HT_RESIZE           7
#define JRNL_COMP_EXT4_HT_MIGRATE          8
#define JRNL_COMP_EXT4_HT_MOVE_EXTENTS     9
#define JRNL_COMP_EXT4_HT_XATTR           10
#define JRNL_COMP_EXT4_HT_EXT_CONVERT     11
#define JRNL_COMP_EXT4_HT_MAX             12

int
jrnl_comp_ext4_mark_iloc_dirty(jrnl_comp_handle_t *handle,
		     struct inode *inode,
		     struct jrnl_comp_ext4_iloc *iloc);

/*
 * On success, We end up with an outstanding reference count against
 * iloc->bh.  This _must_ be cleaned up later.
 */

int jrnl_comp_ext4_reserve_inode_write(jrnl_comp_handle_t *handle, struct inode *inode,
			struct jrnl_comp_ext4_iloc *iloc);

#define jrnl_comp_ext4_mark_inode_dirty(__h, __i)					\
		__jrnl_comp_ext4_mark_inode_dirty((__h), (__i), __func__, __LINE__)
int __jrnl_comp_ext4_mark_inode_dirty(jrnl_comp_handle_t *handle, struct inode *inode,
				const char *func, unsigned int line);

int jrnl_comp_ext4_expand_extra_isize(struct inode *inode,
			    unsigned int new_extra_isize,
			    struct jrnl_comp_ext4_iloc *iloc);
/*
 * Wrapper functions with which jrnl_comp_ext4 calls into JBD.
 */
int __jrnl_comp_ext4_journal_get_write_access(const char *where, unsigned int line,
				    jrnl_comp_handle_t *handle, struct super_block *sb,
				    struct buffer_head *bh,
				    enum jrnl_comp_ext4_journal_trigger_type trigger_type);

int __jrnl_comp_ext4_forget(const char *where, unsigned int line, jrnl_comp_handle_t *handle,
		  int is_metadata, struct inode *inode,
		  struct buffer_head *bh, jrnl_comp_ext4_fsblk_t blocknr);

int __jrnl_comp_ext4_journal_get_create_access(const char *where, unsigned int line,
				jrnl_comp_handle_t *handle, struct super_block *sb,
				struct buffer_head *bh,
				enum jrnl_comp_ext4_journal_trigger_type trigger_type);

int __jrnl_comp_ext4_handle_dirty_metadata(const char *where, unsigned int line,
				 jrnl_comp_handle_t *handle, struct inode *inode,
				 struct buffer_head *bh);
int __jrnl_comp_ext4_handle_dirty_metadata_range(const char *where, unsigned int line,
				 jrnl_comp_handle_t *handle, struct inode *inode,
				 struct buffer_head *bh, unsigned int offset,
				 unsigned int length);

#define jrnl_comp_ext4_journal_get_write_access(handle, sb, bh, trigger_type) \
	__jrnl_comp_ext4_journal_get_write_access(__func__, __LINE__, (handle), (sb), \
					(bh), (trigger_type))
#define jrnl_comp_ext4_forget(handle, is_metadata, inode, bh, block_nr) \
	__jrnl_comp_ext4_forget(__func__, __LINE__, (handle), (is_metadata), (inode), \
		      (bh), (block_nr))
#define jrnl_comp_ext4_journal_get_create_access(handle, sb, bh, trigger_type) \
	__jrnl_comp_ext4_journal_get_create_access(__func__, __LINE__, (handle), (sb), \
					 (bh), (trigger_type))
#define jrnl_comp_ext4_handle_dirty_metadata(handle, inode, bh) \
	__jrnl_comp_ext4_handle_dirty_metadata(__func__, __LINE__, (handle), (inode), \
				     (bh))
#define jrnl_comp_ext4_handle_dirty_metadata_range(handle, inode, bh, offset, length) \
	__jrnl_comp_ext4_handle_dirty_metadata_range(__func__, __LINE__, (handle), (inode), \
					   (bh), (offset), (length))

jrnl_comp_handle_t *__jrnl_comp_ext4_journal_start_sb(struct inode *inode, struct super_block *sb,
				  unsigned int line, int type, int blocks,
				  int rsv_blocks, int revoke_creds);
int __jrnl_comp_ext4_journal_stop(const char *where, unsigned int line, jrnl_comp_handle_t *handle);

#define JRNL_COMP_EXT4_NOJOURNAL_MAX_REF_COUNT ((unsigned long) 4096)

/* Note:  Do not use this for NULL handles.  This is only to determine if
 * a properly allocated handle is using a journal or not. */
static inline int jrnl_comp_ext4_handle_valid(jrnl_comp_handle_t *handle)
{
	if ((unsigned long)handle < JRNL_COMP_EXT4_NOJOURNAL_MAX_REF_COUNT)
		return 0;
	return 1;
}

static inline void jrnl_comp_ext4_handle_sync(jrnl_comp_handle_t *handle)
{
	if (jrnl_comp_ext4_handle_valid(handle))
		handle->h_sync = 1;
}

static inline int jrnl_comp_ext4_handle_is_aborted(jrnl_comp_handle_t *handle)
{
	if (jrnl_comp_ext4_handle_valid(handle))
		return is_handle_aborted(handle);
	return 0;
}

static inline int jrnl_comp_ext4_free_metadata_revoke_credits(struct super_block *sb,
						    int blocks)
{
	/* Freeing each metadata block can result in freeing one cluster */
	return blocks * JRNL_COMP_EXT4_SB(sb)->s_cluster_ratio;
}

static inline int jrnl_comp_ext4_trans_default_revoke_credits(struct super_block *sb)
{
	return jrnl_comp_ext4_free_metadata_revoke_credits(sb, 8);
}

#define jrnl_comp_ext4_journal_start_sb(sb, type, nblocks)			\
	__jrnl_comp_ext4_journal_start_sb(NULL, (sb), __LINE__, (type), (nblocks), 0,\
				jrnl_comp_ext4_trans_default_revoke_credits(sb))

#define jrnl_comp_ext4_journal_start(inode, type, nblocks)			\
	__jrnl_comp_ext4_journal_start((inode), __LINE__, (type), (nblocks), 0,	\
			     jrnl_comp_ext4_trans_default_revoke_credits((inode)->i_sb))

#define jrnl_comp_ext4_journal_start_with_reserve(inode, type, blocks, rsv_blocks)\
	__jrnl_comp_ext4_journal_start((inode), __LINE__, (type), (blocks), (rsv_blocks),\
			     jrnl_comp_ext4_trans_default_revoke_credits((inode)->i_sb))

#define jrnl_comp_ext4_journal_start_with_revoke(inode, type, blocks, revoke_creds) \
	__jrnl_comp_ext4_journal_start((inode), __LINE__, (type), (blocks), 0,	\
			     (revoke_creds))

static inline jrnl_comp_handle_t *__jrnl_comp_ext4_journal_start(struct inode *inode,
					     unsigned int line, int type,
					     int blocks, int rsv_blocks,
					     int revoke_creds)
{
	return __jrnl_comp_ext4_journal_start_sb(inode, inode->i_sb, line, type, blocks,
				       rsv_blocks, revoke_creds);
}

#define jrnl_comp_ext4_journal_stop(handle) \
	__jrnl_comp_ext4_journal_stop(__func__, __LINE__, (handle))

#define jrnl_comp_ext4_journal_start_reserved(handle, type) \
	__jrnl_comp_ext4_journal_start_reserved((handle), __LINE__, (type))

jrnl_comp_handle_t *__jrnl_comp_ext4_journal_start_reserved(jrnl_comp_handle_t *handle, unsigned int line,
					int type);

static inline jrnl_comp_handle_t *jrnl_comp_ext4_journal_current_handle(void)
{
	return jrnl_comp_journal_current_handle();
}

static inline int jrnl_comp_ext4_journal_extend(jrnl_comp_handle_t *handle, int nblocks, int revoke)
{
	if (jrnl_comp_ext4_handle_valid(handle))
		return jrnl_comp_jbd2_journal_extend(handle, nblocks, revoke);
	return 0;
}

static inline int jrnl_comp_ext4_journal_restart(jrnl_comp_handle_t *handle, int nblocks,
				       int revoke)
{
	if (jrnl_comp_ext4_handle_valid(handle))
		return jrnl_comp_jbd2__journal_restart(handle, nblocks, revoke, GFP_NOFS);
	return 0;
}

int __jrnl_comp_ext4_journal_ensure_credits(jrnl_comp_handle_t *handle, int check_cred,
				  int extend_cred, int revoke_cred);


/*
 * Ensure @handle has at least @check_creds credits available. If not,
 * transaction will be extended or restarted to contain at least @extend_cred
 * credits. Before restarting transaction @fn is executed to allow for cleanup
 * before the transaction is restarted.
 *
 * The return value is < 0 in case of error, 0 in case the handle has enough
 * credits or transaction extension succeeded, 1 in case transaction had to be
 * restarted.
 */
#define jrnl_comp_ext4_journal_ensure_credits_fn(handle, check_cred, extend_cred,	\
				       revoke_cred, fn) \
({									\
	__label__ __ensure_end;						\
	int err = __jrnl_comp_ext4_journal_ensure_credits((handle), (check_cred),	\
					(extend_cred), (revoke_cred));	\
									\
	if (err <= 0)							\
		goto __ensure_end;					\
	err = (fn);							\
	if (err < 0)							\
		goto __ensure_end;					\
	err = jrnl_comp_ext4_journal_restart((handle), (extend_cred), (revoke_cred)); \
	if (err == 0)							\
		err = 1;						\
__ensure_end:								\
	err;								\
})

/*
 * Ensure given handle has at least requested amount of credits available,
 * possibly restarting transaction if needed. We also make sure the transaction
 * has space for at least jrnl_comp_ext4_trans_default_revoke_credits(sb) revoke records
 * as freeing one or two blocks is very common pattern and requesting this is
 * very cheap.
 */
static inline int jrnl_comp_ext4_journal_ensure_credits(jrnl_comp_handle_t *handle, int credits,
					      int revoke_creds)
{
	return jrnl_comp_ext4_journal_ensure_credits_fn(handle, credits, credits,
				revoke_creds, 0);
}

static inline int jrnl_comp_ext4_journal_blocks_per_folio(struct inode *inode)
{
	if (JRNL_COMP_EXT4_JOURNAL(inode) != NULL)
		return jrnl_comp_jbd2_journal_blocks_per_folio(inode);
	return 0;
}

static inline int jrnl_comp_ext4_journal_force_commit(jrnl_comp_journal_t *journal)
{
	if (journal)
		return jrnl_comp_jbd2_journal_force_commit(journal);
	return 0;
}

static inline int jrnl_comp_ext4_jrnl_comp_jbd2_inode_add_write(jrnl_comp_handle_t *handle,
		struct inode *inode, loff_t start_byte, loff_t length)
{
	if (jrnl_comp_ext4_handle_valid(handle))
		return jrnl_comp_jbd2_journal_inode_ranged_write(handle,
				JRNL_COMP_EXT4_I(inode)->jinode, start_byte, length);
	return 0;
}

static inline int jrnl_comp_ext4_jrnl_comp_jbd2_inode_add_wait(jrnl_comp_handle_t *handle,
		struct inode *inode, loff_t start_byte, loff_t length)
{
	if (jrnl_comp_ext4_handle_valid(handle))
		return jrnl_comp_jbd2_journal_inode_ranged_wait(handle,
				JRNL_COMP_EXT4_I(inode)->jinode, start_byte, length);
	return 0;
}

static inline void jrnl_comp_ext4_update_inode_fsync_trans(jrnl_comp_handle_t *handle,
						 struct inode *inode,
						 int datasync)
{
	struct jrnl_comp_ext4_inode_info *ei = JRNL_COMP_EXT4_I(inode);

	if (jrnl_comp_ext4_handle_valid(handle) && !is_handle_aborted(handle)) {
		ei->i_sync_tid = handle->h_transaction->t_tid;
		if (datasync)
			ei->i_datasync_tid = handle->h_transaction->t_tid;
	}
}

/* super.c */
int jrnl_comp_ext4_force_commit(struct super_block *sb);

/*
 * Ext4 inode journal modes
 */
#define JRNL_COMP_EXT4_INODE_JOURNAL_DATA_MODE	0x01 /* journal data mode */
#define JRNL_COMP_EXT4_INODE_ORDERED_DATA_MODE	0x02 /* ordered data mode */
#define JRNL_COMP_EXT4_INODE_WRITEBACK_DATA_MODE	0x04 /* writeback data mode */

int jrnl_comp_ext4_inode_journal_mode(struct inode *inode);

static inline int jrnl_comp_ext4_should_journal_data(struct inode *inode)
{
	return jrnl_comp_ext4_inode_journal_mode(inode) & JRNL_COMP_EXT4_INODE_JOURNAL_DATA_MODE;
}

static inline int jrnl_comp_ext4_should_order_data(struct inode *inode)
{
	return jrnl_comp_ext4_inode_journal_mode(inode) & JRNL_COMP_EXT4_INODE_ORDERED_DATA_MODE;
}

static inline int jrnl_comp_ext4_should_writeback_data(struct inode *inode)
{
	return jrnl_comp_ext4_inode_journal_mode(inode) & JRNL_COMP_EXT4_INODE_WRITEBACK_DATA_MODE;
}

static inline int jrnl_comp_ext4_free_data_revoke_credits(struct inode *inode, int blocks)
{
	if (test_opt(inode->i_sb, DATA_FLAGS) == JRNL_COMP_EXT4_MOUNT_JOURNAL_DATA)
		return 0;
	if (!jrnl_comp_ext4_should_journal_data(inode))
		return 0;
	/*
	 * Data blocks in one extent are contiguous, just account for partial
	 * clusters at extent boundaries
	 */
	return blocks + 2*(JRNL_COMP_EXT4_SB(inode->i_sb)->s_cluster_ratio - 1);
}

/*
 * This function controls whether or not we should try to go down the
 * dioread_nolock code paths, which makes it safe to avoid taking
 * i_rwsem for direct I/O reads.  This only works for extent-based
 * files, and it doesn't work if data journaling is enabled, since the
 * dioread_nolock code uses b_private to pass information back to the
 * I/O completion handler, and this conflicts with the jbd's use of
 * b_private.
 */
static inline int jrnl_comp_ext4_should_dioread_nolock(struct inode *inode)
{
	if (!test_opt(inode->i_sb, DIOREAD_NOLOCK))
		return 0;
	if (!S_ISREG(inode->i_mode))
		return 0;
	if (!(jrnl_comp_ext4_test_inode_flag(inode, JRNL_COMP_EXT4_INODE_EXTENTS)))
		return 0;
	if (jrnl_comp_ext4_should_journal_data(inode))
		return 0;
	/* temporary fix to prevent generic/422 test failures */
	if (!test_opt(inode->i_sb, DELALLOC))
		return 0;
	return 1;
}

/*
 * Pass journal explicitly as it may not be cached in the sbi->s_journal in some
 * cases
 */
static inline int jrnl_comp_ext4_journal_destroy(struct jrnl_comp_ext4_sb_info *sbi, jrnl_comp_journal_t *journal)
{
	int err = 0;

	/*
	 * At this point only two things can be operating on the journal.
	 * JRNL_COMP_JBD2 thread performing transaction commit and s_sb_upd_work
	 * issuing sb update through the journal. Once we set
	 * JRNL_COMP_EXT4_JOURNAL_DESTROY, new jrnl_comp_ext4_handle_error() calls will not
	 * queue s_sb_upd_work and jrnl_comp_ext4_force_commit() makes sure any
	 * jrnl_comp_ext4_handle_error() calls from the running transaction commit are
	 * finished. Hence no new s_sb_upd_work can be queued after we
	 * flush it here.
	 */
	jrnl_comp_ext4_set_mount_flag(sbi->s_sb, JRNL_COMP_EXT4_MF_JOURNAL_DESTROY);

	jrnl_comp_ext4_force_commit(sbi->s_sb);
	flush_work(&sbi->s_sb_upd_work);

	err = jrnl_comp_jbd2_journal_destroy(journal);
	sbi->s_journal = NULL;

	return err;
}

#endif	/* _JRNL_COMP_EXT4_JRNL_COMP_JBD2_H */
