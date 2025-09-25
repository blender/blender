/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Custom select code for picking small regions (not efficient for large regions).
 * `gpu_select_pick_*` API.
 */
#include <cfloat>
#include <cstdlib>
#include <cstring>

#include "GPU_debug.hh"
#include "GPU_framebuffer.hh"
#include "GPU_select.hh"
#include "GPU_state.hh"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "gpu_select_private.hh"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

// #define DEBUG_PRINT

/* Alloc number for depths */
#define ALLOC_DEPTHS 200

/* Z-depth of cleared depth buffer */
#define DEPTH_MAX 0xffffffff

/* -------------------------------------------------------------------- */
/** \name #SubRectStride
 * \{ */

/** For looping over a sub-region of a #rcti, could be moved into `rct.c`. */
struct SubRectStride {
  /** Start here. */
  uint start;
  /** Read these. */
  uint span;
  /** `len` times (read span 'len' times). */
  uint span_len;
  /** Skip those. */
  uint skip;
};

/** We may want to change back to float if `uint` isn't well supported. */
using depth_t = uint;

/**
 * Calculate values needed for looping over a sub-region (smaller buffer within a larger buffer).
 *
 * 'src' must be bigger than 'dst'.
 */
static void rect_subregion_stride_calc(const rcti *src, const rcti *dst, SubRectStride *r_sub)
{
  const int src_x = BLI_rcti_size_x(src);
  // const int src_y = BLI_rcti_size_y(src);
  const int dst_x = BLI_rcti_size_x(dst);
  const int dst_y = BLI_rcti_size_y(dst);
  const int x = dst->xmin - src->xmin;
  const int y = dst->ymin - src->ymin;

  BLI_assert(src->xmin <= dst->xmin && src->ymin <= dst->ymin && src->xmax >= dst->xmax &&
             src->ymax >= dst->ymax);
  BLI_assert(x >= 0 && y >= 0);

  r_sub->start = uint((src_x * y) + x);
  r_sub->span = uint(dst_x);
  r_sub->span_len = uint(dst_y);
  r_sub->skip = uint(src_x - dst_x);
}

/**
 * Ignore depth clearing as a change,
 * only check if its been changed _and_ filled in (ignore clearing since XRAY does this).
 */
BLI_INLINE bool depth_is_filled(const depth_t *prev, const depth_t *curr)
{
  return (*prev != *curr) && (*curr != DEPTH_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #DepthBufCache
 *
 * Result of reading #GPU_framebuffer_read_depth,
 * use for both cache and non-cached storage.
 * \{ */

/** Store result of #GPU_framebuffer_read_depth. */
struct DepthBufCache {
  DepthBufCache *next, *prev;
  uint id;
  depth_t buf[0];
};

static DepthBufCache *depth_buf_malloc(uint rect_len)
{
  DepthBufCache *rect = static_cast<DepthBufCache *>(
      MEM_mallocN(sizeof(DepthBufCache) + sizeof(depth_t) * rect_len, __func__));
  rect->id = SELECT_ID_NONE;
  return rect;
}

static bool depth_buf_rect_depth_any(const DepthBufCache *rect_depth, uint rect_len)
{
  const depth_t *curr = rect_depth->buf;
  for (uint i = 0; i < rect_len; i++, curr++) {
    if (*curr != DEPTH_MAX) {
      return true;
    }
  }
  return false;
}

static bool depth_buf_subrect_depth_any(const DepthBufCache *rect_depth,
                                        const SubRectStride *sub_rect)
{
  const depth_t *curr = rect_depth->buf + sub_rect->start;
  for (uint i = 0; i < sub_rect->span_len; i++) {
    const depth_t *curr_end = curr + sub_rect->span;
    for (; curr < curr_end; curr++, curr++) {
      if (*curr != DEPTH_MAX) {
        return true;
      }
    }
    curr += sub_rect->skip;
  }
  return false;
}

static bool depth_buf_rect_depth_any_filled(const DepthBufCache *rect_prev,
                                            const DepthBufCache *rect_curr,
                                            uint rect_len)
{
#if 0
  return memcmp(rect_depth_a->buf, rect_depth_b->buf, rect_len * sizeof(depth_t)) != 0;
#else
  const depth_t *prev = rect_prev->buf;
  const depth_t *curr = rect_curr->buf;
  for (uint i = 0; i < rect_len; i++, curr++, prev++) {
    if (depth_is_filled(prev, curr)) {
      return true;
    }
  }
  return false;
#endif
}

/**
 * Both buffers are the same size, just check if the sub-rect contains any differences.
 */
static bool depth_buf_subrect_depth_any_filled(const DepthBufCache *rect_src,
                                               const DepthBufCache *rect_dst,
                                               const SubRectStride *sub_rect)
{
  /* Same as #depth_buf_rect_depth_any_filled but different rectangle sizes. */
  const depth_t *prev = rect_src->buf + sub_rect->start;
  const depth_t *curr = rect_dst->buf + sub_rect->start;
  for (uint i = 0; i < sub_rect->span_len; i++) {
    const depth_t *curr_end = curr + sub_rect->span;
    for (; curr < curr_end; prev++, curr++) {
      if (depth_is_filled(prev, curr)) {
        return true;
      }
    }
    prev += sub_rect->skip;
    curr += sub_rect->skip;
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #DepthID
 *
 * Internal structure for storing hits.
 * \{ */

struct DepthID {
  uint id;
  depth_t depth;
};

static int depth_id_cmp(const void *v1, const void *v2)
{
  const DepthID *d1 = static_cast<const DepthID *>(v1), *d2 = static_cast<const DepthID *>(v2);
  if (d1->id < d2->id) {
    return -1;
  }
  if (d1->id > d2->id) {
    return 1;
  }

  return 0;
}

static int depth_cmp(const void *v1, const void *v2)
{
  const DepthID *d1 = static_cast<const DepthID *>(v1), *d2 = static_cast<const DepthID *>(v2);
  if (d1->depth < d2->depth) {
    return -1;
  }
  if (d1->depth > d2->depth) {
    return 1;
  }

  return 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Selection Begin/End/Load API
 * \{ */

/** Depth sorting. */
struct GPUPickState {
  /** Cache on initialization. */
  GPUSelectBuffer *buffer;
  /** Mode of this operation. */
  GPUSelectMode mode;

  /** GPU drawing, never use when `is_cached == true`. */
  struct {
    /** The current depth, accumulated while drawing. */
    DepthBufCache *rect_depth;
    /** Scratch buffer, avoid allocations every time (when not caching). */
    DepthBufCache *rect_depth_test;

    /** Pass to `GPU_framebuffer_read_depth(x, y, w, h)`. */
    int clip_readpixels[4];

    /** Set after first draw. */
    bool is_init;
    uint prev_id;
  } gpu;

  /**
   * `src`: data stored in 'cache' and 'gpu',
   * `dst`: use when cached region is smaller (where `src` -> `dst` isn't 1:1).
   */
  struct {
    rcti clip_rect;
    uint rect_len;
  } src, dst;

  /** Store cache between `GPU_select_cache_begin/end` */
  bool use_cache;
  bool is_cached;
  struct {
    /**
     * Cleanup used for iterating over both source and destination buffers:
     * `src.clip_rect` -> `dst.clip_rect`.
     */
    SubRectStride sub_rect;

    /** List of #DepthBufCache, sized of 'src.clip_rect'. */
    ListBase bufs;
  } cache;

  /** Picking methods. */
  union {
    /** #GPU_SELECT_PICK_ALL */
    struct {
      DepthID *hits;
      uint hits_len;
      uint hits_len_alloc;
    } all;

    /** #GPU_SELECT_PICK_NEAREST */
    struct {
      uint *rect_id;
    } nearest;
  };

  /** Previous state to restore after drawing. */
  int viewport[4];
  int scissor[4];
  GPUWriteMask write_mask;
  GPUDepthTest depth_test;
};

static GPUPickState g_pick_state{};

void gpu_select_pick_begin(GPUSelectBuffer *buffer, const rcti *input, GPUSelectMode mode)
{
  GPUPickState *ps = &g_pick_state;

#ifdef DEBUG_PRINT
  printf("%s: mode=%d, use_cache=%d, is_cache=%d\n",
         __func__,
         int(mode),
         ps->use_cache,
         ps->is_cached);
#endif

  GPU_debug_group_begin("Selection Pick");

  ps->buffer = buffer;
  ps->mode = mode;

  const uint rect_len = uint(BLI_rcti_size_x(input) * BLI_rcti_size_y(input));
  ps->dst.clip_rect = *input;
  ps->dst.rect_len = rect_len;

  /* Avoids unnecessary GPU operations when cache is available and they are unnecessary. */
  if (ps->is_cached == false) {
    ps->write_mask = GPU_write_mask_get();
    ps->depth_test = GPU_depth_test_get();
    GPU_scissor_get(ps->scissor);

    /* Disable writing to the frame-buffer. */
    GPU_color_mask(false, false, false, false);

    GPU_depth_mask(true);
    /* Always use #GPU_DEPTH_LESS_EQUAL even though #GPU_SELECT_PICK_ALL always clears the buffer.
     * This is because individual objects themselves might have sections that overlap and we need
     * these to have the correct distance information. */
    GPU_depth_test(GPU_DEPTH_LESS_EQUAL);

    float viewport[4];
    GPU_viewport_size_get_f(viewport);

    ps->src.clip_rect = *input;
    ps->src.rect_len = rect_len;

    ps->gpu.clip_readpixels[0] = int(viewport[0]);
    ps->gpu.clip_readpixels[1] = int(viewport[1]);
    ps->gpu.clip_readpixels[2] = BLI_rcti_size_x(&ps->src.clip_rect);
    ps->gpu.clip_readpixels[3] = BLI_rcti_size_y(&ps->src.clip_rect);

    GPU_viewport(UNPACK4(ps->gpu.clip_readpixels));

    /* It's possible we don't want to clear depth buffer,
     * so existing elements are masked by current z-buffer. */
    GPU_clear_depth(1.0f);

    /* scratch buffer (read new values here) */
    ps->gpu.rect_depth_test = depth_buf_malloc(rect_len);
    ps->gpu.rect_depth = depth_buf_malloc(rect_len);

    /* Set initial 'far' value. */
    for (uint i = 0; i < rect_len; i++) {
      ps->gpu.rect_depth->buf[i] = DEPTH_MAX;
    }

    ps->gpu.is_init = false;
    ps->gpu.prev_id = 0;
  }
  else {
    /* Using cache `ps->is_cached == true`. */
    /* `src.clip_rect` -> `dst.clip_rect`. */
    rect_subregion_stride_calc(&ps->src.clip_rect, &ps->dst.clip_rect, &ps->cache.sub_rect);
    BLI_assert(ps->gpu.rect_depth == nullptr);
    BLI_assert(ps->gpu.rect_depth_test == nullptr);
  }

  if (mode == GPU_SELECT_PICK_ALL) {
    ps->all.hits = static_cast<DepthID *>(
        MEM_mallocN(sizeof(*ps->all.hits) * ALLOC_DEPTHS, __func__));
    ps->all.hits_len = 0;
    ps->all.hits_len_alloc = ALLOC_DEPTHS;
  }
  else {
    /* Set to 0xff for #SELECT_ID_NONE. */
    ps->nearest.rect_id = MEM_malloc_arrayN<uint>(ps->dst.rect_len, __func__);
    memset(ps->nearest.rect_id, 0xff, sizeof(uint) * ps->dst.rect_len);
  }
}

/**
 * Given 2x depths, we know are different - update the depth information
 * use for both cached/uncached depth buffers.
 */
static void gpu_select_load_id_pass_all(const DepthBufCache *rect_curr)
{
  GPUPickState *ps = &g_pick_state;
  const uint id = rect_curr->id;
  /* find the best depth for this pass and store in 'all.hits' */
  depth_t depth_best = DEPTH_MAX;

#define EVAL_TEST() \
  if (depth_best > *curr) { \
    depth_best = *curr; \
  } \
  ((void)0)

  if (ps->is_cached == false) {
    const depth_t *curr = rect_curr->buf;
    BLI_assert(ps->src.rect_len == ps->dst.rect_len);
    const uint rect_len = ps->src.rect_len;
    for (uint i = 0; i < rect_len; i++, curr++) {
      EVAL_TEST();
    }
  }
  else {
    /* Same as above but different rectangle sizes. */
    const depth_t *curr = rect_curr->buf + ps->cache.sub_rect.start;
    for (uint i = 0; i < ps->cache.sub_rect.span_len; i++) {
      const depth_t *curr_end = curr + ps->cache.sub_rect.span;
      for (; curr < curr_end; curr++) {
        EVAL_TEST();
      }
      curr += ps->cache.sub_rect.skip;
    }
  }

#undef EVAL_TEST

  /* Ensure enough space. */
  if (UNLIKELY(ps->all.hits_len == ps->all.hits_len_alloc)) {
    ps->all.hits_len_alloc += ALLOC_DEPTHS;
    ps->all.hits = static_cast<DepthID *>(
        MEM_reallocN(ps->all.hits, ps->all.hits_len_alloc * sizeof(*ps->all.hits)));
  }
  DepthID *d = &ps->all.hits[ps->all.hits_len++];
  d->id = id;
  d->depth = depth_best;
}

static void gpu_select_load_id_pass_nearest(const DepthBufCache *rect_prev,
                                            const DepthBufCache *rect_curr)
{
  GPUPickState *ps = &g_pick_state;
  const uint id = rect_curr->id;
  /* Keep track each pixels ID in `nearest.rect_id`. */
  if (id != SELECT_ID_NONE) {
    uint *id_ptr = ps->nearest.rect_id;

/* Check against DEPTH_MAX because XRAY will clear the buffer,
 * so previously set values will become unset.
 * In this case just leave those id's left as-is. */
#define EVAL_TEST() \
  if (depth_is_filled(prev, curr)) { \
    *id_ptr = id; \
  } \
  ((void)0)

    if (ps->is_cached == false) {
      const depth_t *prev = rect_prev->buf;
      const depth_t *curr = rect_curr->buf;
      BLI_assert(ps->src.rect_len == ps->dst.rect_len);
      const uint rect_len = ps->src.rect_len;
      for (uint i = 0; i < rect_len; i++, curr++, prev++, id_ptr++) {
        EVAL_TEST();
      }
    }
    else {
      /* same as above but different rect sizes */
      const depth_t *prev = rect_prev->buf + ps->cache.sub_rect.start;
      const depth_t *curr = rect_curr->buf + ps->cache.sub_rect.start;
      for (uint i = 0; i < ps->cache.sub_rect.span_len; i++) {
        const depth_t *curr_end = curr + ps->cache.sub_rect.span;
        for (; curr < curr_end; prev++, curr++, id_ptr++) {
          EVAL_TEST();
        }
        prev += ps->cache.sub_rect.skip;
        curr += ps->cache.sub_rect.skip;
      }
    }

#undef EVAL_TEST
  }
}

bool gpu_select_pick_load_id(uint id, bool end)
{
  GPUPickState *ps = &g_pick_state;

  if (ps->gpu.is_init) {
    if (id == ps->gpu.prev_id && !end) {
      /* No need to read if we are still drawing for the same id since
       * all these depths will be merged / de-duplicated in the end. */
      return true;
    }

    const uint rect_len = ps->src.rect_len;
    blender::gpu::FrameBuffer *fb = GPU_framebuffer_active_get();
    GPU_framebuffer_read_depth(
        fb, UNPACK4(ps->gpu.clip_readpixels), GPU_DATA_UINT, ps->gpu.rect_depth_test->buf);
    /* Perform initial check since most cases the array remains unchanged. */

    bool do_pass = false;
    if (g_pick_state.mode == GPU_SELECT_PICK_ALL) {
      if (depth_buf_rect_depth_any(ps->gpu.rect_depth_test, rect_len)) {
        ps->gpu.rect_depth_test->id = ps->gpu.prev_id;
        gpu_select_load_id_pass_all(ps->gpu.rect_depth_test);
        do_pass = true;
      }
    }
    else {
      if (depth_buf_rect_depth_any_filled(ps->gpu.rect_depth, ps->gpu.rect_depth_test, rect_len)) {
        ps->gpu.rect_depth_test->id = ps->gpu.prev_id;
        gpu_select_load_id_pass_nearest(ps->gpu.rect_depth, ps->gpu.rect_depth_test);
        do_pass = true;
      }
    }

    if (do_pass) {
      /* Store depth in cache */
      if (ps->use_cache) {
        BLI_addtail(&ps->cache.bufs, ps->gpu.rect_depth);
        ps->gpu.rect_depth = depth_buf_malloc(ps->src.rect_len);
      }

      std::swap(ps->gpu.rect_depth, ps->gpu.rect_depth_test);

      if (g_pick_state.mode == GPU_SELECT_PICK_ALL) {
        /* (fclem) This is to be on the safe side. I don't know if this is required. */
        bool prev_depth_mask = GPU_depth_mask_get();
        /* we want new depths every time */
        GPU_depth_mask(true);
        GPU_clear_depth(1.0f);

        GPU_depth_mask(prev_depth_mask);
      }
    }
  }

  ps->gpu.is_init = true;
  ps->gpu.prev_id = id;

  return true;
}

uint gpu_select_pick_end()
{
  GPUPickState *ps = &g_pick_state;

#ifdef DEBUG_PRINT
  printf("%s\n", __func__);
#endif

  if (ps->is_cached == false) {
    if (ps->gpu.is_init) {
      /* force finishing last pass */
      gpu_select_pick_load_id(ps->gpu.prev_id, true);
    }
    GPU_write_mask(ps->write_mask);
    GPU_depth_test(ps->depth_test);
    GPU_viewport(UNPACK4(ps->viewport));
  }

  GPU_debug_group_end();

  /* Assign but never free directly since it may be in cache. */
  DepthBufCache *rect_depth_final;

  /* Store depth in cache */
  if (ps->use_cache && !ps->is_cached) {
    BLI_addtail(&ps->cache.bufs, ps->gpu.rect_depth);
    ps->gpu.rect_depth = nullptr;
    rect_depth_final = static_cast<DepthBufCache *>(ps->cache.bufs.last);
  }
  else if (ps->is_cached) {
    rect_depth_final = static_cast<DepthBufCache *>(ps->cache.bufs.last);
  }
  else {
    /* Common case, no cache. */
    rect_depth_final = ps->gpu.rect_depth;
  }

  DepthID *depth_data;
  uint depth_data_len = 0;

  if (g_pick_state.mode == GPU_SELECT_PICK_ALL) {
    depth_data = ps->all.hits;
    depth_data_len = ps->all.hits_len;
    /* Move ownership. */
    ps->all.hits = nullptr;
    ps->all.hits_len = 0;
    ps->all.hits_len_alloc = 0;
  }
  else {
    /* #GPU_SELECT_PICK_NEAREST */

    /* Over allocate (unlikely we have as many depths as pixels). */
    uint depth_data_len_first_pass = 0;
    depth_data = static_cast<DepthID *>(
        MEM_mallocN(ps->dst.rect_len * sizeof(*depth_data), __func__));

    /* Partially de-duplicating copy,
     * when contiguous ID's are found - update their closest depth.
     * This isn't essential but means there is less data to sort. */

#define EVAL_TEST(i_src, i_dst) \
  { \
    const uint id = ps->nearest.rect_id[i_dst]; \
    if (id != SELECT_ID_NONE) { \
      const depth_t depth = rect_depth_final->buf[i_src]; \
      if (depth_last == nullptr || depth_last->id != id) { \
        DepthID *d = &depth_data[depth_data_len_first_pass++]; \
        d->id = id; \
        d->depth = depth; \
      } \
      else if (depth_last->depth > depth) { \
        depth_last->depth = depth; \
      } \
    } \
  } \
  ((void)0)

    {
      DepthID *depth_last = nullptr;
      if (ps->is_cached == false) {
        for (uint i = 0; i < ps->src.rect_len; i++) {
          EVAL_TEST(i, i);
        }
      }
      else {
        /* Same as above but different rectangle sizes. */
        uint i_src = ps->cache.sub_rect.start, i_dst = 0;
        for (uint j = 0; j < ps->cache.sub_rect.span_len; j++) {
          const uint i_src_end = i_src + ps->cache.sub_rect.span;
          for (; i_src < i_src_end; i_src++, i_dst++) {
            EVAL_TEST(i_src, i_dst);
          }
          i_src += ps->cache.sub_rect.skip;
        }
      }
    }

#undef EVAL_TEST

    qsort(depth_data, depth_data_len_first_pass, sizeof(DepthID), depth_id_cmp);

    /* Sort by ID's then keep the best depth for each ID. */
    depth_data_len = 0;
    {
      DepthID *depth_last = nullptr;
      for (uint i = 0; i < depth_data_len_first_pass; i++) {
        if (depth_last == nullptr || depth_last->id != depth_data[i].id) {
          depth_last = &depth_data[depth_data_len++];
          *depth_last = depth_data[i];
        }
        else if (depth_last->depth > depth_data[i].depth) {
          depth_last->depth = depth_data[i].depth;
        }
      }
    }
  }

  /* Finally sort each unique (id, depth) pair by depth
   * so the final hit-list is sorted by depth (nearest first). */
  uint hits = 0;

  /* Leave sorting up to the caller. */
  qsort(depth_data, depth_data_len, sizeof(DepthID), depth_cmp);

  g_pick_state.buffer->storage.reserve(g_pick_state.buffer->storage.size() + depth_data_len);
  for (uint i = 0; i < depth_data_len; i++) {
#ifdef DEBUG_PRINT
    printf("  hit: %u: depth %u\n", depth_data[i].id, depth_data[i].depth);
#endif
    GPUSelectResult hit_result{};
    hit_result.id = depth_data[i].id;
    hit_result.depth = depth_data[i].depth;
    g_pick_state.buffer->storage.append_unchecked(hit_result);
    hits++;
  }

  MEM_freeN(depth_data);

  MEM_SAFE_FREE(ps->gpu.rect_depth);
  MEM_SAFE_FREE(ps->gpu.rect_depth_test);

  if (g_pick_state.mode == GPU_SELECT_PICK_ALL) {
    /* 'hits' already freed as 'depth_data' */
  }
  else {
    MEM_freeN(ps->nearest.rect_id);
    ps->nearest.rect_id = nullptr;
  }

  if (ps->use_cache) {
    ps->is_cached = true;
  }

  return hits;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Caching
 *
 * Support multiple begin/end's reusing depth buffers.
 * \{ */

void gpu_select_pick_cache_begin()
{
  BLI_assert(g_pick_state.use_cache == false);
#ifdef DEBUG_PRINT
  printf("%s\n", __func__);
#endif
  g_pick_state.use_cache = true;
  g_pick_state.is_cached = false;
}

void gpu_select_pick_cache_end()
{
#ifdef DEBUG_PRINT
  printf("%s: with %d buffers\n", __func__, BLI_listbase_count(&g_pick_state.cache.bufs));
#endif
  g_pick_state.use_cache = false;
  g_pick_state.is_cached = false;

  BLI_freelistN(&g_pick_state.cache.bufs);
}

bool gpu_select_pick_is_cached()
{
  return g_pick_state.is_cached;
}

void gpu_select_pick_cache_load_id()
{
  BLI_assert(g_pick_state.is_cached == true);
  GPUPickState *ps = &g_pick_state;
#ifdef DEBUG_PRINT
  printf("%s (building depth from cache)\n", __func__);
#endif
  LISTBASE_FOREACH (DepthBufCache *, rect_depth, &ps->cache.bufs) {
    if (rect_depth->next != nullptr) {
      /* We know the buffers differ, but this sub-region may not.
       * Double check before adding an id-pass. */
      if (g_pick_state.mode == GPU_SELECT_PICK_ALL) {
        if (depth_buf_subrect_depth_any(rect_depth->next, &ps->cache.sub_rect)) {
          gpu_select_load_id_pass_all(rect_depth->next);
        }
      }
      else {
        if (depth_buf_subrect_depth_any_filled(rect_depth, rect_depth->next, &ps->cache.sub_rect))
        {
          gpu_select_load_id_pass_nearest(rect_depth, rect_depth->next);
        }
      }
    }
  }
}

/** \} */
