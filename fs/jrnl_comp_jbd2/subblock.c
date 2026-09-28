// SPDX-License-Identifier: GPL-2.0
/* Checksummed experimental sub-block container primitives. */
#include <linux/buffer_head.h>
#include "subblock.h"

static u32 subblock_csum(jrnl_comp_journal_t *journal, const unsigned char *data)
{
	const __be32 zero = 0;
	u32 crc;

	/* UUID-seeded CRC32C covers identity, framing, payload and zero padding. */
	crc = jrnl_comp_jbd2_chksum(journal->j_csum_seed, data, JRNL_COMP_JBD2_SB_CSUM_OFFSET);
	crc = jrnl_comp_jbd2_chksum(crc, &zero, sizeof(zero));
	return jrnl_comp_jbd2_chksum(crc, data + JRNL_COMP_JBD2_SB_CSUM_OFFSET + sizeof(zero),
		journal->j_blocksize - JRNL_COMP_JBD2_SB_CSUM_OFFSET - sizeof(zero));
}

void jrnl_comp_jbd2_subblock_set_csum(jrnl_comp_journal_t *journal, struct buffer_head *bh)
{
	jrnl_comp_jbd2_sb_put32((unsigned char *)bh->b_data + JRNL_COMP_JBD2_SB_CSUM_OFFSET,
		     subblock_csum(journal, (unsigned char *)bh->b_data));
}

int jrnl_comp_jbd2_subblock_validate(jrnl_comp_journal_t *journal, struct buffer_head *bh, jrnl_comp_tid_t tid)
{
	const unsigned char *data = (unsigned char *)bh->b_data;

	if (!jrnl_comp_jbd2_sb_size_valid(journal->j_blocksize) ||
	    bh->b_size != journal->j_blocksize ||
	    jrnl_comp_jbd2_sb_validate(data, bh->b_size, tid))
		return -EFSCORRUPTED;
	if (jrnl_comp_jbd2_sb_get32(data + JRNL_COMP_JBD2_SB_CSUM_OFFSET) !=
	    subblock_csum(journal, data))
		return -EFSBADCRC;
	return 0;
}
