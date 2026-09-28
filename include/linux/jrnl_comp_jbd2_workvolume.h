/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_JRNL_COMP_JBD2_WORKVOLUME_H
#define _LINUX_JRNL_COMP_JBD2_WORKVOLUME_H
#include <linux/jrnl_comp_jbd2.h>
ssize_t jrnl_comp_jbd2_wv_access(jrnl_comp_journal_t *, struct super_block *, const char *, char *, const char *, size_t);
bool jrnl_comp_jbd2_wv_active(jrnl_comp_journal_t *);
void jrnl_comp_jbd2_wv_destroy(jrnl_comp_journal_t *);
void jrnl_comp_jbd2_wv_image(jrnl_comp_transaction_t *, struct buffer_head *, struct folio *, unsigned int, bool);
void jrnl_comp_jbd2_wv_invalidate(jrnl_comp_journal_t *, jrnl_comp_tid_t);
void jrnl_comp_jbd2_wv_control(jrnl_comp_transaction_t *, int);
#endif
