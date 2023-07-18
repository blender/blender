/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Cache F-Curve look-ups.
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"

#include "BKE_fcurve.h"

/* -------------------------------------------------------------------- */
/** \name F-Curve Path Cache
 *
 * Cache for finding curves by RNA path & array index.
 * \{ */

struct FCurvePathCache_Span {
  /** Index in the #FCurvePathCache.fcurve_array indicating the start of the span. */
  uint index;
  /** Number of items in the span in #FCurvePathCache.fcurve_array that share an RNA path. */
  uint len;
};

struct FCurvePathCache {
  /** All curves sorted by (#FCurve.rna_path, #FCurve.array_index) */
  FCurve **fcurve_array;
  uint fcurve_array_len;
  /** Storage for values of `span_from_rna_path`. */
  struct FCurvePathCache_Span *span_table;
  /** Map `FCurve.rna_path` to elements in #FCurvePathCache.span_table */
  GHash *span_from_rna_path;
};

/**
 * #qsort callback for an #FCurve array.
 */
static int fcurve_cmp_for_cache(const void *fcu_a_p, const void *fcu_b_p)
{
  const FCurve *fcu_a = *((const FCurve **)fcu_a_p);
  const FCurve *fcu_b = *((const FCurve **)fcu_b_p);
  const int cmp = strcmp(fcu_a->rna_path, fcu_b->rna_path);
  if (cmp != 0) {
    return cmp;
  }
  if (fcu_a->array_index < fcu_b->array_index) {
    return -1;
  }
  if (fcu_a->array_index > fcu_b->array_index) {
    return 1;
  }
  return 0;
}

struct FCurvePathCache *BKE_fcurve_pathcache_create(ListBase *list)
{
  const uint fcurve_array_len = BLI_listbase_count(list);
  FCurve **fcurve_array = static_cast<FCurve **>(
      MEM_mallocN(sizeof(*fcurve_array) * fcurve_array_len, __func__));
  uint i;
  LISTBASE_FOREACH_INDEX (FCurve *, fcu, list, i) {
    fcurve_array[i] = fcu;
  }
  qsort(fcurve_array, fcurve_array_len, sizeof(FCurve *), fcurve_cmp_for_cache);

  /* Allow for the case no F-Curves share an RNA-path, otherwise this is over-allocated.
   * Although in practice it's likely to only be 3-4x as large as is needed
   * (with transform channels for e.g.). */
  struct FCurvePathCache_Span *span_table = static_cast<FCurvePathCache_Span *>(
      MEM_mallocN(sizeof(*span_table) * fcurve_array_len, __func__));

  /* May over reserve, harmless. */
  GHash *span_from_rna_path = BLI_ghash_str_new_ex(__func__, fcurve_array_len);
  uint span_index = 0;
  i = 0;
  while (i < fcurve_array_len) {
    uint i_end;
    for (i_end = i + 1; i_end < fcurve_array_len; i_end++) {
      /* As the indices are sorted, we know a decrease means a new RNA path is found. */
      if (fcurve_array[i]->array_index > fcurve_array[i_end]->array_index) {
        BLI_assert(!STREQ(fcurve_array[i]->rna_path, fcurve_array[i_end]->rna_path));
        break;
      }
      if (!STREQ(fcurve_array[i]->rna_path, fcurve_array[i_end]->rna_path)) {
        break;
      }
    }

    struct FCurvePathCache_Span *span = &span_table[span_index++];
    span->index = i;
    span->len = i_end - i;
    BLI_ghash_insert(span_from_rna_path, fcurve_array[i]->rna_path, span);
    i = i_end;
  }

  struct FCurvePathCache *fcache = static_cast<FCurvePathCache *>(
      MEM_callocN(sizeof(FCurvePathCache), __func__));
  fcache->fcurve_array = fcurve_array;
  fcache->fcurve_array_len = fcurve_array_len;
  fcache->span_table = span_table;
  fcache->span_from_rna_path = span_from_rna_path;

  return fcache;
}

void BKE_fcurve_pathcache_destroy(struct FCurvePathCache *fcache)
{
  MEM_freeN(fcache->fcurve_array);
  MEM_freeN(fcache->span_table);
  BLI_ghash_free(fcache->span_from_rna_path, nullptr, nullptr);
  MEM_freeN(fcache);
}

FCurve *BKE_fcurve_pathcache_find(struct FCurvePathCache *fcache,
                                  const char *rna_path,
                                  const int array_index)
{
  const struct FCurvePathCache_Span *span = static_cast<const FCurvePathCache_Span *>(
      BLI_ghash_lookup(fcache->span_from_rna_path, rna_path));
  if (span == nullptr) {
    return nullptr;
  }

  FCurve **fcurve = fcache->fcurve_array + span->index;
  const uint len = span->len;
  for (int i = 0; i < len; i++) {
    if (fcurve[i]->array_index == array_index) {
      return fcurve[i];
    }
    /* As these are sorted, early exit. */
    if (fcurve[i]->array_index > array_index) {
      break;
    }
  }
  return nullptr;
}

int BKE_fcurve_pathcache_find_array(struct FCurvePathCache *fcache,
                                    const char *rna_path,
                                    FCurve **fcurve_result,
                                    int fcurve_result_len)
{
  memset(fcurve_result, 0x0, sizeof(*fcurve_result) * fcurve_result_len);

  const struct FCurvePathCache_Span *span = static_cast<const FCurvePathCache_Span *>(
      BLI_ghash_lookup(fcache->span_from_rna_path, rna_path));
  if (span == nullptr) {
    return 0;
  }

  int found = 0;
  FCurve **fcurve = fcache->fcurve_array + span->index;
  const uint len = span->len;
  for (int i = 0; i < len; i++) {
    /* As these are sorted, early exit. */
    if (uint(fcurve[i]->array_index) > uint(fcurve_result_len)) {
      break;
    }
    fcurve_result[fcurve[i]->array_index] = fcurve[i];
    found += 1;
  }
  return found;
}

/** \} */
