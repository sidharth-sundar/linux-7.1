# SPDX-License-Identifier: GPL-2.0-only
#
# Makefile for the linux journaling routines.
#

obj-$(CONFIG_JBD2) += jbd2.o

jbd2-objs := transaction.o commit.o recovery.o checkpoint.o revoke.o journal.o cjfs.o

ifeq ($(CONFIG_JBD2_INTERVAL_DIAG),y)
jbd2-objs += interval_diag.o
endif
