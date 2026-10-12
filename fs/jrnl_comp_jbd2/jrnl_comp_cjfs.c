// SPDX-License-Identifier: GPL-2.0
/* Quiescent runtime configuration for the JRNL_COMP_CJFS experiment. */
#include <linux/jrnl_comp_jbd2.h>
#include <linux/jrnl_comp_jbd2_workvolume.h>
#include "subblock_format.h"
#define CREATE_TRACE_POINTS
#include <trace/events/jrnl_comp_cjfs.h>

/* Caller holds the configuration mutex and protects the mount lifetime. */
int jrnl_comp_jbd2_jrnl_comp_cjfs_reconfigure(jrnl_comp_journal_t *journal)
{
 int err;

 lockdep_assert_held(&journal->j_jrnl_comp_cjfs_config_mutex);
 /* Copy policy is fixed at journal creation; no live mode transitions.
  * Sub-block logging is supported only at 256 bytes (eager, depth 1);
  * the flush below quiesces the journal before the format changes.
  * Experimental 0-1000us coalescing is bounded to depth 1.
  * Full-block eager/lazy and eager 256-byte logging are admitted below;
  * lazy sub-block logging stays unsupported. The default remains zero.
  */
 if (journal->j_jrnl_comp_cjfs_pending_depth != 1 ||
     journal->j_jrnl_comp_cjfs_pending_eager !=
         journal->j_jrnl_comp_cjfs_eager ||
     journal->j_jrnl_comp_cjfs_pending_window_us > 1000 ||
     (journal->j_subblock_pending_granularity &&
      journal->j_subblock_pending_granularity != 256))
  return -EOPNOTSUPP;
 if (jrnl_comp_jbd2_wv_active(journal))
  return -EBUSY;
 if (!journal->j_jrnl_comp_cjfs_pending_depth ||
     journal->j_jrnl_comp_cjfs_pending_depth > JRNL_COMP_CJFS_MAX_VERSIONS ||
     journal->j_jrnl_comp_cjfs_pending_window_us > 10000)
  return -EINVAL;
 if (is_journal_aborted(journal))
  return -EROFS;
 if (journal->j_subblock_pending_granularity &&
     ((journal->j_subblock_pending_granularity != 64 &&
       journal->j_subblock_pending_granularity != 128 &&
       journal->j_subblock_pending_granularity != 256) ||
      !journal->j_jrnl_comp_cjfs_pending_eager ||
      journal->j_jrnl_comp_cjfs_pending_depth != 1 ||
      !jrnl_comp_jbd2_sb_size_valid(journal->j_blocksize) ||
      !jrnl_comp_jbd2_has_feature_csum3(journal) ||
      jrnl_comp_jbd2_has_feature_fast_commit(journal)))
  return -EOPNOTSUPP;
 WRITE_ONCE(journal->j_jrnl_comp_cjfs_reconfiguring, true);
 wake_up_all(&journal->j_wait_done_flush);
 /* Existing reserved handles must be allowed to finish under this barrier. */
 jrnl_comp_jbd2_journal_lock_updates(journal);
 err = jrnl_comp_jbd2_journal_flush(journal, 0);
 if (err)
  goto out;
 write_lock(&journal->j_state_lock);
 spin_lock(&journal->j_list_lock);
 if (is_journal_aborted(journal)) {
  err = -EIO;
 } else if (journal->j_running_transaction || journal->j_committing_transaction ||
     journal->j_flushing_transactions || journal->j_checkpoint_transactions ||
     journal->j_commit_sequence != journal->j_flush_sequence ||
     atomic_read(&journal->j_reserved_credits)) {
  err = -EBUSY;
 } else {
  journal->j_jrnl_comp_cjfs_depth = journal->j_jrnl_comp_cjfs_pending_depth;
  journal->j_jrnl_comp_cjfs_eager = journal->j_jrnl_comp_cjfs_pending_eager;
  journal->j_jrnl_comp_cjfs_window_us = journal->j_jrnl_comp_cjfs_pending_window_us;
  journal->j_jrnl_comp_cjfs_generation++;
  trace_jrnl_comp_cjfs_event(journal->j_fs_dev->bd_dev, journal->j_flush_sequence,
    journal->j_jrnl_comp_cjfs_generation, "apply", journal->j_jrnl_comp_cjfs_depth);
 }
 spin_unlock(&journal->j_list_lock);
 write_unlock(&journal->j_state_lock);
 if (!err) {
  /* Updates remain barred and the old log is fully checkpointed. The
   * next commit persists the feature before using the empty journal.
   */
  lock_buffer(journal->j_sb_buffer);
  if (journal->j_subblock_pending_granularity)
   jrnl_comp_jbd2_set_feature_subblock(journal);
  else
   jrnl_comp_jbd2_clear_feature_subblock(journal);
  journal->j_subblock_granularity = journal->j_subblock_pending_granularity;
  unlock_buffer(journal->j_sb_buffer);
 }
out:
 WRITE_ONCE(journal->j_jrnl_comp_cjfs_reconfiguring, false);
 jrnl_comp_jbd2_journal_unlock_updates(journal);
 return err;
}
EXPORT_SYMBOL_GPL(jrnl_comp_jbd2_jrnl_comp_cjfs_reconfigure);
