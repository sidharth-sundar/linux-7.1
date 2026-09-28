# SPDX-License-Identifier: GPL-2.0-only
#
# Makefile for the linux journaling routines.
#

obj-$(CONFIG_JRNL_COMP_JBD2) += jrnl_comp_jbd2.o

jrnl_comp_jbd2-objs := transaction.o commit.o recovery.o checkpoint.o revoke.o journal.o jrnl_comp_cjfs.o workvolume.o subblock.o

ifeq ($(CONFIG_JRNL_COMP_JBD2_INTERVAL_DIAG),y)
jrnl_comp_jbd2-objs += interval_diag.o
endif
