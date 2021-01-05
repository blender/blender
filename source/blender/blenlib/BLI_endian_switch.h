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

/* Use a define instead of `#pragma once` because of `BLI_endian_switch_inline.h` */
#ifndef __BLI_ENDIAN_SWITCH_H__
#define __BLI_ENDIAN_SWITCH_H__

/** \file
 * \ingroup bli
 */

#include "BLI_compiler_attrs.h"
#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

/* BLI_endian_switch_inline.h */
BLI_INLINE void BLI_endian_switch_int16(short *val) ATTR_NONNULL(1);
BLI_INLINE void BLI_endian_switch_uint16(unsigned short *val) ATTR_NONNULL(1);
BLI_INLINE void BLI_endian_switch_int32(int *val) ATTR_NONNULL(1);
BLI_INLINE void BLI_endian_switch_uint32(unsigned int *val) ATTR_NONNULL(1);
BLI_INLINE void BLI_endian_switch_float(float *val) ATTR_NONNULL(1);
BLI_INLINE void BLI_endian_switch_int64(int64_t *val) ATTR_NONNULL(1);
BLI_INLINE void BLI_endian_switch_uint64(uint64_t *val) ATTR_NONNULL(1);
BLI_INLINE void BLI_endian_switch_double(double *val) ATTR_NONNULL(1);

/* endian_switch.c */
void BLI_endian_switch_int16_array(short *val, const int size) ATTR_NONNULL(1);
void BLI_endian_switch_uint16_array(unsigned short *val, const int size) ATTR_NONNULL(1);
void BLI_endian_switch_int32_array(int *val, const int size) ATTR_NONNULL(1);
void BLI_endian_switch_uint32_array(unsigned int *val, const int size) ATTR_NONNULL(1);
void BLI_endian_switch_float_array(float *val, const int size) ATTR_NONNULL(1);
void BLI_endian_switch_int64_array(int64_t *val, const int size) ATTR_NONNULL(1);
void BLI_endian_switch_uint64_array(uint64_t *val, const int size) ATTR_NONNULL(1);
void BLI_endian_switch_double_array(double *val, const int size) ATTR_NONNULL(1);

#ifdef __cplusplus
}
#endif

#include "BLI_endian_switch_inline.h"

#endif /* __BLI_ENDIAN_SWITCH_H__ */
