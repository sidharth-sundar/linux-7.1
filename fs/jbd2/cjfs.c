// SPDX-License-Identifier: GPL-2.0
/* Quiescent runtime configuration for the CJFS experiment. */
#include <linux/jbd2.h>
#define CREATE_TRACE_POINTS
#include <trace/events/cjfs.h>

/* Caller holds the configuration mutex and protects the mount lifetime. */
int jbd2_cjfs_reconfigure(journal_t *journal)
{
 int err;

 lockdep_assert_held(&journal->j_cjfs_config_mutex);
 if (!journal->j_cjfs_pending_depth ||
     journal->j_cjfs_pending_depth > CJFS_MAX_VERSIONS ||
     journal->j_cjfs_pending_window_us > 10000)
  return -EINVAL;
 if (is_journal_aborted(journal))
  return -EROFS;
 WRITE_ONCE(journal->j_cjfs_reconfiguring, true);
 wake_up_all(&journal->j_wait_done_flush);
 /* Existing reserved handles must be allowed to finish under this barrier. */
 jbd2_journal_lock_updates(journal);
 err = jbd2_journal_flush(journal, 0);
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
  journal->j_cjfs_depth = journal->j_cjfs_pending_depth;
  journal->j_cjfs_eager = journal->j_cjfs_pending_eager;
  journal->j_cjfs_window_us = journal->j_cjfs_pending_window_us;
  journal->j_cjfs_generation++;
  trace_cjfs_event(journal->j_fs_dev->bd_dev, journal->j_flush_sequence,
    journal->j_cjfs_generation, "apply", journal->j_cjfs_depth);
 }
 spin_unlock(&journal->j_list_lock);
 write_unlock(&journal->j_state_lock);
out:
 WRITE_ONCE(journal->j_cjfs_reconfiguring, false);
 jbd2_journal_unlock_updates(journal);
 return err;
}
EXPORT_SYMBOL_GPL(jbd2_cjfs_reconfigure);
