// SPDX-License-Identifier: GPL-2.0
/*
 *  linux/fs/jrnl_comp_ext4/sysfs.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Theodore Ts'o (tytso@mit.edu)
 *
 */

#include <linux/jrnl_comp_jbd2_workvolume.h>
#include <linux/time.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/part_stat.h>

#include "jrnl_comp_ext4.h"
#include "jrnl_comp_ext4_jrnl_comp_jbd2.h"

typedef enum {
	attr_noop,
	attr_jrnl_comp_cjfs,
	attr_delayed_allocation_blocks,
	attr_session_write_kbytes,
	attr_lifetime_write_kbytes,
	attr_reserved_clusters,
	attr_sra_exceeded_retry_limit,
	attr_inode_readahead,
	attr_trigger_test_error,
	attr_first_error_time,
	attr_last_error_time,
	attr_clusters_in_group,
	attr_mb_order,
	attr_feature,
	attr_pointer_pi,
	attr_pointer_ui,
	attr_pointer_ul,
	attr_pointer_u64,
	attr_pointer_u8,
	attr_pointer_string,
	attr_pointer_atomic,
	attr_journal_task,
	attr_err_report_sec,
} attr_id_t;

typedef enum {
	ptr_explicit,
	ptr_jrnl_comp_ext4_sb_info_offset,
	ptr_jrnl_comp_ext4_super_block_offset,
} attr_ptr_t;

static const char proc_dirname[] = "fs/jrnl_comp_ext4";
static struct proc_dir_entry *jrnl_comp_ext4_proc_root;

struct jrnl_comp_ext4_attr {
	struct attribute attr;
	short attr_id;
	short attr_ptr;
	unsigned short attr_size;
	union {
		int offset;
		void *explicit_ptr;
	} u;
};

/* Trylock is essential: unmount holds s_umount while removing sysfs. */
static ssize_t jrnl_comp_cjfs_attr_access(struct jrnl_comp_ext4_sb_info *sbi, const char *name,
                               char *out, const char *in, size_t count)
{
 struct super_block *sb = sbi->s_sb;
 jrnl_comp_journal_t *j;
 unsigned int value;
 ssize_t ret = -EINVAL;

 if (in && !capable(CAP_SYS_ADMIN))
  return -EPERM;
 if (!down_read_trylock(&sb->s_umount))
  return -EBUSY;
 j = sbi->s_journal;
 if (!j) {
  ret = -ENODEV;
  goto unlock_sb;
 }
 mutex_lock(&j->j_jrnl_comp_cjfs_config_mutex);
 if (!in && !strncmp(name, "collect_", 8)) {
  ret = jrnl_comp_jbd2_wv_access(j, sb, name, out, NULL, 0);
  goto unlock_config;
 }
 if (!in) {
  if (!strcmp(name, "effective"))
   ret = sysfs_emit(out, "%u %s %u %llu\n", j->j_jrnl_comp_cjfs_depth,
      j->j_jrnl_comp_cjfs_eager ? "eager" : "lazy", j->j_jrnl_comp_cjfs_window_us,
      j->j_jrnl_comp_cjfs_generation);
  else if (!strcmp(name, "pending_version_depth"))
   ret = sysfs_emit(out, "%u\n", j->j_jrnl_comp_cjfs_pending_depth);
  else if (!strcmp(name, "pending_copy_mode"))
   ret = sysfs_emit(out, "%s\n", j->j_jrnl_comp_cjfs_pending_eager ? "eager" : "lazy");
  else if (!strcmp(name, "pending_coalesce_us"))
   ret = sysfs_emit(out, "%u\n", j->j_jrnl_comp_cjfs_pending_window_us);
  else if (!strcmp(name, "pending_subblock_granularity"))
   ret = sysfs_emit(out, "%u\n", j->j_subblock_pending_granularity);
  else if (!strcmp(name, "subblock_status"))
   ret = sysfs_emit(out, "%u %llu %llu %llu %llu\n",
      j->j_subblock_granularity,
      READ_ONCE(j->j_subblock_containers), READ_ONCE(j->j_subblock_images),
      READ_ONCE(j->j_subblock_payload_bytes), READ_ONCE(j->j_subblock_replayed));
  goto unlock_config;
 }
 if (in && is_journal_aborted(j) &&
     (!strcmp(name, "collect_phase") || !strcmp(name, "collect_stop"))) {
  ret = jrnl_comp_jbd2_wv_access(j, sb, name, NULL, in, count);
  goto unlock_config;
 }
 if (sb_rdonly(sb) || is_journal_aborted(j)) {
  ret = -EROFS;
  goto unlock_config;
 }
 if (sb->s_writers.frozen != SB_UNFROZEN) {
  ret = -EBUSY;
  goto unlock_config;
 }
 /* Initial experiment supports ordered data with an internal journal. */
 if (test_opt(sb, DATA_FLAGS) != JRNL_COMP_EXT4_MOUNT_ORDERED_DATA ||
     test_opt2(sb, JOURNAL_FAST_COMMIT) || j->j_dev != sb->s_bdev) {
  ret = -EOPNOTSUPP;
  goto unlock_config;
 }
 if (!strncmp(name, "collect_", 8)) {
  ret = jrnl_comp_jbd2_wv_access(j, sb, name, NULL, in, count);
  goto unlock_config;
 }
 if (!strcmp(name, "pending_copy_mode")) {
  if (sysfs_streq(in, "eager"))
   j->j_jrnl_comp_cjfs_pending_eager = true;
  else if (sysfs_streq(in, "lazy"))
   j->j_jrnl_comp_cjfs_pending_eager = false;
  else
   goto unlock_config;
 } else {
  ret = kstrtouint(in, 10, &value);
  if (ret)
   goto unlock_config;
  ret = -EINVAL;
  if (!strcmp(name, "pending_version_depth")) {
   if (!value || value > JRNL_COMP_CJFS_MAX_VERSIONS)
    goto unlock_config;
   j->j_jrnl_comp_cjfs_pending_depth = value;
  } else if (!strcmp(name, "pending_coalesce_us")) {
   if (value > 10000)
    goto unlock_config;
   j->j_jrnl_comp_cjfs_pending_window_us = value;
  } else if (!strcmp(name, "pending_subblock_granularity")) {
   if (value && value != 64 && value != 128 && value != 256)
    goto unlock_config;
   j->j_subblock_pending_granularity = value;
  } else if (!strcmp(name, "apply")) {
   if (value != 1)
    goto unlock_config;
   ret = jrnl_comp_jbd2_jrnl_comp_cjfs_reconfigure(j);
   if (ret)
    goto unlock_config;
  } else {
   goto unlock_config;
  }
 }
 ret = count;
unlock_config:
 mutex_unlock(&j->j_jrnl_comp_cjfs_config_mutex);
unlock_sb:
 up_read(&sb->s_umount);
 return ret;
}

static ssize_t session_write_kbytes_show(struct jrnl_comp_ext4_sb_info *sbi, char *buf)
{
	struct super_block *sb = sbi->s_buddy_cache->i_sb;

	return sysfs_emit(buf, "%lu\n",
			(part_stat_read(sb->s_bdev, sectors[STAT_WRITE]) -
			 sbi->s_sectors_written_start) >> 1);
}

static ssize_t lifetime_write_kbytes_show(struct jrnl_comp_ext4_sb_info *sbi, char *buf)
{
	struct super_block *sb = sbi->s_buddy_cache->i_sb;

	return sysfs_emit(buf, "%llu\n",
			(unsigned long long)(sbi->s_kbytes_written +
			((part_stat_read(sb->s_bdev, sectors[STAT_WRITE]) -
			  JRNL_COMP_EXT4_SB(sb)->s_sectors_written_start) >> 1)));
}

static ssize_t inode_readahead_blks_store(struct jrnl_comp_ext4_sb_info *sbi,
					  const char *buf, size_t count)
{
	unsigned long t;
	int ret;

	ret = kstrtoul(skip_spaces(buf), 0, &t);
	if (ret)
		return ret;

	if (t && (!is_power_of_2(t) || t > 0x40000000))
		return -EINVAL;

	sbi->s_inode_readahead_blks = t;
	return count;
}

static ssize_t reserved_clusters_store(struct jrnl_comp_ext4_sb_info *sbi,
				   const char *buf, size_t count)
{
	unsigned long long val;
	jrnl_comp_ext4_fsblk_t clusters = (jrnl_comp_ext4_blocks_count(sbi->s_es) >>
				 sbi->s_cluster_bits);
	int ret;

	ret = kstrtoull(skip_spaces(buf), 0, &val);
	if (ret || val >= clusters || (s64)val < 0)
		return -EINVAL;

	atomic64_set(&sbi->s_resv_clusters, val);
	return count;
}

static ssize_t trigger_test_error(struct jrnl_comp_ext4_sb_info *sbi,
				  const char *buf, size_t count)
{
	int len = count;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	if (len && buf[len-1] == '\n')
		len--;

	if (len)
		jrnl_comp_ext4_error(sbi->s_sb, "%.*s", len, buf);
	return count;
}

static ssize_t err_report_sec_store(struct jrnl_comp_ext4_sb_info *sbi,
				    const char *buf, size_t count)
{
	unsigned long t;
	int ret;

	ret = kstrtoul(skip_spaces(buf), 0, &t);
	if (ret)
		return ret;

	/*the maximum time interval must not exceed one year.*/
	if (t > (365*24*60*60))
		return -EINVAL;

	if (sbi->s_err_report_sec == t)		/*nothing to do*/
		goto out;
	else if (!sbi->s_err_report_sec && t) {
		timer_setup(&sbi->s_err_report, print_daily_error_info, 0);
	} else if (sbi->s_err_report_sec && !t) {
		timer_delete_sync(&sbi->s_err_report);
		goto out;
	}

	sbi->s_err_report_sec = t;
	mod_timer(&sbi->s_err_report, jiffies + secs_to_jiffies(sbi->s_err_report_sec));

out:
	return count;
}

static ssize_t journal_task_show(struct jrnl_comp_ext4_sb_info *sbi, char *buf)
{
	if (!sbi->s_journal)
		return sysfs_emit(buf, "<none>\n");
	return sysfs_emit(buf, "%d\n",
			task_pid_vnr(sbi->s_journal->j_task));
}

#define JRNL_COMP_EXT4_ATTR(_name,_mode,_id)					\
static struct jrnl_comp_ext4_attr jrnl_comp_ext4_attr_##_name = {				\
	.attr = {.name = __stringify(_name), .mode = _mode },		\
	.attr_id = attr_##_id,						\
}

JRNL_COMP_EXT4_ATTR(pending_version_depth, 0600, jrnl_comp_cjfs);
JRNL_COMP_EXT4_ATTR(pending_copy_mode, 0600, jrnl_comp_cjfs);
JRNL_COMP_EXT4_ATTR(pending_coalesce_us, 0600, jrnl_comp_cjfs);
JRNL_COMP_EXT4_ATTR(effective, 0400, jrnl_comp_cjfs);
JRNL_COMP_EXT4_ATTR(pending_subblock_granularity, 0600, jrnl_comp_cjfs);
JRNL_COMP_EXT4_ATTR(subblock_status, 0400, jrnl_comp_cjfs);
JRNL_COMP_EXT4_ATTR(apply, 0200, jrnl_comp_cjfs);
JRNL_COMP_EXT4_ATTR(collect_pending_mask, 0600, jrnl_comp_cjfs);
JRNL_COMP_EXT4_ATTR(collect_pending_sample_every, 0600, jrnl_comp_cjfs);
JRNL_COMP_EXT4_ATTR(collect_pending_memory_bytes, 0600, jrnl_comp_cjfs);
JRNL_COMP_EXT4_ATTR(collect_apply, 0200, jrnl_comp_cjfs);
JRNL_COMP_EXT4_ATTR(collect_start, 0200, jrnl_comp_cjfs);
JRNL_COMP_EXT4_ATTR(collect_phase, 0600, jrnl_comp_cjfs);
JRNL_COMP_EXT4_ATTR(collect_stop, 0200, jrnl_comp_cjfs);
JRNL_COMP_EXT4_ATTR(collect_status, 0400, jrnl_comp_cjfs);
JRNL_COMP_EXT4_ATTR(collect_workload, 0400, jrnl_comp_cjfs);
JRNL_COMP_EXT4_ATTR(collect_drain, 0400, jrnl_comp_cjfs);
static struct attribute *jrnl_comp_cjfs_attrs[] = {
 &jrnl_comp_ext4_attr_collect_pending_mask.attr,
 &jrnl_comp_ext4_attr_collect_pending_sample_every.attr,
 &jrnl_comp_ext4_attr_collect_pending_memory_bytes.attr,
 &jrnl_comp_ext4_attr_collect_apply.attr,
 &jrnl_comp_ext4_attr_collect_start.attr,
 &jrnl_comp_ext4_attr_collect_phase.attr,
 &jrnl_comp_ext4_attr_collect_stop.attr,
 &jrnl_comp_ext4_attr_collect_status.attr,
 &jrnl_comp_ext4_attr_collect_workload.attr,
 &jrnl_comp_ext4_attr_collect_drain.attr,

 &jrnl_comp_ext4_attr_pending_version_depth.attr,
 &jrnl_comp_ext4_attr_pending_copy_mode.attr,
 &jrnl_comp_ext4_attr_pending_coalesce_us.attr,
 &jrnl_comp_ext4_attr_effective.attr,
 &jrnl_comp_ext4_attr_pending_subblock_granularity.attr,
 &jrnl_comp_ext4_attr_subblock_status.attr,
 &jrnl_comp_ext4_attr_apply.attr,
 NULL,
};
static const struct attribute_group jrnl_comp_cjfs_group = {
 .name = "jrnl_comp_cjfs",
 .attrs = jrnl_comp_cjfs_attrs,
};

#define JRNL_COMP_EXT4_ATTR_FUNC(_name,_mode)  JRNL_COMP_EXT4_ATTR(_name,_mode,_name)

#define JRNL_COMP_EXT4_ATTR_FEATURE(_name)   JRNL_COMP_EXT4_ATTR(_name, 0444, feature)

#define JRNL_COMP_EXT4_ATTR_OFFSET(_name,_mode,_id,_struct,_elname)	\
static struct jrnl_comp_ext4_attr jrnl_comp_ext4_attr_##_name = {			\
	.attr = {.name = __stringify(_name), .mode = _mode },	\
	.attr_id = attr_##_id,					\
	.attr_ptr = ptr_##_struct##_offset,			\
	.u = {							\
		.offset = offsetof(struct _struct, _elname),\
	},							\
}

#define JRNL_COMP_EXT4_ATTR_STRING(_name,_mode,_size,_struct,_elname)	\
static struct jrnl_comp_ext4_attr jrnl_comp_ext4_attr_##_name = {			\
	.attr = {.name = __stringify(_name), .mode = _mode },	\
	.attr_id = attr_pointer_string,				\
	.attr_size = _size,					\
	.attr_ptr = ptr_##_struct##_offset,			\
	.u = {							\
		.offset = offsetof(struct _struct, _elname),\
	},							\
}

#define JRNL_COMP_EXT4_RO_ATTR_ES_UI(_name,_elname)				\
	JRNL_COMP_EXT4_ATTR_OFFSET(_name, 0444, pointer_ui, jrnl_comp_ext4_super_block, _elname)

#define JRNL_COMP_EXT4_RO_ATTR_ES_U8(_name,_elname)				\
	JRNL_COMP_EXT4_ATTR_OFFSET(_name, 0444, pointer_u8, jrnl_comp_ext4_super_block, _elname)

#define JRNL_COMP_EXT4_RO_ATTR_ES_U64(_name,_elname)				\
	JRNL_COMP_EXT4_ATTR_OFFSET(_name, 0444, pointer_u64, jrnl_comp_ext4_super_block, _elname)

#define JRNL_COMP_EXT4_RO_ATTR_ES_STRING(_name,_elname,_size)			\
	JRNL_COMP_EXT4_ATTR_STRING(_name, 0444, _size, jrnl_comp_ext4_super_block, _elname)

#define JRNL_COMP_EXT4_RW_ATTR_SBI_PI(_name,_elname)      \
	JRNL_COMP_EXT4_ATTR_OFFSET(_name, 0644, pointer_pi, jrnl_comp_ext4_sb_info, _elname)

#define JRNL_COMP_EXT4_RW_ATTR_SBI_UI(_name,_elname)	\
	JRNL_COMP_EXT4_ATTR_OFFSET(_name, 0644, pointer_ui, jrnl_comp_ext4_sb_info, _elname)

#define JRNL_COMP_EXT4_RW_ATTR_SBI_UL(_name,_elname)	\
	JRNL_COMP_EXT4_ATTR_OFFSET(_name, 0644, pointer_ul, jrnl_comp_ext4_sb_info, _elname)

#define JRNL_COMP_EXT4_RO_ATTR_SBI_ATOMIC(_name,_elname)	\
	JRNL_COMP_EXT4_ATTR_OFFSET(_name, 0444, pointer_atomic, jrnl_comp_ext4_sb_info, _elname)

#define JRNL_COMP_EXT4_ATTR_PTR(_name,_mode,_id,_ptr) \
static struct jrnl_comp_ext4_attr jrnl_comp_ext4_attr_##_name = {			\
	.attr = {.name = __stringify(_name), .mode = _mode },	\
	.attr_id = attr_##_id,					\
	.attr_ptr = ptr_explicit,				\
	.u = {							\
		.explicit_ptr = _ptr,				\
	},							\
}

#define ATTR_LIST(name) &jrnl_comp_ext4_attr_##name.attr

JRNL_COMP_EXT4_ATTR_FUNC(delayed_allocation_blocks, 0444);
JRNL_COMP_EXT4_ATTR_FUNC(session_write_kbytes, 0444);
JRNL_COMP_EXT4_ATTR_FUNC(lifetime_write_kbytes, 0444);
JRNL_COMP_EXT4_ATTR_FUNC(reserved_clusters, 0644);
JRNL_COMP_EXT4_ATTR_FUNC(sra_exceeded_retry_limit, 0444);

JRNL_COMP_EXT4_ATTR_OFFSET(inode_readahead_blks, 0644, inode_readahead,
		 jrnl_comp_ext4_sb_info, s_inode_readahead_blks);
JRNL_COMP_EXT4_ATTR_OFFSET(mb_group_prealloc, 0644, clusters_in_group,
		 jrnl_comp_ext4_sb_info, s_mb_group_prealloc);
JRNL_COMP_EXT4_ATTR_OFFSET(mb_best_avail_max_trim_order, 0644, mb_order,
		 jrnl_comp_ext4_sb_info, s_mb_best_avail_max_trim_order);
JRNL_COMP_EXT4_ATTR_OFFSET(err_report_sec, 0644, err_report_sec, jrnl_comp_ext4_sb_info, s_err_report_sec);
JRNL_COMP_EXT4_RW_ATTR_SBI_UI(inode_goal, s_inode_goal);
JRNL_COMP_EXT4_RW_ATTR_SBI_UI(mb_stats, s_mb_stats);
JRNL_COMP_EXT4_RW_ATTR_SBI_UI(mb_max_to_scan, s_mb_max_to_scan);
JRNL_COMP_EXT4_RW_ATTR_SBI_UI(mb_min_to_scan, s_mb_min_to_scan);
JRNL_COMP_EXT4_RW_ATTR_SBI_UI(mb_order2_req, s_mb_order2_reqs);
JRNL_COMP_EXT4_RW_ATTR_SBI_UI(mb_stream_req, s_mb_stream_request);
JRNL_COMP_EXT4_RW_ATTR_SBI_UI(mb_max_linear_groups, s_mb_max_linear_groups);
JRNL_COMP_EXT4_RW_ATTR_SBI_UI(extent_max_zeroout_kb, s_extent_max_zeroout_kb);
JRNL_COMP_EXT4_ATTR(trigger_fs_error, 0200, trigger_test_error);
JRNL_COMP_EXT4_RW_ATTR_SBI_PI(err_ratelimit_interval_ms, s_err_ratelimit_state.interval);
JRNL_COMP_EXT4_RW_ATTR_SBI_PI(err_ratelimit_burst, s_err_ratelimit_state.burst);
JRNL_COMP_EXT4_RW_ATTR_SBI_PI(warning_ratelimit_interval_ms, s_warning_ratelimit_state.interval);
JRNL_COMP_EXT4_RW_ATTR_SBI_PI(warning_ratelimit_burst, s_warning_ratelimit_state.burst);
JRNL_COMP_EXT4_RW_ATTR_SBI_PI(msg_ratelimit_interval_ms, s_msg_ratelimit_state.interval);
JRNL_COMP_EXT4_RW_ATTR_SBI_PI(msg_ratelimit_burst, s_msg_ratelimit_state.burst);
#ifdef CONFIG_JRNL_COMP_EXT4_DEBUG
JRNL_COMP_EXT4_RW_ATTR_SBI_UL(simulate_fail, s_simulate_fail);
#endif
JRNL_COMP_EXT4_RO_ATTR_SBI_ATOMIC(warning_count, s_warning_count);
JRNL_COMP_EXT4_RO_ATTR_SBI_ATOMIC(msg_count, s_msg_count);
JRNL_COMP_EXT4_RO_ATTR_ES_UI(errors_count, s_error_count);
JRNL_COMP_EXT4_RO_ATTR_ES_U8(first_error_errcode, s_first_error_errcode);
JRNL_COMP_EXT4_RO_ATTR_ES_U8(last_error_errcode, s_last_error_errcode);
JRNL_COMP_EXT4_RO_ATTR_ES_UI(first_error_ino, s_first_error_ino);
JRNL_COMP_EXT4_RO_ATTR_ES_UI(last_error_ino, s_last_error_ino);
JRNL_COMP_EXT4_RO_ATTR_ES_U64(first_error_block, s_first_error_block);
JRNL_COMP_EXT4_RO_ATTR_ES_U64(last_error_block, s_last_error_block);
JRNL_COMP_EXT4_RO_ATTR_ES_UI(first_error_line, s_first_error_line);
JRNL_COMP_EXT4_RO_ATTR_ES_UI(last_error_line, s_last_error_line);
JRNL_COMP_EXT4_RO_ATTR_ES_STRING(first_error_func, s_first_error_func, 32);
JRNL_COMP_EXT4_RO_ATTR_ES_STRING(last_error_func, s_last_error_func, 32);
JRNL_COMP_EXT4_ATTR(first_error_time, 0444, first_error_time);
JRNL_COMP_EXT4_ATTR(last_error_time, 0444, last_error_time);
JRNL_COMP_EXT4_ATTR(journal_task, 0444, journal_task);
JRNL_COMP_EXT4_RW_ATTR_SBI_UI(mb_prefetch, s_mb_prefetch);
JRNL_COMP_EXT4_RW_ATTR_SBI_UI(mb_prefetch_limit, s_mb_prefetch_limit);
JRNL_COMP_EXT4_RW_ATTR_SBI_UL(last_trim_minblks, s_last_trim_minblks);
JRNL_COMP_EXT4_RW_ATTR_SBI_UI(sb_update_sec, s_sb_update_sec);
JRNL_COMP_EXT4_RW_ATTR_SBI_UI(sb_update_kb, s_sb_update_kb);

static unsigned int old_bump_val = 128;
JRNL_COMP_EXT4_ATTR_PTR(max_writeback_mb_bump, 0444, pointer_ui, &old_bump_val);

static struct attribute *jrnl_comp_ext4_attrs[] = {
	ATTR_LIST(delayed_allocation_blocks),
	ATTR_LIST(session_write_kbytes),
	ATTR_LIST(lifetime_write_kbytes),
	ATTR_LIST(reserved_clusters),
	ATTR_LIST(sra_exceeded_retry_limit),
	ATTR_LIST(inode_readahead_blks),
	ATTR_LIST(inode_goal),
	ATTR_LIST(mb_stats),
	ATTR_LIST(mb_max_to_scan),
	ATTR_LIST(mb_min_to_scan),
	ATTR_LIST(mb_order2_req),
	ATTR_LIST(mb_stream_req),
	ATTR_LIST(mb_group_prealloc),
	ATTR_LIST(mb_max_linear_groups),
	ATTR_LIST(max_writeback_mb_bump),
	ATTR_LIST(extent_max_zeroout_kb),
	ATTR_LIST(trigger_fs_error),
	ATTR_LIST(err_ratelimit_interval_ms),
	ATTR_LIST(err_ratelimit_burst),
	ATTR_LIST(warning_ratelimit_interval_ms),
	ATTR_LIST(warning_ratelimit_burst),
	ATTR_LIST(msg_ratelimit_interval_ms),
	ATTR_LIST(msg_ratelimit_burst),
	ATTR_LIST(mb_best_avail_max_trim_order),
	ATTR_LIST(errors_count),
	ATTR_LIST(warning_count),
	ATTR_LIST(msg_count),
	ATTR_LIST(first_error_ino),
	ATTR_LIST(last_error_ino),
	ATTR_LIST(first_error_block),
	ATTR_LIST(last_error_block),
	ATTR_LIST(first_error_line),
	ATTR_LIST(last_error_line),
	ATTR_LIST(first_error_func),
	ATTR_LIST(last_error_func),
	ATTR_LIST(first_error_errcode),
	ATTR_LIST(last_error_errcode),
	ATTR_LIST(first_error_time),
	ATTR_LIST(last_error_time),
	ATTR_LIST(journal_task),
#ifdef CONFIG_JRNL_COMP_EXT4_DEBUG
	ATTR_LIST(simulate_fail),
#endif
	ATTR_LIST(mb_prefetch),
	ATTR_LIST(mb_prefetch_limit),
	ATTR_LIST(last_trim_minblks),
	ATTR_LIST(sb_update_sec),
	ATTR_LIST(sb_update_kb),
	ATTR_LIST(err_report_sec),
	NULL,
};
ATTRIBUTE_GROUPS(jrnl_comp_ext4);

/* Features this copy of jrnl_comp_ext4 supports */
JRNL_COMP_EXT4_ATTR_FEATURE(lazy_itable_init);
JRNL_COMP_EXT4_ATTR_FEATURE(batched_discard);
JRNL_COMP_EXT4_ATTR_FEATURE(meta_bg_resize);
#ifdef CONFIG_FS_ENCRYPTION
JRNL_COMP_EXT4_ATTR_FEATURE(encryption);
JRNL_COMP_EXT4_ATTR_FEATURE(test_dummy_encryption_v2);
#endif
#if IS_ENABLED(CONFIG_UNICODE)
JRNL_COMP_EXT4_ATTR_FEATURE(casefold);
#endif
#ifdef CONFIG_FS_VERITY
JRNL_COMP_EXT4_ATTR_FEATURE(verity);
#endif
JRNL_COMP_EXT4_ATTR_FEATURE(metadata_csum_seed);
JRNL_COMP_EXT4_ATTR_FEATURE(fast_commit);
#if IS_ENABLED(CONFIG_UNICODE) && defined(CONFIG_FS_ENCRYPTION)
JRNL_COMP_EXT4_ATTR_FEATURE(encrypted_casefold);
#endif
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
JRNL_COMP_EXT4_ATTR_FEATURE(blocksize_gt_pagesize);
#endif

static struct attribute *jrnl_comp_ext4_feat_attrs[] = {
	ATTR_LIST(lazy_itable_init),
	ATTR_LIST(batched_discard),
	ATTR_LIST(meta_bg_resize),
#ifdef CONFIG_FS_ENCRYPTION
	ATTR_LIST(encryption),
	ATTR_LIST(test_dummy_encryption_v2),
#endif
#if IS_ENABLED(CONFIG_UNICODE)
	ATTR_LIST(casefold),
#endif
#ifdef CONFIG_FS_VERITY
	ATTR_LIST(verity),
#endif
	ATTR_LIST(metadata_csum_seed),
	ATTR_LIST(fast_commit),
#if IS_ENABLED(CONFIG_UNICODE) && defined(CONFIG_FS_ENCRYPTION)
	ATTR_LIST(encrypted_casefold),
#endif
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
	ATTR_LIST(blocksize_gt_pagesize),
#endif
	NULL,
};
ATTRIBUTE_GROUPS(jrnl_comp_ext4_feat);

static void *calc_ptr(struct jrnl_comp_ext4_attr *a, struct jrnl_comp_ext4_sb_info *sbi)
{
	switch (a->attr_ptr) {
	case ptr_explicit:
		return a->u.explicit_ptr;
	case ptr_jrnl_comp_ext4_sb_info_offset:
		return (void *) (((char *) sbi) + a->u.offset);
	case ptr_jrnl_comp_ext4_super_block_offset:
		return (void *) (((char *) sbi->s_es) + a->u.offset);
	}
	return NULL;
}

static ssize_t __print_tstamp(char *buf, __le32 lo, __u8 hi)
{
	return sysfs_emit(buf, "%lld\n",
			((time64_t)hi << 32) + le32_to_cpu(lo));
}

#define print_tstamp(buf, es, tstamp) \
	__print_tstamp(buf, (es)->tstamp, (es)->tstamp ## _hi)

static ssize_t jrnl_comp_ext4_generic_attr_show(struct jrnl_comp_ext4_attr *a,
				      struct jrnl_comp_ext4_sb_info *sbi, char *buf)
{
	void *ptr = calc_ptr(a, sbi);

	if (!ptr)
		return 0;

	switch (a->attr_id) {
	case attr_inode_readahead:
	case attr_clusters_in_group:
	case attr_mb_order:
	case attr_pointer_pi:
	case attr_pointer_ui:
		if (a->attr_ptr == ptr_jrnl_comp_ext4_super_block_offset)
			return sysfs_emit(buf, "%u\n", le32_to_cpup(ptr));
		return sysfs_emit(buf, "%u\n", *((unsigned int *) ptr));
	case attr_pointer_ul:
	case attr_err_report_sec:
		return sysfs_emit(buf, "%lu\n", *((unsigned long *) ptr));
	case attr_pointer_u8:
		return sysfs_emit(buf, "%u\n", *((unsigned char *) ptr));
	case attr_pointer_u64:
		if (a->attr_ptr == ptr_jrnl_comp_ext4_super_block_offset)
			return sysfs_emit(buf, "%llu\n", le64_to_cpup(ptr));
		return sysfs_emit(buf, "%llu\n", *((unsigned long long *) ptr));
	case attr_pointer_string:
		return sysfs_emit(buf, "%.*s\n", a->attr_size, (char *) ptr);
	case attr_pointer_atomic:
		return sysfs_emit(buf, "%d\n", atomic_read((atomic_t *) ptr));
	}
	return 0;
}

static ssize_t jrnl_comp_ext4_attr_show(struct kobject *kobj,
			      struct attribute *attr, char *buf)
{
	struct jrnl_comp_ext4_sb_info *sbi = container_of(kobj, struct jrnl_comp_ext4_sb_info,
						s_kobj);
	struct jrnl_comp_ext4_attr *a = container_of(attr, struct jrnl_comp_ext4_attr, attr);

	switch (a->attr_id) {
	case attr_jrnl_comp_cjfs:
		return jrnl_comp_cjfs_attr_access(sbi, attr->name, buf, NULL, 0);
	case attr_delayed_allocation_blocks:
		return sysfs_emit(buf, "%llu\n",
				(s64) JRNL_COMP_EXT4_C2B(sbi,
		       percpu_counter_sum(&sbi->s_dirtyclusters_counter)));
	case attr_session_write_kbytes:
		return session_write_kbytes_show(sbi, buf);
	case attr_lifetime_write_kbytes:
		return lifetime_write_kbytes_show(sbi, buf);
	case attr_reserved_clusters:
		return sysfs_emit(buf, "%llu\n",
				(unsigned long long)
				atomic64_read(&sbi->s_resv_clusters));
	case attr_sra_exceeded_retry_limit:
		return sysfs_emit(buf, "%llu\n",
				(unsigned long long)
			percpu_counter_sum(&sbi->s_sra_exceeded_retry_limit));
	case attr_feature:
		return sysfs_emit(buf, "supported\n");
	case attr_first_error_time:
		return print_tstamp(buf, sbi->s_es, s_first_error_time);
	case attr_last_error_time:
		return print_tstamp(buf, sbi->s_es, s_last_error_time);
	case attr_journal_task:
		return journal_task_show(sbi, buf);
	default:
		return jrnl_comp_ext4_generic_attr_show(a, sbi, buf);
	}
}

static ssize_t jrnl_comp_ext4_generic_attr_store(struct jrnl_comp_ext4_attr *a,
				       struct jrnl_comp_ext4_sb_info *sbi,
				       const char *buf, size_t len)
{
	int ret;
	unsigned int t;
	unsigned long lt;
	void *ptr = calc_ptr(a, sbi);

	if (!ptr)
		return 0;

	switch (a->attr_id) {
	case attr_pointer_pi:
		ret = kstrtouint(skip_spaces(buf), 0, &t);
		if (ret)
			return ret;
		if ((int)t < 0)
			return -EINVAL;
		*((unsigned int *) ptr) = t;
		return len;
	case attr_pointer_ui:
		ret = kstrtouint(skip_spaces(buf), 0, &t);
		if (ret)
			return ret;
		if (a->attr_ptr == ptr_jrnl_comp_ext4_super_block_offset)
			*((__le32 *) ptr) = cpu_to_le32(t);
		else
			*((unsigned int *) ptr) = t;
		return len;
	case attr_mb_order:
		ret = kstrtouint(skip_spaces(buf), 0, &t);
		if (ret)
			return ret;
		if (t > 64)
			return -EINVAL;
		*((unsigned int *) ptr) = t;
		return len;
	case attr_clusters_in_group:
		ret = kstrtouint(skip_spaces(buf), 0, &t);
		if (ret)
			return ret;
		if (t > sbi->s_clusters_per_group)
			return -EINVAL;
		*((unsigned int *) ptr) = t;
		return len;
	case attr_pointer_ul:
		ret = kstrtoul(skip_spaces(buf), 0, &lt);
		if (ret)
			return ret;
		*((unsigned long *) ptr) = lt;
		return len;
	}
	return 0;
}

static ssize_t jrnl_comp_ext4_attr_store(struct kobject *kobj,
			       struct attribute *attr,
			       const char *buf, size_t len)
{
	struct jrnl_comp_ext4_sb_info *sbi = container_of(kobj, struct jrnl_comp_ext4_sb_info,
						s_kobj);
	struct jrnl_comp_ext4_attr *a = container_of(attr, struct jrnl_comp_ext4_attr, attr);

	switch (a->attr_id) {
	case attr_jrnl_comp_cjfs:
		return jrnl_comp_cjfs_attr_access(sbi, attr->name, NULL, buf, len);
	case attr_reserved_clusters:
		return reserved_clusters_store(sbi, buf, len);
	case attr_inode_readahead:
		return inode_readahead_blks_store(sbi, buf, len);
	case attr_trigger_test_error:
		return trigger_test_error(sbi, buf, len);
	case attr_err_report_sec:
		return err_report_sec_store(sbi, buf, len);
	default:
		return jrnl_comp_ext4_generic_attr_store(a, sbi, buf, len);
	}
}

static void jrnl_comp_ext4_sb_release(struct kobject *kobj)
{
	struct jrnl_comp_ext4_sb_info *sbi = container_of(kobj, struct jrnl_comp_ext4_sb_info,
						s_kobj);
	complete(&sbi->s_kobj_unregister);
}

static void jrnl_comp_ext4_feat_release(struct kobject *kobj)
{
	kfree(kobj);
}

static const struct sysfs_ops jrnl_comp_ext4_attr_ops = {
	.show	= jrnl_comp_ext4_attr_show,
	.store	= jrnl_comp_ext4_attr_store,
};

static const struct kobj_type jrnl_comp_ext4_sb_ktype = {
	.default_groups = jrnl_comp_ext4_groups,
	.sysfs_ops	= &jrnl_comp_ext4_attr_ops,
	.release	= jrnl_comp_ext4_sb_release,
};

static const struct kobj_type jrnl_comp_ext4_feat_ktype = {
	.default_groups = jrnl_comp_ext4_feat_groups,
	.sysfs_ops	= &jrnl_comp_ext4_attr_ops,
	.release	= jrnl_comp_ext4_feat_release,
};

void jrnl_comp_ext4_notify_error_sysfs(struct jrnl_comp_ext4_sb_info *sbi)
{
	mutex_lock(&sbi->s_error_notify_mutex);
	if (sbi->s_kobj.state_in_sysfs)
		sysfs_notify(&sbi->s_kobj, NULL, "errors_count");
	mutex_unlock(&sbi->s_error_notify_mutex);
}

static struct kobject *jrnl_comp_ext4_root;

static struct kobject *jrnl_comp_ext4_feat;

int jrnl_comp_ext4_register_sysfs(struct super_block *sb)
{
	struct jrnl_comp_ext4_sb_info *sbi = JRNL_COMP_EXT4_SB(sb);
	int err;

	init_completion(&sbi->s_kobj_unregister);
	mutex_lock(&sbi->s_error_notify_mutex);
	err = kobject_init_and_add(&sbi->s_kobj, &jrnl_comp_ext4_sb_ktype, jrnl_comp_ext4_root,
				   "%s", sb->s_id);
	mutex_unlock(&sbi->s_error_notify_mutex);
	if (err) {
		kobject_put(&sbi->s_kobj);
		wait_for_completion(&sbi->s_kobj_unregister);
		return err;
	}

 if (sbi->s_journal) {
  err = sysfs_create_group(&sbi->s_kobj, &jrnl_comp_cjfs_group);
  if (err) {
   kobject_put(&sbi->s_kobj);
   wait_for_completion(&sbi->s_kobj_unregister);
   return err;
  }
 }

	if (jrnl_comp_ext4_proc_root)
		sbi->s_proc = proc_mkdir(sb->s_id, jrnl_comp_ext4_proc_root);
	if (sbi->s_proc) {
		proc_create_single_data("options", S_IRUGO, sbi->s_proc,
				jrnl_comp_ext4_seq_options_show, sb);
		proc_create_single_data("es_shrinker_info", S_IRUGO,
				sbi->s_proc, jrnl_comp_ext4_seq_es_shrinker_info_show,
				sb);
		proc_create_single_data("fc_info", 0444, sbi->s_proc,
					jrnl_comp_ext4_fc_info_show, sb);
		proc_create_seq_data("mb_groups", S_IRUGO, sbi->s_proc,
				&jrnl_comp_ext4_mb_seq_groups_ops, sb);
		proc_create_single_data("mb_stats", 0444, sbi->s_proc,
				jrnl_comp_ext4_seq_mb_stats_show, sb);
		proc_create_seq_data("mb_structs_summary", 0444, sbi->s_proc,
				&jrnl_comp_ext4_mb_seq_structs_summary_ops, sb);
	}
	return 0;
}

void jrnl_comp_ext4_unregister_sysfs(struct super_block *sb)
{
	struct jrnl_comp_ext4_sb_info *sbi = JRNL_COMP_EXT4_SB(sb);

	if (sbi->s_proc)
		remove_proc_subtree(sb->s_id, jrnl_comp_ext4_proc_root);

	mutex_lock(&sbi->s_error_notify_mutex);
	if (sbi->s_journal)
		sysfs_remove_group(&sbi->s_kobj, &jrnl_comp_cjfs_group);
	kobject_del(&sbi->s_kobj);
	mutex_unlock(&sbi->s_error_notify_mutex);
}

int __init jrnl_comp_ext4_init_sysfs(void)
{
	int ret;

	jrnl_comp_ext4_root = kobject_create_and_add("jrnl-comp-ext4", fs_kobj);
	if (!jrnl_comp_ext4_root)
		return -ENOMEM;

	jrnl_comp_ext4_feat = kzalloc_obj(*jrnl_comp_ext4_feat);
	if (!jrnl_comp_ext4_feat) {
		ret = -ENOMEM;
		goto root_err;
	}

	ret = kobject_init_and_add(jrnl_comp_ext4_feat, &jrnl_comp_ext4_feat_ktype,
				   jrnl_comp_ext4_root, "features");
	if (ret)
		goto feat_err;

	jrnl_comp_ext4_proc_root = proc_mkdir(proc_dirname, NULL);
	return ret;

feat_err:
	kobject_put(jrnl_comp_ext4_feat);
	jrnl_comp_ext4_feat = NULL;
root_err:
	kobject_put(jrnl_comp_ext4_root);
	jrnl_comp_ext4_root = NULL;
	return ret;
}

void jrnl_comp_ext4_exit_sysfs(void)
{
	kobject_put(jrnl_comp_ext4_feat);
	jrnl_comp_ext4_feat = NULL;
	kobject_put(jrnl_comp_ext4_root);
	jrnl_comp_ext4_root = NULL;
	remove_proc_entry(proc_dirname, NULL);
	jrnl_comp_ext4_proc_root = NULL;
}
