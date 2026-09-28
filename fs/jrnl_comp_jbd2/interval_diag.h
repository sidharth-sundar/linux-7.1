/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _JRNL_COMP_JBD2_INTERVAL_DIAG_H
#define _JRNL_COMP_JBD2_INTERVAL_DIAG_H
/* IDs are stable within the diagnostic artifact, not a public ABI. */
enum jrnl_comp_jbd2_interval_op {
	JI_START = 1, JI_STOP, JI_RESTART, JI_RESERVED, JI_EXTEND,
	JI_WRITE_ACCESS, JI_CREATE_ACCESS, JI_UNDO_ACCESS, JI_DIRTY,
	JI_FORGET, JI_REVOKE, JI_REQUEST, JI_COMPLETE, JI_COMMIT_WAIT,
	JI_SLOT_WAIT = 32, JI_DIRECT_IO, JI_BUFFER_LOCK, JI_COPY_ALLOC,
	JI_LAZY_COPY, JI_ADMISSION, JI_SWITCH, JI_RESERVED_WAIT,
	JI_LOG_SPACE, JI_BARRIER, JI_SYNC_BATCH, JI_FORGET_IO, JI_LOG_COPY,
	JI_FLUSH = 64, JI_FLUSH_IO, JI_DEVICE_FLUSH, JI_COMMIT, JI_COALESCE, JI_DISPATCH,
};
#ifdef CONFIG_JRNL_COMP_JBD2_INTERVAL_DIAG
#include <trace/events/jrnl_comp_jbd2_interval.h>
struct jrnl_comp_jbd2_interval_ctx {
	dev_t dev;
	jrnl_comp_tid_t tid;
	unsigned long object;
};
static inline struct jrnl_comp_jbd2_interval_ctx
ji_journal(jrnl_comp_journal_t *journal, jrnl_comp_tid_t tid, unsigned long object)
{
	return (struct jrnl_comp_jbd2_interval_ctx) {
		.dev = journal ? journal->j_fs_dev->bd_dev : 0,
		.tid = tid,
		.object = object,
	};
}
static inline struct jrnl_comp_jbd2_interval_ctx ji_handle(jrnl_comp_handle_t *handle)
{
	if (handle->h_reserved)
		return ji_journal(handle->h_journal, 0, (unsigned long)handle);
	if (handle->h_transaction)
		return ji_journal(handle->h_transaction->t_journal,
				  handle->h_transaction->t_tid,
				  (unsigned long)handle);
	return ji_journal(NULL, 0, (unsigned long)handle);
}
static inline void ji_emit(struct jrnl_comp_jbd2_interval_ctx ctx, unsigned int op,
			   bool begin, int result)
{
	trace_jrnl_comp_jbd2_interval(ctx.dev, ctx.tid, op, begin, ctx.object, result);
}
/* Snapshot only scalars before a call that may drop the transaction/handle. */
#define ji_wait(journal, tid, op, object, ...) do { \
	bool __enabled = trace_jrnl_comp_jbd2_interval_enabled(); \
	struct jrnl_comp_jbd2_interval_ctx __ctx; \
	if (__enabled) { \
		__ctx = ji_journal(journal, tid, (unsigned long)(object)); \
		ji_emit(__ctx, op, true, 0); \
	} \
	__VA_ARGS__; \
	if (__enabled) \
		ji_emit(__ctx, op, false, 0); \
} while (0)
#else
#define ji_wait(journal, tid, op, object, ...) do { __VA_ARGS__; } while (0)
#endif
#endif
