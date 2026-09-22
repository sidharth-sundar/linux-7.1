/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM rwsem_diag
#if !defined(_TRACE_RWSEM_DIAG_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_RWSEM_DIAG_H
#include <linux/tracepoint.h>
TRACE_EVENT(rwsem_diag_writer,
	TP_PROTO(void *lock, unsigned int stage, unsigned int ticks,
		 unsigned long generation, unsigned long deadline),
	TP_ARGS(lock, stage, ticks, generation, deadline),
	TP_STRUCT__entry(
		__field(void *, lock)
		__field(unsigned int, stage)
		__field(unsigned int, ticks)
		__field(unsigned long, generation)
		__field(unsigned long, deadline)
	),
	TP_fast_assign(
		__entry->lock = lock;
		__entry->stage = stage;
		__entry->ticks = ticks;
		__entry->generation = generation;
		__entry->deadline = deadline;
	),
	TP_printk("lock=%p stage=%u ticks=%u generation=%lu deadline=%lu",
		__entry->lock, __entry->stage, __entry->ticks,
		__entry->generation, __entry->deadline)
);
#endif
#include <trace/define_trace.h>
