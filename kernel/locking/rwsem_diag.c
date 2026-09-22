// SPDX-License-Identifier: GPL-2.0
/* Temporary, single-directory writer-handoff experiment. */
#include <linux/capability.h>
#include <linux/debugfs.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kdev_t.h>
#include <linux/mutex.h>
#include <linux/rcupdate.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "rwsem_diag.h"
#define CREATE_TRACE_POINTS
#include <trace/events/rwsem_diag.h>

struct rwsem_diag_config {
	struct file *file;
	struct rw_semaphore *sem;
	unsigned int ticks;
	unsigned long generation;
	atomic64_t deadlines;
	atomic64_t handoffs;
};
static DEFINE_MUTEX(rwsem_diag_mutex);
static struct rwsem_diag_config __rcu *rwsem_diag_config;
static unsigned long rwsem_diag_generation;

unsigned long rwsem_diag_deadline(struct rw_semaphore *sem,
				 unsigned long default_ticks)
{
	struct rwsem_diag_config *cfg;
	unsigned long deadline;

	rcu_read_lock();
	cfg = rcu_dereference(rwsem_diag_config);
	if (cfg && cfg->sem == sem) {
		deadline = jiffies + cfg->ticks;
		atomic64_inc(&cfg->deadlines);
		trace_rwsem_diag_writer(sem, 0, cfg->ticks, cfg->generation,
					deadline);
	} else {
		deadline = jiffies + default_ticks;
	}
	rcu_read_unlock();
	return deadline;
}

void rwsem_diag_handoff(struct rw_semaphore *sem, unsigned long deadline)
{
	struct rwsem_diag_config *cfg;

	rcu_read_lock();
	cfg = rcu_dereference(rwsem_diag_config);
	if (cfg && cfg->sem == sem) {
		atomic64_inc(&cfg->handoffs);
		trace_rwsem_diag_writer(sem, 1, cfg->ticks, cfg->generation,
					deadline);
	}
	rcu_read_unlock();
}

static int rwsem_diag_show(struct seq_file *m, void *unused)
{
	struct rwsem_diag_config *cfg;
	struct inode *inode;

	mutex_lock(&rwsem_diag_mutex);
	cfg = rcu_dereference_protected(rwsem_diag_config,
				       lockdep_is_held(&rwsem_diag_mutex));
	seq_printf(m, "hz=%u default_ticks=%u\n", HZ, DIV_ROUND_UP(HZ, 250));
	if (!cfg) {
		seq_puts(m, "registered=0\n");
	} else {
		inode = file_inode(cfg->file);
		seq_printf(m, "registered=1 dev=%u:%u ino=%llu ticks=%u generation=%lu deadlines=%lld handoffs=%lld\n",
			MAJOR(inode->i_sb->s_dev), MINOR(inode->i_sb->s_dev),
			inode->i_ino, cfg->ticks, cfg->generation,
			atomic64_read(&cfg->deadlines),
			atomic64_read(&cfg->handoffs));
	}
	mutex_unlock(&rwsem_diag_mutex);
	return 0;
}

static int rwsem_diag_open(struct inode *inode, struct file *file)
{
	return single_open(file, rwsem_diag_show, NULL);
}

static ssize_t rwsem_diag_write(struct file *file, const char __user *buf,
			       size_t len, loff_t *pos)
{
	struct rwsem_diag_config *next = NULL, *old;
	char command[64], extra;
	unsigned int fd, ticks;
	int err = 0;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	if (!len || len >= sizeof(command))
		return -EINVAL;
	if (copy_from_user(command, buf, len))
		return -EFAULT;
	/* Reject embedded NULs rather than silently ignoring a suffix. */
	if (memchr(command, '\0', len))
		return -EINVAL;
	command[len] = '\0';
	if (sysfs_streq(command, "unregister"))
		goto replace;
	if (sscanf(command, "register %u %u %c", &fd, &ticks, &extra) != 2 ||
	    fd > INT_MAX || (ticks != 1 && ticks != 4 && ticks != 8))
		return -EINVAL;
	next = kzalloc_obj(*next);
	if (!next)
		return -ENOMEM;
	next->file = fget(fd);
	if (!next->file) {
		err = -EBADF;
		goto free_next;
	}
	if (!S_ISDIR(file_inode(next->file)->i_mode)) {
		err = -ENOTDIR;
		goto put_next;
	}
	next->sem = &file_inode(next->file)->i_rwsem;
	next->ticks = ticks;
	atomic64_set(&next->deadlines, 0);
	atomic64_set(&next->handoffs, 0);
replace:
	/* The experiment must stop workers before replacing this configuration.
	 * RCU/file references protect lifetime even if that rule is violated;
	 * already assigned waiter deadlines are deliberately never rewritten.
	 */
	mutex_lock(&rwsem_diag_mutex);
	if (next)
		next->generation = ++rwsem_diag_generation;
	old = rcu_replace_pointer(rwsem_diag_config, next,
				  lockdep_is_held(&rwsem_diag_mutex));
	synchronize_rcu();
	if (old) {
		fput(old->file);
		kfree(old);
	}
	mutex_unlock(&rwsem_diag_mutex);
	return len;
put_next:
	fput(next->file);
free_next:
	kfree(next);
	return err;
}

static const struct file_operations rwsem_diag_fops = {
	.open = rwsem_diag_open,
	.read = seq_read,
	.write = rwsem_diag_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init rwsem_diag_init(void)
{
	struct dentry *dir;

	dir = debugfs_create_dir("rwsem_handoff_diag", NULL);
	if (IS_ERR(dir))
		return PTR_ERR(dir);
	debugfs_create_file("control", 0600, dir, NULL, &rwsem_diag_fops);
	return 0;
}
late_initcall(rwsem_diag_init);
