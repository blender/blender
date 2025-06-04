/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 *
 * \brief Run length encoding for arrays.
 *
 * The intended use is to pre-process arrays before storing in #BArrayStore.
 * This should be used in cases arrays are likely to contain large spans of contiguous data
 * (which doesn't de-duplicate so well).
 *
 * Intended for byte arrays as there is no special logic to handle alignment.
 * Note that this could be supported and would be useful to de-duplicate
 * repeating patterns of non-byte data.
 *
 * Notes:
 * - For random data, the size overhead is only `sizeof(size_t[4])` (header & footer).
 *
 * - The main down-side in that case of random data is detecting there are no spans to RLE encode,
 *   and creating the "encoded" copy.
 *
 * - For an array containing a single value the resulting size
 *   will be `sizeof(size_t[3]) + sizeof(uint8_t)`.
 *
 * - This is not intended to be used for compression, it would be possible
 *   to use less memory by packing the size of short spans into fewer bits.
 *   This isn't done as it requires more computation when encoding.
 *
 * - This RLE implementation is a balance between working well for random bytes
 *   as well as arrays containing large contiguous spans.
 *
 *   There is *some* bias towards performing well with arrays containing contiguous spans
 *   mainly because the benefits are greater and the likelihood is that RLE encoding is used
 *   because there is a probability the data will be able to take advantage of RLE.
 *   Having said this - encoding random bytes must not be *slow* either.
 */

#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_assert.h"
#include "BLI_utildefines.h"

#include "BLI_array_store.h" /* Own include. */

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

/**
 * Use faster method of spanning for change by stepping over larger values.
 *
 * NOTE(@ideasman42) In practice this gives ~3.5x overall speedup when encoding large arrays.
 * For random data the performance is worse, about ~5% slower.
 */
#define USE_FIND_FASTPATH

static size_t find_byte_not_equal_to(const uint8_t *data,
                                     size_t offset,
                                     const size_t size,
                                     const uint8_t value)
{
  BLI_assert(offset <= size);

#ifdef USE_FIND_FASTPATH
  using fast_int = uintptr_t;

  /* In the case of random data, early exit without entering more involved steps. */

  /* Calculate the minimum size which may use an optimized search. */
  constexpr size_t min_size_for_fast_path = (
      /* Pass 1: scans a fixed size. */
      sizeof(size_t[2]) +
      /* Pass 2: scans a fixed size but aligns to `fast_int`. */
      sizeof(size_t) + sizeof(fast_int) +
      /* Pass 3: trims the end of `data` by `fast_int`
       * add to ensure there is at least one item to read. */
      sizeof(fast_int));

  if (LIKELY(size - offset > min_size_for_fast_path)) {

    /* Pass 1: Scan forward  with a fixed size to check if an early exit
     * is needed (this may exit on the first few bytes). */
    const uint8_t *p = data + offset;
    const uint8_t *p_end = p + sizeof(size_t[2]);
    do {
      if (LIKELY(*p != value)) {
        return size_t(p - data);
      }
      p++;
    } while (p < p_end);
    /* `offset` is no longer valid and needs to be updated from `p` before use. */

    /* Pass 2: Scan forward at least `sizeof(size_t)` bytes,
     * aligned to the next `sizeof(fast_int)` aligned boundary. */
    p_end = reinterpret_cast<const uint8_t *>(
        ((uintptr_t(p) + sizeof(size_t) + sizeof(fast_int)) & ~(sizeof(fast_int) - 1)));
    do {
      if (LIKELY(*p != value)) {
        return size_t(p - data);
      }
      p++;
    } while (p < p_end);

    /* Pass 3: Scan forward the `fast_int` aligned chunks (the fast path).
     * This block is responsible for scanning over large spans of contiguous bytes. */

    /* There are at least `sizeof(size_t[2])` number of bytes all equal.
     * Use `fast_int` aligned reads for a faster search. */
    BLI_assert((uintptr_t(p) & (sizeof(fast_int) - 1)) == 0);
    const fast_int *p_fast = reinterpret_cast<const fast_int *>(p);
    /* Not aligned, but this doesn't matter as it's only used for comparison. */
    const fast_int *p_fast_last = reinterpret_cast<const fast_int *>(data +
                                                                     (size - sizeof(fast_int)));
    BLI_assert(p_fast <= p_fast_last);
    fast_int value_fast;
    memset(&value_fast, value, sizeof(value_fast));
    do {
      /* Use unlikely given many of the previous bytes match. */
      if (UNLIKELY(*p_fast != value_fast)) {
        break;
      }
      p_fast++;
    } while (p_fast <= p_fast_last);
    offset = size_t(reinterpret_cast<const uint8_t *>(p_fast) - data);
    /* Perform byte level check with any trailing data. */
  }
#endif /* USE_FIND_FASTPATH */

  while ((offset < size) && (value == data[offset])) {
    offset += 1;
  }
  return offset;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Private API
 * \{ */

struct RLE_Head {
  /**
   * - When zero, this struct is interpreted as a #RLE_Literal.
   * - When non-zero, this struct is interpreted as a #RLE_Span.
   *   The `value` is a `uint_8` (to reduce the size of the struct).
   */
  size_t span_size;
};

struct RLE_Literal {
  uint8_t _span_size_pad[sizeof(size_t)];
  size_t value;
};

struct RLE_Span {
  uint8_t _span_size_pad[sizeof(size_t)];
  uint8_t value;
};
BLI_STATIC_ASSERT(sizeof(RLE_Span) == sizeof(size_t) + sizeof(uint8_t), "");

struct RLE_Elem {
  union {
    RLE_Head head;
    RLE_Span span;
    RLE_Literal literal;
  };
};

struct RLE_ElemChunk {
  RLE_ElemChunk *next;
  size_t links_num;
  /** Use 4KB chunks for efficient small allocations. */
  RLE_Elem links[(4096 / sizeof(RLE_Elem)) -
                 (sizeof(RLE_ElemChunk *) + sizeof(size_t) + MEM_SIZE_OVERHEAD)];
};
BLI_STATIC_ASSERT(sizeof(RLE_ElemChunk) <= 4096 - MEM_SIZE_OVERHEAD, "");

struct RLE_ElemChunkIter {
  RLE_ElemChunk *iter;
  size_t link_curr;
};

static void rle_link_chunk_iter_new(RLE_ElemChunk *links_block, RLE_ElemChunkIter *link_block_iter)
{
  link_block_iter->iter = links_block;
  link_block_iter->link_curr = 0;
}

static RLE_Elem *rle_link_chunk_iter_step(RLE_ElemChunkIter *link_block_iter)
{
  RLE_ElemChunk *link_block = link_block_iter->iter;
  if (link_block_iter->link_curr < link_block->links_num) {
    return &link_block->links[link_block_iter->link_curr++];
  }
  if (link_block->next) {
    link_block = link_block_iter->iter = link_block->next;
    link_block_iter->link_curr = 1;
    return &link_block->links[0];
  }
  return nullptr;
}

static RLE_ElemChunk *rle_link_chunk_new()
{
  RLE_ElemChunk *link_block = MEM_mallocN<RLE_ElemChunk>(__func__);
  link_block->next = nullptr;
  link_block->links_num = 0;
  return link_block;
}

static void rle_link_chunk_free_all(RLE_ElemChunk *link_block)
{
  while (RLE_ElemChunk *link_iter = link_block) {
    link_block = link_iter->next;
    MEM_freeN(link_iter);
  }
}

static RLE_Elem *rle_link_chunk_elem_new(RLE_ElemChunk **link_block_p)
{
  RLE_ElemChunk *link_block = *link_block_p;
  if (UNLIKELY(link_block->links_num == ARRAY_SIZE(link_block->links))) {
    RLE_ElemChunk *link_block_next = rle_link_chunk_new();
    link_block->next = link_block_next;
    link_block = link_block_next;
    *link_block_p = link_block_next;
  }
  return &link_block->links[link_block->links_num++];
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

uint8_t *BLI_array_store_rle_encode(const uint8_t *data_dec,
                                    const size_t data_dec_len,
                                    const size_t data_enc_extra_size,
                                    size_t *r_data_enc_len)
{
  size_t data_enc_alloc_size = data_enc_extra_size +
                               sizeof(RLE_Literal); /* A single null terminator. */

  /* Notes on the threshold for choosing when to include literal data or RLE encode.
   * From testing a ~4 million array of booleans.
   *
   * Regarding space efficiency:
   *
   * - For data with fewer changes: `sizeof(RLE_Literal)` (16 on a 64bit system) is optimal.
   *   The improvement varies, between 5-20%.
   * - For random data: `sizeof(RLE_Literal) + sizeof(size_t)` (24 on a 64bit system) is optimal.
   *   The improvement is only ~5% though.
   *
   * The time difference between each is roughly the same.
   */
  constexpr size_t rle_skip_threshold = sizeof(RLE_Literal);

  RLE_ElemChunk *link_blocks = rle_link_chunk_new();
  RLE_ElemChunk *link_blocks_first = link_blocks;

  /* Re-use results from scanning ahead (as needed). */
  for (size_t ofs_dec = 0, span_skip_next = 1; ofs_dec < data_dec_len;) {
    /* Scan ahead to detect the size of the non-RLE span. */
    size_t ofs_dec_next = ofs_dec + span_skip_next;
    span_skip_next = 1;

    /* Detect and use the `span` if possible. */
    uint8_t value_start = data_dec[ofs_dec];
    ofs_dec_next = find_byte_not_equal_to(data_dec, ofs_dec_next, data_dec_len, value_start);

    RLE_Elem *e = rle_link_chunk_elem_new(&link_blocks);
    const size_t span = ofs_dec_next - ofs_dec;
    if (span >= rle_skip_threshold) {
      /* Catch off by one errors. */
      BLI_assert(data_dec[ofs_dec] == data_dec[(ofs_dec + span) - 1]);
      BLI_assert((ofs_dec + span == data_dec_len) ||
                 (data_dec[ofs_dec] != data_dec[(ofs_dec + span)]));
      e->head.span_size = span;
      e->span.value = value_start;
      data_enc_alloc_size += sizeof(RLE_Span);
    }
    else {
      /* A large enough span was not found,
       * scan ahead to detect the size of the non-RLE span. */

      /* Check the offset isn't at the very end of the array. */
      size_t ofs_dec_test = ofs_dec_next + 1;
      if (LIKELY(ofs_dec_test < data_dec_len)) {
        /* The first value that changed, start searching here. */
        size_t ofs_dec_test_start = ofs_dec_next;
        value_start = data_dec[ofs_dec_test_start];
        while (true) {
          if (value_start == data_dec[ofs_dec_test]) {
            ofs_dec_test += 1;
            const size_t span_test = ofs_dec_test - ofs_dec_test_start;
            BLI_assert(span_test <= rle_skip_threshold);
            if (span_test == rle_skip_threshold) {
              /* Write the span of non-RLE data,
               * then start scanning the magnitude of the RLE span at the start of the loop. */
              span_skip_next = span_test;
              ofs_dec_next = ofs_dec_test_start;
              break;
            }
          }
          else {
            BLI_assert(ofs_dec_test - ofs_dec_test_start < rle_skip_threshold);
            value_start = data_dec[ofs_dec_test];
            ofs_dec_test_start = ofs_dec_test;
            ofs_dec_test += 1;
          }

          if (UNLIKELY(ofs_dec_test == data_dec_len)) {
            ofs_dec_next = data_dec_len;
            break;
          }
        }
      }
      else {
        ofs_dec_next = data_dec_len;
      }

      /* Interleave the #RLE_Literal. */
      const size_t non_rle_span = ofs_dec_next - ofs_dec;
      e->head.span_size = 0;
      e->literal.value = non_rle_span;
      data_enc_alloc_size += sizeof(RLE_Literal) + non_rle_span;
    }

    ofs_dec = ofs_dec_next;
  }

  /* Encode RLE and literal data into this flat buffer. */
  uint8_t *data_enc = MEM_malloc_arrayN<uint8_t>(data_enc_alloc_size, __func__);
  data_enc += data_enc_extra_size;

  size_t ofs_enc = 0;
  size_t ofs_dec = 0;

  RLE_ElemChunkIter link_block_iter;
  rle_link_chunk_iter_new(link_blocks_first, &link_block_iter);
  while (RLE_Elem *e = rle_link_chunk_iter_step(&link_block_iter)) {
    BLI_assert(ofs_dec <= data_dec_len);

    if (e->head.span_size) {
      memcpy(data_enc + ofs_enc, &e->span, sizeof(RLE_Span));
      ofs_enc += sizeof(RLE_Span);
      ofs_dec += e->head.span_size;
    }
    else {
      memcpy(data_enc + ofs_enc, &e->literal, sizeof(RLE_Literal));
      ofs_enc += sizeof(RLE_Literal);
      BLI_assert(e->literal.value > 0);
      const size_t non_rle_span = e->literal.value;
      memcpy(data_enc + ofs_enc, data_dec + ofs_dec, non_rle_span);
      ofs_enc += non_rle_span;
      ofs_dec += non_rle_span;
    }
  }
  rle_link_chunk_free_all(link_blocks_first);
  BLI_assert(data_enc_extra_size + ofs_enc + sizeof(RLE_Literal) == data_enc_alloc_size);
  BLI_assert(ofs_dec == data_dec_len);

  /* Set the `RLE_Literal` span & value to 0 to terminate. */
  memset(data_enc + ofs_enc, 0x0, sizeof(RLE_Literal));

  *r_data_enc_len = data_enc_alloc_size - data_enc_extra_size;

  data_enc -= data_enc_extra_size;
  return data_enc;
}

void BLI_array_store_rle_decode(const uint8_t *data_enc,
                                const size_t data_enc_len,
                                void *data_dec_v,
                                const size_t data_dec_len)
{
  /* NOTE: `data_enc_len` & `data_dec_len` could be omitted.
   * They're just to ensure data isn't corrupt. */
  uint8_t *data_dec = reinterpret_cast<uint8_t *>(data_dec_v);
  size_t ofs_enc = 0;
  size_t ofs_dec = 0;

  while (true) {
    /* Copy as this may not be aligned. */
    RLE_Head e;
    memcpy(&e, data_enc + ofs_enc, sizeof(RLE_Head));
    ofs_enc += sizeof(RLE_Head);
    if (e.span_size != 0) {
      /* Read #RLE_Span::value directly from memory. */
      const uint8_t value = *reinterpret_cast<const uint8_t *>(data_enc + ofs_enc);
      memset(data_dec + ofs_dec, int(value), e.span_size);
      ofs_enc += sizeof(uint8_t);
      ofs_dec += e.span_size;
    }
    else {
      /* Read #RLE_Literal::value directly from memory. */
      size_t non_rle_span;
      memcpy(&non_rle_span, data_enc + ofs_enc, sizeof(size_t));
      ofs_enc += sizeof(size_t);
      if (non_rle_span) {
        memcpy(data_dec + ofs_dec, data_enc + ofs_enc, non_rle_span);
        ofs_enc += non_rle_span;
        ofs_dec += non_rle_span;
      }
      else {
        /* Both are zero - an end-of-buffer signal. */
        break;
      }
    }
  }
  BLI_assert(ofs_enc == data_enc_len);
  BLI_assert(ofs_dec == data_dec_len);
  UNUSED_VARS_NDEBUG(data_enc_len, data_dec_len);
}

/** \} */
