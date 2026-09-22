/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM jbd2_interval
#if !defined(_TRACE_JBD2_INTERVAL_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_JBD2_INTERVAL_H
#include <linux/tracepoint.h>
TRACE_EVENT(jbd2_interval,
	TP_PROTO(dev_t dev, unsigned int tid, unsigned int op, bool begin,
		 unsigned long object, int result),
	TP_ARGS(dev, tid, op, begin, object, result),
	TP_STRUCT__entry(
		__field(dev_t, dev)
		__field(unsigned int, tid)
		__field(unsigned int, op)
		__field(bool, begin)
		__field(unsigned long, object)
		__field(int, result)
	),
	TP_fast_assign(
		__entry->dev = dev;
		__entry->tid = tid;
		__entry->op = op;
		__entry->begin = begin;
		__entry->object = object;
		__entry->result = result;
	),
	TP_printk("dev=%u:%u tid=%u op=%u begin=%u object=%lx result=%d",
		MAJOR(__entry->dev), MINOR(__entry->dev), __entry->tid,
		__entry->op, __entry->begin, __entry->object, __entry->result)
);
#endif
#include <trace/define_trace.h>
