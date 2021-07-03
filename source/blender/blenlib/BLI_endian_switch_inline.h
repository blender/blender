/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* only include from header */
#ifndef __BLI_ENDIAN_SWITCH_H__
#  error "this file isn't to be directly included"
#endif

/** \file
 * \ingroup bli
 */

/* NOTE: using a temp char to switch endian is a lot slower,
 * use bit shifting instead. */

/* *** 16 *** */
BLI_INLINE void BLI_endian_switch_int16(short *val)
{
  BLI_endian_switch_uint16((unsigned short *)val);
}
BLI_INLINE void BLI_endian_switch_uint16(unsigned short *val)
{
#if (defined(__GNUC__) && ((__GNUC__ * 100 + __GNUC_MINOR__) >= 408)) /* gcc4.8+ only */
  *val = __builtin_bswap16(*val);
#else
  unsigned short tval = *val;
  *val = (tval >> 8) | (tval << 8);
#endif
}

/* *** 32 *** */
BLI_INLINE void BLI_endian_switch_int32(int *val)
{
  BLI_endian_switch_uint32((unsigned int *)val);
}
BLI_INLINE void BLI_endian_switch_uint32(unsigned int *val)
{
#ifdef __GNUC__
  *val = __builtin_bswap32(*val);
#else
  unsigned int tval = *val;
  *val = ((tval >> 24)) | ((tval << 8) & 0x00ff0000) | ((tval >> 8) & 0x0000ff00) | ((tval << 24));
#endif
}
BLI_INLINE void BLI_endian_switch_float(float *val)
{
  BLI_endian_switch_uint32((unsigned int *)val);
}

/* *** 64 *** */
BLI_INLINE void BLI_endian_switch_int64(int64_t *val)
{
  BLI_endian_switch_uint64((uint64_t *)val);
}
BLI_INLINE void BLI_endian_switch_uint64(uint64_t *val)
{
#ifdef __GNUC__
  *val = __builtin_bswap64(*val);
#else
  uint64_t tval = *val;
  *val = ((tval >> 56)) | ((tval << 40) & 0x00ff000000000000ll) |
         ((tval << 24) & 0x0000ff0000000000ll) | ((tval << 8) & 0x000000ff00000000ll) |
         ((tval >> 8) & 0x00000000ff000000ll) | ((tval >> 24) & 0x0000000000ff0000ll) |
         ((tval >> 40) & 0x000000000000ff00ll) | ((tval << 56));
#endif
}
BLI_INLINE void BLI_endian_switch_double(double *val)
{
  BLI_endian_switch_uint64((uint64_t *)val);
}

#ifdef __cplusplus
}
#endif
