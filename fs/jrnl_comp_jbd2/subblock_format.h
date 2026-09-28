/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Experimental sub-block container v1. Pure byte codec shared with userspace
 * tests. This does not enable a journal feature or authorize partial replay.
 * All integers are big endian; no C struct layout appears on disk.
 */
#ifndef _JRNL_COMP_JBD2_SUBBLOCK_FORMAT_H
#define _JRNL_COMP_JBD2_SUBBLOCK_FORMAT_H

#define JRNL_COMP_JBD2_SB_BLOCK_SIZE 4096U
#define JRNL_COMP_JBD2_SB_HEADER_SIZE 32U
#define JRNL_COMP_JBD2_SB_RECORD_SIZE 12U
#define JRNL_COMP_JBD2_SB_MAGIC 0xc03b3998U
#define JRNL_COMP_JBD2_SB_TYPE 6U
#define JRNL_COMP_JBD2_SB_VERSION 1U
#define JRNL_COMP_JBD2_SB_CSUM_OFFSET 24U

/* Container size is the journal block size, not an on-disk fixed constant.
 * 4096 remains the maximum representable source for the 64-unit bitmap.
 */
static inline int jrnl_comp_jbd2_sb_size_valid(unsigned int size)
{
	return size == 1024 || size == 2048 || size == 4096;
}

static inline unsigned int jrnl_comp_jbd2_sb_get16(const unsigned char *p)
{
	return ((unsigned int)p[0] << 8) | p[1];
}

static inline unsigned int jrnl_comp_jbd2_sb_get32(const unsigned char *p)
{
	return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
	       ((unsigned int)p[2] << 8) | p[3];
}

static inline unsigned long long jrnl_comp_jbd2_sb_get64(const unsigned char *p)
{
	return ((unsigned long long)jrnl_comp_jbd2_sb_get32(p) << 32) |
	       jrnl_comp_jbd2_sb_get32(p + 4);
}

static inline void jrnl_comp_jbd2_sb_put16(unsigned char *p, unsigned int v)
{
	p[0] = v >> 8;
	p[1] = v;
}

static inline void jrnl_comp_jbd2_sb_put32(unsigned char *p, unsigned int v)
{
	p[0] = v >> 24;
	p[1] = v >> 16;
	p[2] = v >> 8;
	p[3] = v;
}

static inline void jrnl_comp_jbd2_sb_put64(unsigned char *p, unsigned long long v)
{
	jrnl_comp_jbd2_sb_put32(p, v >> 32);
	jrnl_comp_jbd2_sb_put32(p + 4, v);
}

static inline int jrnl_comp_jbd2_sb_granularity_valid(unsigned int granularity)
{
	return granularity == 64 || granularity == 128 || granularity == 256;
}

/* Caller provides exactly one writable filesystem block. */
static inline int jrnl_comp_jbd2_sb_init(unsigned char *data, unsigned int size,
			      unsigned int sequence, unsigned int granularity)
{
	unsigned int i;

	if (!jrnl_comp_jbd2_sb_size_valid(size) ||
	    !jrnl_comp_jbd2_sb_granularity_valid(granularity))
		return -1;
	for (i = 0; i < size; i++)
		data[i] = 0;
	jrnl_comp_jbd2_sb_put32(data, JRNL_COMP_JBD2_SB_MAGIC);
	jrnl_comp_jbd2_sb_put32(data + 4, JRNL_COMP_JBD2_SB_TYPE);
	jrnl_comp_jbd2_sb_put32(data + 8, sequence);
	jrnl_comp_jbd2_sb_put16(data + 12, JRNL_COMP_JBD2_SB_VERSION);
	jrnl_comp_jbd2_sb_put16(data + 14, granularity);
	jrnl_comp_jbd2_sb_put16(data + 16, JRNL_COMP_JBD2_SB_HEADER_SIZE);
	return 0;
}

/*
 * Append from a stable, normalized full image. -1: invalid input, -2: no room.
 * No mutation on error. Source bytes are copied, never retained by reference.
 * This builder consumes its own previously initialized/validated container.
 */
static inline int jrnl_comp_jbd2_sb_append(unsigned char *data, unsigned int size,
		unsigned long long block, unsigned int offset, unsigned int length,
		const unsigned char *image)
{
	unsigned int granularity, used, records, i;

	if (!jrnl_comp_jbd2_sb_size_valid(size))
		return -1;
	granularity = jrnl_comp_jbd2_sb_get16(data + 14);
	used = jrnl_comp_jbd2_sb_get16(data + 16);
	records = jrnl_comp_jbd2_sb_get16(data + 18);
	if (!jrnl_comp_jbd2_sb_granularity_valid(granularity) ||
	    used < JRNL_COMP_JBD2_SB_HEADER_SIZE || used > size ||
	    offset >= size || !length || length > size - offset ||
	    offset % granularity || length % granularity)
		return -1;
	if (size - used < JRNL_COMP_JBD2_SB_RECORD_SIZE ||
	    length > size - used - JRNL_COMP_JBD2_SB_RECORD_SIZE)
		return -2;
	jrnl_comp_jbd2_sb_put64(data + used, block);
	jrnl_comp_jbd2_sb_put16(data + used + 8, offset);
	jrnl_comp_jbd2_sb_put16(data + used + 10, length);
	for (i = 0; i < length; i++)
		data[used + JRNL_COMP_JBD2_SB_RECORD_SIZE + i] = image[offset + i];
	jrnl_comp_jbd2_sb_put16(data + 16, used + JRNL_COMP_JBD2_SB_RECORD_SIZE + length);
	jrnl_comp_jbd2_sb_put16(data + 18, records + 1);
	/* Appending invalidates the previous container checksum. */
	jrnl_comp_jbd2_sb_put32(data + JRNL_COMP_JBD2_SB_CSUM_OFFSET, 0);
	return 0;
}

/*
 * Full structural validation before any record is replayed. Checksum validation
 * is deliberately separate and mandatory at the kernel boundary. Target-device
 * bounds, reserved blocks, transaction order, revokes and feature negotiation
 * are also caller responsibilities. Empty containers are not valid log records.
 */
static inline int jrnl_comp_jbd2_sb_validate(const unsigned char *data, unsigned int size,
				 unsigned int sequence)
{
	unsigned int granularity, used, records, pos, i;

	if (!jrnl_comp_jbd2_sb_size_valid(size))
		return -1;
	if (jrnl_comp_jbd2_sb_get32(data) != JRNL_COMP_JBD2_SB_MAGIC ||
	    jrnl_comp_jbd2_sb_get32(data + 4) != JRNL_COMP_JBD2_SB_TYPE ||
	    jrnl_comp_jbd2_sb_get32(data + 8) != sequence ||
	    jrnl_comp_jbd2_sb_get16(data + 12) != JRNL_COMP_JBD2_SB_VERSION ||
	    jrnl_comp_jbd2_sb_get32(data + 20) || jrnl_comp_jbd2_sb_get32(data + 28))
		return -1;
	granularity = jrnl_comp_jbd2_sb_get16(data + 14);
	used = jrnl_comp_jbd2_sb_get16(data + 16);
	records = jrnl_comp_jbd2_sb_get16(data + 18);
	if (!jrnl_comp_jbd2_sb_granularity_valid(granularity) ||
	    used < JRNL_COMP_JBD2_SB_HEADER_SIZE || used > size || !records ||
	    records > (used - JRNL_COMP_JBD2_SB_HEADER_SIZE) / JRNL_COMP_JBD2_SB_RECORD_SIZE)
		return -1;
	pos = JRNL_COMP_JBD2_SB_HEADER_SIZE;
	for (i = 0; i < records; i++) {
		unsigned int offset, length;

		if (used - pos < JRNL_COMP_JBD2_SB_RECORD_SIZE)
			return -1;
		offset = jrnl_comp_jbd2_sb_get16(data + pos + 8);
		length = jrnl_comp_jbd2_sb_get16(data + pos + 10);
		pos += JRNL_COMP_JBD2_SB_RECORD_SIZE;
		if (offset >= size || !length || length > size - offset ||
		    offset % granularity || length % granularity ||
		    length > used - pos)
			return -1;
		pos += length;
	}
	if (pos != used)
		return -1;
	/* No unaccounted records or stale memory may follow the record stream. */
	for (i = used; i < size; i++)
		if (data[i])
			return -1;
	return 0;
}

/* Convert byte extents into a 64-byte-unit bitmap, checking overflow first. */
static inline int jrnl_comp_jbd2_sb_mark(unsigned long long *mask, unsigned int offset,
			       unsigned int length)
{
	unsigned int first, last, i;

	if (offset >= JRNL_COMP_JBD2_SB_BLOCK_SIZE || !length ||
	    length > JRNL_COMP_JBD2_SB_BLOCK_SIZE - offset)
		return -1;
	first = offset / 64;
	last = (offset + length - 1) / 64;
	for (i = first; i <= last; i++)
		*mask |= 1ULL << i;
	return 0;
}

/* Expand fine dirty units outward to a configured granularity. */
static inline unsigned long long jrnl_comp_jbd2_sb_expand(unsigned long long mask,
					       unsigned int granularity)
{
	unsigned long long expanded = 0, bits;
	unsigned int i, units;

	if (!jrnl_comp_jbd2_sb_granularity_valid(granularity))
		return 0;
	units = granularity / 64;
	for (i = 0; i < 64; i += units) {
		bits = ((1ULL << units) - 1) << i;
		if (mask & bits)
			expanded |= bits;
	}
	return expanded;
}
#endif
