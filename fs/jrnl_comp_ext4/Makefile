# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the linux jrnl_comp_ext4-filesystem routines.
#

obj-$(CONFIG_JRNL_COMP_EXT4_FS) += jrnl_comp_ext4.o

jrnl_comp_ext4-y	:= balloc.o bitmap.o block_validity.o dir.o jrnl_comp_ext4_jrnl_comp_jbd2.o extents.o \
		extents_status.o file.o fsmap.o fsync.o hash.o ialloc.o \
		indirect.o inline.o inode.o ioctl.o mballoc.o migrate.o \
		mmp.o move_extent.o namei.o page-io.o readpage.o resize.o \
		super.o symlink.o sysfs.o xattr.o xattr_hurd.o xattr_trusted.o \
		xattr_user.o fast_commit.o orphan.o

jrnl_comp_ext4-$(CONFIG_JRNL_COMP_EXT4_FS_POSIX_ACL)	+= acl.o
jrnl_comp_ext4-$(CONFIG_JRNL_COMP_EXT4_FS_SECURITY)		+= xattr_security.o
jrnl_comp_ext4-test-objs				+= inode-test.o mballoc-test.o \
					   extents-test.o
obj-$(CONFIG_JRNL_COMP_EXT4_KUNIT_TESTS)		+= jrnl_comp_ext4-test.o
jrnl_comp_ext4-$(CONFIG_FS_VERITY)		+= verity.o
jrnl_comp_ext4-$(CONFIG_FS_ENCRYPTION)		+= crypto.o
