// SPDX-License-Identifier: GPL-2.0
/* Private handle storage: never interpret or replace task->journal_info. */
#include <linux/hashtable.h>
#include <linux/jrnl_comp_jbd2.h>
#include <linux/sched/task.h>
#include <linux/slab.h>

struct jrnl_comp_task_context {
	struct hlist_node node;
	struct task_struct *task;
	jrnl_comp_handle_t *handle;
};

static DEFINE_HASHTABLE(contexts, 8);
static DEFINE_SPINLOCK(context_lock);

jrnl_comp_handle_t *jrnl_comp_journal_current_handle(void)
{
	struct jrnl_comp_task_context *ctx;
	jrnl_comp_handle_t *handle = NULL;

	spin_lock(&context_lock);
	hash_for_each_possible(contexts, ctx, node, (unsigned long)current) {
		if (ctx->task == current) {
			handle = ctx->handle;
			break;
		}
	}
	spin_unlock(&context_lock);
	return handle;
}
EXPORT_SYMBOL_GPL(jrnl_comp_journal_current_handle);

/* Only the current task accesses its entry. Starts/stops must remain paired;
 * no child inherits a handle. A task reference prevents address reuse even if
 * a buggy caller abandons a handle (which must be detected by lifecycle tests).
 * NOFAIL matches the journal's existing no-fail bookkeeping allocation policy;
 * callers may sleep and must not hold spinlocks here.
 */
void jrnl_comp_journal_set_current_handle(jrnl_comp_handle_t *handle)
{
	struct jrnl_comp_task_context *ctx;

	spin_lock(&context_lock);
	hash_for_each_possible(contexts, ctx, node, (unsigned long)current) {
		if (ctx->task != current)
			continue;
		if (handle) {
			ctx->handle = handle;
			spin_unlock(&context_lock);
			return;
		}
		hash_del(&ctx->node);
		spin_unlock(&context_lock);
		put_task_struct(ctx->task);
		kfree(ctx);
		return;
	}
	spin_unlock(&context_lock);
	if (!handle)
		return;
	ctx = kzalloc(sizeof(*ctx), GFP_NOFS | __GFP_NOFAIL);
	ctx->task = current;
	ctx->handle = handle;
	get_task_struct(current);
	spin_lock(&context_lock);
	hash_add(contexts, &ctx->node, (unsigned long)current);
	spin_unlock(&context_lock);
}
EXPORT_SYMBOL_GPL(jrnl_comp_journal_set_current_handle);
