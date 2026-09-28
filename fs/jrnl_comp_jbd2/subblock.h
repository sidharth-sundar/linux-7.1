/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _JRNL_COMP_JBD2_SUBBLOCK_H
#define _JRNL_COMP_JBD2_SUBBLOCK_H
#include <linux/jrnl_comp_jbd2.h>
#include "subblock_format.h"

/* These functions do not enable compact logging; commit/replay must opt in. */
void jrnl_comp_jbd2_subblock_set_csum(jrnl_comp_journal_t *journal, struct buffer_head *bh);
int jrnl_comp_jbd2_subblock_validate(jrnl_comp_journal_t *journal, struct buffer_head *bh, jrnl_comp_tid_t tid);
#endif
