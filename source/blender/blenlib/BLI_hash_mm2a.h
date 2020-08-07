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

/** \file
 * \ingroup bli
 */

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BLI_HashMurmur2A {
  uint32_t hash;
  uint32_t tail;
  uint32_t count;
  uint32_t size;
} BLI_HashMurmur2A;

void BLI_hash_mm2a_init(BLI_HashMurmur2A *mm2, uint32_t seed);

void BLI_hash_mm2a_add(BLI_HashMurmur2A *mm2, const unsigned char *data, size_t len);

void BLI_hash_mm2a_add_int(BLI_HashMurmur2A *mm2, int data);

uint32_t BLI_hash_mm2a_end(BLI_HashMurmur2A *mm2);

uint32_t BLI_hash_mm2(const unsigned char *data, size_t len, uint32_t seed);

#ifdef __cplusplus
}
#endif
