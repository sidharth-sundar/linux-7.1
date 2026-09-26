// SPDX-License-Identifier: GPL-2.0
/* Checksummed experimental sub-block container primitives. */
#include <linux/buffer_head.h>
#include "subblock.h"

static u32 subblock_csum(journal_t *journal, const unsigned char *data)
{
	const __be32 zero = 0;
	u32 crc;

	/* UUID-seeded CRC32C covers identity, framing, payload and zero padding. */
	crc = jbd2_chksum(journal->j_csum_seed, data, JBD2_SB_CSUM_OFFSET);
	crc = jbd2_chksum(crc, &zero, sizeof(zero));
	return jbd2_chksum(crc, data + JBD2_SB_CSUM_OFFSET + sizeof(zero),
		journal->j_blocksize - JBD2_SB_CSUM_OFFSET - sizeof(zero));
}

void jbd2_subblock_set_csum(journal_t *journal, struct buffer_head *bh)
{
	jbd2_sb_put32((unsigned char *)bh->b_data + JBD2_SB_CSUM_OFFSET,
		     subblock_csum(journal, (unsigned char *)bh->b_data));
}

int jbd2_subblock_validate(journal_t *journal, struct buffer_head *bh, tid_t tid)
{
	const unsigned char *data = (unsigned char *)bh->b_data;

	if (!jbd2_sb_size_valid(journal->j_blocksize) ||
	    bh->b_size != journal->j_blocksize ||
	    jbd2_sb_validate(data, bh->b_size, tid))
		return -EFSCORRUPTED;
	if (jbd2_sb_get32(data + JBD2_SB_CSUM_OFFSET) !=
	    subblock_csum(journal, data))
		return -EFSBADCRC;
	return 0;
}
