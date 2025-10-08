/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 * \brief Array storage to minimize duplication.
 *
 * This is done by splitting arrays into chunks and using copy-on-evaluation,
 * to de-duplicate chunks, from the users perspective this is an implementation detail.
 *
 * Overview
 * ========
 *
 * Data Structure
 * --------------
 *
 * This diagram is an overview of the structure of a single array-store.
 *
 * \note The only 2 structures here which are referenced externally are the.
 *
 * - #BArrayStore: The whole array store.
 * - #BArrayState: Represents a single state (array) of data.
 *   These can be add using a reference state,
 *   while this could be considered the previous or parent state.
 *   no relationship is kept,
 *   so the caller is free to add any state from the same #BArrayStore as a reference.
 *
 * <pre>
 * <+> #BArrayStore: root data-structure,
 *  |  can store many 'states', which share memory.
 *  |
 *  |  This can store many arrays, however they must share the same 'stride'.
 *  |  Arrays of different types will need to use a new #BArrayStore.
 *  |
 *  +- <+> states (Collection of #BArrayState's):
 *  |   |  Each represents an array added by the user of this API.
 *  |   |  and references a chunk_list (each state is a chunk_list user).
 *  |   |  Note that the list order has no significance.
 *  |   |
 *  |   +- <+> chunk_list (#BChunkList):
 *  |       |  The chunks that make up this state.
 *  |       |  Each state is a chunk_list user,
 *  |       |  avoids duplicating lists when there is no change between states.
 *  |       |
 *  |       +- chunk_refs (List of #BChunkRef): Each chunk_ref links to a #BChunk.
 *  |          Each reference is a chunk user,
 *  |          avoids duplicating smaller chunks of memory found in multiple states.
 *  |
 *  +- info (#BArrayInfo):
 *  |  Sizes and offsets for this array-store.
 *  |  Also caches some variables for reuse.
 *  |
 *  +- <+> memory (#BArrayMemory):
 *      |  Memory pools for storing #BArrayStore data.
 *      |
 *      +- chunk_list (Pool of #BChunkList):
 *      |  All chunk_lists, (reference counted, used by #BArrayState).
 *      |
 *      +- chunk_ref (Pool of #BChunkRef):
 *      |  All chunk_refs (link between #BChunkList & #BChunk).
 *      |
 *      +- chunks (Pool of #BChunk):
 *         All chunks, (reference counted, used by #BChunkList).
 *         These have their headers hashed for reuse so we can quickly check for duplicates.
 * </pre>
 *
 * De-Duplication
 * --------------
 *
 * When creating a new state, a previous state can be given as a reference,
 * matching chunks from this state are re-used in the new state.
 *
 * First matches at either end of the array are detected.
 * For identical arrays this is all that's needed.
 *
 * De-duplication is performed on any remaining chunks, by hashing the first few bytes of the chunk
 * (see: #BCHUNK_HASH_TABLE_ACCUMULATE_STEPS).
 *
 * \note This is cached for reuse since the referenced data never changes.
 *
 * An array is created to store hash values at every 'stride',
 * then stepped over to search for matching chunks.
 *
 * Once a match is found, there is a high chance next chunks match too,
 * so this is checked to avoid performing so many hash-lookups.
 * Otherwise new chunks are created.
 */

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_assert.h"
#include "BLI_listbase.h"
#include "BLI_mempool.h"
#include "BLI_utildefines.h"

#include "BLI_array_store.h" /* Own include. */
#include "BLI_ghash.h"       /* Only for #BLI_array_store_is_valid. */

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

struct BChunkList;

/* -------------------------------------------------------------------- */
/** \name Defines
 *
 * Some of the logic for merging is quite involved,
 * support disabling some parts of this.
 * \{ */

/**
 * Scan first chunks (happy path when beginning of the array matches).
 * When the array is a perfect match, we can re-use the entire list.
 *
 * Note that disabling makes some tests fail that check for output-size.
 */
#define USE_FASTPATH_CHUNKS_FIRST

/**
 * Scan last chunks (happy path when end of the array matches).
 * When the end of the array matches, we can quickly add these chunks.
 * note that we will add contiguous matching chunks
 * so this isn't as useful as #USE_FASTPATH_CHUNKS_FIRST,
 * however it avoids adding matching chunks into the lookup table,
 * so creating the lookup table won't be as expensive.
 */
#ifdef USE_FASTPATH_CHUNKS_FIRST
#  define USE_FASTPATH_CHUNKS_LAST
#endif

/**
 * For arrays of matching length, test that *enough* of the chunks are aligned,
 * and simply step over both arrays, using matching chunks.
 * This avoids overhead of using a lookup table for cases
 * when we can assume they're mostly aligned.
 */
#define USE_ALIGN_CHUNKS_TEST

/**
 * Accumulate hashes from right to left so we can create a hash for the chunk-start.
 * This serves to increase uniqueness and will help when there is many values which are the same.
 */
#define USE_HASH_TABLE_ACCUMULATE

#ifdef USE_HASH_TABLE_ACCUMULATE
/* Number of times to propagate hashes back.
 * Effectively a 'triangle-number'.
 * so 3 -> 7, 4 -> 11, 5 -> 16, 6 -> 22, 7 -> 29, ... etc.
 *
 * \note additional steps are expensive, so avoid high values unless necessary
 * (with low strides, between 1-4) where a low value would cause the hashes to
 * be un-evenly distributed.
 */
#  define BCHUNK_HASH_TABLE_ACCUMULATE_STEPS_DEFAULT 3
#  define BCHUNK_HASH_TABLE_ACCUMULATE_STEPS_32BITS 4
#  define BCHUNK_HASH_TABLE_ACCUMULATE_STEPS_16BITS 5
/**
 * Single bytes (or boolean) arrays need a higher number of steps
 * because the resulting values are not unique enough to result in evenly distributed values.
 * Use more accumulation when the size of the structs is small, see: #105046.
 *
 * With 6 -> 22, one byte each - means an array of booleans can be combined into 22 bits
 * representing 4,194,303 different combinations.
 */
#  define BCHUNK_HASH_TABLE_ACCUMULATE_STEPS_8BITS 6
#else
/**
 * How many items to hash (multiplied by stride).
 * The more values, the greater the chance this block has a unique hash.
 */
#  define BCHUNK_HASH_LEN 16
#endif

/**
 * Calculate the key once and reuse it.
 */
#define USE_HASH_TABLE_KEY_CACHE
#ifdef USE_HASH_TABLE_KEY_CACHE
#  define HASH_TABLE_KEY_UNSET ((hash_key) - 1)
#  define HASH_TABLE_KEY_FALLBACK ((hash_key) - 2)
#endif

/**
 * Ensure duplicate entries aren't added to temporary hash table
 * needed for arrays where many values match (e.g. an array of booleans all true/false).
 *
 * Without this, a huge number of duplicates are added a single bucket, making hash lookups slow.
 * While de-duplication adds some cost, it's only performed with other chunks in the same bucket
 * so cases when all chunks are unique will quickly detect and exit the `memcmp` in most cases.
 */
#define USE_HASH_TABLE_DEDUPLICATE

/**
 * How much larger the table is then the total number of chunks.
 */
#define BCHUNK_HASH_TABLE_MUL 3

/**
 * Merge too small/large chunks:
 *
 * Using this means chunks below a threshold will be merged together.
 * Even though short term this uses more memory,
 * long term the overhead of maintaining many small chunks is reduced.
 * This is defined by setting the minimum chunk size (as a fraction of the regular chunk size).
 *
 * Chunks may also become too large (when incrementally growing an array),
 * this also enables chunk splitting.
 */
#define USE_MERGE_CHUNKS

#ifdef USE_MERGE_CHUNKS
/** Merge chunks smaller then: (#BArrayInfo::chunk_byte_size / #BCHUNK_SIZE_MIN_DIV). */
#  define BCHUNK_SIZE_MIN_DIV 8

/**
 * Disallow chunks bigger than the regular chunk size scaled by this value.
 *
 * \note must be at least 2!
 * however, this code runs won't run in tests unless it's ~1.1 ugh.
 * so lower only to check splitting works.
 */
#  define BCHUNK_SIZE_MAX_MUL 2
#endif /* USE_MERGE_CHUNKS */

/** Slow (keep disabled), but handy for debugging. */
// #define USE_VALIDATE_LIST_SIZE

// #define USE_VALIDATE_LIST_DATA_PARTIAL

// #define USE_PARANOID_CHECKS

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Structs
 * \{ */

using hash_key = uint32_t;

struct BArrayInfo {
  size_t chunk_stride;
  // uint chunk_count;  /* UNUSED (other values are derived from this) */

  /* Pre-calculated. */
  size_t chunk_byte_size;
  /* Min/max limits (inclusive) */
  size_t chunk_byte_size_min;
  size_t chunk_byte_size_max;
  /**
   * The read-ahead value should never exceed `chunk_byte_size`,
   * otherwise the hash would be based on values in the next chunk.
   */
  size_t accum_read_ahead_bytes;
#ifdef USE_HASH_TABLE_ACCUMULATE
  size_t accum_steps;
  size_t accum_read_ahead_len;
#endif
};

struct BArrayMemory {
  BLI_mempool *chunk_list; /* #BChunkList. */
  BLI_mempool *chunk_ref;  /* #BChunkRef. */
  BLI_mempool *chunk;      /* #BChunk. */
};

/**
 * Main storage for all states.
 */
struct BArrayStore {
  /* Static. */
  BArrayInfo info;

  /** Memory storage. */
  BArrayMemory memory;

  /**
   * #BArrayState may be in any order (logic should never depend on state order).
   */
  ListBase states;
};

/**
 * A single instance of an array.
 *
 * This is how external API's hold a reference to an in-memory state,
 * although the struct is private.
 *
 * \note Currently each 'state' is allocated separately.
 * While this could be moved to a memory pool,
 * it makes it easier to trace invalid usage, so leave as-is for now.
 */
struct BArrayState {
  /** linked list in #BArrayStore.states. */
  BArrayState *next, *prev;
  /** Shared chunk list, this reference must hold a #BChunkList::users. */
  BChunkList *chunk_list;
};

struct BChunkList {
  /** List of #BChunkRef's. */
  ListBase chunk_refs;
  /** Result of `BLI_listbase_count(chunks)`, store for reuse. */
  uint chunk_refs_len;
  /** Size of all chunks (expanded). */
  size_t total_expanded_size;

  /** Number of #BArrayState using this. */
  int users;
};

/** A chunk of memory in an array (unit of de-duplication). */
struct BChunk {
  const uchar *data;
  size_t data_len;
  /** number of #BChunkList using this. */
  int users;

#ifdef USE_HASH_TABLE_KEY_CACHE
  hash_key key;
#endif
};

/**
 * Links to store #BChunk data in #BChunkList.chunk_refs.
 */
struct BChunkRef {
  BChunkRef *next, *prev;
  BChunk *link;
};

/**
 * Single linked list used when putting chunks into a temporary table,
 * used for lookups.
 *
 * Point to the #BChunkRef, not the #BChunk,
 * to allow talking down the chunks in-order until a mismatch is found,
 * this avoids having to do so many table lookups.
 */
struct BTableRef {
  BTableRef *next;
  const BChunkRef *cref;
};

/** \} */

static size_t bchunk_list_size(const BChunkList *chunk_list);

/* -------------------------------------------------------------------- */
/** \name Internal BChunk API
 * \{ */

static BChunk *bchunk_new(BArrayMemory *bs_mem, const uchar *data, const size_t data_len)
{
  BChunk *chunk = static_cast<BChunk *>(BLI_mempool_alloc(bs_mem->chunk));
  chunk->data = data;
  chunk->data_len = data_len;
  chunk->users = 0;
#ifdef USE_HASH_TABLE_KEY_CACHE
  chunk->key = HASH_TABLE_KEY_UNSET;
#endif
  return chunk;
}

static BChunk *bchunk_new_copydata(BArrayMemory *bs_mem, const uchar *data, const size_t data_len)
{
  uchar *data_copy = MEM_malloc_arrayN<uchar>(data_len, __func__);
  memcpy(data_copy, data, data_len);
  return bchunk_new(bs_mem, data_copy, data_len);
}

static void bchunk_decref(BArrayMemory *bs_mem, BChunk *chunk)
{
  BLI_assert(chunk->users > 0);
  if (chunk->users == 1) {
    MEM_freeN(chunk->data);
    BLI_mempool_free(bs_mem->chunk, chunk);
  }
  else {
    chunk->users -= 1;
  }
}

BLI_INLINE bool bchunk_data_compare_unchecked(const BChunk *chunk,
                                              const uchar *data_base,
                                              const size_t data_base_len,
                                              const size_t offset)
{
  BLI_assert(offset + size_t(chunk->data_len) <= data_base_len);
  UNUSED_VARS_NDEBUG(data_base_len);
  return (memcmp(&data_base[offset], chunk->data, chunk->data_len) == 0);
}

static bool bchunk_data_compare(const BChunk *chunk,
                                const uchar *data_base,
                                const size_t data_base_len,
                                const size_t offset)
{
  if (offset + size_t(chunk->data_len) <= data_base_len) {
    return bchunk_data_compare_unchecked(chunk, data_base, data_base_len, offset);
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal BChunkList API
 * \{ */

static BChunkList *bchunk_list_new(BArrayMemory *bs_mem, size_t total_expanded_size)
{
  BChunkList *chunk_list = static_cast<BChunkList *>(BLI_mempool_alloc(bs_mem->chunk_list));

  BLI_listbase_clear(&chunk_list->chunk_refs);
  chunk_list->chunk_refs_len = 0;
  chunk_list->total_expanded_size = total_expanded_size;
  chunk_list->users = 0;
  return chunk_list;
}

static void bchunk_list_decref(BArrayMemory *bs_mem, BChunkList *chunk_list)
{
  BLI_assert(chunk_list->users > 0);
  if (chunk_list->users == 1) {
    for (BChunkRef *cref = static_cast<BChunkRef *>(chunk_list->chunk_refs.first), *cref_next;
         cref;
         cref = cref_next)
    {
      cref_next = cref->next;
      bchunk_decref(bs_mem, cref->link);
      BLI_mempool_free(bs_mem->chunk_ref, cref);
    }

    BLI_mempool_free(bs_mem->chunk_list, chunk_list);
  }
  else {
    chunk_list->users -= 1;
  }
}

#ifdef USE_VALIDATE_LIST_SIZE
#  ifndef NDEBUG
#    define ASSERT_CHUNKLIST_SIZE(chunk_list, n) BLI_assert(bchunk_list_size(chunk_list) == n)
#  endif
#endif
#ifndef ASSERT_CHUNKLIST_SIZE
#  define ASSERT_CHUNKLIST_SIZE(chunk_list, n) (EXPR_NOP(chunk_list), EXPR_NOP(n))
#endif

#ifdef USE_VALIDATE_LIST_DATA_PARTIAL
static size_t bchunk_list_data_check(const BChunkList *chunk_list, const uchar *data)
{
  size_t offset = 0;
  LISTBASE_FOREACH (BChunkRef *, cref, &chunk_list->chunk_refs) {
    if (memcmp(&data[offset], cref->link->data, cref->link->data_len) != 0) {
      return false;
    }
    offset += cref->link->data_len;
  }
  return true;
}
#  define ASSERT_CHUNKLIST_DATA(chunk_list, data) \
    BLI_assert(bchunk_list_data_check(chunk_list, data))
#else
#  define ASSERT_CHUNKLIST_DATA(chunk_list, data) (EXPR_NOP(chunk_list), EXPR_NOP(data))
#endif

#ifdef USE_MERGE_CHUNKS
static void bchunk_list_ensure_min_size_last(const BArrayInfo *info,
                                             BArrayMemory *bs_mem,
                                             BChunkList *chunk_list)
{
  BChunkRef *cref = static_cast<BChunkRef *>(chunk_list->chunk_refs.last);
  if (cref && cref->prev) {
    /* Both are decrefed after use (end of this block). */
    BChunk *chunk_curr = cref->link;
    BChunk *chunk_prev = cref->prev->link;

    if (std::min(chunk_prev->data_len, chunk_curr->data_len) < info->chunk_byte_size_min) {
      const size_t data_merge_len = chunk_prev->data_len + chunk_curr->data_len;
      /* We could pass, but no need. */
      if (data_merge_len <= info->chunk_byte_size_max) {
        /* We have enough space to merge. */

        /* Remove last from the linked-list. */
        BLI_assert(chunk_list->chunk_refs.last != chunk_list->chunk_refs.first);
        cref->prev->next = nullptr;
        chunk_list->chunk_refs.last = cref->prev;
        chunk_list->chunk_refs_len -= 1;

        uchar *data_merge = MEM_malloc_arrayN<uchar>(data_merge_len, __func__);
        memcpy(data_merge, chunk_prev->data, chunk_prev->data_len);
        memcpy(&data_merge[chunk_prev->data_len], chunk_curr->data, chunk_curr->data_len);

        cref->prev->link = bchunk_new(bs_mem, data_merge, data_merge_len);
        cref->prev->link->users += 1;

        BLI_mempool_free(bs_mem->chunk_ref, cref);
      }
      else {
        /* If we always merge small slices, we should _almost_
         * never end up having very large chunks.
         * Gradual expanding on contracting will cause this.
         *
         * if we do, the code below works (test by setting 'BCHUNK_SIZE_MAX_MUL = 1.2') */

        /* Keep chunk on the left hand side a regular size. */
        const size_t split = info->chunk_byte_size;

        /* Merge and split. */
        const size_t data_prev_len = split;
        const size_t data_curr_len = data_merge_len - split;
        uchar *data_prev = MEM_malloc_arrayN<uchar>(data_prev_len, __func__);
        uchar *data_curr = MEM_malloc_arrayN<uchar>(data_curr_len, __func__);

        if (data_prev_len <= chunk_prev->data_len) {
          const size_t data_curr_shrink_len = chunk_prev->data_len - data_prev_len;

          /* Setup 'data_prev'. */
          memcpy(data_prev, chunk_prev->data, data_prev_len);

          /* Setup 'data_curr'. */
          memcpy(data_curr, &chunk_prev->data[data_prev_len], data_curr_shrink_len);
          memcpy(&data_curr[data_curr_shrink_len], chunk_curr->data, chunk_curr->data_len);
        }
        else {
          BLI_assert(data_curr_len <= chunk_curr->data_len);
          BLI_assert(data_prev_len >= chunk_prev->data_len);

          const size_t data_prev_grow_len = data_prev_len - chunk_prev->data_len;

          /* Setup 'data_prev'. */
          memcpy(data_prev, chunk_prev->data, chunk_prev->data_len);
          memcpy(&data_prev[chunk_prev->data_len], chunk_curr->data, data_prev_grow_len);

          /* Setup 'data_curr'. */
          memcpy(data_curr, &chunk_curr->data[data_prev_grow_len], data_curr_len);
        }

        cref->prev->link = bchunk_new(bs_mem, data_prev, data_prev_len);
        cref->prev->link->users += 1;

        cref->link = bchunk_new(bs_mem, data_curr, data_curr_len);
        cref->link->users += 1;
      }

      /* Free zero users. */
      bchunk_decref(bs_mem, chunk_curr);
      bchunk_decref(bs_mem, chunk_prev);
    }
  }
}
#endif /* USE_MERGE_CHUNKS */

/**
 * Split length into 2 values
 * \param r_data_trim_len: Length which is aligned to the #BArrayInfo.chunk_byte_size
 * \param r_data_last_chunk_len: The remaining bytes.
 *
 * \note This function ensures the size of \a r_data_last_chunk_len
 * is larger than #BArrayInfo.chunk_byte_size_min.
 */
static void bchunk_list_calc_trim_len(const BArrayInfo *info,
                                      const size_t data_len,
                                      size_t *r_data_trim_len,
                                      size_t *r_data_last_chunk_len)
{
  size_t data_last_chunk_len = 0;
  size_t data_trim_len = data_len;

#ifdef USE_MERGE_CHUNKS
  /* Avoid creating too-small chunks more efficient than merging after. */
  if (data_len > info->chunk_byte_size) {
    data_last_chunk_len = (data_trim_len % info->chunk_byte_size);
    data_trim_len = data_trim_len - data_last_chunk_len;
    if (data_last_chunk_len) {
      if (data_last_chunk_len < info->chunk_byte_size_min) {
        /* May be zero and that's OK. */
        data_trim_len -= info->chunk_byte_size;
        data_last_chunk_len += info->chunk_byte_size;
      }
    }
  }
  else {
    data_trim_len = 0;
    data_last_chunk_len = data_len;
  }

  BLI_assert((data_trim_len == 0) || (data_trim_len >= info->chunk_byte_size));
#else
  data_last_chunk_len = (data_trim_len % info->chunk_byte_size);
  data_trim_len = data_trim_len - data_last_chunk_len;
#endif

  BLI_assert(data_trim_len + data_last_chunk_len == data_len);

  *r_data_trim_len = data_trim_len;
  *r_data_last_chunk_len = data_last_chunk_len;
}

/**
 * Append and don't manage merging small chunks.
 */
static void bchunk_list_append_only(BArrayMemory *bs_mem, BChunkList *chunk_list, BChunk *chunk)
{
  BChunkRef *cref = static_cast<BChunkRef *>(BLI_mempool_alloc(bs_mem->chunk_ref));
  BLI_addtail(&chunk_list->chunk_refs, cref);
  cref->link = chunk;
  chunk_list->chunk_refs_len += 1;
  chunk->users += 1;
}

/**
 * \note This is for writing single chunks,
 * use #bchunk_list_append_data_n when writing large blocks of memory into many chunks.
 */
static void bchunk_list_append_data(const BArrayInfo *info,
                                    BArrayMemory *bs_mem,
                                    BChunkList *chunk_list,
                                    const uchar *data,
                                    const size_t data_len)
{
  BLI_assert(data_len != 0);

#ifdef USE_MERGE_CHUNKS
  BLI_assert(data_len <= info->chunk_byte_size_max);

  if (!BLI_listbase_is_empty(&chunk_list->chunk_refs)) {
    BChunkRef *cref = static_cast<BChunkRef *>(chunk_list->chunk_refs.last);
    BChunk *chunk_prev = cref->link;

    if (std::min(chunk_prev->data_len, data_len) < info->chunk_byte_size_min) {
      const size_t data_merge_len = chunk_prev->data_len + data_len;
      /* Re-allocate for single user. */
      if (cref->link->users == 1) {
        uchar *data_merge = static_cast<uchar *>(
            MEM_reallocN((void *)cref->link->data, data_merge_len));
        memcpy(&data_merge[chunk_prev->data_len], data, data_len);
        cref->link->data = data_merge;
        cref->link->data_len = data_merge_len;
      }
      else {
        uchar *data_merge = MEM_malloc_arrayN<uchar>(data_merge_len, __func__);
        memcpy(data_merge, chunk_prev->data, chunk_prev->data_len);
        memcpy(&data_merge[chunk_prev->data_len], data, data_len);
        cref->link = bchunk_new(bs_mem, data_merge, data_merge_len);
        cref->link->users += 1;
        bchunk_decref(bs_mem, chunk_prev);
      }
      return;
    }
  }
#else
  UNUSED_VARS(info);
#endif /* USE_MERGE_CHUNKS */

  BChunk *chunk = bchunk_new_copydata(bs_mem, data, data_len);
  bchunk_list_append_only(bs_mem, chunk_list, chunk);

  /* Don't run this, instead preemptively avoid creating a chunk only to merge it (above). */
#if 0
#  ifdef USE_MERGE_CHUNKS
  bchunk_list_ensure_min_size_last(info, bs_mem, chunk_list);
#  endif
#endif
}

/**
 * Similar to #bchunk_list_append_data, but handle multiple chunks.
 * Use for adding arrays of arbitrary sized memory at once.
 *
 * \note This function takes care not to perform redundant chunk-merging checks,
 * so we can write successive fixed size chunks quickly.
 */
static void bchunk_list_append_data_n(const BArrayInfo *info,
                                      BArrayMemory *bs_mem,
                                      BChunkList *chunk_list,
                                      const uchar *data,
                                      size_t data_len)
{
  size_t data_trim_len, data_last_chunk_len;
  bchunk_list_calc_trim_len(info, data_len, &data_trim_len, &data_last_chunk_len);

  if (data_trim_len != 0) {
    size_t i_prev;

    {
      const size_t i = info->chunk_byte_size;
      bchunk_list_append_data(info, bs_mem, chunk_list, data, i);
      i_prev = i;
    }

    while (i_prev != data_trim_len) {
      const size_t i = i_prev + info->chunk_byte_size;
      BChunk *chunk = bchunk_new_copydata(bs_mem, &data[i_prev], i - i_prev);
      bchunk_list_append_only(bs_mem, chunk_list, chunk);
      i_prev = i;
    }

    if (data_last_chunk_len) {
      BChunk *chunk = bchunk_new_copydata(bs_mem, &data[i_prev], data_last_chunk_len);
      bchunk_list_append_only(bs_mem, chunk_list, chunk);
      // i_prev = data_len;  /* UNUSED */
    }
  }
  else {
    /* If we didn't write any chunks previously, we may need to merge with the last. */
    if (data_last_chunk_len) {
      bchunk_list_append_data(info, bs_mem, chunk_list, data, data_last_chunk_len);
      // i_prev = data_len;  /* UNUSED */
    }
  }

#ifdef USE_MERGE_CHUNKS
  if (data_len > info->chunk_byte_size) {
    BLI_assert(((BChunkRef *)chunk_list->chunk_refs.last)->link->data_len >=
               info->chunk_byte_size_min);
  }
#endif
}

static void bchunk_list_append(const BArrayInfo *info,
                               BArrayMemory *bs_mem,
                               BChunkList *chunk_list,
                               BChunk *chunk)
{
  bchunk_list_append_only(bs_mem, chunk_list, chunk);

#ifdef USE_MERGE_CHUNKS
  bchunk_list_ensure_min_size_last(info, bs_mem, chunk_list);
#else
  UNUSED_VARS(info);
#endif
}

static void bchunk_list_fill_from_array(const BArrayInfo *info,
                                        BArrayMemory *bs_mem,
                                        BChunkList *chunk_list,
                                        const uchar *data,
                                        const size_t data_len)
{
  BLI_assert(BLI_listbase_is_empty(&chunk_list->chunk_refs));

  size_t data_trim_len, data_last_chunk_len;
  bchunk_list_calc_trim_len(info, data_len, &data_trim_len, &data_last_chunk_len);

  size_t i_prev = 0;
  while (i_prev != data_trim_len) {
    const size_t i = i_prev + info->chunk_byte_size;
    BChunk *chunk = bchunk_new_copydata(bs_mem, &data[i_prev], i - i_prev);
    bchunk_list_append_only(bs_mem, chunk_list, chunk);
    i_prev = i;
  }

  if (data_last_chunk_len) {
    BChunk *chunk = bchunk_new_copydata(bs_mem, &data[i_prev], data_last_chunk_len);
    bchunk_list_append_only(bs_mem, chunk_list, chunk);
    // i_prev = data_len;
  }

#ifdef USE_MERGE_CHUNKS
  if (data_len > info->chunk_byte_size) {
    BLI_assert(((BChunkRef *)chunk_list->chunk_refs.last)->link->data_len >=
               info->chunk_byte_size_min);
  }
#endif

  /* Works but better avoid redundant re-allocation. */
#if 0
#  ifdef USE_MERGE_CHUNKS
  bchunk_list_ensure_min_size_last(info, bs_mem, chunk_list);
#  endif
#endif

  ASSERT_CHUNKLIST_SIZE(chunk_list, data_len);
  ASSERT_CHUNKLIST_DATA(chunk_list, data);
}

/** \} */

/*
 * Internal Table Lookup Functions.
 */

/* -------------------------------------------------------------------- */
/** \name Internal Hashing/De-Duplication API
 *
 * Only used by #bchunk_list_from_data_merge
 * \{ */

#define HASH_INIT (5381)

BLI_INLINE hash_key hash_data_single(const uchar p)
{
  return ((HASH_INIT << 5) + HASH_INIT) + (hash_key) * ((signed char *)&p);
}

/* Hash bytes, from #BLI_ghashutil_strhash_n. */
static hash_key hash_data(const uchar *key, size_t n)
{
  const signed char *p;
  hash_key h = HASH_INIT;

  for (p = (const signed char *)key; n--; p++) {
    h = (hash_key)((h << 5) + h) + (hash_key)*p;
  }

  return h;
}

#undef HASH_INIT

#ifdef USE_HASH_TABLE_ACCUMULATE
static void hash_array_from_data(const BArrayInfo *info,
                                 const uchar *data_slice,
                                 const size_t data_slice_len,
                                 hash_key *hash_array)
{
  if (info->chunk_stride != 1) {
    for (size_t i = 0, i_step = 0; i_step < data_slice_len; i++, i_step += info->chunk_stride) {
      hash_array[i] = hash_data(&data_slice[i_step], info->chunk_stride);
    }
  }
  else {
    /* Fast-path for bytes. */
    for (size_t i = 0; i < data_slice_len; i++) {
      hash_array[i] = hash_data_single(data_slice[i]);
    }
  }
}

/**
 * Similar to hash_array_from_data,
 * but able to step into the next chunk if we run-out of data.
 */
static void hash_array_from_cref(const BArrayInfo *info,
                                 const BChunkRef *cref,
                                 const size_t data_len,
                                 hash_key *hash_array)
{
  const size_t hash_array_len = data_len / info->chunk_stride;
  size_t i = 0;
  do {
    size_t i_next = hash_array_len - i;
    size_t data_trim_len = i_next * info->chunk_stride;
    if (data_trim_len > cref->link->data_len) {
      data_trim_len = cref->link->data_len;
      i_next = data_trim_len / info->chunk_stride;
    }
    BLI_assert(data_trim_len <= cref->link->data_len);
    hash_array_from_data(info, cref->link->data, data_trim_len, &hash_array[i]);
    i += i_next;
    cref = cref->next;
  } while ((i < hash_array_len) && (cref != nullptr));

  /* If this isn't equal, the caller didn't properly check
   * that there was enough data left in all chunks. */
  BLI_assert(i == hash_array_len);
}

BLI_INLINE void hash_accum_impl(hash_key *hash_array, const size_t i_dst, const size_t i_ahead)
{
  /* Tested to give good results when accumulating unique values from an array of booleans.
   * (least unused cells in the `BTableRef **table`). */
  BLI_assert(i_dst < i_ahead);
  hash_array[i_dst] += ((hash_array[i_ahead] << 3) ^ (hash_array[i_dst] >> 1));
}

static void hash_accum(hash_key *hash_array, const size_t hash_array_len, size_t iter_steps)
{
  /* _very_ unlikely, can happen if you select a chunk-size of 1 for example. */
  if (UNLIKELY(iter_steps > hash_array_len)) {
    iter_steps = hash_array_len;
  }

  const size_t hash_array_search_len = hash_array_len - iter_steps;
  while (iter_steps != 0) {
    const size_t hash_offset = iter_steps;
    for (size_t i = 0; i < hash_array_search_len; i++) {
      hash_accum_impl(hash_array, i, i + hash_offset);
    }
    iter_steps -= 1;
  }
}

/**
 * When we only need a single value, can use a small optimization.
 * we can avoid accumulating the tail of the array a little, each iteration.
 */
static void hash_accum_single(hash_key *hash_array, const size_t hash_array_len, size_t iter_steps)
{
  BLI_assert(iter_steps <= hash_array_len);
  if (UNLIKELY(!(iter_steps <= hash_array_len))) {
    /* While this shouldn't happen, avoid crashing. */
    iter_steps = hash_array_len;
  }
  /* We can increase this value each step to avoid accumulating quite as much
   * while getting the same results as hash_accum. */
  size_t iter_steps_sub = iter_steps;

  while (iter_steps != 0) {
    const size_t hash_array_search_len = hash_array_len - iter_steps_sub;
    const size_t hash_offset = iter_steps;
    for (uint i = 0; i < hash_array_search_len; i++) {
      hash_accum_impl(hash_array, i, i + hash_offset);
    }
    iter_steps -= 1;
    iter_steps_sub += iter_steps;
  }
}

static hash_key key_from_chunk_ref(const BArrayInfo *info,
                                   const BChunkRef *cref,
                                   /* Avoid reallocating each time. */
                                   hash_key *hash_store,
                                   const size_t hash_store_len)
{
  /* In C, will fill in a reusable array. */
  BChunk *chunk = cref->link;
  BLI_assert((info->accum_read_ahead_bytes * info->chunk_stride) != 0);

  if (info->accum_read_ahead_bytes <= chunk->data_len) {
    hash_key key;

#  ifdef USE_HASH_TABLE_KEY_CACHE
    key = chunk->key;
    if (key != HASH_TABLE_KEY_UNSET) {
      /* Using key cache!
       * avoids calculating every time. */
    }
    else {
      hash_array_from_cref(info, cref, info->accum_read_ahead_bytes, hash_store);
      hash_accum_single(hash_store, hash_store_len, info->accum_steps);
      key = hash_store[0];

      /* Cache the key. */
      if (UNLIKELY(key == HASH_TABLE_KEY_UNSET)) {
        key = HASH_TABLE_KEY_FALLBACK;
      }
      chunk->key = key;
    }
#  else
    hash_array_from_cref(info, cref, info->accum_read_ahead_bytes, hash_store);
    hash_accum_single(hash_store, hash_store_len, info->accum_steps);
    key = hash_store[0];
#  endif
    return key;
  }
  /* Corner case - we're too small, calculate the key each time. */

  hash_array_from_cref(info, cref, info->accum_read_ahead_bytes, hash_store);
  hash_accum_single(hash_store, hash_store_len, info->accum_steps);
  hash_key key = hash_store[0];

#  ifdef USE_HASH_TABLE_KEY_CACHE
  if (UNLIKELY(key == HASH_TABLE_KEY_UNSET)) {
    key = HASH_TABLE_KEY_FALLBACK;
  }
#  endif
  return key;
}

static const BChunkRef *table_lookup(const BArrayInfo *info,
                                     BTableRef **table,
                                     const size_t table_len,
                                     const size_t i_table_start,
                                     const uchar *data,
                                     const size_t data_len,
                                     const size_t offset,
                                     const hash_key *table_hash_array)
{
  const hash_key key = table_hash_array[((offset - i_table_start) / info->chunk_stride)];
  const uint key_index = uint(key % (hash_key)table_len);
  const BTableRef *tref = table[key_index];
  if (tref != nullptr) {
    const size_t size_left = data_len - offset;
    do {
      const BChunkRef *cref = tref->cref;
#  ifdef USE_HASH_TABLE_KEY_CACHE
      if (cref->link->key == key)
#  endif
      {
        const BChunk *chunk_test = cref->link;
        if (chunk_test->data_len <= size_left) {
          if (bchunk_data_compare_unchecked(chunk_test, data, data_len, offset)) {
            /* We could remove the chunk from the table, to avoid multiple hits. */
            return cref;
          }
        }
      }
    } while ((tref = tref->next));
  }
  return nullptr;
}

#else /* USE_HASH_TABLE_ACCUMULATE */

/* NON USE_HASH_TABLE_ACCUMULATE code (simply hash each chunk). */

static hash_key key_from_chunk_ref(const BArrayInfo *info, const BChunkRef *cref)
{
  hash_key key;
  BChunk *chunk = cref->link;
  const size_t data_hash_len = std::min(chunk->data_len, BCHUNK_HASH_LEN * info->chunk_stride);

#  ifdef USE_HASH_TABLE_KEY_CACHE
  key = chunk->key;
  if (key != HASH_TABLE_KEY_UNSET) {
    /* Using key cache!
     * avoids calculating every time. */
  }
  else {
    /* Cache the key. */
    key = hash_data(chunk->data, data_hash_len);
    if (key == HASH_TABLE_KEY_UNSET) {
      key = HASH_TABLE_KEY_FALLBACK;
    }
    chunk->key = key;
  }
#  else
  key = hash_data(chunk->data, data_hash_len);
#  endif

  return key;
}

static const BChunkRef *table_lookup(const BArrayInfo *info,
                                     BTableRef **table,
                                     const size_t table_len,
                                     const uint UNUSED(i_table_start),
                                     const uchar *data,
                                     const size_t data_len,
                                     const size_t offset,
                                     const hash_key *UNUSED(table_hash_array))
{
  const size_t data_hash_len = BCHUNK_HASH_LEN * info->chunk_stride; /* TODO: cache. */

  const size_t size_left = data_len - offset;
  const hash_key key = hash_data(&data[offset], std::min(data_hash_len, size_left));
  const uint key_index = uint(key % (hash_key)table_len);
  for (BTableRef *tref = table[key_index]; tref; tref = tref->next) {
    const BChunkRef *cref = tref->cref;
#  ifdef USE_HASH_TABLE_KEY_CACHE
    if (cref->link->key == key)
#  endif
    {
      BChunk *chunk_test = cref->link;
      if (chunk_test->data_len <= size_left) {
        if (bchunk_data_compare_unchecked(chunk_test, data, data_len, offset)) {
          /* We could remove the chunk from the table, to avoid multiple hits. */
          return cref;
        }
      }
    }
  }
  return nullptr;
}

#endif /* USE_HASH_TABLE_ACCUMULATE */

/* End Table Lookup
 * ---------------- */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Data De-Duplication Function
 * \{ */

/**
 * \param data: Data to store in the returned value.
 * \param data_len_original: Length of data in bytes.
 * \param chunk_list_reference: Reuse this list or chunks within it, don't modify its content.
 * \note Caller is responsible for adding the user.
 */
static BChunkList *bchunk_list_from_data_merge(const BArrayInfo *info,
                                               BArrayMemory *bs_mem,
                                               const uchar *data,
                                               const size_t data_len_original,
                                               const BChunkList *chunk_list_reference)
{
  ASSERT_CHUNKLIST_SIZE(chunk_list_reference, chunk_list_reference->total_expanded_size);

  /* -----------------------------------------------------------------------
   * Fast-Path for exact match
   * Check for exact match, if so, return the current list.
   */

  const BChunkRef *cref_match_first = nullptr;

  uint chunk_list_reference_skip_len = 0;
  size_t chunk_list_reference_skip_bytes = 0;
  size_t i_prev = 0;

#ifdef USE_FASTPATH_CHUNKS_FIRST
  {
    bool full_match = true;

    const BChunkRef *cref = static_cast<const BChunkRef *>(chunk_list_reference->chunk_refs.first);
    while (i_prev < data_len_original) {
      if (cref != nullptr && bchunk_data_compare(cref->link, data, data_len_original, i_prev)) {
        cref_match_first = cref;
        chunk_list_reference_skip_len += 1;
        chunk_list_reference_skip_bytes += cref->link->data_len;
        i_prev += cref->link->data_len;
        cref = cref->next;
      }
      else {
        full_match = false;
        break;
      }
    }

    if (full_match) {
      if (chunk_list_reference->total_expanded_size == data_len_original) {
        return (BChunkList *)chunk_list_reference;
      }
    }
  }

  /* End Fast-Path (first)
   * --------------------- */

#endif /* USE_FASTPATH_CHUNKS_FIRST */

  /* Copy until we have a mismatch. */
  BChunkList *chunk_list = bchunk_list_new(bs_mem, data_len_original);
  if (cref_match_first != nullptr) {
    size_t chunk_size_step = 0;
    const BChunkRef *cref = static_cast<const BChunkRef *>(chunk_list_reference->chunk_refs.first);
    while (true) {
      BChunk *chunk = cref->link;
      chunk_size_step += chunk->data_len;
      bchunk_list_append_only(bs_mem, chunk_list, chunk);
      ASSERT_CHUNKLIST_SIZE(chunk_list, chunk_size_step);
      ASSERT_CHUNKLIST_DATA(chunk_list, data);
      if (cref == cref_match_first) {
        break;
      }
      cref = cref->next;
    }
    /* Happens when bytes are removed from the end of the array. */
    if (chunk_size_step == data_len_original) {
      return chunk_list;
    }

    i_prev = chunk_size_step;
  }
  else {
    i_prev = 0;
  }

  /* ------------------------------------------------------------------------
   * Fast-Path for end chunks
   *
   * Check for trailing chunks.
   */

  /* In this case use 'chunk_list_reference_last' to define the last index
   * `index_match_last = -1`. */

  /* Warning, from now on don't use len(data) since we want to ignore chunks already matched. */
  size_t data_len = data_len_original;
#define data_len_original invalid_usage
#ifdef data_len_original
  /* Quiet warning. */
#endif

  const BChunkRef *chunk_list_reference_last = nullptr;

#ifdef USE_FASTPATH_CHUNKS_LAST
  if (!BLI_listbase_is_empty(&chunk_list_reference->chunk_refs)) {
    const BChunkRef *cref = static_cast<const BChunkRef *>(chunk_list_reference->chunk_refs.last);
    while ((cref->prev != nullptr) && (cref != cref_match_first) &&
           (cref->link->data_len <= data_len - i_prev))
    {
      const BChunk *chunk_test = cref->link;
      size_t offset = data_len - chunk_test->data_len;
      if (bchunk_data_compare(chunk_test, data, data_len, offset)) {
        data_len = offset;
        chunk_list_reference_last = cref;
        chunk_list_reference_skip_len += 1;
        chunk_list_reference_skip_bytes += cref->link->data_len;
        cref = cref->prev;
      }
      else {
        break;
      }
    }
  }

  /* End Fast-Path (last)
   * -------------------- */
#endif /* USE_FASTPATH_CHUNKS_LAST */

  /* -----------------------------------------------------------------------
   * Check for aligned chunks
   *
   * This saves a lot of searching, so use simple heuristics to detect aligned arrays.
   * (may need to tweak exact method).
   */

  bool use_aligned = false;

#ifdef USE_ALIGN_CHUNKS_TEST
  if (chunk_list->total_expanded_size == chunk_list_reference->total_expanded_size) {
    /* If we're already a quarter aligned. */
    if (data_len - i_prev <= chunk_list->total_expanded_size / 4) {
      use_aligned = true;
    }
    else {
      /* TODO: walk over chunks and check if some arbitrary amount align. */
    }
  }
#endif /* USE_ALIGN_CHUNKS_TEST */

  /* End Aligned Chunk Case
   * ----------------------- */

  if (use_aligned) {
    /* Copy matching chunks, creates using the same 'layout' as the reference. */
    const BChunkRef *cref = cref_match_first ? cref_match_first->next :
                                               static_cast<const BChunkRef *>(
                                                   chunk_list_reference->chunk_refs.first);
    while (i_prev != data_len) {
      const size_t i = i_prev + cref->link->data_len;
      BLI_assert(i != i_prev);

      if ((cref != chunk_list_reference_last) &&
          bchunk_data_compare(cref->link, data, data_len, i_prev))
      {
        bchunk_list_append(info, bs_mem, chunk_list, cref->link);
        ASSERT_CHUNKLIST_SIZE(chunk_list, i);
        ASSERT_CHUNKLIST_DATA(chunk_list, data);
      }
      else {
        bchunk_list_append_data(info, bs_mem, chunk_list, &data[i_prev], i - i_prev);
        ASSERT_CHUNKLIST_SIZE(chunk_list, i);
        ASSERT_CHUNKLIST_DATA(chunk_list, data);
      }

      cref = cref->next;

      i_prev = i;
    }
  }
  else if ((data_len - i_prev >= info->chunk_byte_size) &&
           (chunk_list_reference->chunk_refs_len >= chunk_list_reference_skip_len) &&
           (chunk_list_reference->chunk_refs.first != nullptr))
  {

    /* --------------------------------------------------------------------
     * Non-Aligned Chunk De-Duplication. */

    /* Only create a table if we have at least one chunk to search
     * otherwise just make a new one.
     *
     * Support re-arranged chunks. */

#ifdef USE_HASH_TABLE_ACCUMULATE
    size_t i_table_start = i_prev;
    const size_t table_hash_array_len = (data_len - i_prev) / info->chunk_stride;
    hash_key *table_hash_array = MEM_malloc_arrayN<hash_key>(table_hash_array_len, __func__);
    hash_array_from_data(info, &data[i_prev], data_len - i_prev, table_hash_array);

    hash_accum(table_hash_array, table_hash_array_len, info->accum_steps);
#else
    /* Dummy vars. */
    uint i_table_start = 0;
    hash_key *table_hash_array = nullptr;
#endif

    const uint chunk_list_reference_remaining_len = (chunk_list_reference->chunk_refs_len -
                                                     chunk_list_reference_skip_len) +
                                                    1;
    BTableRef *table_ref_stack = MEM_malloc_arrayN<BTableRef>(chunk_list_reference_remaining_len,
                                                              __func__);
    uint table_ref_stack_n = 0;

    const size_t table_len = chunk_list_reference_remaining_len * BCHUNK_HASH_TABLE_MUL;
    BTableRef **table = MEM_calloc_arrayN<BTableRef *>(table_len, __func__);

    /* Table_make - inline
     * include one matching chunk, to allow for repeating values. */
    {
#ifdef USE_HASH_TABLE_ACCUMULATE
      const size_t hash_store_len = info->accum_read_ahead_len;
      hash_key *hash_store = MEM_malloc_arrayN<hash_key>(hash_store_len, __func__);
#endif

      const BChunkRef *cref;
      size_t chunk_list_reference_bytes_remaining = chunk_list_reference->total_expanded_size -
                                                    chunk_list_reference_skip_bytes;

      if (cref_match_first) {
        cref = cref_match_first;
        chunk_list_reference_bytes_remaining += cref->link->data_len;
      }
      else {
        cref = static_cast<const BChunkRef *>(chunk_list_reference->chunk_refs.first);
      }

#ifdef USE_PARANOID_CHECKS
      {
        size_t test_bytes_len = 0;
        const BChunkRef *cr = cref;
        while (cr != chunk_list_reference_last) {
          test_bytes_len += cr->link->data_len;
          cr = cr->next;
        }
        BLI_assert(test_bytes_len == chunk_list_reference_bytes_remaining);
      }
#endif

      while ((cref != chunk_list_reference_last) &&
             (chunk_list_reference_bytes_remaining >= info->accum_read_ahead_bytes))
      {
        hash_key key = key_from_chunk_ref(info,
                                          cref

#ifdef USE_HASH_TABLE_ACCUMULATE
                                          ,
                                          hash_store,
                                          hash_store_len
#endif
        );
        const uint key_index = uint(key % (hash_key)table_len);
        BTableRef *tref_prev = table[key_index];
        BLI_assert(table_ref_stack_n < chunk_list_reference_remaining_len);
#ifdef USE_HASH_TABLE_DEDUPLICATE
        bool is_duplicate = false;
        if (tref_prev) {
          const BChunk *chunk_a = cref->link;
          const BTableRef *tref = tref_prev;
          do {
            /* Not an error, it just isn't expected the links are ever shared. */
            BLI_assert(tref->cref != cref);
            const BChunk *chunk_b = tref->cref->link;
#  ifdef USE_HASH_TABLE_KEY_CACHE
            if (key == chunk_b->key)
#  endif
            {
              if (chunk_a != chunk_b) {
                if (chunk_a->data_len == chunk_b->data_len) {
                  if (memcmp(chunk_a->data, chunk_b->data, chunk_a->data_len) == 0) {
                    is_duplicate = true;
                    break;
                  }
                }
              }
            }
          } while ((tref = tref->next));
        }

        if (!is_duplicate)
#endif /* USE_HASH_TABLE_DEDUPLICATE */
        {
          BTableRef *tref = &table_ref_stack[table_ref_stack_n++];
          tref->cref = cref;
          tref->next = tref_prev;
          table[key_index] = tref;
        }

        chunk_list_reference_bytes_remaining -= cref->link->data_len;
        cref = cref->next;
      }

      BLI_assert(table_ref_stack_n <= chunk_list_reference_remaining_len);

#ifdef USE_HASH_TABLE_ACCUMULATE
      MEM_freeN(hash_store);
#endif
    }
    /* Done making the table. */

    BLI_assert(i_prev <= data_len);
    for (size_t i = i_prev; i < data_len;) {
      /* Assumes exiting chunk isn't a match! */

      const BChunkRef *cref_found = table_lookup(
          info, table, table_len, i_table_start, data, data_len, i, table_hash_array);
      if (cref_found != nullptr) {
        BLI_assert(i < data_len);
        if (i != i_prev) {
          bchunk_list_append_data_n(info, bs_mem, chunk_list, &data[i_prev], i - i_prev);
          i_prev = i;
        }

        /* Now add the reference chunk. */
        {
          BChunk *chunk_found = cref_found->link;
          i += chunk_found->data_len;
          bchunk_list_append(info, bs_mem, chunk_list, chunk_found);
        }
        i_prev = i;
        BLI_assert(i_prev <= data_len);
        ASSERT_CHUNKLIST_SIZE(chunk_list, i_prev);
        ASSERT_CHUNKLIST_DATA(chunk_list, data);

        /* Its likely that the next chunk in the list will be a match, so check it! */
        while (!ELEM(cref_found->next, nullptr, chunk_list_reference_last)) {
          cref_found = cref_found->next;
          BChunk *chunk_found = cref_found->link;

          if (bchunk_data_compare(chunk_found, data, data_len, i_prev)) {
            /* May be useful to remove table data, assuming we don't have
             * repeating memory where it would be useful to re-use chunks. */
            i += chunk_found->data_len;
            bchunk_list_append(info, bs_mem, chunk_list, chunk_found);
            /* Chunk_found may be freed! */
            i_prev = i;
            BLI_assert(i_prev <= data_len);
            ASSERT_CHUNKLIST_SIZE(chunk_list, i_prev);
            ASSERT_CHUNKLIST_DATA(chunk_list, data);
          }
          else {
            break;
          }
        }
      }
      else {
        i = i + info->chunk_stride;
      }
    }

#ifdef USE_HASH_TABLE_ACCUMULATE
    MEM_freeN(table_hash_array);
#endif
    MEM_freeN(table);
    MEM_freeN(table_ref_stack);

    /* End Table Lookup
     * ---------------- */
  }

  ASSERT_CHUNKLIST_SIZE(chunk_list, i_prev);
  ASSERT_CHUNKLIST_DATA(chunk_list, data);

  /* -----------------------------------------------------------------------
   * No Duplicates to copy, write new chunks
   *
   * Trailing chunks, no matches found in table lookup above.
   * Write all new data. */
  if (i_prev != data_len) {
    bchunk_list_append_data_n(info, bs_mem, chunk_list, &data[i_prev], data_len - i_prev);
    i_prev = data_len;
  }

  BLI_assert(i_prev == data_len);
  UNUSED_VARS_NDEBUG(i_prev);

#ifdef USE_FASTPATH_CHUNKS_LAST
  if (chunk_list_reference_last != nullptr) {
    /* Write chunk_list_reference_last since it hasn't been written yet. */
    const BChunkRef *cref = chunk_list_reference_last;
    while (cref != nullptr) {
      BChunk *chunk = cref->link;
      // BLI_assert(bchunk_data_compare(chunk, data, data_len, i_prev));
      i_prev += chunk->data_len;
      /* Use simple since we assume the references chunks have already been sized correctly. */
      bchunk_list_append_only(bs_mem, chunk_list, chunk);
      ASSERT_CHUNKLIST_DATA(chunk_list, data);
      cref = cref->next;
    }
  }
#endif

#undef data_len_original

  BLI_assert(i_prev == data_len_original);
  UNUSED_VARS_NDEBUG(i_prev);

  /* Check we're the correct size and that we didn't accidentally modify the reference. */
  ASSERT_CHUNKLIST_SIZE(chunk_list, data_len_original);
  ASSERT_CHUNKLIST_SIZE(chunk_list_reference, chunk_list_reference->total_expanded_size);

  ASSERT_CHUNKLIST_DATA(chunk_list, data);

  return chunk_list;
}
/* End private API. */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Array Storage API
 * \{ */

BArrayStore *BLI_array_store_create(uint stride, uint chunk_count)
{
  BLI_assert(stride > 0 && chunk_count > 0);

  BArrayStore *bs = MEM_callocN<BArrayStore>(__func__);

  bs->info.chunk_stride = stride;
  // bs->info.chunk_count = chunk_count;

  bs->info.chunk_byte_size = chunk_count * stride;
#ifdef USE_MERGE_CHUNKS
  bs->info.chunk_byte_size_min = std::max(1u, chunk_count / BCHUNK_SIZE_MIN_DIV) * stride;
  bs->info.chunk_byte_size_max = (chunk_count * BCHUNK_SIZE_MAX_MUL) * stride;
#endif

#ifdef USE_HASH_TABLE_ACCUMULATE
  /* One is always subtracted from this `accum_steps`, this is intentional
   * as it results in reading ahead the expected amount. */
  if (stride <= sizeof(int8_t)) {
    bs->info.accum_steps = BCHUNK_HASH_TABLE_ACCUMULATE_STEPS_8BITS + 1;
  }
  else if (stride <= sizeof(int16_t)) {
    bs->info.accum_steps = BCHUNK_HASH_TABLE_ACCUMULATE_STEPS_16BITS + 1;
  }
  else if (stride <= sizeof(int32_t)) {
    bs->info.accum_steps = BCHUNK_HASH_TABLE_ACCUMULATE_STEPS_32BITS + 1;
  }
  else {
    bs->info.accum_steps = BCHUNK_HASH_TABLE_ACCUMULATE_STEPS_DEFAULT + 1;
  }

  do {
    bs->info.accum_steps -= 1;
    /* Triangle number, identifying now much read-ahead we need:
     * https://en.wikipedia.org/wiki/Triangular_number (+ 1) */
    bs->info.accum_read_ahead_len = ((bs->info.accum_steps * (bs->info.accum_steps + 1)) / 2) + 1;
    /* Only small chunk counts are likely to exceed the read-ahead length. */
  } while (UNLIKELY(chunk_count < bs->info.accum_read_ahead_len));

  bs->info.accum_read_ahead_bytes = bs->info.accum_read_ahead_len * stride;
#else
  bs->info.accum_read_ahead_bytes = std::min(size_t(BCHUNK_HASH_LEN), chunk_count) * stride;
#endif

  bs->memory.chunk_list = BLI_mempool_create(sizeof(BChunkList), 0, 512, BLI_MEMPOOL_NOP);
  bs->memory.chunk_ref = BLI_mempool_create(sizeof(BChunkRef), 0, 512, BLI_MEMPOOL_NOP);
  /* Allow iteration to simplify freeing, otherwise its not needed
   * (we could loop over all states as an alternative). */
  bs->memory.chunk = BLI_mempool_create(sizeof(BChunk), 0, 512, BLI_MEMPOOL_ALLOW_ITER);

  BLI_assert(bs->info.accum_read_ahead_bytes <= bs->info.chunk_byte_size);

  return bs;
}

static void array_store_free_data(BArrayStore *bs)
{
  /* Free chunk data. */
  {
    BLI_mempool_iter iter;
    BChunk *chunk;
    BLI_mempool_iternew(bs->memory.chunk, &iter);
    while ((chunk = static_cast<BChunk *>(BLI_mempool_iterstep(&iter)))) {
      BLI_assert(chunk->users > 0);
      MEM_freeN(chunk->data);
    }
  }

  /* Free states. */
  for (BArrayState *state = static_cast<BArrayState *>(bs->states.first), *state_next; state;
       state = state_next)
  {
    state_next = state->next;
    MEM_freeN(state);
  }
}

void BLI_array_store_destroy(BArrayStore *bs)
{
  array_store_free_data(bs);

  BLI_mempool_destroy(bs->memory.chunk_list);
  BLI_mempool_destroy(bs->memory.chunk_ref);
  BLI_mempool_destroy(bs->memory.chunk);

  MEM_freeN(bs);
}

void BLI_array_store_clear(BArrayStore *bs)
{
  array_store_free_data(bs);

  BLI_listbase_clear(&bs->states);

  BLI_mempool_clear(bs->memory.chunk_list);
  BLI_mempool_clear(bs->memory.chunk_ref);
  BLI_mempool_clear(bs->memory.chunk);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BArrayStore Statistics
 * \{ */

size_t BLI_array_store_calc_size_expanded_get(const BArrayStore *bs)
{
  size_t size_accum = 0;
  LISTBASE_FOREACH (const BArrayState *, state, &bs->states) {
    size_accum += state->chunk_list->total_expanded_size;
  }
  return size_accum;
}

size_t BLI_array_store_calc_size_compacted_get(const BArrayStore *bs)
{
  size_t size_total = 0;
  BLI_mempool_iter iter;
  const BChunk *chunk;
  BLI_mempool_iternew(bs->memory.chunk, &iter);
  while ((chunk = static_cast<BChunk *>(BLI_mempool_iterstep(&iter)))) {
    BLI_assert(chunk->users > 0);
    size_total += size_t(chunk->data_len);
  }
  return size_total;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BArrayState Access
 * \{ */

BArrayState *BLI_array_store_state_add(BArrayStore *bs,
                                       const void *data,
                                       const size_t data_len,
                                       const BArrayState *state_reference)
{
  /* Ensure we're aligned to the stride. */
  BLI_assert((data_len % bs->info.chunk_stride) == 0);

#ifdef USE_PARANOID_CHECKS
  if (state_reference) {
    BLI_assert(BLI_findindex(&bs->states, state_reference) != -1);
  }
#endif

  BChunkList *chunk_list;
  if (state_reference) {
    chunk_list = bchunk_list_from_data_merge(&bs->info,
                                             &bs->memory,
                                             (const uchar *)data,
                                             data_len,
                                             /* Re-use reference chunks. */
                                             state_reference->chunk_list);
  }
  else {
    chunk_list = bchunk_list_new(&bs->memory, data_len);
    bchunk_list_fill_from_array(&bs->info, &bs->memory, chunk_list, (const uchar *)data, data_len);
  }

  chunk_list->users += 1;

  BArrayState *state = MEM_callocN<BArrayState>(__func__);
  state->chunk_list = chunk_list;

  BLI_addtail(&bs->states, state);

#ifdef USE_PARANOID_CHECKS
  {
    size_t data_test_len;
    void *data_test = BLI_array_store_state_data_get_alloc(state, &data_test_len);
    BLI_assert(data_test_len == data_len);
    BLI_assert(memcmp(data_test, data, data_len) == 0);
    MEM_freeN(data_test);
  }
#endif

  return state;
}

void BLI_array_store_state_remove(BArrayStore *bs, BArrayState *state)
{
#ifdef USE_PARANOID_CHECKS
  BLI_assert(BLI_findindex(&bs->states, state) != -1);
#endif

  bchunk_list_decref(&bs->memory, state->chunk_list);
  BLI_remlink(&bs->states, state);

  MEM_freeN(state);
}

size_t BLI_array_store_state_size_get(const BArrayState *state)
{
  return state->chunk_list->total_expanded_size;
}

void BLI_array_store_state_data_get(const BArrayState *state, void *data)
{
#ifdef USE_PARANOID_CHECKS
  size_t data_test_len = 0;
  LISTBASE_FOREACH (BChunkRef *, cref, &state->chunk_list->chunk_refs) {
    data_test_len += cref->link->data_len;
  }
  BLI_assert(data_test_len == state->chunk_list->total_expanded_size);
#endif

  uchar *data_step = (uchar *)data;
  LISTBASE_FOREACH (BChunkRef *, cref, &state->chunk_list->chunk_refs) {
    BLI_assert(cref->link->users > 0);
    memcpy(data_step, cref->link->data, cref->link->data_len);
    data_step += cref->link->data_len;
  }
}

void *BLI_array_store_state_data_get_alloc(const BArrayState *state, size_t *r_data_len)
{
  void *data = MEM_mallocN(state->chunk_list->total_expanded_size, __func__);
  BLI_array_store_state_data_get(state, data);
  *r_data_len = state->chunk_list->total_expanded_size;
  return data;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debugging API (for testing).
 * \{ */

/* Only for test validation. */
static size_t bchunk_list_size(const BChunkList *chunk_list)
{
  size_t total_expanded_size = 0;
  LISTBASE_FOREACH (BChunkRef *, cref, &chunk_list->chunk_refs) {
    total_expanded_size += cref->link->data_len;
  }
  return total_expanded_size;
}

bool BLI_array_store_is_valid(BArrayStore *bs)
{
  bool ok = true;

  /* Check Length
   * ------------ */

  LISTBASE_FOREACH (BArrayState *, state, &bs->states) {
    BChunkList *chunk_list = state->chunk_list;
    if (!(bchunk_list_size(chunk_list) == chunk_list->total_expanded_size)) {
      return false;
    }

    if (BLI_listbase_count(&chunk_list->chunk_refs) != int(chunk_list->chunk_refs_len)) {
      return false;
    }

#ifdef USE_MERGE_CHUNKS
    /* Ensure we merge all chunks that could be merged. */
    if (chunk_list->total_expanded_size > bs->info.chunk_byte_size_min) {
      LISTBASE_FOREACH (BChunkRef *, cref, &chunk_list->chunk_refs) {
        if (cref->link->data_len < bs->info.chunk_byte_size_min) {
          return false;
        }
      }
    }
#endif
  }

  {
    BLI_mempool_iter iter;
    BChunk *chunk;
    BLI_mempool_iternew(bs->memory.chunk, &iter);
    while ((chunk = static_cast<BChunk *>(BLI_mempool_iterstep(&iter)))) {
      if (!(MEM_allocN_len(chunk->data) >= chunk->data_len)) {
        return false;
      }
    }
  }

  /* Check User Count & Lost References
   * ---------------------------------- */
  {
    GHashIterator gh_iter;

#define GHASH_PTR_ADD_USER(gh, pt) \
  { \
    void **val; \
    if (BLI_ghash_ensure_p((gh), (pt), &val)) { \
      *((int *)val) += 1; \
    } \
    else { \
      *((int *)val) = 1; \
    } \
  } \
  ((void)0)

    /* Count chunk_list's. */
    GHash *chunk_list_map = BLI_ghash_ptr_new(__func__);
    GHash *chunk_map = BLI_ghash_ptr_new(__func__);

    int totrefs = 0;
    LISTBASE_FOREACH (BArrayState *, state, &bs->states) {
      GHASH_PTR_ADD_USER(chunk_list_map, state->chunk_list);
    }
    GHASH_ITER (gh_iter, chunk_list_map) {
      const BChunkList *chunk_list = static_cast<const BChunkList *>(
          BLI_ghashIterator_getKey(&gh_iter));
      const int users = POINTER_AS_INT(BLI_ghashIterator_getValue(&gh_iter));
      if (!(chunk_list->users == users)) {
        ok = false;
        goto user_finally;
      }
    }
    if (!(BLI_mempool_len(bs->memory.chunk_list) == int(BLI_ghash_len(chunk_list_map)))) {
      ok = false;
      goto user_finally;
    }

    /* Count chunk's. */
    GHASH_ITER (gh_iter, chunk_list_map) {
      const BChunkList *chunk_list = static_cast<const BChunkList *>(
          BLI_ghashIterator_getKey(&gh_iter));
      LISTBASE_FOREACH (const BChunkRef *, cref, &chunk_list->chunk_refs) {
        GHASH_PTR_ADD_USER(chunk_map, cref->link);
        totrefs += 1;
      }
    }
    if (!(BLI_mempool_len(bs->memory.chunk) == int(BLI_ghash_len(chunk_map)))) {
      ok = false;
      goto user_finally;
    }
    if (!(BLI_mempool_len(bs->memory.chunk_ref) == totrefs)) {
      ok = false;
      goto user_finally;
    }

    GHASH_ITER (gh_iter, chunk_map) {
      const BChunk *chunk = static_cast<const BChunk *>(BLI_ghashIterator_getKey(&gh_iter));
      const int users = POINTER_AS_INT(BLI_ghashIterator_getValue(&gh_iter));
      if (!(chunk->users == users)) {
        ok = false;
        goto user_finally;
      }
    }

#undef GHASH_PTR_ADD_USER

  user_finally:
    BLI_ghash_free(chunk_list_map, nullptr, nullptr);
    BLI_ghash_free(chunk_map, nullptr, nullptr);
  }

  return ok;
  /* TODO: dangling pointer checks. */
}

/** \} */
