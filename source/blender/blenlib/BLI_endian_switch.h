/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

void BLI_endian_switch_int16_array(short *val, int size) ATTR_NONNULL(1);
void BLI_endian_switch_uint16_array(unsigned short *val, int size) ATTR_NONNULL(1);
void BLI_endian_switch_int32_array(int *val, int size) ATTR_NONNULL(1);
void BLI_endian_switch_uint32_array(unsigned int *val, int size) ATTR_NONNULL(1);
void BLI_endian_switch_float_array(float *val, int size) ATTR_NONNULL(1);
void BLI_endian_switch_int64_array(int64_t *val, int size) ATTR_NONNULL(1);
void BLI_endian_switch_uint64_array(uint64_t *val, int size) ATTR_NONNULL(1);
void BLI_endian_switch_double_array(double *val, int size) ATTR_NONNULL(1);

#ifdef __cplusplus
}
#endif

#include "BLI_endian_switch_inline.h"

#endif /* __BLI_ENDIAN_SWITCH_H__ */
