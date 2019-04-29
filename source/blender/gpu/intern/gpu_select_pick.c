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
 * The Original Code is Copyright (C) 2017 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * Custom select code for picking small regions (not efficient for large regions).
 * `gpu_select_pick_*` API.
 */
#include <string.h>
#include <stdlib.h>
#include <float.h>

#include "GPU_immediate.h"
#include "GPU_draw.h"
#include "GPU_select.h"
#include "GPU_extensions.h"
#include "GPU_glew.h"

#include "MEM_guardedalloc.h"

#include "BLI_rect.h"
#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "gpu_select_private.h"

#include "BLI_strict_flags.h"

/* #define DEBUG_PRINT */

/* Alloc number for depths */
#define ALLOC_DEPTHS 200

/* Z-depth of cleared depth buffer */
#define DEPTH_MAX 0xffffffff

/* ----------------------------------------------------------------------------
 * SubRectStride
 */

/* For looping over a sub-region of a rect, could be moved into 'rct.c'*/
typedef struct SubRectStride {
  uint start;    /* start here */
  uint span;     /* read these */
  uint span_len; /* len times (read span 'len' times). */
  uint skip;     /* skip those */
} SubRectStride;

/* we may want to change back to float if uint isn't well supported */
typedef uint depth_t;

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

  r_sub->start = (uint)((src_x * y) + x);
  r_sub->span = (uint)dst_x;
  r_sub->span_len = (uint)dst_y;
  r_sub->skip = (uint)(src_x - dst_x);
}

/**
 * Ignore depth clearing as a change,
 * only check if its been changed _and_ filled in (ignore clearing since XRAY does this).
 */
BLI_INLINE bool depth_is_filled(const depth_t *prev, const depth_t *curr)
{
  return (*prev != *curr) && (*curr != DEPTH_MAX);
}

/* ----------------------------------------------------------------------------
 * DepthBufCache
 *
 * Result of reading glReadPixels,
 * use for both cache and non-cached storage.
 */

/* store result of glReadPixels */
typedef struct DepthBufCache {
  struct DepthBufCache *next, *prev;
  uint id;
  depth_t buf[0];
} DepthBufCache;

static DepthBufCache *depth_buf_malloc(uint rect_len)
{
  DepthBufCache *rect = MEM_mallocN(sizeof(DepthBufCache) + sizeof(depth_t) * rect_len, __func__);
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
  /* same as above but different rect sizes */
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

/* ----------------------------------------------------------------------------
 * DepthID
 *
 * Internal structure for storing hits.
 */

typedef struct DepthID {
  uint id;
  depth_t depth;
} DepthID;

static int depth_id_cmp(const void *v1, const void *v2)
{
  const DepthID *d1 = v1, *d2 = v2;
  if (d1->id < d2->id) {
    return -1;
  }
  else if (d1->id > d2->id) {
    return 1;
  }
  else {
    return 0;
  }
}

static int depth_cmp(const void *v1, const void *v2)
{
  const DepthID *d1 = v1, *d2 = v2;
  if (d1->depth < d2->depth) {
    return -1;
  }
  else if (d1->depth > d2->depth) {
    return 1;
  }
  else {
    return 0;
  }
}

/* depth sorting */
typedef struct GPUPickState {
  /* cache on initialization */
  uint (*buffer)[4];

  /* buffer size (stores number of integers, for actual size multiply by sizeof integer)*/
  uint bufsize;
  /* mode of operation */
  char mode;

  /* OpenGL drawing, never use when (is_cached == true). */
  struct {
    /* The current depth, accumulated as we draw */
    DepthBufCache *rect_depth;
    /* Scratch buffer, avoid allocs every time (when not caching) */
    DepthBufCache *rect_depth_test;

    /* Pass to glReadPixels (x, y, w, h) */
    int clip_readpixels[4];

    /* Set after first draw */
    bool is_init;
    uint prev_id;
  } gl;

  /* src: data stored in 'cache' and 'gl',
   * dst: use when cached region is smaller (where src -> dst isn't 1:1) */
  struct {
    rcti clip_rect;
    uint rect_len;
  } src, dst;

  /* Store cache between `GPU_select_cache_begin/end` */
  bool use_cache;
  bool is_cached;
  struct {
    /* Cleanup used for iterating over both source and destination buffers:
     * src.clip_rect -> dst.clip_rect */
    SubRectStride sub_rect;

    /* List of DepthBufCache, sized of 'src.clip_rect' */
    ListBase bufs;
  } cache;

  /* Pickign methods */
  union {
    /* GPU_SELECT_PICK_ALL */
    struct {
      DepthID *hits;
      uint hits_len;
      uint hits_len_alloc;
    } all;

    /* GPU_SELECT_PICK_NEAREST */
    struct {
      uint *rect_id;
    } nearest;
  };
} GPUPickState;

static GPUPickState g_pick_state = {0};

void gpu_select_pick_begin(uint (*buffer)[4], uint bufsize, const rcti *input, char mode)
{
  GPUPickState *ps = &g_pick_state;

#ifdef DEBUG_PRINT
  printf("%s: mode=%d, use_cache=%d, is_cache=%d\n", __func__, mode, ps->use_cache, ps->is_cached);
#endif

  ps->bufsize = bufsize;
  ps->buffer = buffer;
  ps->mode = mode;

  const uint rect_len = (uint)(BLI_rcti_size_x(input) * BLI_rcti_size_y(input));
  ps->dst.clip_rect = *input;
  ps->dst.rect_len = rect_len;

  /* Restrict OpenGL operations for when we don't have cache */
  if (ps->is_cached == false) {
    gpuPushAttr(GPU_DEPTH_BUFFER_BIT | GPU_VIEWPORT_BIT);

    /* disable writing to the framebuffer */
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    if (mode == GPU_SELECT_PICK_ALL) {
      /* Note that other depth settings (such as #GL_LEQUAL) work too,
       * since the depth is always cleared.
       * Noting this for cases when depth picking is used where
       * drawing calls change depth settings. */
      glDepthFunc(GL_ALWAYS);
    }
    else {
      glDepthFunc(GL_LEQUAL);
    }

    float viewport[4];
    glGetFloatv(GL_VIEWPORT, viewport);

    ps->src.clip_rect = *input;
    ps->src.rect_len = rect_len;

    ps->gl.clip_readpixels[0] = (int)viewport[0];
    ps->gl.clip_readpixels[1] = (int)viewport[1];
    ps->gl.clip_readpixels[2] = BLI_rcti_size_x(&ps->src.clip_rect);
    ps->gl.clip_readpixels[3] = BLI_rcti_size_y(&ps->src.clip_rect);

    glViewport(UNPACK4(ps->gl.clip_readpixels));

    /* It's possible we don't want to clear depth buffer,
     * so existing elements are masked by current z-buffer. */
    glClear(GL_DEPTH_BUFFER_BIT);

    /* scratch buffer (read new values here) */
    ps->gl.rect_depth_test = depth_buf_malloc(rect_len);
    ps->gl.rect_depth = depth_buf_malloc(rect_len);

    /* set initial 'far' value */
#if 0
    glReadPixels(UNPACK4(ps->gl.clip_readpixels),
                 GL_DEPTH_COMPONENT,
                 GL_UNSIGNED_INT,
                 ps->gl.rect_depth->buf);
#else
    for (uint i = 0; i < rect_len; i++) {
      ps->gl.rect_depth->buf[i] = DEPTH_MAX;
    }
#endif

    ps->gl.is_init = false;
    ps->gl.prev_id = 0;
  }
  else {
    /* Using cache (ps->is_cached == true) */
    /* src.clip_rect -> dst.clip_rect */
    rect_subregion_stride_calc(&ps->src.clip_rect, &ps->dst.clip_rect, &ps->cache.sub_rect);
    BLI_assert(ps->gl.rect_depth == NULL);
    BLI_assert(ps->gl.rect_depth_test == NULL);
  }

  if (mode == GPU_SELECT_PICK_ALL) {
    ps->all.hits = MEM_mallocN(sizeof(*ps->all.hits) * ALLOC_DEPTHS, __func__);
    ps->all.hits_len = 0;
    ps->all.hits_len_alloc = ALLOC_DEPTHS;
  }
  else {
    /* Set to 0xff for SELECT_ID_NONE */
    ps->nearest.rect_id = MEM_mallocN(sizeof(uint) * ps->dst.rect_len, __func__);
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
    /* same as above but different rect sizes */
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

  /* ensure enough space */
  if (UNLIKELY(ps->all.hits_len == ps->all.hits_len_alloc)) {
    ps->all.hits_len_alloc += ALLOC_DEPTHS;
    ps->all.hits = MEM_reallocN(ps->all.hits, ps->all.hits_len_alloc * sizeof(*ps->all.hits));
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
  /* keep track each pixels ID in 'nearest.rect_id' */
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

  if (ps->gl.is_init) {
    if (id == ps->gl.prev_id && !end) {
      /* No need to read if we are still drawing for the same id since
       * all these depths will be merged / de-duplicated in the end. */
      return true;
    }

    const uint rect_len = ps->src.rect_len;
    glReadPixels(UNPACK4(ps->gl.clip_readpixels),
                 GL_DEPTH_COMPONENT,
                 GL_UNSIGNED_INT,
                 ps->gl.rect_depth_test->buf);
    /* perform initial check since most cases the array remains unchanged  */

    bool do_pass = false;
    if (g_pick_state.mode == GPU_SELECT_PICK_ALL) {
      if (depth_buf_rect_depth_any(ps->gl.rect_depth_test, rect_len)) {
        ps->gl.rect_depth_test->id = ps->gl.prev_id;
        gpu_select_load_id_pass_all(ps->gl.rect_depth_test);
        do_pass = true;
      }
    }
    else {
      if (depth_buf_rect_depth_any_filled(ps->gl.rect_depth, ps->gl.rect_depth_test, rect_len)) {
        ps->gl.rect_depth_test->id = ps->gl.prev_id;
        gpu_select_load_id_pass_nearest(ps->gl.rect_depth, ps->gl.rect_depth_test);
        do_pass = true;
      }
    }

    if (do_pass) {
      /* Store depth in cache */
      if (ps->use_cache) {
        BLI_addtail(&ps->cache.bufs, ps->gl.rect_depth);
        ps->gl.rect_depth = depth_buf_malloc(ps->src.rect_len);
      }

      SWAP(DepthBufCache *, ps->gl.rect_depth, ps->gl.rect_depth_test);

      if (g_pick_state.mode == GPU_SELECT_PICK_ALL) {
        /* we want new depths every time */
        glClear(GL_DEPTH_BUFFER_BIT);
      }
    }
  }

  ps->gl.is_init = true;
  ps->gl.prev_id = id;

  return true;
}

uint gpu_select_pick_end(void)
{
  GPUPickState *ps = &g_pick_state;

#ifdef DEBUG_PRINT
  printf("%s\n", __func__);
#endif

  if (ps->is_cached == false) {
    if (ps->gl.is_init) {
      /* force finishing last pass */
      gpu_select_pick_load_id(ps->gl.prev_id, true);
    }
    gpuPopAttr();
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  }

  /* assign but never free directly since it may be in cache */
  DepthBufCache *rect_depth_final;

  /* Store depth in cache */
  if (ps->use_cache && !ps->is_cached) {
    BLI_addtail(&ps->cache.bufs, ps->gl.rect_depth);
    ps->gl.rect_depth = NULL;
    rect_depth_final = ps->cache.bufs.last;
  }
  else if (ps->is_cached) {
    rect_depth_final = ps->cache.bufs.last;
  }
  else {
    /* common case, no cache */
    rect_depth_final = ps->gl.rect_depth;
  }

  uint maxhits = g_pick_state.bufsize;
  DepthID *depth_data;
  uint depth_data_len = 0;

  if (g_pick_state.mode == GPU_SELECT_PICK_ALL) {
    depth_data = ps->all.hits;
    depth_data_len = ps->all.hits_len;
    /* move ownership */
    ps->all.hits = NULL;
    ps->all.hits_len = 0;
    ps->all.hits_len_alloc = 0;
  }
  else {
    /* GPU_SELECT_PICK_NEAREST */

    /* Over alloc (unlikely we have as many depths as pixels) */
    uint depth_data_len_first_pass = 0;
    depth_data = MEM_mallocN(ps->dst.rect_len * sizeof(*depth_data), __func__);

    /* Partially de-duplicating copy,
     * when contiguous ID's are found - update their closest depth.
     * This isn't essential but means there is less data to sort. */

#define EVAL_TEST(i_src, i_dst) \
  { \
    const uint id = ps->nearest.rect_id[i_dst]; \
    if (id != SELECT_ID_NONE) { \
      const depth_t depth = rect_depth_final->buf[i_src]; \
      if (depth_last == NULL || depth_last->id != id) { \
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
      DepthID *depth_last = NULL;
      if (ps->is_cached == false) {
        for (uint i = 0; i < ps->src.rect_len; i++) {
          EVAL_TEST(i, i);
        }
      }
      else {
        /* same as above but different rect sizes */
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

    /* Sort by ID's then keep the best depth for each ID */
    depth_data_len = 0;
    {
      DepthID *depth_last = NULL;
      for (uint i = 0; i < depth_data_len_first_pass; i++) {
        if (depth_last == NULL || depth_last->id != depth_data[i].id) {
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
   * so the final hit-list is sorted by depth (nearest first) */
  uint hits = 0;

  if (depth_data_len > maxhits) {
    hits = (uint)-1;
  }
  else {
    /* leave sorting up to the caller */
    qsort(depth_data, depth_data_len, sizeof(DepthID), depth_cmp);

    for (uint i = 0; i < depth_data_len; i++) {
#ifdef DEBUG_PRINT
      printf("  hit: %u: depth %u\n", depth_data[i].id, depth_data[i].depth);
#endif
      /* first 3 are dummy values */
      g_pick_state.buffer[hits][0] = 1;
      g_pick_state.buffer[hits][1] = 0x0; /* depth_data[i].depth; */ /* unused */
      g_pick_state.buffer[hits][2] = 0x0; /* z-far is currently never used. */
      g_pick_state.buffer[hits][3] = depth_data[i].id;
      hits++;
    }
    BLI_assert(hits < maxhits);
  }

  MEM_freeN(depth_data);

  MEM_SAFE_FREE(ps->gl.rect_depth);
  MEM_SAFE_FREE(ps->gl.rect_depth_test);

  if (g_pick_state.mode == GPU_SELECT_PICK_ALL) {
    /* 'hits' already freed as 'depth_data' */
  }
  else {
    MEM_freeN(ps->nearest.rect_id);
    ps->nearest.rect_id = NULL;
  }

  if (ps->use_cache) {
    ps->is_cached = true;
  }

  return hits;
}

/* ----------------------------------------------------------------------------
 * Caching
 *
 * Support multiple begin/end's reusing depth buffers.
 */

void gpu_select_pick_cache_begin(void)
{
  BLI_assert(g_pick_state.use_cache == false);
#ifdef DEBUG_PRINT
  printf("%s\n", __func__);
#endif
  g_pick_state.use_cache = true;
  g_pick_state.is_cached = false;
}

void gpu_select_pick_cache_end(void)
{
#ifdef DEBUG_PRINT
  printf("%s: with %d buffers\n", __func__, BLI_listbase_count(&g_pick_state.cache.bufs));
#endif
  g_pick_state.use_cache = false;
  g_pick_state.is_cached = false;

  BLI_freelistN(&g_pick_state.cache.bufs);
}

/* is drawing needed? */
bool gpu_select_pick_is_cached(void)
{
  return g_pick_state.is_cached;
}

void gpu_select_pick_cache_load_id(void)
{
  BLI_assert(g_pick_state.is_cached == true);
  GPUPickState *ps = &g_pick_state;
#ifdef DEBUG_PRINT
  printf("%s (building depth from cache)\n", __func__);
#endif
  for (DepthBufCache *rect_depth = ps->cache.bufs.first; rect_depth;
       rect_depth = rect_depth->next) {
    if (rect_depth->next != NULL) {
      /* we know the buffers differ, but this sub-region may not.
       * double check before adding an id-pass */
      if (g_pick_state.mode == GPU_SELECT_PICK_ALL) {
        if (depth_buf_subrect_depth_any(rect_depth->next, &ps->cache.sub_rect)) {
          gpu_select_load_id_pass_all(rect_depth->next);
        }
      }
      else {
        if (depth_buf_subrect_depth_any_filled(
                rect_depth, rect_depth->next, &ps->cache.sub_rect)) {
          gpu_select_load_id_pass_nearest(rect_depth, rect_depth->next);
        }
      }
    }
  }
}
