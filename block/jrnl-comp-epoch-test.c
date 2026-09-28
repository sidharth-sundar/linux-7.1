// SPDX-License-Identifier: GPL-2.0
/* State-only tests: no devices, queue allocations, or experimental mounts. */
#include <kunit/test.h>
#include <linux/blkdev.h>

static void epoch_init_test(struct epoch *e, int pending, bool closed)
{
	memset(e, 0, sizeof(*e));
	spin_lock_init(&e->state_lock);
	atomic_set(&e->pending, pending);
	e->barrier = closed;
}

static void ordered_success(struct kunit *test)
{
	struct epoch e;

	epoch_init_test(&e, 2, true);
	KUNIT_EXPECT_FALSE(test, blk_epoch_retire_pending(&e, true));
	KUNIT_EXPECT_TRUE(test, blk_epoch_retire_pending(&e, true));
	KUNIT_EXPECT_EQ(test, atomic_read(&e.pending), 0);
	KUNIT_EXPECT_EQ(test, atomic_read(&e.dispatch), 2);
}

static void failed_before_dispatch(struct kunit *test)
{
	struct epoch e;

	epoch_init_test(&e, 2, true);
	KUNIT_EXPECT_FALSE(test, blk_epoch_retire_pending(&e, false));
	KUNIT_EXPECT_FALSE(test, blk_epoch_retire_pending(&e, true));
	KUNIT_EXPECT_EQ(test, atomic_read(&e.pending), 0);
	KUNIT_EXPECT_EQ(test, atomic_read(&e.dispatch), 1);
	KUNIT_EXPECT_EQ(test, e.error, (unsigned int)EIO);
	KUNIT_EXPECT_EQ(test, e.barrier, 0U);
}

static void failure_is_last(struct kunit *test)
{
	struct epoch e;

	epoch_init_test(&e, 2, true);
	KUNIT_EXPECT_FALSE(test, blk_epoch_retire_pending(&e, true));
	KUNIT_EXPECT_FALSE(test, blk_epoch_retire_pending(&e, false));
	KUNIT_EXPECT_EQ(test, atomic_read(&e.pending), 0);
	KUNIT_EXPECT_EQ(test, e.barrier, 0U);
}

static void unclosed_epoch(struct kunit *test)
{
	struct epoch e;

	epoch_init_test(&e, 1, false);
	KUNIT_EXPECT_FALSE(test, blk_epoch_retire_pending(&e, true));
	KUNIT_EXPECT_EQ(test, atomic_read(&e.pending), 0);
}

static struct kunit_case epoch_cases[] = {
	KUNIT_CASE(ordered_success),
	KUNIT_CASE(failed_before_dispatch),
	KUNIT_CASE(failure_is_last),
	KUNIT_CASE(unclosed_epoch),
	{}
};

static struct kunit_suite epoch_suite = {
	.name = "jrnl-comp-epoch-state",
	.test_cases = epoch_cases,
};
kunit_test_suite(epoch_suite);
MODULE_LICENSE("GPL");
