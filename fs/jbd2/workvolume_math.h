/* SPDX-License-Identifier: GPL-2.0 */
/* Pure encoding arithmetic, shared with the userspace validation test. */
#ifndef _JBD2_WORKVOLUME_MATH_H
#define _JBD2_WORKVOLUME_MATH_H
static inline unsigned long long jbd2_wv_changed(const unsigned char *old,
 const unsigned char *image, unsigned int size, unsigned int gran, int escaped)
{
 unsigned int pos, k, len;
 unsigned long long changed = 0;
 /* JBD2 magic bytes restored from the escape marker for comparison. */
 const unsigned char magic[4] = { 0xc0, 0x3b, 0x39, 0x98 };
 for (pos = 0; pos < size; pos += gran) {
  len = size - pos < gran ? size - pos : gran;
  for (k = 0; k < len; k++) {
   unsigned int at = pos + k;
   unsigned char byte = escaped && at < 4 ? magic[at] : image[at];
   if (old[at] != byte) { changed += len; break; }
  }
 }
 return changed;
}
static inline unsigned long long jbd2_wv_record(unsigned int size,
 unsigned int gran, unsigned long long changed)
{
 unsigned long long bitmap = ((size + gran - 1) / gran + 7) / 8;
 unsigned long long partial = (24 + bitmap + changed + 7) & ~7ULL;
 unsigned long long full = (24ULL + size + 7) & ~7ULL;
 return changed ? (partial < full ? partial : full) : 0;
}
#endif
