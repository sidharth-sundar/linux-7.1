/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LOCKING_RWSEM_DIAG_H
#define _LOCKING_RWSEM_DIAG_H
#include <linux/jiffies.h>
struct rw_semaphore;
#ifdef CONFIG_RWSEM_HANDOFF_DIAG
unsigned long rwsem_diag_deadline(struct rw_semaphore *sem,
				 unsigned long default_ticks);
void rwsem_diag_handoff(struct rw_semaphore *sem, unsigned long deadline);
#else
static inline unsigned long rwsem_diag_deadline(struct rw_semaphore *sem,
					       unsigned long default_ticks)
{
	return jiffies + default_ticks;
}
static inline void rwsem_diag_handoff(struct rw_semaphore *sem,
				     unsigned long deadline) { }
#endif
#endif
