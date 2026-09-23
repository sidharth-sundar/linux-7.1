/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_JBD2_WORKVOLUME_H
#define _LINUX_JBD2_WORKVOLUME_H
#include <linux/jbd2.h>
ssize_t jbd2_wv_access(journal_t *, struct super_block *, const char *, char *, const char *, size_t);
bool jbd2_wv_active(journal_t *);
void jbd2_wv_destroy(journal_t *);
void jbd2_wv_image(transaction_t *, struct buffer_head *, struct folio *, unsigned int, bool);
void jbd2_wv_invalidate(journal_t *, tid_t);
void jbd2_wv_control(transaction_t *, int);
#endif
