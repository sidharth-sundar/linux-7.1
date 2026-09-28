/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM jrnl_comp_cjfs
#if !defined(_TRACE_JRNL_COMP_CJFS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_JRNL_COMP_CJFS_H
#include <linux/tracepoint.h>
TRACE_EVENT(jrnl_comp_cjfs_event,
 TP_PROTO(dev_t dev, jrnl_comp_tid_t tid, u64 generation, const char *event, u64 value),
 TP_ARGS(dev, tid, generation, event, value),
 TP_STRUCT__entry(
  __field(dev_t, dev)
  __field(jrnl_comp_tid_t, tid)
  __field(u64, generation)
  __string(event, event)
  __field(u64, value)
 ),
 TP_fast_assign(
  __entry->dev = dev;
  __entry->tid = tid;
  __entry->generation = generation;
  __assign_str(event);
  __entry->value = value;
 ),
 TP_printk("dev=%d:%d tid=%u generation=%llu event=%s value=%llu",
  MAJOR(__entry->dev), MINOR(__entry->dev), __entry->tid,
  __entry->generation, __get_str(event), __entry->value)
);
#endif
#include <trace/define_trace.h>
