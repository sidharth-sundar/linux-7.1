/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _JBD2_SUBBLOCK_H
#define _JBD2_SUBBLOCK_H
#include <linux/jbd2.h>
#include "subblock_format.h"

/* These functions do not enable compact logging; commit/replay must opt in. */
void jbd2_subblock_set_csum(journal_t *journal, struct buffer_head *bh);
int jbd2_subblock_validate(journal_t *journal, struct buffer_head *bh, tid_t tid);
#endif
