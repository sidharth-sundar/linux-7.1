// SPDX-License-Identifier: GPL-2.0
/* Observational work accounting. Never changes the journal's on-disk format. */
#include <linux/jrnl_comp_jbd2_workvolume.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/hash.h>
#include <linux/highmem.h>
#include <linux/sort.h>
#include <linux/vmalloc.h>
#include <trace/events/block.h>
#include <trace/events/jrnl_comp_cjfs.h>
#include "workvolume_math.h"

#define WV_BASIC 1
#define WV_RANGES 2
#define WV_DEFAULT_MEMORY (8U << 20)
#define WV_MAX_MEMORY (64U << 20)
#define WV_METRICS(X) \
 X(write_bios) X(write_bytes) X(journal_bytes) X(nonjournal_unclassified_bytes) \
 X(preflush_bios) X(fua_bios) X(discard_bytes) X(write_zeroes_bytes) \
 X(whole_device_completion_errors) X(transactions_submitted) X(transactions_durable) \
 X(submitted_handles) X(lazy_copy_bytes) X(eager_copy_bytes) X(escape_copy_bytes) \
 X(flush_calls) X(metadata_images) X(metadata_image_bytes) \
 X(descriptor_blocks_staged) X(revoke_blocks_staged) X(commit_blocks_staged) \
 X(range_sampled_images) X(range_compared_images) X(range_compared_bytes) \
 X(range_first_images) X(range_collision_images) X(range_invalidated_images) \
 X(range_old_images) X(range_unsampled_images) X(range_invalidations) \
 X(range_unchanged_images) \
 X(range_changed_bytes_64) X(range_changed_bytes_128) X(range_changed_bytes_256) \
 X(range_record_bytes_64) X(range_record_bytes_128) X(range_record_bytes_256) \
 X(range_packed_bytes_64) X(range_packed_bytes_128) X(range_packed_bytes_256) \
 X(range_full_packed_bytes) X(range_transactions) X(range_unfinished_transactions)
#define WV_ENUM(n) WV_##n,
enum wv_metric { WV_METRICS(WV_ENUM) WV_COUNT };
#define WV_NAME(n) #n,
static const char * const wv_names[] = { WV_METRICS(WV_NAME) };
struct wv_extent { sector_t start, end; };
struct wv_slot {
 u64 block, epoch;
 jrnl_comp_tid_t tid;
 bool valid;
 u8 image[];
};
struct jrnl_comp_jbd2_workvolume {
 jrnl_comp_journal_t *journal;
 bool active, probes;
 unsigned int pending_mask, mask, pending_sample, sample;
 unsigned int pending_memory, memory, phase;
 u64 session, start_ns, stop_ns, drain_ns;
 atomic64_t counters[2][WV_COUNT];
 spinlock_t lock; /* Range histories only. Never acquires journal locks. */
 void *slots;
 unsigned int nr_slots, stride;
 u64 epoch;
 jrnl_comp_tid_t invalid_tid, range_tid;
 bool invalid_tid_valid, range_pending;
 unsigned int range_phase;
 u64 record_bytes[3], full_record_bytes;
 struct wv_extent *extents;
 unsigned int nr_extents;
 struct block_device *whole;
 sector_t start, end;
};
static void wv_add(struct jrnl_comp_jbd2_workvolume *w, unsigned int phase,
                  enum wv_metric metric, u64 value)
{
 atomic64_add(value, &w->counters[phase][metric]);
}
static unsigned int wv_phase(struct jrnl_comp_jbd2_workvolume *w)
{
 return READ_ONCE(w->phase);
}
static void __maybe_unused wv_bio(void *data, struct bio *bio)
{
 struct jrnl_comp_jbd2_workvolume *w = data;
 sector_t start, end, lo, hi;
 u64 bytes, journal = 0;
 unsigned int i, phase;
 if (!READ_ONCE(w->active) || bdev_whole(bio->bi_bdev) != w->whole)
  return;
 start = bio->bi_iter.bi_sector;
 end = start + bio_sectors(bio);
 lo = max(start, w->start);
 hi = min(end, w->end);
 if (hi <= lo) {
  /* Empty flushes cannot be attributed to one partition. */
  return;
 }
 phase = wv_phase(w);
 bytes = (u64)(hi - lo) << SECTOR_SHIFT;
 if (bio_op(bio) == REQ_OP_DISCARD) {
  wv_add(w, phase, WV_discard_bytes, bytes);
  return;
 }
 if (bio_op(bio) == REQ_OP_WRITE_ZEROES) {
  wv_add(w, phase, WV_write_zeroes_bytes, bytes);
  return;
 }
 if (bio_op(bio) != REQ_OP_WRITE)
  return;
 for (i = 0; i < w->nr_extents; i++) {
  sector_t a, b;
  if (w->extents[i].start >= hi)
   break;
  a = max(lo, w->extents[i].start);
  b = min(hi, w->extents[i].end);
  if (b > a)
   journal += (u64)(b - a) << SECTOR_SHIFT;
 }
 wv_add(w, phase, WV_write_bios, 1);
 wv_add(w, phase, WV_write_bytes, bytes);
 wv_add(w, phase, WV_journal_bytes, journal);
 wv_add(w, phase, WV_nonjournal_unclassified_bytes, bytes - journal);
 wv_add(w, phase, WV_preflush_bios, !!(bio->bi_opf & REQ_PREFLUSH));
 wv_add(w, phase, WV_fua_bios, !!(bio->bi_opf & REQ_FUA));
}
static void __maybe_unused wv_complete(void *data, struct request_queue *q, struct bio *bio)
{
 struct jrnl_comp_jbd2_workvolume *w = data;
 /* Error notification only, not a submitted/completed byte reconciliation. */
 if (READ_ONCE(w->active) && bdev_whole(bio->bi_bdev) == w->whole && bio->bi_status)
  wv_add(w, wv_phase(w), WV_whole_device_completion_errors, 1);
}
static void wv_finish_ranges(struct jrnl_comp_jbd2_workvolume *w, jrnl_comp_tid_t tid)
{
 unsigned long flags;
 unsigned int i, phase, block = w->journal->j_blocksize;
 spin_lock_irqsave(&w->lock, flags);
 if (!w->range_pending)
  goto out;
 phase = w->range_phase;
 if (w->range_tid != tid) {
  wv_add(w, phase, WV_range_unfinished_transactions, 1);
 } else {
  /* Hypothetical format: 32-byte transaction header, 24-byte record
   * header, bitmap for partial records, 8-byte record alignment,
   * packed body rounded to block size and one separate commit block.
   */
  for (i = 0; i < 3; i++)
   wv_add(w, phase, WV_range_packed_bytes_64 + i,
          round_up(w->record_bytes[i] + 32, block) + block);
  wv_add(w, phase, WV_range_full_packed_bytes,
         round_up(w->full_record_bytes + 32, block) + block);
  wv_add(w, phase, WV_range_transactions, 1);
 }
 w->range_pending = false;
 memset(w->record_bytes, 0, sizeof(w->record_bytes));
 w->full_record_bytes = 0;
out:
 spin_unlock_irqrestore(&w->lock, flags);
}
static void __maybe_unused wv_event(void *data, dev_t dev, jrnl_comp_tid_t tid, u64 generation,
                     const char *event, u64 value)
{
 struct jrnl_comp_jbd2_workvolume *w = data;
 enum wv_metric metric;
 unsigned int phase;
 if (!READ_ONCE(w->active) || dev != w->journal->j_fs_dev->bd_dev)
  return;
 phase = wv_phase(w);
 if (!strcmp(event, "submitted_handles")) {
  wv_add(w, phase, WV_transactions_submitted, 1);
  wv_add(w, phase, WV_submitted_handles, value);
  if (w->mask & WV_RANGES)
   wv_finish_ranges(w, tid);
  return;
 }
 if (!strcmp(event, "durable")) { metric = WV_transactions_durable; value = 1; }
 else if (!strcmp(event, "lazy_copy")) metric = WV_lazy_copy_bytes;
 else if (!strcmp(event, "eager_copy")) metric = WV_eager_copy_bytes;
 else if (!strcmp(event, "escape_copy")) metric = WV_escape_copy_bytes;
 else if (!strcmp(event, "flush_call")) metric = WV_flush_calls;
 else return;
 wv_add(w, phase, metric, value);
}
void jrnl_comp_jbd2_wv_control(jrnl_comp_transaction_t *t, int type)
{
 struct jrnl_comp_jbd2_workvolume *w = smp_load_acquire(&t->t_journal->j_workvolume);
 enum wv_metric metric;
 if (!w || !READ_ONCE(w->active) || !(w->mask & WV_BASIC))
  return;
 switch (type) {
 case JRNL_COMP_JBD2_DESCRIPTOR_BLOCK: metric = WV_descriptor_blocks_staged; break;
 case JRNL_COMP_JBD2_REVOKE_BLOCK: metric = WV_revoke_blocks_staged; break;
 case JRNL_COMP_JBD2_COMMIT_BLOCK: metric = WV_commit_blocks_staged; break;
 default: return;
 }
 wv_add(w, wv_phase(w), metric, 1);
}
void jrnl_comp_jbd2_wv_invalidate(jrnl_comp_journal_t *j, jrnl_comp_tid_t tid)
{
 struct jrnl_comp_jbd2_workvolume *w = smp_load_acquire(&j->j_workvolume);
 unsigned long flags;
 if (!w || !READ_ONCE(w->active) || !(w->mask & WV_RANGES))
  return;
 spin_lock_irqsave(&w->lock, flags);
 if (w->active) {
  /* Conservative: invalidate all histories on any create-access, forget
   * or revoke. Old committing images cannot reseed a newer incarnation.
   */
  w->epoch++;
  if (!w->invalid_tid_valid || tid_gt(tid, w->invalid_tid))
   w->invalid_tid = tid;
  w->invalid_tid_valid = true;
  wv_add(w, wv_phase(w), WV_range_invalidations, 1);
 }
 spin_unlock_irqrestore(&w->lock, flags);
}
void jrnl_comp_jbd2_wv_image(jrnl_comp_transaction_t *t, struct buffer_head *bh,
                   struct folio *folio, unsigned int offset, bool escaped)
{
 struct jrnl_comp_jbd2_workvolume *w = smp_load_acquire(&t->t_journal->j_workvolume);
 struct wv_slot *slot;
 unsigned long flags;
 unsigned int i, phase, size = bh->b_size;
 const u8 *image;
 u64 changed[3] = {0}, records[3], hash, full;
 bool compare = false;
 if (!w || !READ_ONCE(w->active) || !(w->mask & WV_BASIC))
  return;
 phase = wv_phase(w);
 wv_add(w, phase, WV_metadata_images, 1);
 wv_add(w, phase, WV_metadata_image_bytes, size);
 if (!(w->mask & WV_RANGES))
  return;
 spin_lock_irqsave(&w->lock, flags);
 if (!w->active)
  goto unlock;
 full = round_up((u64)size + 24, 8);
 for (i = 0; i < 3; i++)
  records[i] = full;
 hash = hash_64(bh->b_blocknr, 32);
 if (size != w->journal->j_blocksize || hash % w->sample) {
  wv_add(w, phase, WV_range_unsampled_images, 1);
  goto account;
 }
 wv_add(w, phase, WV_range_sampled_images, 1);
 slot = w->slots + ((hash / w->sample) % w->nr_slots) * w->stride;
 if (w->invalid_tid_valid && tid_gt(w->invalid_tid, t->t_tid)) {
  wv_add(w, phase, WV_range_old_images, 1);
  goto account;
 }
 if (!slot->valid)
  wv_add(w, phase, WV_range_first_images, 1);
 else if (slot->block != bh->b_blocknr)
  wv_add(w, phase, WV_range_collision_images, 1);
 else if (slot->epoch != w->epoch)
  wv_add(w, phase, WV_range_invalidated_images, 1);
 else if (!tid_gt(t->t_tid, slot->tid)) {
  wv_add(w, phase, WV_range_old_images, 1);
  goto account;
 } else
  compare = true;
 image = kmap_local_folio(folio, offset);
 if (compare) {
  for (i = 0; i < 3; i++)
   changed[i] = jrnl_comp_jbd2_wv_changed(slot->image, image, size, 64U << i, escaped);
  wv_add(w, phase, WV_range_compared_images, 1);
  wv_add(w, phase, WV_range_compared_bytes, size);
  if (!changed[0])
   wv_add(w, phase, WV_range_unchanged_images, 1);
  for (i = 0; i < 3; i++) {
   wv_add(w, phase, WV_range_changed_bytes_64 + i, changed[i]);
   /* Zero-change records omitted only in this hypothetical encoder. */
   records[i] = jrnl_comp_jbd2_wv_record(size, 64U << i, changed[i]);
  }
 }
 memcpy(slot->image, image, size);
 if (escaped)
  *(__be32 *)slot->image = cpu_to_be32(JRNL_COMP_JBD2_MAGIC_NUMBER);
 kunmap_local(image);
 slot->block = bh->b_blocknr;
 slot->epoch = w->epoch;
 slot->tid = t->t_tid;
 slot->valid = true;
account:
 if (w->range_pending && w->range_tid != t->t_tid) {
  wv_add(w, phase, WV_range_unfinished_transactions, 1);
  memset(w->record_bytes, 0, sizeof(w->record_bytes));
  w->full_record_bytes = 0;
 }
 if (!w->range_pending || w->range_tid != t->t_tid)
  w->range_phase = phase;
 w->range_pending = true;
 w->range_tid = t->t_tid;
 w->full_record_bytes += full;
 for (i = 0; i < 3; i++) {
  w->record_bytes[i] += records[i];
  wv_add(w, phase, WV_range_record_bytes_64 + i, records[i]);
 }
unlock:
 spin_unlock_irqrestore(&w->lock, flags);
}
static int wv_extent_cmp(const void *a, const void *b)
{
 const struct wv_extent *x = a, *y = b;
 return (x->start > y->start) - (x->start < y->start);
}
static int wv_map(struct jrnl_comp_jbd2_workvolume *w)
{
 jrnl_comp_journal_t *j = w->journal;
 struct wv_extent *map;
 unsigned int i, n = 0;
 unsigned long long block;
 int err;
 /* Rebuild the complete journal map outside the measurement. */
 map = kvmalloc_array(j->j_total_len, sizeof(*map), GFP_KERNEL);
 if (!map)
  return -ENOMEM;
 for (i = 0; i < j->j_total_len; i++) {
  err = jrnl_comp_jbd2_journal_bmap(j, i, &block);
  if (err)
   goto fail;
  map[i].start = ((sector_t)block * (j->j_blocksize >> 9)) + w->start;
  map[i].end = map[i].start + (j->j_blocksize >> 9);
  if (map[i].start < w->start || map[i].end > w->end) {
   err = -EINVAL;
   goto fail;
  }
 }
 sort(map, j->j_total_len, sizeof(*map), wv_extent_cmp, NULL);
 for (i = 0; i < j->j_total_len; i++) {
  if (n && map[i].start < map[n-1].end) { err = -EINVAL; goto fail; }
  if (n && map[i].start == map[n-1].end)
   map[n-1].end = map[i].end;
  else
   map[n++] = map[i];
 }
 kvfree(w->extents);
 w->extents = map;
 w->nr_extents = n;
 return 0;
fail:
 kvfree(map);
 return err;
}
static void wv_unregister(struct jrnl_comp_jbd2_workvolume *w)
{
 WARN_ON_ONCE(w->probes);
}
static int wv_register(struct jrnl_comp_jbd2_workvolume *w)
{
 /* Stock kernels do not export block_bio_queue to modules. Do not add a
  * shared-core export merely for optional preliminary instrumentation.
  * Reject collection rather than silently report missing I/O as zero.
  */
 return -EOPNOTSUPP;
}
static void wv_barrier_end(jrnl_comp_journal_t *j)
{
 WRITE_ONCE(j->j_jrnl_comp_cjfs_reconfiguring, false);
 jrnl_comp_jbd2_journal_unlock_updates(j);
}
static int wv_barrier_begin(jrnl_comp_journal_t *j)
{
 int err;
 WRITE_ONCE(j->j_jrnl_comp_cjfs_reconfiguring, true);
 wake_up_all(&j->j_wait_done_flush);
 jrnl_comp_jbd2_journal_lock_updates(j);
 err = jrnl_comp_jbd2_journal_flush(j, 0);
 if (err)
  wv_barrier_end(j);
 return err; /* Success leaves handles blocked through reconfiguration. */
}
static ssize_t wv_stats(struct jrnl_comp_jbd2_workvolume *w, char *out, unsigned int phase)
{
 unsigned int i;
 ssize_t len = sysfs_emit(out, "schema 1\n");
 for (i = 0; i < WV_COUNT; i++)
  len += sysfs_emit_at(out, len, "%s %lld\n", wv_names[i],
                       atomic64_read(&w->counters[phase][i]));
 return len;
}
ssize_t jrnl_comp_jbd2_wv_access(jrnl_comp_journal_t *j, struct super_block *sb,
                       const char *name, char *out,
                       const char *in, size_t count)
{
 struct jrnl_comp_jbd2_workvolume *w = j->j_workvolume;
 unsigned int value;
 unsigned long flags;
 int err = 0;
 lockdep_assert_held(&j->j_jrnl_comp_cjfs_config_mutex);
 if (!w) {
  w = kzalloc(sizeof(*w), GFP_KERNEL);
  if (!w)
   return -ENOMEM;
  w->journal = j;
  w->pending_mask = WV_BASIC;
  w->pending_sample = w->sample = 1;
  w->pending_memory = w->memory = WV_DEFAULT_MEMORY;
  spin_lock_init(&w->lock);
  smp_store_release(&j->j_workvolume, w);
 }
 if (!in) {
  if (!strcmp(name, "collect_workload")) return wv_stats(w, out, 0);
  if (!strcmp(name, "collect_drain")) return wv_stats(w, out, 1);
  if (!strcmp(name, "collect_pending_mask")) return sysfs_emit(out, "%u\n", w->pending_mask);
  if (!strcmp(name, "collect_pending_sample_every")) return sysfs_emit(out, "%u\n", w->pending_sample);
  if (!strcmp(name, "collect_pending_memory_bytes")) return sysfs_emit(out, "%u\n", w->pending_memory);
  if (!strcmp(name, "collect_phase")) return sysfs_emit(out, "%u\n", w->phase);
  if (!strcmp(name, "collect_status"))
   return sysfs_emit(out, "schema 1\nsession %llu\nactive %u\nmask %u\nsample_every %u\nmemory_limit_bytes %u\nhistory_bytes %u\nhistory_slots %u\njournal_extents %u\nphase %u\nstart_ns %llu\ndrain_ns %llu\nstop_ns %llu\njournal_aborted %u\n",
    w->session, w->active, w->mask, w->sample, w->memory,
    w->nr_slots * w->stride, w->nr_slots, w->nr_extents, w->phase,
    w->start_ns, w->drain_ns, w->stop_ns, is_journal_aborted(j));
  return -EINVAL;
 }
 if (kstrtouint(in, 10, &value)) return -EINVAL;
 if (!strcmp(name, "collect_pending_mask")) {
  if (value != 0 && value != WV_BASIC && value != (WV_BASIC | WV_RANGES)) return -EINVAL;
  w->pending_mask = value;
 } else if (!strcmp(name, "collect_pending_sample_every")) {
  if (!value || value > 65536) return -EINVAL;
  w->pending_sample = value;
 } else if (!strcmp(name, "collect_pending_memory_bytes")) {
  if (value < (1U << 20) || value > WV_MAX_MEMORY) return -EINVAL;
  w->pending_memory = value;
 } else if (!strcmp(name, "collect_apply")) {
  void *slots = NULL;
  unsigned int stride = ALIGN(sizeof(struct wv_slot) + j->j_blocksize, 8);
  unsigned int nr = w->pending_memory / stride;
  if (value != 1) return -EINVAL;
  if (w->active) return -EBUSY;
  if (w->pending_mask & WV_RANGES) {
   slots = kvcalloc(nr, stride, GFP_KERNEL);
   if (!slots) return -ENOMEM;
  }
  err = wv_barrier_begin(j);
  if (err) { kvfree(slots); return err; }
  kvfree(w->slots);
  w->slots = slots;
  w->stride = stride;
  w->nr_slots = slots ? nr : 0;
  w->mask = w->pending_mask;
  w->sample = w->pending_sample;
  w->memory = w->pending_memory;
  wv_barrier_end(j);
 } else if (!strcmp(name, "collect_start")) {
  if (value != 1) return -EINVAL;
  if (w->active) return -EBUSY;
  err = sync_filesystem(sb);
  if (err) return err;
  err = wv_barrier_begin(j);
  if (err) return err;
  w->whole = bdev_whole(j->j_fs_dev);
  w->start = get_start_sect(j->j_fs_dev);
  w->end = w->start + bdev_nr_sectors(j->j_fs_dev);
  if (w->mask) {
   err = wv_map(w);
   if (err) { wv_barrier_end(j); return err; }
  }
  memset(w->counters, 0, sizeof(w->counters));
  if (w->slots) memset(w->slots, 0, w->nr_slots * w->stride);
  w->range_pending = false;
  w->invalid_tid_valid = false;
  memset(w->record_bytes, 0, sizeof(w->record_bytes));
  w->full_record_bytes = 0;
  w->epoch = 1;
  w->phase = 0;
  w->stop_ns = w->drain_ns = 0;
  if (w->mask) {
   err = wv_register(w);
   if (err) { wv_barrier_end(j); return err; }
  }
  w->session++;
  w->start_ns = ktime_get_ns();
  WRITE_ONCE(w->active, true);
  wv_barrier_end(j);
 } else if (!strcmp(name, "collect_phase")) {
  if (!w->active || value != 1 || w->phase != 0) return -EINVAL;
  w->drain_ns = ktime_get_ns();
  WRITE_ONCE(w->phase, 1);
 } else if (!strcmp(name, "collect_stop")) {
  int drain_err;
  if (value != 1 || !w->active) return -EINVAL;
  if (w->phase != 1) return -EINVAL;
  err = sync_filesystem(sb);
  drain_err = wv_barrier_begin(j);
  spin_lock_irqsave(&w->lock, flags);
  WRITE_ONCE(w->active, false);
  if (w->range_pending) wv_add(w, w->range_phase, WV_range_unfinished_transactions, 1);
  spin_unlock_irqrestore(&w->lock, flags);
  wv_unregister(w);
  w->stop_ns = ktime_get_ns();
  if (!drain_err) wv_barrier_end(j);
  if (err || drain_err) return err ? err : drain_err;
 } else return -EINVAL;
 return count;
}
EXPORT_SYMBOL_GPL(jrnl_comp_jbd2_wv_access);
void jrnl_comp_jbd2_wv_destroy(jrnl_comp_journal_t *j)
{
 struct jrnl_comp_jbd2_workvolume *w = j->j_workvolume;
 if (!w) return;
 WRITE_ONCE(w->active, false);
 wv_unregister(w);
 kvfree(w->slots);
 kvfree(w->extents);
 kfree(w);
 j->j_workvolume = NULL;
}

bool jrnl_comp_jbd2_wv_active(jrnl_comp_journal_t *j)
{
 struct jrnl_comp_jbd2_workvolume *w = smp_load_acquire(&j->j_workvolume);
 return w && READ_ONCE(w->active);
}
