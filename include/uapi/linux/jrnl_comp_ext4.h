/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef _UAPI_LINUX_JRNL_COMP_EXT4_H
#define _UAPI_LINUX_JRNL_COMP_EXT4_H
#include <linux/fiemap.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/types.h>

/*
 * jrnl_comp_ext4-specific ioctl commands
 */
#define	JRNL_COMP_EXT4_IOC_GETVERSION		_IOR('f', 3, long)
#define	JRNL_COMP_EXT4_IOC_SETVERSION		_IOW('f', 4, long)
#define	JRNL_COMP_EXT4_IOC_GETVERSION_OLD		FS_IOC_GETVERSION
#define	JRNL_COMP_EXT4_IOC_SETVERSION_OLD		FS_IOC_SETVERSION
#define JRNL_COMP_EXT4_IOC_GETRSVSZ		_IOR('f', 5, long)
#define JRNL_COMP_EXT4_IOC_SETRSVSZ		_IOW('f', 6, long)
#define JRNL_COMP_EXT4_IOC_GROUP_EXTEND		_IOW('f', 7, unsigned long)
#define JRNL_COMP_EXT4_IOC_GROUP_ADD		_IOW('f', 8, struct jrnl_comp_ext4_new_group_input)
#define JRNL_COMP_EXT4_IOC_MIGRATE		_IO('f', 9)
 /* note ioctl 10 reserved for an early version of the FIEMAP ioctl */
 /* note ioctl 11 reserved for filesystem-independent FIEMAP ioctl */
#define JRNL_COMP_EXT4_IOC_ALLOC_DA_BLKS		_IO('f', 12)
#define JRNL_COMP_EXT4_IOC_MOVE_EXT		_IOWR('f', 15, struct move_extent)
#define JRNL_COMP_EXT4_IOC_RESIZE_FS		_IOW('f', 16, __u64)
#define JRNL_COMP_EXT4_IOC_SWAP_BOOT		_IO('f', 17)
#define JRNL_COMP_EXT4_IOC_PRECACHE_EXTENTS	_IO('f', 18)
/* ioctl codes 19--39 are reserved for fscrypt */
#define JRNL_COMP_EXT4_IOC_CLEAR_ES_CACHE		_IO('f', 40)
#define JRNL_COMP_EXT4_IOC_GETSTATE		_IOW('f', 41, __u32)
#define JRNL_COMP_EXT4_IOC_GET_ES_CACHE		_IOWR('f', 42, struct fiemap)
#define JRNL_COMP_EXT4_IOC_CHECKPOINT		_IOW('f', 43, __u32)
#define JRNL_COMP_EXT4_IOC_GETFSUUID		_IOR('f', 44, struct fsuuid)
#define JRNL_COMP_EXT4_IOC_SETFSUUID		_IOW('f', 44, struct fsuuid)
#define JRNL_COMP_EXT4_IOC_GET_TUNE_SB_PARAM	_IOR('f', 45, struct jrnl_comp_ext4_tune_sb_params)
#define JRNL_COMP_EXT4_IOC_SET_TUNE_SB_PARAM	_IOW('f', 46, struct jrnl_comp_ext4_tune_sb_params)

#define JRNL_COMP_EXT4_IOC_SHUTDOWN _IOR('X', 125, __u32)

/*
 * ioctl commands in 32 bit emulation
 */
#define JRNL_COMP_EXT4_IOC32_GETVERSION		_IOR('f', 3, int)
#define JRNL_COMP_EXT4_IOC32_SETVERSION		_IOW('f', 4, int)
#define JRNL_COMP_EXT4_IOC32_GETRSVSZ		_IOR('f', 5, int)
#define JRNL_COMP_EXT4_IOC32_SETRSVSZ		_IOW('f', 6, int)
#define JRNL_COMP_EXT4_IOC32_GROUP_EXTEND		_IOW('f', 7, unsigned int)
#define JRNL_COMP_EXT4_IOC32_GROUP_ADD		_IOW('f', 8, struct compat_jrnl_comp_ext4_new_group_input)
#define JRNL_COMP_EXT4_IOC32_GETVERSION_OLD	FS_IOC32_GETVERSION
#define JRNL_COMP_EXT4_IOC32_SETVERSION_OLD	FS_IOC32_SETVERSION

/*
 * Flags returned by JRNL_COMP_EXT4_IOC_GETSTATE
 *
 * We only expose to userspace a subset of the state flags in
 * i_state_flags
 */
#define JRNL_COMP_EXT4_STATE_FLAG_EXT_PRECACHED	0x00000001
#define JRNL_COMP_EXT4_STATE_FLAG_NEW		0x00000002
#define JRNL_COMP_EXT4_STATE_FLAG_NEWENTRY	0x00000004
#define JRNL_COMP_EXT4_STATE_FLAG_DA_ALLOC_CLOSE	0x00000008

/*
 * Flags for ioctl JRNL_COMP_EXT4_IOC_CHECKPOINT
 */
#define JRNL_COMP_EXT4_IOC_CHECKPOINT_FLAG_DISCARD	0x1
#define JRNL_COMP_EXT4_IOC_CHECKPOINT_FLAG_ZEROOUT	0x2
#define JRNL_COMP_EXT4_IOC_CHECKPOINT_FLAG_DRY_RUN	0x4
#define JRNL_COMP_EXT4_IOC_CHECKPOINT_FLAG_VALID		(JRNL_COMP_EXT4_IOC_CHECKPOINT_FLAG_DISCARD | \
						JRNL_COMP_EXT4_IOC_CHECKPOINT_FLAG_ZEROOUT | \
						JRNL_COMP_EXT4_IOC_CHECKPOINT_FLAG_DRY_RUN)

/*
 * Structure for JRNL_COMP_EXT4_IOC_GETFSUUID/JRNL_COMP_EXT4_IOC_SETFSUUID
 */
struct fsuuid {
	__u32       fsu_len;
	__u32       fsu_flags;
	__u8        fsu_uuid[];
};

/*
 * Structure for JRNL_COMP_EXT4_IOC_MOVE_EXT
 */
struct move_extent {
	__u32 reserved;		/* should be zero */
	__u32 donor_fd;		/* donor file descriptor */
	__u64 orig_start;	/* logical start offset in block for orig */
	__u64 donor_start;	/* logical start offset in block for donor */
	__u64 len;		/* block length to be moved */
	__u64 moved_len;	/* moved block length */
};

/*
 * Flags used by JRNL_COMP_EXT4_IOC_SHUTDOWN
 */
#define JRNL_COMP_EXT4_GOING_FLAGS_DEFAULT		0x0	/* going down */
#define JRNL_COMP_EXT4_GOING_FLAGS_LOGFLUSH		0x1	/* flush log but not data */
#define JRNL_COMP_EXT4_GOING_FLAGS_NOLOGFLUSH		0x2	/* don't flush log nor data */

/* Used to pass group descriptor data when online resize is done */
struct jrnl_comp_ext4_new_group_input {
	__u32 group;		/* Group number for this data */
	__u64 block_bitmap;	/* Absolute block number of block bitmap */
	__u64 inode_bitmap;	/* Absolute block number of inode bitmap */
	__u64 inode_table;	/* Absolute block number of inode table start */
	__u32 blocks_count;	/* Total number of blocks in this group */
	__u16 reserved_blocks;	/* Number of reserved blocks in this group */
	__u16 unused;
};

struct jrnl_comp_ext4_tune_sb_params {
	__u32 set_flags;
	__u32 checkinterval;
	__u16 errors_behavior;
	__u16 mnt_count;
	__u16 max_mnt_count;
	__u16 raid_stride;
	__u64 last_check_time;
	__u64 reserved_blocks;
	__u64 blocks_count;
	__u32 default_mnt_opts;
	__u32 reserved_uid;
	__u32 reserved_gid;
	__u32 raid_stripe_width;
	__u16 encoding;
	__u16 encoding_flags;
	__u8  def_hash_alg;
	__u8  pad_1;
	__u16 pad_2;
	__u32 feature_compat;
	__u32 feature_incompat;
	__u32 feature_ro_compat;
	__u32 set_feature_compat_mask;
	__u32 set_feature_incompat_mask;
	__u32 set_feature_ro_compat_mask;
	__u32 clear_feature_compat_mask;
	__u32 clear_feature_incompat_mask;
	__u32 clear_feature_ro_compat_mask;
	__u8  mount_opts[64];
	__u8  pad[68];
};

#define JRNL_COMP_EXT4_TUNE_FL_ERRORS_BEHAVIOR	0x00000001
#define JRNL_COMP_EXT4_TUNE_FL_MNT_COUNT		0x00000002
#define JRNL_COMP_EXT4_TUNE_FL_MAX_MNT_COUNT	0x00000004
#define JRNL_COMP_EXT4_TUNE_FL_CHECKINTRVAL	0x00000008
#define JRNL_COMP_EXT4_TUNE_FL_LAST_CHECK_TIME	0x00000010
#define JRNL_COMP_EXT4_TUNE_FL_RESERVED_BLOCKS	0x00000020
#define JRNL_COMP_EXT4_TUNE_FL_RESERVED_UID	0x00000040
#define JRNL_COMP_EXT4_TUNE_FL_RESERVED_GID	0x00000080
#define JRNL_COMP_EXT4_TUNE_FL_DEFAULT_MNT_OPTS	0x00000100
#define JRNL_COMP_EXT4_TUNE_FL_DEF_HASH_ALG	0x00000200
#define JRNL_COMP_EXT4_TUNE_FL_RAID_STRIDE	0x00000400
#define JRNL_COMP_EXT4_TUNE_FL_RAID_STRIPE_WIDTH	0x00000800
#define JRNL_COMP_EXT4_TUNE_FL_MOUNT_OPTS		0x00001000
#define JRNL_COMP_EXT4_TUNE_FL_FEATURES		0x00002000
#define JRNL_COMP_EXT4_TUNE_FL_EDIT_FEATURES	0x00004000
#define JRNL_COMP_EXT4_TUNE_FL_FORCE_FSCK		0x00008000
#define JRNL_COMP_EXT4_TUNE_FL_ENCODING		0x00010000
#define JRNL_COMP_EXT4_TUNE_FL_ENCODING_FLAGS	0x00020000

/*
 * Returned by JRNL_COMP_EXT4_IOC_GET_ES_CACHE as an additional possible flag.
 * It indicates that the entry in extent status cache is for a hole.
 */
#define JRNL_COMP_EXT4_FIEMAP_EXTENT_HOLE		0x08000000

#endif /* _UAPI_LINUX_JRNL_COMP_EXT4_H */
