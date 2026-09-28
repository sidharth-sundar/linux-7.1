// SPDX-License-Identifier: GPL-2.0
/*
 *  linux/fs/jrnl_comp_ext4/bitmap.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 */

#include <linux/buffer_head.h>
#include "jrnl_comp_ext4.h"

unsigned int jrnl_comp_ext4_count_free(char *bitmap, unsigned int numchars)
{
	return numchars * BITS_PER_BYTE - memweight(bitmap, numchars);
}

int jrnl_comp_ext4_inode_bitmap_csum_verify(struct super_block *sb,
				  struct jrnl_comp_ext4_group_desc *gdp,
				  struct buffer_head *bh)
{
	__u32 hi;
	__u32 provided, calculated;
	struct jrnl_comp_ext4_sb_info *sbi = JRNL_COMP_EXT4_SB(sb);
	int sz;

	if (!jrnl_comp_ext4_has_feature_metadata_csum(sb))
		return 1;

	sz = JRNL_COMP_EXT4_INODES_PER_GROUP(sb) >> 3;
	provided = le16_to_cpu(gdp->bg_inode_bitmap_csum_lo);
	calculated = jrnl_comp_ext4_chksum(sbi->s_csum_seed, (__u8 *)bh->b_data, sz);
	if (sbi->s_desc_size >= JRNL_COMP_EXT4_BG_INODE_BITMAP_CSUM_HI_END) {
		hi = le16_to_cpu(gdp->bg_inode_bitmap_csum_hi);
		provided |= (hi << 16);
	} else
		calculated &= 0xFFFF;

	return provided == calculated;
}

void jrnl_comp_ext4_inode_bitmap_csum_set(struct super_block *sb,
				struct jrnl_comp_ext4_group_desc *gdp,
				struct buffer_head *bh)
{
	__u32 csum;
	struct jrnl_comp_ext4_sb_info *sbi = JRNL_COMP_EXT4_SB(sb);
	int sz;

	if (!jrnl_comp_ext4_has_feature_metadata_csum(sb))
		return;

	sz = JRNL_COMP_EXT4_INODES_PER_GROUP(sb) >> 3;
	csum = jrnl_comp_ext4_chksum(sbi->s_csum_seed, (__u8 *)bh->b_data, sz);
	gdp->bg_inode_bitmap_csum_lo = cpu_to_le16(csum & 0xFFFF);
	if (sbi->s_desc_size >= JRNL_COMP_EXT4_BG_INODE_BITMAP_CSUM_HI_END)
		gdp->bg_inode_bitmap_csum_hi = cpu_to_le16(csum >> 16);
}

int jrnl_comp_ext4_block_bitmap_csum_verify(struct super_block *sb,
				  struct jrnl_comp_ext4_group_desc *gdp,
				  struct buffer_head *bh)
{
	__u32 hi;
	__u32 provided, calculated;
	struct jrnl_comp_ext4_sb_info *sbi = JRNL_COMP_EXT4_SB(sb);
	int sz = JRNL_COMP_EXT4_CLUSTERS_PER_GROUP(sb) / 8;

	if (!jrnl_comp_ext4_has_feature_metadata_csum(sb))
		return 1;

	provided = le16_to_cpu(gdp->bg_block_bitmap_csum_lo);
	calculated = jrnl_comp_ext4_chksum(sbi->s_csum_seed, (__u8 *)bh->b_data, sz);
	if (sbi->s_desc_size >= JRNL_COMP_EXT4_BG_BLOCK_BITMAP_CSUM_HI_END) {
		hi = le16_to_cpu(gdp->bg_block_bitmap_csum_hi);
		provided |= (hi << 16);
	} else
		calculated &= 0xFFFF;

	return provided == calculated;
}

void jrnl_comp_ext4_block_bitmap_csum_set(struct super_block *sb,
				struct jrnl_comp_ext4_group_desc *gdp,
				struct buffer_head *bh)
{
	int sz = JRNL_COMP_EXT4_CLUSTERS_PER_GROUP(sb) / 8;
	__u32 csum;
	struct jrnl_comp_ext4_sb_info *sbi = JRNL_COMP_EXT4_SB(sb);

	if (!jrnl_comp_ext4_has_feature_metadata_csum(sb))
		return;

	csum = jrnl_comp_ext4_chksum(sbi->s_csum_seed, (__u8 *)bh->b_data, sz);
	gdp->bg_block_bitmap_csum_lo = cpu_to_le16(csum & 0xFFFF);
	if (sbi->s_desc_size >= JRNL_COMP_EXT4_BG_BLOCK_BITMAP_CSUM_HI_END)
		gdp->bg_block_bitmap_csum_hi = cpu_to_le16(csum >> 16);
}
