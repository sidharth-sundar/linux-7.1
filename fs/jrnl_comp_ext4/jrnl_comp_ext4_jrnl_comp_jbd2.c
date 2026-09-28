// SPDX-License-Identifier: GPL-2.0
/*
 * Interface between jrnl_comp_ext4 and JBD
 */

#include "jrnl_comp_ext4_jrnl_comp_jbd2.h"

#include <trace/events/jrnl_comp_ext4.h>

int jrnl_comp_ext4_inode_journal_mode(struct inode *inode)
{
	if (JRNL_COMP_EXT4_JOURNAL(inode) == NULL)
		return JRNL_COMP_EXT4_INODE_WRITEBACK_DATA_MODE;	/* writeback */
	/* We do not support data journalling with delayed allocation */
	if (!S_ISREG(inode->i_mode) ||
	    jrnl_comp_ext4_test_inode_flag(inode, JRNL_COMP_EXT4_INODE_EA_INODE) ||
	    test_opt(inode->i_sb, DATA_FLAGS) == JRNL_COMP_EXT4_MOUNT_JOURNAL_DATA ||
	    (jrnl_comp_ext4_test_inode_flag(inode, JRNL_COMP_EXT4_INODE_JOURNAL_DATA) &&
	    !test_opt(inode->i_sb, DELALLOC))) {
		/* We do not support data journalling for encrypted data */
		if (S_ISREG(inode->i_mode) && IS_ENCRYPTED(inode))
			return JRNL_COMP_EXT4_INODE_ORDERED_DATA_MODE;  /* ordered */
		return JRNL_COMP_EXT4_INODE_JOURNAL_DATA_MODE;	/* journal data */
	}
	if (test_opt(inode->i_sb, DATA_FLAGS) == JRNL_COMP_EXT4_MOUNT_ORDERED_DATA)
		return JRNL_COMP_EXT4_INODE_ORDERED_DATA_MODE;	/* ordered */
	if (test_opt(inode->i_sb, DATA_FLAGS) == JRNL_COMP_EXT4_MOUNT_WRITEBACK_DATA)
		return JRNL_COMP_EXT4_INODE_WRITEBACK_DATA_MODE;	/* writeback */
	BUG();
}

/* Just increment the non-pointer handle value */
static jrnl_comp_handle_t *jrnl_comp_ext4_get_nojournal(void)
{
	jrnl_comp_handle_t *handle = jrnl_comp_journal_current_handle();
	unsigned long ref_cnt = (unsigned long)handle;

	BUG_ON(ref_cnt >= JRNL_COMP_EXT4_NOJOURNAL_MAX_REF_COUNT);

	ref_cnt++;
	handle = (jrnl_comp_handle_t *)ref_cnt;

	jrnl_comp_journal_set_current_handle(handle);
	return handle;
}


/* Decrement the non-pointer handle value */
static void jrnl_comp_ext4_put_nojournal(jrnl_comp_handle_t *handle)
{
	unsigned long ref_cnt = (unsigned long)handle;

	BUG_ON(ref_cnt == 0);

	ref_cnt--;
	handle = (jrnl_comp_handle_t *)ref_cnt;

	jrnl_comp_journal_set_current_handle(handle);
}

/*
 * Wrappers for jrnl_comp_jbd2_journal_start/end.
 */
static int jrnl_comp_ext4_journal_check_start(struct super_block *sb)
{
	int ret;
	jrnl_comp_journal_t *journal;

	might_sleep();

	ret = jrnl_comp_ext4_emergency_state(sb);
	if (unlikely(ret))
		return ret;

	if (WARN_ON_ONCE(sb_rdonly(sb)))
		return -EROFS;

	WARN_ON(sb->s_writers.frozen == SB_FREEZE_COMPLETE);
	journal = JRNL_COMP_EXT4_SB(sb)->s_journal;
	/*
	 * Special case here: if the journal has aborted behind our
	 * backs (eg. EIO in the commit thread), then we still need to
	 * take the FS itself readonly cleanly.
	 */
	if (journal && is_journal_aborted(journal)) {
		jrnl_comp_ext4_abort(sb, -journal->j_errno, "Detected aborted journal");
		return -EROFS;
	}
	return 0;
}

jrnl_comp_handle_t *__jrnl_comp_ext4_journal_start_sb(struct inode *inode,
				  struct super_block *sb, unsigned int line,
				  int type, int blocks, int rsv_blocks,
				  int revoke_creds)
{
	jrnl_comp_journal_t *journal;
	int err;
	if (inode)
		trace_jrnl_comp_ext4_journal_start_inode(inode, blocks, rsv_blocks,
					revoke_creds, type,
					_RET_IP_);
	else
		trace_jrnl_comp_ext4_journal_start_sb(sb, blocks, rsv_blocks,
					revoke_creds, type,
					_RET_IP_);
	err = jrnl_comp_ext4_journal_check_start(sb);
	if (err < 0)
		return ERR_PTR(err);

	journal = JRNL_COMP_EXT4_SB(sb)->s_journal;
	if (!journal || (JRNL_COMP_EXT4_SB(sb)->s_mount_state & JRNL_COMP_EXT4_FC_REPLAY))
		return jrnl_comp_ext4_get_nojournal();
	return jrnl_comp_jbd2__journal_start(journal, blocks, rsv_blocks, revoke_creds,
				   GFP_NOFS, type, line);
}

int __jrnl_comp_ext4_journal_stop(const char *where, unsigned int line, jrnl_comp_handle_t *handle)
{
	struct super_block *sb;
	int err;
	int rc;

	if (!jrnl_comp_ext4_handle_valid(handle)) {
		jrnl_comp_ext4_put_nojournal(handle);
		return 0;
	}

	err = handle->h_err;
	if (!handle->h_transaction) {
		rc = jrnl_comp_jbd2_journal_stop(handle);
		return err ? err : rc;
	}

	sb = handle->h_transaction->t_journal->j_private;
	rc = jrnl_comp_jbd2_journal_stop(handle);

	if (!err)
		err = rc;
	if (err)
		__jrnl_comp_ext4_std_error(sb, where, line, err);
	return err;
}

jrnl_comp_handle_t *__jrnl_comp_ext4_journal_start_reserved(jrnl_comp_handle_t *handle, unsigned int line,
					int type)
{
	struct super_block *sb;
	int err;

	if (!jrnl_comp_ext4_handle_valid(handle))
		return jrnl_comp_ext4_get_nojournal();

	sb = handle->h_journal->j_private;
	trace_jrnl_comp_ext4_journal_start_reserved(sb,
				jrnl_comp_jbd2_handle_buffer_credits(handle), _RET_IP_);
	err = jrnl_comp_ext4_journal_check_start(sb);
	if (err < 0) {
		jrnl_comp_jbd2_journal_free_reserved(handle);
		return ERR_PTR(err);
	}

	err = jrnl_comp_jbd2_journal_start_reserved(handle, type, line);
	if (err < 0)
		return ERR_PTR(err);
	return handle;
}

int __jrnl_comp_ext4_journal_ensure_credits(jrnl_comp_handle_t *handle, int check_cred,
				  int extend_cred, int revoke_cred)
{
	if (!jrnl_comp_ext4_handle_valid(handle))
		return 0;
	if (is_handle_aborted(handle))
		return -EROFS;
	if (jrnl_comp_jbd2_handle_buffer_credits(handle) >= check_cred &&
	    handle->h_revoke_credits >= revoke_cred)
		return 0;
	extend_cred = max(0, extend_cred - jrnl_comp_jbd2_handle_buffer_credits(handle));
	revoke_cred = max(0, revoke_cred - handle->h_revoke_credits);
	return jrnl_comp_ext4_journal_extend(handle, extend_cred, revoke_cred);
}

static void jrnl_comp_ext4_journal_abort_handle(const char *caller, unsigned int line,
				      const char *err_fn,
				      struct buffer_head *bh,
				      jrnl_comp_handle_t *handle, int err)
{
	char nbuf[16];
	const char *errstr = jrnl_comp_ext4_decode_error(NULL, err, nbuf);

	BUG_ON(!jrnl_comp_ext4_handle_valid(handle));

	if (bh)
		BUFFER_TRACE(bh, "abort");

	if (!handle->h_err)
		handle->h_err = err;

	if (is_handle_aborted(handle))
		return;

	printk(KERN_ERR "JRNL_COMP_EXT4-fs: %s:%d: aborting transaction: %s in %s\n",
	       caller, line, errstr, err_fn);

	jrnl_comp_jbd2_journal_abort_handle(handle);
}

static void jrnl_comp_ext4_check_bdev_write_error(struct super_block *sb)
{
	struct address_space *mapping = sb->s_bdev->bd_mapping;
	struct jrnl_comp_ext4_sb_info *sbi = JRNL_COMP_EXT4_SB(sb);
	int err;

	/*
	 * If the block device has write error flag, it may have failed to
	 * async write out metadata buffers in the background. In this case,
	 * we could read old data from disk and write it out again, which
	 * may lead to on-disk filesystem inconsistency.
	 */
	if (errseq_check(&mapping->wb_err, READ_ONCE(sbi->s_bdev_wb_err))) {
		spin_lock(&sbi->s_bdev_wb_lock);
		err = errseq_check_and_advance(&mapping->wb_err, &sbi->s_bdev_wb_err);
		spin_unlock(&sbi->s_bdev_wb_lock);
		if (err)
			jrnl_comp_ext4_error_err(sb, -err,
				       "Error while async write back metadata");
	}
}

int __jrnl_comp_ext4_journal_get_write_access(const char *where, unsigned int line,
				    jrnl_comp_handle_t *handle, struct super_block *sb,
				    struct buffer_head *bh,
				    enum jrnl_comp_ext4_journal_trigger_type trigger_type)
{
	int err;

	might_sleep();

	if (jrnl_comp_ext4_handle_valid(handle)) {
		err = jrnl_comp_jbd2_journal_get_write_access(handle, bh);
		if (err) {
			jrnl_comp_ext4_journal_abort_handle(where, line, __func__, bh,
						  handle, err);
			return err;
		}
	} else
		jrnl_comp_ext4_check_bdev_write_error(sb);
	if (trigger_type == JRNL_COMP_EXT4_JTR_NONE ||
	    !jrnl_comp_ext4_has_feature_metadata_csum(sb))
		return 0;
	BUG_ON(trigger_type >= JRNL_COMP_EXT4_JOURNAL_TRIGGER_COUNT);
	jrnl_comp_jbd2_journal_set_triggers(bh,
		&JRNL_COMP_EXT4_SB(sb)->s_journal_triggers[trigger_type].tr_triggers);
	return 0;
}

/*
 * The jrnl_comp_ext4 forget function must perform a revoke if we are freeing data
 * which has been journaled.  Metadata (eg. indirect blocks) must be
 * revoked in all cases.
 *
 * "bh" may be NULL: a metadata block may have been freed from memory
 * but there may still be a record of it in the journal, and that record
 * still needs to be revoked.
 */
int __jrnl_comp_ext4_forget(const char *where, unsigned int line, jrnl_comp_handle_t *handle,
		  int is_metadata, struct inode *inode,
		  struct buffer_head *bh, jrnl_comp_ext4_fsblk_t blocknr)
{
	int err;

	might_sleep();

	trace_jrnl_comp_ext4_forget(inode, is_metadata, blocknr);
	BUFFER_TRACE(bh, "enter");

	jrnl_comp_ext4_debug("forgetting bh %p: is_metadata=%d, mode %o, data mode %x\n",
		  bh, is_metadata, inode->i_mode,
		  test_opt(inode->i_sb, DATA_FLAGS));

	/*
	 * In the no journal case, we should wait for the ongoing buffer
	 * to complete and do a forget.
	 */
	if (!jrnl_comp_ext4_handle_valid(handle)) {
		if (bh) {
			clear_buffer_dirty(bh);
			wait_on_buffer(bh);
			__bforget(bh);
		}
		return 0;
	}

	/* Never use the revoke function if we are doing full data
	 * journaling: there is no need to, and a V1 superblock won't
	 * support it.  Otherwise, only skip the revoke on un-journaled
	 * data blocks. */

	if (test_opt(inode->i_sb, DATA_FLAGS) == JRNL_COMP_EXT4_MOUNT_JOURNAL_DATA ||
	    (!is_metadata && !jrnl_comp_ext4_should_journal_data(inode))) {
		if (bh) {
			BUFFER_TRACE(bh, "call jrnl_comp_jbd2_journal_forget");
			err = jrnl_comp_jbd2_journal_forget(handle, bh);
			if (err)
				jrnl_comp_ext4_journal_abort_handle(where, line, __func__,
							  bh, handle, err);
			return err;
		}
		return 0;
	}

	/*
	 * data!=journal && (is_metadata || should_journal_data(inode))
	 */
	BUFFER_TRACE(bh, "call jrnl_comp_jbd2_journal_revoke");
	err = jrnl_comp_jbd2_journal_revoke(handle, blocknr, bh);
	if (err) {
		jrnl_comp_ext4_journal_abort_handle(where, line, __func__,
					  bh, handle, err);
		__jrnl_comp_ext4_error(inode->i_sb, where, line, true, -err, 0,
			     "error %d when attempting revoke", err);
	}
	BUFFER_TRACE(bh, "exit");
	return err;
}

int __jrnl_comp_ext4_journal_get_create_access(const char *where, unsigned int line,
				jrnl_comp_handle_t *handle, struct super_block *sb,
				struct buffer_head *bh,
				enum jrnl_comp_ext4_journal_trigger_type trigger_type)
{
	int err;

	if (!jrnl_comp_ext4_handle_valid(handle))
		return 0;

	err = jrnl_comp_jbd2_journal_get_create_access(handle, bh);
	if (err) {
		jrnl_comp_ext4_journal_abort_handle(where, line, __func__, bh, handle,
					  err);
		return err;
	}
	if (trigger_type == JRNL_COMP_EXT4_JTR_NONE ||
	    !jrnl_comp_ext4_has_feature_metadata_csum(sb))
		return 0;
	BUG_ON(trigger_type >= JRNL_COMP_EXT4_JOURNAL_TRIGGER_COUNT);
	jrnl_comp_jbd2_journal_set_triggers(bh,
		&JRNL_COMP_EXT4_SB(sb)->s_journal_triggers[trigger_type].tr_triggers);
	return 0;
}

int __jrnl_comp_ext4_handle_dirty_metadata_range(const char *where, unsigned int line,
				 jrnl_comp_handle_t *handle, struct inode *inode,
				 struct buffer_head *bh, unsigned int offset,
				 unsigned int length)
{
	int err = 0;

	might_sleep();

	set_buffer_meta(bh);
	set_buffer_prio(bh);
	set_buffer_uptodate(bh);
	if (jrnl_comp_ext4_handle_valid(handle)) {
		err = jrnl_comp_jbd2_journal_dirty_metadata_range(handle, bh, offset, length);
		/* Errors can only happen due to aborted journal or a nasty bug */
		if (!is_handle_aborted(handle) && WARN_ON_ONCE(err)) {
			jrnl_comp_ext4_journal_abort_handle(where, line, __func__, bh,
						  handle, err);
			if (inode == NULL) {
				pr_err("JRNL_COMP_EXT4: jrnl_comp_jbd2_journal_dirty_metadata "
				       "failed: handle type %u started at "
				       "line %u, credits %u/%u, errcode %d",
				       handle->h_type,
				       handle->h_line_no,
				       handle->h_requested_credits,
				       jrnl_comp_jbd2_handle_buffer_credits(handle), err);
				return err;
			}
			jrnl_comp_ext4_error_inode(inode, where, line,
					 bh->b_blocknr,
					 "journal_dirty_metadata failed: "
					 "handle type %u started at line %u, "
					 "credits %u/%u, errcode %d",
					 handle->h_type,
					 handle->h_line_no,
					 handle->h_requested_credits,
					 jrnl_comp_jbd2_handle_buffer_credits(handle),
					 err);
		}
	} else {
		if (inode)
			mmb_mark_buffer_dirty(bh,
					      &JRNL_COMP_EXT4_I(inode)->i_metadata_bhs);
		else
			mark_buffer_dirty(bh);
		if (inode && inode_needs_sync(inode)) {
			sync_dirty_buffer(bh);
			if (buffer_req(bh) && !buffer_uptodate(bh)) {
				jrnl_comp_ext4_error_inode_err(inode, where, line,
						     bh->b_blocknr, EIO,
					"IO error syncing itable block");
				err = -EIO;
			}
		}
	}
	return err;
}

int __jrnl_comp_ext4_handle_dirty_metadata(const char *where, unsigned int line,
				 jrnl_comp_handle_t *handle, struct inode *inode,
				 struct buffer_head *bh)
{
	return __jrnl_comp_ext4_handle_dirty_metadata_range(where, line, handle, inode, bh,
						 0, bh->b_size);
}
