/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

/**
 * Non-incremental version, quicker for small keys.
 */
uint32_t BLI_hash_mm2(const unsigned char *data, size_t len, uint32_t seed);

#ifdef __cplusplus
}
#endif
