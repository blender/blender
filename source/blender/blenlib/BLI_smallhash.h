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
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_compiler_attrs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uintptr_t key;
  void *val;
} SmallHashEntry;

/* How much stack space to use before dynamically allocating memory.
 * set to match one of the values in 'hashsizes' to avoid too many mallocs. */
#define SMSTACKSIZE 131
typedef struct SmallHash {
  unsigned int nbuckets;
  unsigned int nentries;
  unsigned int cursize;

  SmallHashEntry *buckets;
  SmallHashEntry buckets_stack[SMSTACKSIZE];
} SmallHash;

typedef struct {
  const SmallHash *sh;
  unsigned int i;
} SmallHashIter;

void BLI_smallhash_init_ex(SmallHash *sh, const unsigned int nentries_reserve) ATTR_NONNULL(1);
void BLI_smallhash_init(SmallHash *sh) ATTR_NONNULL(1);
void BLI_smallhash_release(SmallHash *sh) ATTR_NONNULL(1);
void BLI_smallhash_insert(SmallHash *sh, uintptr_t key, void *item) ATTR_NONNULL(1);
bool BLI_smallhash_reinsert(SmallHash *sh, uintptr_t key, void *item) ATTR_NONNULL(1);
bool BLI_smallhash_remove(SmallHash *sh, uintptr_t key) ATTR_NONNULL(1);
void *BLI_smallhash_lookup(const SmallHash *sh, uintptr_t key)
    ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;
void **BLI_smallhash_lookup_p(const SmallHash *sh, uintptr_t key)
    ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;
bool BLI_smallhash_haskey(const SmallHash *sh, uintptr_t key) ATTR_NONNULL(1);
int BLI_smallhash_len(const SmallHash *sh) ATTR_NONNULL(1);
void *BLI_smallhash_iternext(SmallHashIter *iter, uintptr_t *key)
    ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;
void **BLI_smallhash_iternext_p(SmallHashIter *iter, uintptr_t *key)
    ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;
void *BLI_smallhash_iternew(const SmallHash *sh, SmallHashIter *iter, uintptr_t *key)
    ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;
void **BLI_smallhash_iternew_p(const SmallHash *sh, SmallHashIter *iter, uintptr_t *key)
    ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;
/* void BLI_smallhash_print(SmallHash *sh); */ /* UNUSED */

#ifdef DEBUG
double BLI_smallhash_calc_quality(SmallHash *sh);
#endif

#ifdef __cplusplus
}
#endif
