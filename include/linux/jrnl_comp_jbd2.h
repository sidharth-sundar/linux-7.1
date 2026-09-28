/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * linux/include/linux/jrnl_comp_jbd2.h
 *
 * Written by Stephen C. Tweedie <sct@redhat.com>
 *
 * Copyright 1998-2000 Red Hat, Inc --- All Rights Reserved
 *
 * Definitions for transaction data structures for the buffer cache
 * filesystem journaling support.
 */

#ifndef _LINUX_JRNL_COMP_JBD2_H
#define _LINUX_JRNL_COMP_JBD2_H

/* Allow this file to be included directly into e2fsprogs */
#ifndef __KERNEL__
#include "jfs_compat.h"
#define JRNL_COMP_JBD2_DEBUG
#else

#include <linux/types.h>
#include <linux/buffer_head.h>
#include <linux/jrnl_comp_journal-head.h>
#include <linux/stddef.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/bit_spinlock.h>
#include <linux/blkdev.h>
#include <linux/crc32c.h>
#endif

#define journal_oom_retry 1

/*
 * Define JRNL_COMP_JBD2_PARANIOD_IOFAIL to cause a kernel BUG() if jrnl_comp_ext4 finds
 * certain classes of error which can occur due to failed IOs.  Under
 * normal use we want jrnl_comp_ext4 to continue after such errors, because
 * hardware _can_ fail, but for debugging purposes when running tests on
 * known-good hardware we may want to trap these errors.
 */
#undef JRNL_COMP_JBD2_PARANOID_IOFAIL

/*
 * The default maximum commit age, in seconds.
 */
#define JRNL_COMP_JBD2_DEFAULT_MAX_COMMIT_AGE 5

#ifdef CONFIG_JRNL_COMP_JBD2_DEBUG
/*
 * Define JRNL_COMP_JBD2_EXPENSIVE_CHECKING to enable more expensive internal
 * consistency checks.  By default we don't do this unless
 * CONFIG_JRNL_COMP_JBD2_DEBUG is on.
 */
#define JRNL_COMP_JBD2_EXPENSIVE_CHECKING
void __jrnl_comp_jbd2_debug(int level, const char *file, const char *func,
		  unsigned int line, const char *fmt, ...);

#define jrnl_comp_jbd2_debug(n, fmt, a...) \
	__jrnl_comp_jbd2_debug((n), __FILE__, __func__, __LINE__, (fmt), ##a)
#else
#define jrnl_comp_jbd2_debug(n, fmt, a...)  no_printk(fmt, ##a)
#endif

extern void *jrnl_comp_jbd2_alloc(size_t size, gfp_t flags);
extern void jrnl_comp_jbd2_free(void *ptr, size_t size);

#define JRNL_COMP_JBD2_MIN_JOURNAL_BLOCKS 1024
#define JRNL_COMP_JBD2_DEFAULT_FAST_COMMIT_BLOCKS 256

#ifdef __KERNEL__

/**
 * typedef jrnl_comp_handle_t - The jrnl_comp_handle_t type represents a single atomic update being performed by some process.
 *
 * All filesystem modifications made by the process go
 * through this handle.  Recursive operations (such as quota operations)
 * are gathered into a single update.
 *
 * The buffer credits field is used to account for journaled buffers
 * being modified by the running process.  To ensure that there is
 * enough log space for all outstanding operations, we need to limit the
 * number of outstanding buffers possible at any time.  When the
 * operation completes, any buffer credits not used are credited back to
 * the transaction, so that at all times we know how many buffers the
 * outstanding updates on a transaction might possibly touch.
 *
 * This is an opaque datatype.
 **/
typedef struct jrnl_comp_jbd2_journal_handle jrnl_comp_handle_t;	/* Atomic operation type */


/**
 * typedef jrnl_comp_journal_t - The jrnl_comp_journal_t maintains all of the journaling state information for a single filesystem.
 *
 * jrnl_comp_journal_t is linked to from the fs superblock structure.
 *
 * We use the jrnl_comp_journal_t to keep track of all outstanding transaction
 * activity on the filesystem, and to manage the state of the log
 * writing process.
 *
 * This is an opaque datatype.
 **/
typedef struct jrnl_comp_journal_s	jrnl_comp_journal_t;	/* Journal control structure */
#endif

/*
 * Internal structures used by the logging mechanism:
 */

#define JRNL_COMP_JBD2_MAGIC_NUMBER 0xc03b3998U /* The first 4 bytes of /dev/random! */

/*
 * On-disk structures
 */

/*
 * Descriptor block types:
 */

#define JRNL_COMP_JBD2_DESCRIPTOR_BLOCK	1
#define JRNL_COMP_JBD2_COMMIT_BLOCK	2
#define JRNL_COMP_JBD2_SUPERBLOCK_V1	3
#define JRNL_COMP_JBD2_SUPERBLOCK_V2	4
#define JRNL_COMP_JBD2_REVOKE_BLOCK	5

/*
 * Standard header for all descriptor blocks:
 */
typedef struct journal_header_s
{
	__be32		h_magic;
	__be32		h_blocktype;
	__be32		h_sequence;
} journal_header_t;

/*
 * Checksum types.
 */
#define JRNL_COMP_JBD2_CRC32_CHKSUM   1
#define JRNL_COMP_JBD2_MD5_CHKSUM     2
#define JRNL_COMP_JBD2_SHA1_CHKSUM    3
#define JRNL_COMP_JBD2_CRC32C_CHKSUM  4

#define JRNL_COMP_JBD2_CRC32_CHKSUM_SIZE 4

#define JRNL_COMP_JBD2_CHECKSUM_BYTES (32 / sizeof(u32))
/*
 * Commit block header for storing transactional checksums:
 *
 * NOTE: If FEATURE_COMPAT_CHECKSUM (checksum v1) is set, the h_chksum*
 * fields are used to store a checksum of the descriptor and data blocks.
 *
 * If FEATURE_INCOMPAT_CSUM_V2 (checksum v2) is set, then the h_chksum
 * field is used to store crc32c(uuid+commit_block).  Each journal metadata
 * block gets its own checksum, and data block checksums are stored in
 * journal_block_tag (in the descriptor).  The other h_chksum* fields are
 * not used.
 *
 * If FEATURE_INCOMPAT_CSUM_V3 is set, the descriptor block uses
 * journal_block_tag3_t to store a full 32-bit checksum.  Everything else
 * is the same as v2.
 *
 * Checksum v1, v2, and v3 are mutually exclusive features.
 */
struct commit_header {
	__be32		h_magic;
	__be32          h_blocktype;
	__be32          h_sequence;
	unsigned char   h_chksum_type;
	unsigned char   h_chksum_size;
	unsigned char 	h_padding[2];
	__be32 		h_chksum[JRNL_COMP_JBD2_CHECKSUM_BYTES];
	__be64		h_commit_sec;
	__be32		h_commit_nsec;
};

/*
 * The block tag: used to describe a single buffer in the journal.
 * t_blocknr_high is only used if INCOMPAT_64BIT is set, so this
 * raw struct shouldn't be used for pointer math or sizeof() - use
 * journal_tag_bytes(journal) instead to compute this.
 */
typedef struct journal_block_tag3_s
{
	__be32		t_blocknr;	/* The on-disk block number */
	__be32		t_flags;	/* See below */
	__be32		t_blocknr_high; /* most-significant high 32bits. */
	__be32		t_checksum;	/* crc32c(uuid+seq+block) */
} journal_block_tag3_t;

typedef struct journal_block_tag_s
{
	__be32		t_blocknr;	/* The on-disk block number */
	__be16		t_checksum;	/* truncated crc32c(uuid+seq+block) */
	__be16		t_flags;	/* See below */
	__be32		t_blocknr_high; /* most-significant high 32bits. */
} journal_block_tag_t;

/* Tail of descriptor or revoke block, for checksumming */
struct jrnl_comp_jbd2_journal_block_tail {
	__be32		t_checksum;	/* crc32c(uuid+descr_block) */
};

/*
 * The revoke descriptor: used on disk to describe a series of blocks to
 * be revoked from the log
 */
typedef struct jrnl_comp_jbd2_journal_revoke_header_s
{
	journal_header_t r_header;
	__be32		 r_count;	/* Count of bytes used in the block */
} jrnl_comp_jbd2_journal_revoke_header_t;

/* Definitions for the journal tag flags word: */
#define JRNL_COMP_JBD2_FLAG_ESCAPE		1	/* on-disk block is escaped */
#define JRNL_COMP_JBD2_FLAG_SAME_UUID	2	/* block has same uuid as previous */
#define JRNL_COMP_JBD2_FLAG_DELETED	4	/* block deleted by this transaction */
#define JRNL_COMP_JBD2_FLAG_LAST_TAG	8	/* last tag in this descriptor block */


/*
 * The journal superblock.  All fields are in big-endian byte order.
 */
typedef struct journal_superblock_s
{
/* 0x0000 */
	journal_header_t s_header;

/* 0x000C */
	/* Static information describing the journal */
	__be32	s_blocksize;		/* journal device blocksize */
	__be32	s_maxlen;		/* total blocks in journal file */
	__be32	s_first;		/* first block of log information */

/* 0x0018 */
	/* Dynamic information describing the current state of the log */
	__be32	s_sequence;		/* first commit ID expected in log */
	__be32	s_start;		/* blocknr of start of log */

/* 0x0020 */
	/* Error value, as set by jrnl_comp_jbd2_journal_abort(). */
	__be32	s_errno;

/* 0x0024 */
	/* Remaining fields are only valid in a version-2 superblock */
	__be32	s_feature_compat;	/* compatible feature set */
	__be32	s_feature_incompat;	/* incompatible feature set */
	__be32	s_feature_ro_compat;	/* readonly-compatible feature set */
/* 0x0030 */
	__u8	s_uuid[16];		/* 128-bit uuid for journal */

/* 0x0040 */
	__be32	s_nr_users;		/* Nr of filesystems sharing log */

	__be32	s_dynsuper;		/* Blocknr of dynamic superblock copy*/

/* 0x0048 */
	__be32	s_max_transaction;	/* Limit of journal blocks per trans.*/
	__be32	s_max_trans_data;	/* Limit of data blocks per trans. */

/* 0x0050 */
	__u8	s_checksum_type;	/* checksum type */
	__u8	s_padding2[3];
/* 0x0054 */
	__be32	s_num_fc_blks;		/* Number of fast commit blocks */
	__be32	s_head;			/* blocknr of head of log, only uptodate
					 * while the filesystem is clean */
/* 0x005C */
	__u32	s_padding[40];
	__be32	s_checksum;		/* crc32c(superblock) */

/* 0x0100 */
	__u8	s_users[16*48];		/* ids of all fs'es sharing the log */
/* 0x0400 */
} journal_superblock_t;

#define JRNL_COMP_JBD2_FEATURE_COMPAT_CHECKSUM		0x00000001

#define JRNL_COMP_JBD2_FEATURE_INCOMPAT_REVOKE		0x00000001
#define JRNL_COMP_JBD2_FEATURE_INCOMPAT_64BIT		0x00000002
#define JRNL_COMP_JBD2_FEATURE_INCOMPAT_ASYNC_COMMIT	0x00000004
#define JRNL_COMP_JBD2_FEATURE_INCOMPAT_CSUM_V2		0x00000008
#define JRNL_COMP_JBD2_FEATURE_INCOMPAT_CSUM_V3		0x00000010
#define JRNL_COMP_JBD2_FEATURE_INCOMPAT_FAST_COMMIT	0x00000020
/* Private research format; deliberately rejected by unmodified readers. */
#define JRNL_COMP_JBD2_FEATURE_INCOMPAT_SUBBLOCK		0x80000000U

/* See "journal feature predicate functions" below */

/* Features known to this kernel version: */
#define JRNL_COMP_JBD2_KNOWN_COMPAT_FEATURES	JRNL_COMP_JBD2_FEATURE_COMPAT_CHECKSUM
#define JRNL_COMP_JBD2_KNOWN_ROCOMPAT_FEATURES	0
#define JRNL_COMP_JBD2_KNOWN_INCOMPAT_FEATURES	(JRNL_COMP_JBD2_FEATURE_INCOMPAT_REVOKE | \
					JRNL_COMP_JBD2_FEATURE_INCOMPAT_64BIT | \
					JRNL_COMP_JBD2_FEATURE_INCOMPAT_ASYNC_COMMIT | \
					JRNL_COMP_JBD2_FEATURE_INCOMPAT_CSUM_V2 | \
					JRNL_COMP_JBD2_FEATURE_INCOMPAT_CSUM_V3 | \
					JRNL_COMP_JBD2_FEATURE_INCOMPAT_FAST_COMMIT | \
					JRNL_COMP_JBD2_FEATURE_INCOMPAT_SUBBLOCK)

#ifdef __KERNEL__

#include <linux/fs.h>
#include <linux/sched.h>

enum jrnl_comp_jbd_state_bits {
	BH_JBD			/* Has an attached ext3 jrnl_comp_journal_head */
	  = BH_PrivateStart,
	BH_JWrite,		/* Being written to log (@@@ DEBUGGING) */
	BH_Freed,		/* Has been freed (truncated) */
	BH_Revoked,		/* Has been revoked from the log */
	BH_RevokeValid,		/* Revoked flag is valid */
	BH_JBDDirty,		/* Is dirty but journaled */
	BH_JournalHead,		/* Pins bh->b_private and jh->b_bh */
	BH_Shadow,		/* IO on shadow buffer is running */
	BH_Verified,		/* Metadata block has been verified ok */
	BH_JBDPrivateStart,	/* First bit available for private use by FS */
};

BUFFER_FNS(JBD, jbd)
BUFFER_FNS(JWrite, jwrite)
BUFFER_FNS(JBDDirty, jbddirty)
TAS_BUFFER_FNS(JBDDirty, jbddirty)
BUFFER_FNS(Revoked, revoked)
TAS_BUFFER_FNS(Revoked, revoked)
BUFFER_FNS(RevokeValid, revokevalid)
TAS_BUFFER_FNS(RevokeValid, revokevalid)
BUFFER_FNS(Freed, freed)
BUFFER_FNS(Shadow, shadow)
BUFFER_FNS(Verified, verified)

static inline struct buffer_head *jrnl_comp_jh2bh(struct jrnl_comp_journal_head *jh)
{
	return jh->b_bh;
}

static inline struct jrnl_comp_journal_head *jrnl_comp_bh2jh(struct buffer_head *bh)
{
	return bh->b_private;
}

static inline void jrnl_comp_jbd_lock_bh_journal_head(struct buffer_head *bh)
{
	bit_spin_lock(BH_JournalHead, &bh->b_state);
}

static inline void jrnl_comp_jbd_unlock_bh_journal_head(struct buffer_head *bh)
{
	bit_spin_unlock(BH_JournalHead, &bh->b_state);
}

#define J_ASSERT(assert)	BUG_ON(!(assert))

#define J_ASSERT_BH(bh, expr)	J_ASSERT(expr)
#define J_ASSERT_JH(jh, expr)	J_ASSERT(expr)

#if defined(JRNL_COMP_JBD2_PARANOID_IOFAIL)
#define J_EXPECT(expr, why...)		J_ASSERT(expr)
#define J_EXPECT_BH(bh, expr, why...)	J_ASSERT_BH(bh, expr)
#define J_EXPECT_JH(jh, expr, why...)	J_ASSERT_JH(jh, expr)
#else
#define __journal_expect(expr, why...)					     \
	({								     \
		int val = (expr);					     \
		if (!val) {						     \
			printk(KERN_ERR					     \
			       "JRNL_COMP_JBD2 unexpected failure: %s: %s;\n",	     \
			       __func__, #expr);			     \
			printk(KERN_ERR why "\n");			     \
		}							     \
		val;							     \
	})
#define J_EXPECT(expr, why...)		__journal_expect(expr, ## why)
#define J_EXPECT_BH(bh, expr, why...)	__journal_expect(expr, ## why)
#define J_EXPECT_JH(jh, expr, why...)	__journal_expect(expr, ## why)
#endif

/* Flags in jrnl_comp_jbd_inode->i_flags */
#define __JI_COMMIT_RUNNING 0
#define __JI_WRITE_DATA 1
#define __JI_WAIT_DATA 2

/*
 * Commit of the inode data in progress. We use this flag to protect us from
 * concurrent deletion of inode. We cannot use reference to inode for this
 * since we cannot afford doing last iput() on behalf of kjournald
 */
#define JI_COMMIT_RUNNING (1 << __JI_COMMIT_RUNNING)
/* Write allocated dirty buffers in this inode before commit */
#define JI_WRITE_DATA (1 << __JI_WRITE_DATA)
/* Wait for outstanding data writes for this inode before commit */
#define JI_WAIT_DATA (1 << __JI_WAIT_DATA)

/**
 * struct jrnl_comp_jbd2_inode - The jrnl_comp_jbd_inode type is the structure linking inodes in
 * ordered mode present in a transaction so that we can sync them during commit.
 */
struct jrnl_comp_jbd2_inode {
	/**
	 * @i_transaction:
	 *
	 * Which transaction does this inode belong to? Either the running
	 * transaction or the committing one. [j_list_lock]
	 */
	jrnl_comp_transaction_t *i_transaction;

	/**
	 * @i_next_transaction:
	 *
	 * Pointer to the running transaction modifying inode's data in case
	 * there is already a committing transaction touching it. [j_list_lock]
	 */
	jrnl_comp_transaction_t *i_next_transaction;

	/**
	 * @i_list: List of inodes in the i_transaction [j_list_lock]
	 */
	struct list_head i_list;

	/**
	 * @i_vfs_inode:
	 *
	 * VFS inode this inode belongs to [constant for lifetime of structure]
	 */
	struct inode *i_vfs_inode;

	/**
	 * @i_flags: Flags of inode [j_list_lock]
	 */
	unsigned long i_flags;

	/**
	 * @i_dirty_start_page:
	 *
	 * Dirty range start in PAGE_SIZE units.
	 *
	 * The dirty range is empty if @i_dirty_start_page is greater than or
	 * equal to @i_dirty_end_page.
	 *
	 * [j_list_lock]
	 */
	pgoff_t i_dirty_start_page;

	/**
	 * @i_dirty_end_page:
	 *
	 * Dirty range end in PAGE_SIZE units (exclusive).
	 *
	 * [j_list_lock]
	 */
	pgoff_t i_dirty_end_page;
};

/*
 * Lockless readers treat start_page >= end_page as an empty range.
 * Writers publish a new non-empty range by storing i_dirty_end_page before
 * i_dirty_start_page.
 */
static inline bool jrnl_comp_jbd2_jinode_get_dirty_range(const struct jrnl_comp_jbd2_inode *jinode,
					       loff_t *start, loff_t *end)
{
	pgoff_t start_page = READ_ONCE(jinode->i_dirty_start_page);
	pgoff_t end_page = READ_ONCE(jinode->i_dirty_end_page);

	if (start_page >= end_page)
		return false;

	*start = (loff_t)start_page << PAGE_SHIFT;
	*end = ((loff_t)end_page << PAGE_SHIFT) - 1;
	return true;
}

struct jrnl_comp_jbd2_revoke_table_s;

/**
 * struct jrnl_comp_jbd2_journal_handle - The jrnl_comp_jbd2_journal_handle type is the concrete
 *     type associated with jrnl_comp_handle_t.
 * @h_transaction: Which compound transaction is this update a part of?
 * @h_journal: Which journal handle belongs to - used iff h_reserved set.
 * @h_rsv_handle: Handle reserved for finishing the logical operation.
 * @h_total_credits: Number of remaining buffers we are allowed to add to
 *	journal. These are dirty buffers and revoke descriptor blocks.
 * @h_revoke_credits: Number of remaining revoke records available for handle
 * @h_ref: Reference count on this handle.
 * @h_err: Field for caller's use to track errors through large fs operations.
 * @h_sync: Flag for sync-on-close.
 * @h_reserved: Flag for handle for reserved credits.
 * @h_aborted: Flag indicating fatal error on handle.
 * @h_type: For handle statistics.
 * @h_line_no: For handle statistics.
 * @h_start_jiffies: Handle Start time.
 * @h_requested_credits: Holds @h_total_credits after handle is started.
 * @h_revoke_credits_requested: Holds @h_revoke_credits after handle is started.
 * @saved_alloc_context: Saved context while transaction is open.
 **/

/* Docbook can't yet cope with the bit fields, but will leave the documentation
 * in so it can be fixed later.
 */

struct jrnl_comp_jbd2_journal_handle
{
	union {
		jrnl_comp_transaction_t	*h_transaction;
		/* Which journal handle belongs to - used iff h_reserved set */
		jrnl_comp_journal_t	*h_journal;
	};

	jrnl_comp_handle_t		*h_rsv_handle;
	int			h_total_credits;
	int			h_revoke_credits;
	int			h_revoke_credits_requested;
	int			h_ref;
	int			h_err;

	/* Flags [no locking] */
	unsigned int	h_sync:		1;
	unsigned int	h_reserved:	1;
	unsigned int	h_aborted:	1;
	unsigned int	h_type:		8;
	unsigned int	h_line_no:	16;

	unsigned long		h_start_jiffies;
	unsigned int		h_requested_credits;

	unsigned int		saved_alloc_context;
};


/*
 * Some stats for checkpoint phase
 */
struct transaction_chp_stats_s {
	unsigned long		cs_chp_time;
	__u32			cs_forced_to_close;
	__u32			cs_written;
	__u32			cs_dropped;
};

/* The jrnl_comp_transaction_t type is the guts of the journaling mechanism.  It
 * tracks a compound transaction through its various states:
 *
 * RUNNING:	accepting new updates
 * LOCKED:	Updates still running but we don't accept new ones
 * RUNDOWN:	Updates are tidying up but have finished requesting
 *		new buffers to modify (state not used for now)
 * FLUSH:       All updates complete, but we are still writing to disk
 * COMMIT:      All data on disk, writing commit record
 * FINISHED:	We still have to keep the transaction for checkpointing.
 *
 * The transaction keeps track of all of the buffers modified by a
 * running transaction, and all of the buffers committed but not yet
 * flushed to home for finished transactions.
 * (Locking Documentation improved by LockDoc)
 */

/*
 * Lock ranking:
 *
 *    j_list_lock
 *      ->jrnl_comp_jbd_lock_bh_journal_head()	(This is "innermost")
 *
 *    j_state_lock
 *    ->b_state_lock
 *
 *    b_state_lock
 *    ->j_list_lock
 *
 *    j_state_lock
 *    ->j_list_lock			(journal_unmap_buffer)
 *
 */

struct transaction_run_stats_s {
	unsigned long		rs_wait;
	unsigned long		rs_request_delay;
	unsigned long		rs_running;
	unsigned long		rs_locked;
	unsigned long		rs_flushing;
	unsigned long		rs_logging;

	__u32			rs_handle_count;
	__u32			rs_blocks;
	__u32			rs_blocks_logged;
};

struct transaction_stats_s {
	unsigned long		ts_tid;
	unsigned long		ts_requested;
	struct transaction_run_stats_s run;
};

struct jrnl_comp_transaction_s
{
	/* Zero keeps the existing full-block format. Frozen at transaction start. */
	unsigned int t_subblock_granularity;
	unsigned int t_jrnl_comp_cjfs_depth;
	bool t_jrnl_comp_cjfs_eager;
	unsigned int t_jrnl_comp_cjfs_window_us;
	u64 t_jrnl_comp_cjfs_generation;
	/* Pointer to the journal for this transaction. [no locking] */
	jrnl_comp_journal_t		*t_journal;

	/* Sequence number for this transaction [no locking] */
	jrnl_comp_tid_t			t_tid;

	/*
	 * Transaction's current state
	 * [no locking - only kjournald2 alters this]
	 * [j_list_lock] guards transition of a transaction into T_FINISHED
	 * state and subsequent call of __jrnl_comp_jbd2_journal_drop_transaction()
	 * FIXME: needs barriers
	 * KLUDGE: [use j_state_lock]
	 */
	enum {
		T_RUNNING,
		T_LOCKED,
		T_SWITCH,
		T_FLUSH,
		T_COMMIT,
		T_COMMIT_DFLUSH,
		T_COMMIT_JFLUSH,
		T_COMMIT_CALLBACK,
		T_FINISHED
	}			t_state;

	/*
	 * Where in the log does this transaction's commit start? [no locking]
	 */
	unsigned long		t_log_start;

	/*
	 * Number of buffers on the t_buffers list [j_list_lock, no locks
	 * needed for jrnl_comp_jbd2 thread]
	 */
	int			t_nr_buffers;

	/*
	 * Doubly-linked circular list of all buffers reserved but not yet
	 * modified by this transaction [j_list_lock, no locks needed fo
	 * jrnl_comp_jbd2 thread]
	 */
	struct jrnl_comp_journal_head	*t_reserved_list;

	/*
	 * Doubly-linked circular list of all metadata buffers owned by this
	 * transaction [j_list_lock, no locks needed for jrnl_comp_jbd2 thread]
	 */
	struct jrnl_comp_journal_head	*t_buffers;

	/*
	 * Doubly-linked circular list of all forget buffers (superseded
	 * buffers which we can un-checkpoint once this transaction commits)
	 * [j_list_lock]
	 */
	struct jrnl_comp_journal_head	*t_forget;

	/*
	 * Doubly-linked circular list of all buffers still to be flushed before
	 * this transaction can be checkpointed. [j_list_lock]
	 */
	struct jrnl_comp_journal_head	*t_checkpoint_list;

	/*
	 * Doubly-linked circular list of metadata buffers being
	 * shadowed by log IO.  The IO buffers on the iobuf list and
	 * the shadow buffers on this list match each other one for
	 * one at all times. [j_list_lock, no locks needed for jrnl_comp_jbd2
	 * thread]
	 */
	struct jrnl_comp_journal_head	*t_shadow_list;

	/*
	 * Buffer head of the journal commit block.
	 * - 2022. 8. 25. Joontaek Oh.
	 */
	struct buffer_head	*t_cbh;

	/*
	 * List of inodes associated with the transaction; e.g., jrnl_comp_ext4 uses
	 * this to track inodes in data=ordered and data=journal mode that
	 * need special handling on transaction commit; also used by ocfs2.
	 * [j_list_lock]
	 */
	struct list_head	t_inode_list;

	/*
	 * List of log blocks which are dispatched by the commit thread.
	 * - 2022. 8. 25. Joontaek Oh.
	 */
	struct list_head	t_io_bufs;

	/*
	 * List of journal blocks such as a journal descriptor block,
	 * a journal commit block, and a revoke block which are
	 * dispatched by the commit thread.
	 * - 2022. 8. 25. Joontaek Oh.
	 */
	struct list_head	t_log_bufs;

	/*
	 * Protects info related to handles
	 */
	spinlock_t		t_handle_lock;

	/*
	 * Longest time some handle had to wait for running transaction
	 */
	unsigned long		t_max_wait;

	/*
	 * When transaction started
	 */
	unsigned long		t_start;

	/*
	 * When commit was requested [j_state_lock]
	 */
	unsigned long		t_requested;

	/*
	 * Checkpointing stats [j_list_lock]
	 */
	struct transaction_chp_stats_s t_chp_stats;

	/*
	 * Number of outstanding updates running on this transaction
	 * [none]
	 */
	atomic_t		t_updates;

	/*
	 * Number of blocks reserved for this transaction in the journal.
	 * This is including all credits reserved when starting transaction
	 * handles as well as all journal descriptor blocks needed for this
	 * transaction. [none]
	 */
	atomic_t		t_outstanding_credits;

	/*
	 * Number of revoke records for this transaction added by already
	 * stopped handles. [none]
	 */
	atomic_t		t_outstanding_revokes;

	/*
	 * How many handles used this transaction? [none]
	 */
	atomic_t		t_handle_count;

	/*
	 * Forward and backward links for the circular list of all transactions
	 * awaiting checkpoint. [j_list_lock]
	 */
	jrnl_comp_transaction_t		*t_cpnext, *t_cpprev;

	/*
	 * Forward and backward links for the circular list of all transactions
	 * awaiting flush. [j_flush_list_lock]
	 * - 2022. 8. 27. Joontaek Oh.
	 */
	jrnl_comp_transaction_t		*t_flushnext, *t_flushprev;

	/*
	 * Count of the conflict between this transaction and other transactions.
	 * - 2022. 8. 27. Joontaek Oh.
	 */
	atomic_t		t_conflict_count;

	/* JRNL_COMP_CJFS - Profiling */
	atomic_t		t_pconflict_count;

	/*
	 * When will the transaction expire (become due for commit), in jiffies?
	 * [no locking]
	 */
	unsigned long		t_expires;

	/*
	 * When this transaction started, in nanoseconds [no locking]
	 */
	ktime_t			t_start_time;

	/* JRNL_COMP_CJFS - Profiling */
	ktime_t			t_commit_start_time;

	/* JRNL_COMP_CJFS - Profiling */
	u64 			commit_time;

	/*
	 * This transaction is being forced and some process is
	 * waiting for it to finish.
	 */
	unsigned int t_synchronous_commit:1;

	/* Disk flush needs to be sent to fs partition [no locking] */
	int			t_need_data_flush;

	/*
	 * These fields were the local variable jrnl_comp_jbd2_journal_commit_transaction
	 * Once the jrnl_comp_jbd2_journal_commit_transaction is devided into two,
	 * it is moved to here.
	 * - 2022. 8. 25. Joontaek Oh.
	 */
	int			t_update_tail;
	unsigned long 		t_first_block;
	jrnl_comp_tid_t 			t_first_tid;
	__u32			t_crc32_sum;

	/*
	 * For use by the filesystem to store fs-specific data
	 * structures associated with the transaction
	 */
	struct list_head	t_private_list;
	/* JRNL_COMP_CJFS */
	struct transaction_stats_s stats;
	int 			seq;
};

static inline unsigned long
jrnl_comp_jbd2_time_diff(unsigned long start, unsigned long end)
{
	if (end >= start)
		return end - start;

	return end + (MAX_JIFFY_OFFSET - start);
}

#define JRNL_COMP_JBD2_NR_BATCH	64

enum passtype {PASS_SCAN, PASS_REVOKE, PASS_REPLAY};

#define JRNL_COMP_JBD2_FC_REPLAY_STOP	0
#define JRNL_COMP_JBD2_FC_REPLAY_CONTINUE	1

/**
 * struct jrnl_comp_journal_s - The jrnl_comp_journal_s type is the concrete type associated with
 *     jrnl_comp_journal_t.
 */
struct jrnl_comp_journal_s
{
	/* Configuration mutex is never acquired by commit/flush paths. */
	struct mutex j_jrnl_comp_cjfs_config_mutex;
	struct jrnl_comp_jbd2_workvolume *j_workvolume;
	unsigned int j_jrnl_comp_cjfs_depth, j_jrnl_comp_cjfs_window_us;
	bool j_jrnl_comp_cjfs_eager, j_jrnl_comp_cjfs_reconfiguring;
	unsigned int j_jrnl_comp_cjfs_pending_depth, j_jrnl_comp_cjfs_pending_window_us;
	bool j_jrnl_comp_cjfs_pending_eager;
	/* Experimental compact journal; no runtime enablement until replay exists. */
	unsigned int j_subblock_granularity;
	unsigned int j_subblock_pending_granularity;
	/* Commit-thread counters; snapshot with READ_ONCE, not a durability ledger. */
	u64 j_subblock_containers, j_subblock_images, j_subblock_payload_bytes;
	u64 j_subblock_replayed;
	u64 j_jrnl_comp_cjfs_generation;
	/**
	 * @j_flags: General journaling state flags [j_state_lock,
	 * no lock for quick racy checks]
	 */
	unsigned long		j_flags;

	/**
	 * @j_errno:
	 *
	 * Is there an outstanding uncleared error on the journal (from a prior
	 * abort)? [j_state_lock]
	 */
	int			j_errno;

	/**
	 * @j_abort_mutex: Lock the whole aborting procedure.
	 */
	struct mutex		j_abort_mutex;

	/**
	 * @j_sb_buffer: The first part of the superblock buffer.
	 */
	struct buffer_head	*j_sb_buffer;

	/**
	 * @j_superblock: The second part of the superblock buffer.
	 */
	journal_superblock_t	*j_superblock;

	/**
	 * @j_state_lock: Protect the various scalars in the journal.
	 */
	rwlock_t		j_state_lock;

	/**
	 * @j_barrier_count:
	 *
	 * Number of processes waiting to create a barrier lock [j_state_lock,
	 * no lock for quick racy checks]
	 */
	int			j_barrier_count;

	/**
	 * @j_barrier: The barrier lock itself.
	 */
	struct mutex		j_barrier;

	/**
	 * @j_running_transaction:
	 *
	 * Transactions: The current running transaction...
	 * [j_state_lock, no lock for quick racy checks] [caller holding
	 * open handle]
	 */
	jrnl_comp_transaction_t		*j_running_transaction;

	/**
	 * @j_committing_transaction:
	 *
	 * the transaction we are pushing to disk
	 * [j_state_lock] [caller holding open handle]
	 */
	jrnl_comp_transaction_t		*j_committing_transaction;

	/**
	 * @j_flushing_transaction:
	 *
	 * the transaction we are flushing to disk
	 * [j_state_lock] [caller holding open handle]
	 */
	jrnl_comp_transaction_t		*j_flushing_transactions;

	/**
	 * @j_checkpoint_transactions:
	 *
	 * ... and a linked circular list of all transactions waiting for
	 * checkpointing. [j_list_lock]
	 */
	jrnl_comp_transaction_t		*j_checkpoint_transactions;

	/**
	 * @j_wait_transaction_locked:
	 *
	 * Wait queue for waiting for a locked transaction to start committing,
	 * or for a barrier lock to be released.
	 */
	wait_queue_head_t	j_wait_transaction_locked;

	/**
	 * @j_wait_done_commit: Wait queue for waiting for commit to complete.
	 */
	wait_queue_head_t	j_wait_done_commit;

	/**
	 * @j_wait_done_flush: Wait queue for waiting for flush to complete.
	 */
	wait_queue_head_t	j_wait_done_flush;

	/**
	 * @j_wait_commit: Wait queue to trigger commit.
	 */
	wait_queue_head_t	j_wait_commit;

	/**
	 * @j_wait_flush: Wait queue to trigger flush.
	 */
	wait_queue_head_t	j_wait_flush;

	/**
	 * @j_wait_updates: Wait queue to wait for updates to complete.
	 */
	wait_queue_head_t	j_wait_updates;

	/**
	 * @j_wait_reserved:
	 *
	 * Wait queue to wait for reserved buffer credits to drop.
	 */
	wait_queue_head_t	j_wait_reserved;

	/**
	 * @j_fc_wait:
	 *
	 * Wait queue to wait for completion of async fast commits.
	 */
	wait_queue_head_t	j_fc_wait;

	/**
	 * @j_checkpoint_mutex:
	 *
	 * Semaphore for locking against concurrent checkpoints.
	 */
	struct mutex		j_checkpoint_mutex;

	/**
	 * @j_chkpt_bhs:
	 *
	 * List of buffer heads used by the checkpoint routine.  This
	 * was moved from jrnl_comp_jbd2_log_do_checkpoint() to reduce stack
	 * usage.  Access to this array is controlled by the
	 * @j_checkpoint_mutex.  [j_checkpoint_mutex]
	 */
	struct buffer_head	*j_chkpt_bhs[JRNL_COMP_JBD2_NR_BATCH];

	/**
	 * @j_shrinker:
	 *
	 * Journal head shrinker, reclaim buffer's journal head which
	 * has been written back.
	 */
	struct shrinker		*j_shrinker;

	/**
	 * @j_checkpoint_jh_count:
	 *
	 * Number of journal buffers on the checkpoint list. [j_list_lock]
	 */
	struct percpu_counter	j_checkpoint_jh_count;

	/**
	 * @j_shrink_transaction:
	 *
	 * Record next transaction will shrink on the checkpoint list.
	 * [j_list_lock]
	 */
	jrnl_comp_transaction_t		*j_shrink_transaction;

	/**
	 * @j_head:
	 *
	 * Journal head: identifies the first unused block in the journal.
	 * [j_state_lock]
	 */
	unsigned long		j_head;

	/**
	 * @j_tail:
	 *
	 * Journal tail: identifies the oldest still-used block in the journal.
	 * [j_state_lock]
	 */
	unsigned long		j_tail;

	/**
	 * @j_free:
	 *
	 * Journal free: how many free blocks are there in the journal?
	 * [j_state_lock]
	 */
	unsigned long		j_free;

	/**
	 * @j_first:
	 *
	 * The block number of the first usable block in the journal
	 * [j_state_lock].
	 */
	unsigned long		j_first;

	/**
	 * @j_last:
	 *
	 * The block number one beyond the last usable block in the journal
	 * [j_state_lock].
	 */
	unsigned long		j_last;

	/**
	 * @j_fc_first:
	 *
	 * The block number of the first fast commit block in the journal
	 * [j_state_lock].
	 */
	unsigned long		j_fc_first;

	/**
	 * @j_fc_off:
	 *
	 * Number of fast commit blocks currently allocated. Accessed only
	 * during fast commit. Currently only process can do fast commit, so
	 * this field is not protected by any lock.
	 */
	unsigned long		j_fc_off;

	/**
	 * @j_fc_last:
	 *
	 * The block number one beyond the last fast commit block in the journal
	 * [j_state_lock].
	 */
	unsigned long		j_fc_last;

	/**
	 * @j_dev: Device where we store the journal.
	 */
	struct block_device	*j_dev;

	/**
	 * @j_blocksize: Block size for the location where we store the journal.
	 */
	int			j_blocksize;

	/**
	 * @j_blk_offset:
	 *
	 * Starting block offset into the device where we store the journal.
	 */
	unsigned long long	j_blk_offset;

	/**
	 * @j_devname: Journal device name.
	 */
	char			j_devname[BDEVNAME_SIZE+24];

	/**
	 * @j_fs_dev:
	 *
	 * Device which holds the client fs.  For internal journal this will be
	 * equal to j_dev.
	 */
	struct block_device	*j_fs_dev;

	/**
	 * @j_fs_dev_wb_err:
	 *
	 * Records the errseq of the client fs's backing block device.
	 */
	errseq_t		j_fs_dev_wb_err;

	/**
	 * @j_total_len: Total maximum capacity of the journal region on disk.
	 */
	unsigned int		j_total_len;

	/**
	 * @j_reserved_credits:
	 *
	 * Number of buffers reserved from the running transaction.
	 */
	atomic_t		j_reserved_credits;

	/**
	 * @j_list_lock: Protects the buffer lists and internal buffer state.
	 */
	spinlock_t		j_list_lock;

	/**
	 * @j_flush_list_lock: Protects the flush lists.
	 * - 2022. 8. 27. Joontaek Oh.
	 */
	spinlock_t		j_flush_list_lock;

	/**
	 * @j_inode:
	 *
	 * Optional inode where we store the journal.  If present, all
	 * journal block numbers are mapped into this inode via bmap().
	 */
	struct inode		*j_inode;

	/**
	 * @j_tail_sequence:
	 *
	 * Sequence number of the oldest transaction in the log [j_state_lock]
	 */
	jrnl_comp_tid_t			j_tail_sequence;

	/**
	 * @j_transaction_sequence:
	 *
	 * Sequence number of the next transaction to grant [j_state_lock]
	 */
	jrnl_comp_tid_t			j_transaction_sequence;

	/**
	 * @j_commit_sequence:
	 *
	 * Sequence number of the most recently committed transaction
	 * [j_state_lock, no lock for quick racy checks]
	 */
	jrnl_comp_tid_t			j_commit_sequence;
	jrnl_comp_tid_t			j_flush_sequence;
	jrnl_comp_tid_t			j_transfer_sequence;

	/**
	 * @j_commit_request:
	 *
	 * Sequence number of the most recent transaction wanting commit
	 * [j_state_lock, no lock for quick racy checks]
	 */
	jrnl_comp_tid_t			j_commit_request;

	/**
	 * @j_uuid:
	 *
	 * Journal uuid: identifies the object (filesystem, LVM volume etc)
	 * backed by this journal.  This will eventually be replaced by an array
	 * of uuids, allowing us to index multiple devices within a single
	 * journal and to perform atomic updates across them.
	 */
	__u8			j_uuid[16];

	/**
	 * @j_task: Pointer to the current commit thread for this journal.
	 */
	struct task_struct	*j_task;

	/**
	 * @j_task_flush: Pointer to the current flush thread for this journal.
	 */
	struct task_struct	*j_task_flush;

	/**
	 * @j_max_transaction_buffers:
	 *
	 * Maximum number of metadata buffers to allow in a single compound
	 * commit transaction.
	 */
	int			j_max_transaction_buffers;

	/**
	 * @j_revoke_records_per_block:
	 *
	 * Number of revoke records that fit in one descriptor block.
	 */
	int			j_revoke_records_per_block;

	/**
	 * @j_transaction_overhead_buffers:
	 *
	 * Number of blocks each transaction needs for its own bookkeeping
	 */
	int			j_transaction_overhead_buffers;

	/**
	 * @j_commit_interval:
	 *
	 * What is the maximum transaction lifetime before we begin a commit?
	 */
	unsigned long		j_commit_interval;

	/**
	 * @j_commit_timer: The timer used to wakeup the commit thread.
	 */
	struct timer_list	j_commit_timer;

	/**
	 * @j_revoke_lock: Protect the revoke table.
	 */
	spinlock_t		j_revoke_lock;

	/**
	 * @j_revoke:
	 *
	 * The revoke table - maintains the list of revoked blocks in the
	 * current transaction.
	 */
	struct jrnl_comp_jbd2_revoke_table_s *j_revoke;

	/**
	 * @j_revoke_table: Alternate revoke tables for j_revoke.
	 */
	struct jrnl_comp_jbd2_revoke_table_s *j_revoke_table[2];

	/**
	 * @j_wbuf: Array of bhs for jrnl_comp_jbd2_journal_commit_transaction.
	 */
	struct buffer_head	**j_wbuf;

	/**
	 * @j_fc_wbuf: Array of fast commit bhs for fast commit. Accessed only
	 * during a fast commit. Currently only process can do fast commit, so
	 * this field is not protected by any lock.
	 */
	struct buffer_head	**j_fc_wbuf;

	/**
	 * @j_wbufsize:
	 *
	 * Size of @j_wbuf array.
	 */
	int			j_wbufsize;

	/**
	 * @j_fc_wbufsize:
	 *
	 * Size of @j_fc_wbuf array.
	 */
	int			j_fc_wbufsize;

	/**
	 * @j_last_sync_writer:
	 *
	 * The pid of the last person to run a synchronous operation
	 * through the journal.
	 */
	pid_t			j_last_sync_writer;

	/**
	 * @j_average_commit_time:
	 *
	 * The average amount of time in nanoseconds it takes to commit a
	 * transaction to disk. [j_state_lock]
	 */
	u64			j_average_commit_time;

	/**
	 * @j_min_batch_time:
	 *
	 * Minimum time that we should wait for additional filesystem operations
	 * to get batched into a synchronous handle in microseconds.
	 */
	u32			j_min_batch_time;

	/**
	 * @j_max_batch_time:
	 *
	 * Maximum time that we should wait for additional filesystem operations
	 * to get batched into a synchronous handle in microseconds.
	 */
	u32			j_max_batch_time;

	/**
	 * @j_commit_callback:
	 *
	 * This function is called when a transaction is closed.
	 */
	void			(*j_commit_callback)(jrnl_comp_journal_t *,
						     jrnl_comp_transaction_t *);

	/**
	 * @j_submit_inode_data_buffers:
	 *
	 * This function is called for all inodes associated with the
	 * committing transaction marked with JI_WRITE_DATA flag
	 * before we start to write out the transaction to the journal.
	 */
	int			(*j_submit_inode_data_buffers)
					(struct jrnl_comp_jbd2_inode *);

	/**
	 * @j_finish_inode_data_buffers:
	 *
	 * This function is called for all inodes associated with the
	 * committing transaction marked with JI_WAIT_DATA flag
	 * after we have written the transaction to the journal
	 * but before we write out the commit block.
	 */
	int			(*j_finish_inode_data_buffers)
					(struct jrnl_comp_jbd2_inode *);

	/*
	 * Journal statistics
	 */

	/**
	 * @j_history_lock: Protect the transactions statistics history.
	 */
	spinlock_t		j_history_lock;

	/**
	 * @j_proc_entry: procfs entry for the jbd statistics directory.
	 */
	struct proc_dir_entry	*j_proc_entry;

	/**
	 * @j_stats: Overall statistics.
	 */
	struct transaction_stats_s j_stats;

	/**
	 * @j_failed_commit: Failed journal commit ID.
	 */
	unsigned int		j_failed_commit;

	/**
	 * @j_private:
	 *
	 * An opaque pointer to fs-private information.  ext3 puts its
	 * superblock pointer here.
	 */
	void *j_private;

	/**
	 * @j_csum_seed:
	 *
	 * Precomputed journal UUID checksum for seeding other checksums.
	 */
	__u32 j_csum_seed;

#ifdef CONFIG_DEBUG_LOCK_ALLOC
	/**
	 * @j_trans_commit_map:
	 *
	 * Lockdep entity to track transaction commit dependencies. Handles
	 * hold this "lock" for read, when we wait for commit, we acquire the
	 * "lock" for writing. This matches the properties of jrnl_comp_jbd2 journalling
	 * where the running transaction has to wait for all handles to be
	 * dropped to commit that transaction and also acquiring a handle may
	 * require transaction commit to finish.
	 */
	struct lockdep_map	j_trans_commit_map;
#endif
	/**
	 * @jrnl_comp_jbd2_trans_commit_key:
	 *
	 * "struct lock_class_key" for @j_trans_commit_map
	 */
	struct lock_class_key	jrnl_comp_jbd2_trans_commit_key;

	/**
	 * @j_fc_cleanup_callback:
	 *
	 * Clean-up after fast commit or full commit. JRNL_COMP_JBD2 calls this function
	 * after every commit operation.
	 */
	void (*j_fc_cleanup_callback)(struct jrnl_comp_journal_s *journal, int full, jrnl_comp_tid_t tid);

	/**
	 * @j_fc_replay_callback:
	 *
	 * File-system specific function that performs replay of a fast
	 * commit. JRNL_COMP_JBD2 calls this function for each fast commit block found in
	 * the journal. This function should return JRNL_COMP_JBD2_FC_REPLAY_CONTINUE
	 * to indicate that the block was processed correctly and more fast
	 * commit replay should continue. Return value of JRNL_COMP_JBD2_FC_REPLAY_STOP
	 * indicates the end of replay (no more blocks remaining). A negative
	 * return value indicates error.
	 */
	int (*j_fc_replay_callback)(struct jrnl_comp_journal_s *journal,
				    struct buffer_head *bh,
				    enum passtype pass, int off,
				    jrnl_comp_tid_t expected_commit_id);

	/**
	 * @j_bmap:
	 *
	 * Bmap function that should be used instead of the generic
	 * VFS bmap function.
	 */
	int (*j_bmap)(struct jrnl_comp_journal_s *journal, sector_t *block);
};

#define jrnl_comp_jbd2_might_wait_for_commit(j) \
	do { \
		rwsem_acquire(&j->j_trans_commit_map, 0, 0, _THIS_IP_); \
		rwsem_release(&j->j_trans_commit_map, _THIS_IP_); \
	} while (0)

/*
 * We can support any known requested features iff the
 * superblock is not in version 1.  Otherwise we fail to support any
 * extended sb features.
 */
static inline bool jrnl_comp_jbd2_format_support_feature(jrnl_comp_journal_t *j)
{
	return j->j_superblock->s_header.h_blocktype !=
					cpu_to_be32(JRNL_COMP_JBD2_SUPERBLOCK_V1);
}

/* journal feature predicate functions */
#define JRNL_COMP_JBD2_FEATURE_COMPAT_FUNCS(name, flagname) \
static inline bool jrnl_comp_jbd2_has_feature_##name(jrnl_comp_journal_t *j) \
{ \
	return (jrnl_comp_jbd2_format_support_feature(j) && \
		((j)->j_superblock->s_feature_compat & \
		 cpu_to_be32(JRNL_COMP_JBD2_FEATURE_COMPAT_##flagname)) != 0); \
} \
static inline void jrnl_comp_jbd2_set_feature_##name(jrnl_comp_journal_t *j) \
{ \
	(j)->j_superblock->s_feature_compat |= \
		cpu_to_be32(JRNL_COMP_JBD2_FEATURE_COMPAT_##flagname); \
} \
static inline void jrnl_comp_jbd2_clear_feature_##name(jrnl_comp_journal_t *j) \
{ \
	(j)->j_superblock->s_feature_compat &= \
		~cpu_to_be32(JRNL_COMP_JBD2_FEATURE_COMPAT_##flagname); \
}

#define JRNL_COMP_JBD2_FEATURE_RO_COMPAT_FUNCS(name, flagname) \
static inline bool jrnl_comp_jbd2_has_feature_##name(jrnl_comp_journal_t *j) \
{ \
	return (jrnl_comp_jbd2_format_support_feature(j) && \
		((j)->j_superblock->s_feature_ro_compat & \
		 cpu_to_be32(JRNL_COMP_JBD2_FEATURE_RO_COMPAT_##flagname)) != 0); \
} \
static inline void jrnl_comp_jbd2_set_feature_##name(jrnl_comp_journal_t *j) \
{ \
	(j)->j_superblock->s_feature_ro_compat |= \
		cpu_to_be32(JRNL_COMP_JBD2_FEATURE_RO_COMPAT_##flagname); \
} \
static inline void jrnl_comp_jbd2_clear_feature_##name(jrnl_comp_journal_t *j) \
{ \
	(j)->j_superblock->s_feature_ro_compat &= \
		~cpu_to_be32(JRNL_COMP_JBD2_FEATURE_RO_COMPAT_##flagname); \
}

#define JRNL_COMP_JBD2_FEATURE_INCOMPAT_FUNCS(name, flagname) \
static inline bool jrnl_comp_jbd2_has_feature_##name(jrnl_comp_journal_t *j) \
{ \
	return (jrnl_comp_jbd2_format_support_feature(j) && \
		((j)->j_superblock->s_feature_incompat & \
		 cpu_to_be32(JRNL_COMP_JBD2_FEATURE_INCOMPAT_##flagname)) != 0); \
} \
static inline void jrnl_comp_jbd2_set_feature_##name(jrnl_comp_journal_t *j) \
{ \
	(j)->j_superblock->s_feature_incompat |= \
		cpu_to_be32(JRNL_COMP_JBD2_FEATURE_INCOMPAT_##flagname); \
} \
static inline void jrnl_comp_jbd2_clear_feature_##name(jrnl_comp_journal_t *j) \
{ \
	(j)->j_superblock->s_feature_incompat &= \
		~cpu_to_be32(JRNL_COMP_JBD2_FEATURE_INCOMPAT_##flagname); \
}

JRNL_COMP_JBD2_FEATURE_COMPAT_FUNCS(checksum,		CHECKSUM)

JRNL_COMP_JBD2_FEATURE_INCOMPAT_FUNCS(revoke,		REVOKE)
JRNL_COMP_JBD2_FEATURE_INCOMPAT_FUNCS(64bit,		64BIT)
JRNL_COMP_JBD2_FEATURE_INCOMPAT_FUNCS(async_commit,	ASYNC_COMMIT)
JRNL_COMP_JBD2_FEATURE_INCOMPAT_FUNCS(csum2,		CSUM_V2)
JRNL_COMP_JBD2_FEATURE_INCOMPAT_FUNCS(csum3,		CSUM_V3)
JRNL_COMP_JBD2_FEATURE_INCOMPAT_FUNCS(fast_commit,	FAST_COMMIT)
JRNL_COMP_JBD2_FEATURE_INCOMPAT_FUNCS(subblock,	SUBBLOCK)

/* Journal high priority write IO operation flags */
#define JRNL_COMP_JBD2_JOURNAL_REQ_FLAGS		(REQ_META | REQ_SYNC | REQ_IDLE)

/*
 * Journal flag definitions
 */
#define JRNL_COMP_JBD2_UNMOUNT	0x001	/* Journal thread is being destroyed */
#define JRNL_COMP_JBD2_ABORT	0x002	/* Journaling has been aborted for errors. */
#define JRNL_COMP_JBD2_ACK_ERR	0x004	/* The errno in the sb has been acked */
#define JRNL_COMP_JBD2_FLUSHED	0x008	/* The journal superblock has been flushed */
#define JRNL_COMP_JBD2_LOADED	0x010	/* The journal superblock has been loaded */
#define JRNL_COMP_JBD2_BARRIER	0x020	/* Use IDE barriers */
#define JRNL_COMP_JBD2_CYCLE_RECORD		0x080	/* Journal cycled record log on
						 * clean and empty filesystem
						 * logging area */
#define JRNL_COMP_JBD2_FAST_COMMIT_ONGOING	0x100	/* Fast commit is ongoing */
#define JRNL_COMP_JBD2_FULL_COMMIT_ONGOING	0x200	/* Full commit is ongoing */
#define JRNL_COMP_JBD2_JOURNAL_FLUSH_DISCARD	0x0001
#define JRNL_COMP_JBD2_JOURNAL_FLUSH_ZEROOUT	0x0002
#define JRNL_COMP_JBD2_JOURNAL_FLUSH_VALID	(JRNL_COMP_JBD2_JOURNAL_FLUSH_DISCARD | \
					JRNL_COMP_JBD2_JOURNAL_FLUSH_ZEROOUT)

/*
 * Function declarations for the journaling transaction and buffer
 * management
 */

/* Filing buffers */
extern bool __jrnl_comp_jbd2_journal_refile_buffer(struct jrnl_comp_journal_head *);
extern void jrnl_comp_jbd2_journal_refile_buffer(jrnl_comp_journal_t *, struct jrnl_comp_journal_head *);
extern void __jrnl_comp_jbd2_journal_file_buffer(struct jrnl_comp_journal_head *, jrnl_comp_transaction_t *, int);
extern void jrnl_comp_jbd2_journal_file_buffer(struct jrnl_comp_journal_head *, jrnl_comp_transaction_t *, int);

/* JRNL_COMP_CJFS */
extern void jrnl_comp_jbd2_journal_unfile_buffer_entry(jrnl_comp_journal_t *, struct jrnl_comp_journal_head *, int);
extern bool __jrnl_comp_jbd2_journal_refile_buffer_entry(struct jrnl_comp_journal_head *, jrnl_comp_transaction_t *);
extern void jrnl_comp_jbd2_journal_refile_buffer_entry(jrnl_comp_journal_t *, struct jrnl_comp_journal_head *,
		jrnl_comp_transaction_t *);


static inline void jrnl_comp_jbd2_file_log_bh(struct list_head *head, struct buffer_head *bh)
{
	list_add_tail(&bh->b_assoc_buffers, head);
}
static inline void jrnl_comp_jbd2_unfile_log_bh(struct buffer_head *bh)
{
	list_del_init(&bh->b_assoc_buffers);
}

/* Log buffer allocation */
struct buffer_head *jrnl_comp_jbd2_journal_get_descriptor_buffer(jrnl_comp_transaction_t *, int);
void jrnl_comp_jbd2_descriptor_block_csum_set(jrnl_comp_journal_t *, struct buffer_head *);
int jrnl_comp_jbd2_journal_next_log_block(jrnl_comp_journal_t *, unsigned long long *);
int jrnl_comp_jbd2_journal_get_log_tail(jrnl_comp_journal_t *journal, jrnl_comp_tid_t *tid,
			      unsigned long *block);
int __jrnl_comp_jbd2_update_log_tail(jrnl_comp_journal_t *journal, jrnl_comp_tid_t tid, unsigned long block);
void jrnl_comp_jbd2_update_log_tail(jrnl_comp_journal_t *journal, jrnl_comp_tid_t tid, unsigned long block);

/* Commit management */
extern void jrnl_comp_jbd2_journal_commit_transaction(jrnl_comp_journal_t *);
extern void jrnl_comp_jbd2_journal_barrier_commit_transaction(jrnl_comp_journal_t *);
extern void jrnl_comp_jbd2_journal_barrier_flush_transaction(jrnl_comp_journal_t *);

/* Checkpoint list management */
enum jrnl_comp_jbd2_shrink_type {JRNL_COMP_JBD2_SHRINK_DESTROY, JRNL_COMP_JBD2_SHRINK_BUSY_STOP, JRNL_COMP_JBD2_SHRINK_BUSY_SKIP};

void __jrnl_comp_jbd2_journal_clean_checkpoint_list(jrnl_comp_journal_t *journal, enum jrnl_comp_jbd2_shrink_type type);
unsigned long jrnl_comp_jbd2_journal_shrink_checkpoint_list(jrnl_comp_journal_t *journal, unsigned long *nr_to_scan);
int __jrnl_comp_jbd2_journal_remove_checkpoint(struct jrnl_comp_journal_head *);
int jrnl_comp_jbd2_journal_try_remove_checkpoint(struct jrnl_comp_journal_head *jh);
void jrnl_comp_jbd2_journal_destroy_checkpoint(jrnl_comp_journal_t *journal);
void __jrnl_comp_jbd2_journal_insert_checkpoint(struct jrnl_comp_journal_head *, jrnl_comp_transaction_t *);


/*
 * Triggers
 */

struct jrnl_comp_jbd2_buffer_trigger_type {
	/*
	 * Fired a the moment data to write to the journal are known to be
	 * stable - so either at the moment b_frozen_data is created or just
	 * before a buffer is written to the journal.  mapped_data is a mapped
	 * buffer that is the frozen data for commit.
	 */
	void (*t_frozen)(struct jrnl_comp_jbd2_buffer_trigger_type *type,
			 struct buffer_head *bh, void *mapped_data,
			 size_t size);

	/*
	 * Fired during journal abort for dirty buffers that will not be
	 * committed.
	 */
	void (*t_abort)(struct jrnl_comp_jbd2_buffer_trigger_type *type,
			struct buffer_head *bh);
};

extern void jrnl_comp_jbd2_buffer_frozen_trigger(struct jrnl_comp_journal_head *jh,
				       void *mapped_data,
				       struct jrnl_comp_jbd2_buffer_trigger_type *triggers);
extern void jrnl_comp_jbd2_buffer_abort_trigger(struct jrnl_comp_journal_head *jh,
				      struct jrnl_comp_jbd2_buffer_trigger_type *triggers);

/* Buffer IO */
extern int jrnl_comp_jbd2_journal_write_metadata_buffer(jrnl_comp_transaction_t *transaction,
					      struct jrnl_comp_journal_head *jh_in,
					      struct buffer_head **bh_out,
					      sector_t blocknr);

/* Transaction cache support */
extern void jrnl_comp_jbd2_journal_destroy_transaction_cache(void);
extern int __init jrnl_comp_jbd2_journal_init_transaction_cache(void);
extern void jrnl_comp_jbd2_journal_free_transaction(jrnl_comp_transaction_t *);

/*
 * Journal locking.
 *
 * We need to lock the journal during transaction state changes so that nobody
 * ever tries to take a handle on the running transaction while we are in the
 * middle of moving it to the commit phase.  j_state_lock does this.
 *
 * Note that the locking is completely interrupt unsafe.  We never touch
 * journal structures from interrupts.
 */

static inline jrnl_comp_handle_t *jrnl_comp_journal_current_handle(void)
{
	return current->jrnl_comp_journal_info;
}

/* The journaling code user interface:
 *
 * Create and destroy handles
 * Register buffer modifications against the current transaction.
 */

extern jrnl_comp_handle_t *jrnl_comp_jbd2_journal_start(jrnl_comp_journal_t *, int nblocks);
extern jrnl_comp_handle_t *jrnl_comp_jbd2__journal_start(jrnl_comp_journal_t *, int blocks, int rsv_blocks,
				     int revoke_records, gfp_t gfp_mask,
				     unsigned int type, unsigned int line_no);
extern int	 jrnl_comp_jbd2_journal_restart(jrnl_comp_handle_t *, int nblocks);
extern int	 jrnl_comp_jbd2__journal_restart(jrnl_comp_handle_t *, int nblocks,
				       int revoke_records, gfp_t gfp_mask);
extern int	 jrnl_comp_jbd2_journal_start_reserved(jrnl_comp_handle_t *handle,
				unsigned int type, unsigned int line_no);
extern void	 jrnl_comp_jbd2_journal_free_reserved(jrnl_comp_handle_t *handle);
extern int	 jrnl_comp_jbd2_journal_extend(jrnl_comp_handle_t *handle, int nblocks,
				     int revoke_records);
extern int	 jrnl_comp_jbd2_journal_get_write_access(jrnl_comp_handle_t *, struct buffer_head *);
extern int	 jrnl_comp_jbd2_journal_get_create_access (jrnl_comp_handle_t *, struct buffer_head *);
extern int	 jrnl_comp_jbd2_journal_get_undo_access(jrnl_comp_handle_t *, struct buffer_head *);
void		 jrnl_comp_jbd2_journal_set_triggers(struct buffer_head *,
					   struct jrnl_comp_jbd2_buffer_trigger_type *type);
extern int	 jrnl_comp_jbd2_journal_dirty_metadata (jrnl_comp_handle_t *, struct buffer_head *);
extern int jrnl_comp_jbd2_journal_dirty_metadata_range(jrnl_comp_handle_t *, struct buffer_head *,
					    unsigned int, unsigned int);
extern int	 jrnl_comp_jbd2_journal_forget (jrnl_comp_handle_t *, struct buffer_head *);
int jrnl_comp_jbd2_journal_invalidate_folio(jrnl_comp_journal_t *, struct folio *,
					size_t offset, size_t length);
bool jrnl_comp_jbd2_journal_try_to_free_buffers(jrnl_comp_journal_t *journal, struct folio *folio);
extern int	 jrnl_comp_jbd2_journal_stop(jrnl_comp_handle_t *);
extern int	 jrnl_comp_jbd2_journal_flush(jrnl_comp_journal_t *journal, unsigned int flags);
extern void	 jrnl_comp_jbd2_journal_lock_updates (jrnl_comp_journal_t *);
extern void	 jrnl_comp_jbd2_journal_unlock_updates (jrnl_comp_journal_t *);

void jrnl_comp_jbd2_journal_wait_updates(jrnl_comp_journal_t *);

extern jrnl_comp_journal_t * jrnl_comp_jbd2_journal_init_dev(struct block_device *bdev,
				struct block_device *fs_dev,
				unsigned long long start, int len, int bsize);
extern jrnl_comp_journal_t * jrnl_comp_jbd2_journal_init_inode (struct inode *);
extern int	   jrnl_comp_jbd2_journal_update_format (jrnl_comp_journal_t *);
extern int	   jrnl_comp_jbd2_journal_check_used_features
		   (jrnl_comp_journal_t *, unsigned long, unsigned long, unsigned long);
extern int	   jrnl_comp_jbd2_journal_check_available_features
		   (jrnl_comp_journal_t *, unsigned long, unsigned long, unsigned long);
extern int	   jrnl_comp_jbd2_journal_set_features
		   (jrnl_comp_journal_t *, unsigned long, unsigned long, unsigned long);
extern void	   jrnl_comp_jbd2_journal_clear_features
		   (jrnl_comp_journal_t *, unsigned long, unsigned long, unsigned long);
extern int	   jrnl_comp_jbd2_journal_load       (jrnl_comp_journal_t *journal);
extern int	   jrnl_comp_jbd2_journal_destroy    (jrnl_comp_journal_t *);
extern int	   jrnl_comp_jbd2_journal_recover    (jrnl_comp_journal_t *journal);
extern int	   jrnl_comp_jbd2_journal_wipe       (jrnl_comp_journal_t *, int);
extern int	   jrnl_comp_jbd2_journal_skip_recovery	(jrnl_comp_journal_t *);
extern void	   jrnl_comp_jbd2_journal_update_sb_errno(jrnl_comp_journal_t *);
extern int	   jrnl_comp_jbd2_journal_update_sb_log_tail	(jrnl_comp_journal_t *, jrnl_comp_tid_t,
				unsigned long, blk_opf_t);
extern void	   jrnl_comp_jbd2_journal_abort      (jrnl_comp_journal_t *, int);
extern int	   jrnl_comp_jbd2_journal_errno      (jrnl_comp_journal_t *);
extern void	   jrnl_comp_jbd2_journal_ack_err    (jrnl_comp_journal_t *);
extern int	   jrnl_comp_jbd2_journal_clear_err  (jrnl_comp_journal_t *);
extern int	   jrnl_comp_jbd2_journal_bmap(jrnl_comp_journal_t *, unsigned long, unsigned long long *);
extern int	   jrnl_comp_jbd2_journal_force_commit(jrnl_comp_journal_t *);
extern int	   jrnl_comp_jbd2_journal_force_commit_nested(jrnl_comp_journal_t *);
extern int	   jrnl_comp_jbd2_journal_inode_ranged_write(jrnl_comp_handle_t *handle,
			struct jrnl_comp_jbd2_inode *inode, loff_t start_byte,
			loff_t length);
extern int	   jrnl_comp_jbd2_journal_inode_ranged_wait(jrnl_comp_handle_t *handle,
			struct jrnl_comp_jbd2_inode *inode, loff_t start_byte,
			loff_t length);
extern int	   jrnl_comp_jbd2_journal_finish_inode_data_buffers(
			struct jrnl_comp_jbd2_inode *jinode);
extern int	   jrnl_comp_jbd2_journal_begin_ordered_truncate(jrnl_comp_journal_t *journal,
				struct jrnl_comp_jbd2_inode *inode, loff_t new_size);
extern void	   jrnl_comp_jbd2_journal_init_jrnl_comp_jbd_inode(struct jrnl_comp_jbd2_inode *jinode, struct inode *inode);
extern void	   jrnl_comp_jbd2_journal_release_jrnl_comp_jbd_inode(jrnl_comp_journal_t *journal, struct jrnl_comp_jbd2_inode *jinode);

/*
 * jrnl_comp_journal_head management
 */
struct jrnl_comp_journal_head *jrnl_comp_jbd2_journal_add_journal_head(struct buffer_head *bh);
struct jrnl_comp_journal_head *jrnl_comp_jbd2_journal_grab_journal_head(struct buffer_head *bh);
void jrnl_comp_jbd2_journal_put_journal_head(struct jrnl_comp_journal_head *jh);

/*
 * handle management
 */
extern struct kmem_cache *jrnl_comp_jbd2_handle_cache;

/*
 * This specialized allocator has to be a macro for its allocations to be
 * accounted separately (to have a separate alloc_tag). The typecast is
 * intentional to enforce typesafety.
 */
#define jrnl_comp_jbd2_alloc_handle(_gfp_flags)	\
		((jrnl_comp_handle_t *)kmem_cache_zalloc(jrnl_comp_jbd2_handle_cache, _gfp_flags))

static inline void jrnl_comp_jbd2_free_handle(jrnl_comp_handle_t *handle)
{
	kmem_cache_free(jrnl_comp_jbd2_handle_cache, handle);
}

/*
 * jrnl_comp_jbd2_inode management (optional, for those file systems that want to use
 * dynamically allocated jrnl_comp_jbd2_inode structures)
 */
extern struct kmem_cache *jrnl_comp_jbd2_inode_cache;

/*
 * This specialized allocator has to be a macro for its allocations to be
 * accounted separately (to have a separate alloc_tag). The typecast is
 * intentional to enforce typesafety.
 */
#define jrnl_comp_jbd2_alloc_inode(_gfp_flags)	\
		((struct jrnl_comp_jbd2_inode *)kmem_cache_alloc(jrnl_comp_jbd2_inode_cache, _gfp_flags))

static inline void jrnl_comp_jbd2_free_inode(struct jrnl_comp_jbd2_inode *jinode)
{
	kmem_cache_free(jrnl_comp_jbd2_inode_cache, jinode);
}

/* Primary revoke support */
#define JOURNAL_REVOKE_DEFAULT_HASH 256
extern int	   jrnl_comp_jbd2_journal_init_revoke(jrnl_comp_journal_t *, int);
extern void	   jrnl_comp_jbd2_journal_destroy_revoke_record_cache(void);
extern void	   jrnl_comp_jbd2_journal_destroy_revoke_table_cache(void);
extern int __init jrnl_comp_jbd2_journal_init_revoke_record_cache(void);
extern int __init jrnl_comp_jbd2_journal_init_revoke_table_cache(void);
struct jrnl_comp_jbd2_revoke_table_s *jrnl_comp_jbd2_journal_init_revoke_table(int hash_size);
void jrnl_comp_jbd2_journal_destroy_revoke_table(struct jrnl_comp_jbd2_revoke_table_s *table);

extern void	   jrnl_comp_jbd2_journal_destroy_revoke(jrnl_comp_journal_t *);
extern int	   jrnl_comp_jbd2_journal_revoke (jrnl_comp_handle_t *, unsigned long long, struct buffer_head *);
extern void	   jrnl_comp_jbd2_journal_cancel_revoke(jrnl_comp_handle_t *, struct jrnl_comp_journal_head *);
extern void	   jrnl_comp_jbd2_journal_write_revoke_records(jrnl_comp_transaction_t *transaction,
						     struct list_head *log_bufs);

/* Recovery revoke support */
extern int	jrnl_comp_jbd2_journal_set_revoke(jrnl_comp_journal_t *, unsigned long long, jrnl_comp_tid_t);
extern int	jrnl_comp_jbd2_journal_test_revoke(jrnl_comp_journal_t *, unsigned long long, jrnl_comp_tid_t);
extern void	jrnl_comp_jbd2_journal_clear_revoke(jrnl_comp_journal_t *);
extern void	jrnl_comp_jbd2_journal_switch_revoke_table(jrnl_comp_journal_t *journal);
extern void	jrnl_comp_jbd2_clear_buffer_revoked_flags(jrnl_comp_journal_t *journal);

/*
 * The log thread user interface:
 *
 * Request space in the current transaction, and force transaction commit
 * transitions on demand.
 */

int jrnl_comp_jbd2_log_start_commit(jrnl_comp_journal_t *journal, jrnl_comp_tid_t tid);
int jrnl_comp_jbd2_journal_start_commit(jrnl_comp_journal_t *journal, jrnl_comp_tid_t *tid);
int jrnl_comp_jbd2_log_wait_commit(jrnl_comp_journal_t *journal, jrnl_comp_tid_t tid);
int jrnl_comp_jbd2_transaction_committed(jrnl_comp_journal_t *journal, jrnl_comp_tid_t tid);
int jrnl_comp_jbd2_complete_transaction(jrnl_comp_journal_t *journal, jrnl_comp_tid_t tid);
int jrnl_comp_jbd2_log_do_checkpoint(jrnl_comp_journal_t *journal);
int jrnl_comp_jbd2_trans_will_send_data_barrier(jrnl_comp_journal_t *journal, jrnl_comp_tid_t tid);

void __jrnl_comp_jbd2_log_wait_for_space(jrnl_comp_journal_t *journal);
extern void __jrnl_comp_jbd2_journal_drop_transaction(jrnl_comp_journal_t *, jrnl_comp_transaction_t *);
extern int jrnl_comp_jbd2_cleanup_journal_tail(jrnl_comp_journal_t *);

/* Fast commit related APIs */
int jrnl_comp_jbd2_fc_begin_commit(jrnl_comp_journal_t *journal, jrnl_comp_tid_t tid);
int jrnl_comp_jbd2_fc_end_commit(jrnl_comp_journal_t *journal);
int jrnl_comp_jbd2_fc_end_commit_fallback(jrnl_comp_journal_t *journal);
int jrnl_comp_jbd2_fc_get_buf(jrnl_comp_journal_t *journal, struct buffer_head **bh_out);
int jrnl_comp_jbd2_submit_inode_data(jrnl_comp_journal_t *journal, struct jrnl_comp_jbd2_inode *jinode);
int jrnl_comp_jbd2_wait_inode_data(jrnl_comp_journal_t *journal, struct jrnl_comp_jbd2_inode *jinode);
int jrnl_comp_jbd2_fc_wait_bufs(jrnl_comp_journal_t *journal, int num_blks);
void jrnl_comp_jbd2_fc_release_bufs(jrnl_comp_journal_t *journal);

/*
 * is_journal_abort
 *
 * Simple test wrapper function to test the JRNL_COMP_JBD2_ABORT state flag.  This
 * bit, when set, indicates that we have had a fatal error somewhere,
 * either inside the journaling layer or indicated to us by the client
 * (eg. ext3), and that we and should not commit any further
 * transactions.
 */

static inline int is_journal_aborted(jrnl_comp_journal_t *journal)
{
	return journal->j_flags & JRNL_COMP_JBD2_ABORT;
}

static inline int is_handle_aborted(jrnl_comp_handle_t *handle)
{
	if (handle->h_aborted || !handle->h_transaction)
		return 1;
	return is_journal_aborted(handle->h_transaction->t_journal);
}

static inline void jrnl_comp_jbd2_journal_abort_handle(jrnl_comp_handle_t *handle)
{
	handle->h_aborted = 1;
}

static inline void jrnl_comp_jbd2_init_fs_dev_write_error(jrnl_comp_journal_t *journal)
{
	struct address_space *mapping = journal->j_fs_dev->bd_mapping;

	/*
	 * Save the original wb_err value of client fs's bdev mapping which
	 * could be used to detect the client fs's metadata async write error.
	 */
	errseq_check_and_advance(&mapping->wb_err, &journal->j_fs_dev_wb_err);
}

static inline int jrnl_comp_jbd2_check_fs_dev_write_error(jrnl_comp_journal_t *journal)
{
	struct address_space *mapping = journal->j_fs_dev->bd_mapping;

	return errseq_check(&mapping->wb_err,
			    READ_ONCE(journal->j_fs_dev_wb_err));
}

#endif /* __KERNEL__   */

/* Comparison functions for transaction IDs: perform comparisons using
 * modulo arithmetic so that they work over sequence number wraps. */

static inline int tid_gt(jrnl_comp_tid_t x, jrnl_comp_tid_t y)
{
	int difference = (x - y);
	return (difference > 0);
}

static inline int tid_geq(jrnl_comp_tid_t x, jrnl_comp_tid_t y)
{
	int difference = (x - y);
	return (difference >= 0);
}

extern int jrnl_comp_jbd2_journal_blocks_per_folio(struct inode *inode);
extern size_t journal_tag_bytes(jrnl_comp_journal_t *journal);

static inline int jrnl_comp_jbd2_journal_has_csum_v2or3(jrnl_comp_journal_t *journal)
{
	return jrnl_comp_jbd2_has_feature_csum2(journal) ||
	       jrnl_comp_jbd2_has_feature_csum3(journal);
}

static inline int jrnl_comp_jbd2_journal_get_num_fc_blks(journal_superblock_t *jsb)
{
	int num_fc_blocks = be32_to_cpu(jsb->s_num_fc_blks);

	return num_fc_blocks ? num_fc_blocks : JRNL_COMP_JBD2_DEFAULT_FAST_COMMIT_BLOCKS;
}

/*
 * Return number of free blocks in the log. Must be called under j_state_lock.
 */
static inline unsigned long jrnl_comp_jbd2_log_space_left(jrnl_comp_journal_t *journal)
{
	/* Allow for rounding errors */
	long free = journal->j_free - 32;

	if (journal->j_committing_transaction) {
		free -= atomic_read(&journal->
                        j_committing_transaction->t_outstanding_credits);
	}
	return max_t(long, free, 0);
}

int jrnl_comp_jbd2_jrnl_comp_cjfs_reconfigure(jrnl_comp_journal_t *journal);

static inline unsigned int jrnl_comp_jbd2_jrnl_comp_cjfs_slot(const jrnl_comp_transaction_t *t)
{
	return t->t_tid % t->t_jrnl_comp_cjfs_depth;
}

/* Caller holds b_state_lock or the journal list lock. */
static inline jrnl_comp_transaction_t *jrnl_comp_jbd2_jh_latest_transaction(struct jrnl_comp_journal_head *jh)
{
	jrnl_comp_transaction_t *latest = NULL;
	int v;

	for (v = 0; v < JRNL_COMP_CJFS_MAX_VERSIONS; v++) {
		jrnl_comp_transaction_t *t = jh->b_jh_entry[v]->b_next_transaction;
		if (!t)
			t = jh->b_jh_entry[v]->b_transaction;
		if (t && (!latest || tid_gt(t->t_tid, latest->t_tid)))
			latest = t;
	}
	return latest;
}

/*
 * Definitions which augment the buffer_head layer
 */

/* journaling buffer types */
#define BJ_None		0	/* Not journaled */
#define BJ_Metadata	1	/* Normal journaled metadata */
#define BJ_Forget	2	/* Buffer superseded by this transaction */
#define BJ_Shadow	3	/* Buffer contents being shadowed to the log */
#define BJ_Reserved	4	/* Buffer is reserved for access by journal */
#define BJ_Types	5

static inline u32 jrnl_comp_jbd2_chksum(u32 crc, const void *address, unsigned int length)
{
	return crc32c(crc, address, length);
}

/* Return most recent uncommitted transaction */
static inline jrnl_comp_tid_t  jrnl_comp_jbd2_get_latest_transaction(jrnl_comp_journal_t *journal)
{
	jrnl_comp_tid_t tid;

	read_lock(&journal->j_state_lock);
	tid = journal->j_commit_request;
	if (journal->j_running_transaction)
		tid = journal->j_running_transaction->t_tid;
	read_unlock(&journal->j_state_lock);
	return tid;
}

static inline int jrnl_comp_jbd2_handle_buffer_credits(jrnl_comp_handle_t *handle)
{
	jrnl_comp_journal_t *journal;

	if (!handle->h_reserved)
		journal = handle->h_transaction->t_journal;
	else
		journal = handle->h_journal;

	return handle->h_total_credits -
		DIV_ROUND_UP(handle->h_revoke_credits_requested,
			     journal->j_revoke_records_per_block);
}

#ifdef __KERNEL__

#define buffer_trace_init(bh)	do {} while (0)
#define print_buffer_fields(bh)	do {} while (0)
#define print_buffer_trace(bh)	do {} while (0)
#define BUFFER_TRACE(bh, info)	do {} while (0)
#define BUFFER_TRACE2(bh, bh2, info)	do {} while (0)
#define JBUFFER_TRACE(jh, info)	do {} while (0)

#endif	/* __KERNEL__ */

#endif	/* _LINUX_JRNL_COMP_JBD2_H */
