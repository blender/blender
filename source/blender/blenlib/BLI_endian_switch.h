/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_ENDIAN_SWITCH_H__
#define __BLI_ENDIAN_SWITCH_H__

/** \file BLI_endian_switch.h
 *  \ingroup bli
 */

#include "BLI_endian_switch_inline.h"

/* endian_switch.c */
void BLI_endian_switch_int16_array(short *val, const int size);
void BLI_endian_switch_uint16_array(unsigned short *val, const int size);
void BLI_endian_switch_int32_array(int *val, const int size);
void BLI_endian_switch_uint32_array(unsigned int *val, const int size);
void BLI_endian_switch_float_array(float *val, const int size);
void BLI_endian_switch_int64_array(int64_t *val, const int size);
void BLI_endian_switch_uint64_array(uint64_t *val, const int size);
void BLI_endian_switch_double_array(double *val, const int size);

#endif  /* __BLI_ENDIAN_SWITCH_H__ */
