/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Experimental sub-block container v1. Pure byte codec shared with userspace
 * tests. This does not enable a journal feature or authorize partial replay.
 * All integers are big endian; no C struct layout appears on disk.
 */
#ifndef _JBD2_SUBBLOCK_FORMAT_H
#define _JBD2_SUBBLOCK_FORMAT_H

#define JBD2_SB_BLOCK_SIZE 4096U
#define JBD2_SB_HEADER_SIZE 32U
#define JBD2_SB_RECORD_SIZE 12U
#define JBD2_SB_MAGIC 0xc03b3998U
#define JBD2_SB_TYPE 6U
#define JBD2_SB_VERSION 1U
#define JBD2_SB_CSUM_OFFSET 24U

/* Container size is the journal block size, not an on-disk fixed constant.
 * 4096 remains the maximum representable source for the 64-unit bitmap.
 */
static inline int jbd2_sb_size_valid(unsigned int size)
{
	return size == 1024 || size == 2048 || size == 4096;
}

static inline unsigned int jbd2_sb_get16(const unsigned char *p)
{
	return ((unsigned int)p[0] << 8) | p[1];
}

static inline unsigned int jbd2_sb_get32(const unsigned char *p)
{
	return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
	       ((unsigned int)p[2] << 8) | p[3];
}

static inline unsigned long long jbd2_sb_get64(const unsigned char *p)
{
	return ((unsigned long long)jbd2_sb_get32(p) << 32) |
	       jbd2_sb_get32(p + 4);
}

static inline void jbd2_sb_put16(unsigned char *p, unsigned int v)
{
	p[0] = v >> 8;
	p[1] = v;
}

static inline void jbd2_sb_put32(unsigned char *p, unsigned int v)
{
	p[0] = v >> 24;
	p[1] = v >> 16;
	p[2] = v >> 8;
	p[3] = v;
}

static inline void jbd2_sb_put64(unsigned char *p, unsigned long long v)
{
	jbd2_sb_put32(p, v >> 32);
	jbd2_sb_put32(p + 4, v);
}

static inline int jbd2_sb_granularity_valid(unsigned int granularity)
{
	return granularity == 64 || granularity == 128 || granularity == 256;
}

/* Caller provides exactly one writable filesystem block. */
static inline int jbd2_sb_init(unsigned char *data, unsigned int size,
			      unsigned int sequence, unsigned int granularity)
{
	unsigned int i;

	if (!jbd2_sb_size_valid(size) ||
	    !jbd2_sb_granularity_valid(granularity))
		return -1;
	for (i = 0; i < size; i++)
		data[i] = 0;
	jbd2_sb_put32(data, JBD2_SB_MAGIC);
	jbd2_sb_put32(data + 4, JBD2_SB_TYPE);
	jbd2_sb_put32(data + 8, sequence);
	jbd2_sb_put16(data + 12, JBD2_SB_VERSION);
	jbd2_sb_put16(data + 14, granularity);
	jbd2_sb_put16(data + 16, JBD2_SB_HEADER_SIZE);
	return 0;
}

/*
 * Append from a stable, normalized full image. -1: invalid input, -2: no room.
 * No mutation on error. Source bytes are copied, never retained by reference.
 * This builder consumes its own previously initialized/validated container.
 */
static inline int jbd2_sb_append(unsigned char *data, unsigned int size,
		unsigned long long block, unsigned int offset, unsigned int length,
		const unsigned char *image)
{
	unsigned int granularity, used, records, i;

	if (!jbd2_sb_size_valid(size))
		return -1;
	granularity = jbd2_sb_get16(data + 14);
	used = jbd2_sb_get16(data + 16);
	records = jbd2_sb_get16(data + 18);
	if (!jbd2_sb_granularity_valid(granularity) ||
	    used < JBD2_SB_HEADER_SIZE || used > size ||
	    offset >= size || !length || length > size - offset ||
	    offset % granularity || length % granularity)
		return -1;
	if (size - used < JBD2_SB_RECORD_SIZE ||
	    length > size - used - JBD2_SB_RECORD_SIZE)
		return -2;
	jbd2_sb_put64(data + used, block);
	jbd2_sb_put16(data + used + 8, offset);
	jbd2_sb_put16(data + used + 10, length);
	for (i = 0; i < length; i++)
		data[used + JBD2_SB_RECORD_SIZE + i] = image[offset + i];
	jbd2_sb_put16(data + 16, used + JBD2_SB_RECORD_SIZE + length);
	jbd2_sb_put16(data + 18, records + 1);
	/* Appending invalidates the previous container checksum. */
	jbd2_sb_put32(data + JBD2_SB_CSUM_OFFSET, 0);
	return 0;
}

/*
 * Full structural validation before any record is replayed. Checksum validation
 * is deliberately separate and mandatory at the kernel boundary. Target-device
 * bounds, reserved blocks, transaction order, revokes and feature negotiation
 * are also caller responsibilities. Empty containers are not valid log records.
 */
static inline int jbd2_sb_validate(const unsigned char *data, unsigned int size,
				 unsigned int sequence)
{
	unsigned int granularity, used, records, pos, i;

	if (!jbd2_sb_size_valid(size))
		return -1;
	if (jbd2_sb_get32(data) != JBD2_SB_MAGIC ||
	    jbd2_sb_get32(data + 4) != JBD2_SB_TYPE ||
	    jbd2_sb_get32(data + 8) != sequence ||
	    jbd2_sb_get16(data + 12) != JBD2_SB_VERSION ||
	    jbd2_sb_get32(data + 20) || jbd2_sb_get32(data + 28))
		return -1;
	granularity = jbd2_sb_get16(data + 14);
	used = jbd2_sb_get16(data + 16);
	records = jbd2_sb_get16(data + 18);
	if (!jbd2_sb_granularity_valid(granularity) ||
	    used < JBD2_SB_HEADER_SIZE || used > size || !records ||
	    records > (used - JBD2_SB_HEADER_SIZE) / JBD2_SB_RECORD_SIZE)
		return -1;
	pos = JBD2_SB_HEADER_SIZE;
	for (i = 0; i < records; i++) {
		unsigned int offset, length;

		if (used - pos < JBD2_SB_RECORD_SIZE)
			return -1;
		offset = jbd2_sb_get16(data + pos + 8);
		length = jbd2_sb_get16(data + pos + 10);
		pos += JBD2_SB_RECORD_SIZE;
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
static inline int jbd2_sb_mark(unsigned long long *mask, unsigned int offset,
			       unsigned int length)
{
	unsigned int first, last, i;

	if (offset >= JBD2_SB_BLOCK_SIZE || !length ||
	    length > JBD2_SB_BLOCK_SIZE - offset)
		return -1;
	first = offset / 64;
	last = (offset + length - 1) / 64;
	for (i = first; i <= last; i++)
		*mask |= 1ULL << i;
	return 0;
}

/* Expand fine dirty units outward to a configured granularity. */
static inline unsigned long long jbd2_sb_expand(unsigned long long mask,
					       unsigned int granularity)
{
	unsigned long long expanded = 0, bits;
	unsigned int i, units;

	if (!jbd2_sb_granularity_valid(granularity))
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
