/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Interface for accessing GPU-related methods for selection. The semantics are
 * similar to `glRenderMode(GL_SELECT)` from older OpenGL versions.
 */
#include <cstdlib>
#include <cstring>

#include "GPU_select.hh"

#include "BLI_rect.h"

#include "BLI_utildefines.h"

#include "gpu_select_private.hh"

/* -------------------------------------------------------------------- */
/** \name Internal Types
 * \{ */

/* Internal algorithm used */
enum GPUSelectAlgo {
  /**
   * `glBegin/EndQuery(GL_SAMPLES_PASSED... )`, `gpu_select_query.c`
   * Only sets 4th component (ID) correctly.
   */
  ALGO_SAMPLE_QUERY = 1,
  /**
   * Read depth buffer for every drawing pass and extract depths, `gpu_select_pick.cc`
   * Only sets 4th component (ID) correctly.
   */
  ALGO_DEPTH_PICK = 2,
  /** Use Select-Next draw engine. */
  ALGO_SELECT_NEXT = 3,
};

struct GPUSelectState {
  /* To ignore selection id calls when not initialized */
  bool select_is_active;
  /* mode of operation */
  GPUSelectMode mode;
  /* internal algorithm for selection */
  GPUSelectAlgo algorithm;
  /* allow GPU_select_begin/end without drawing */
  bool use_cache;
  /**
   * Signifies that #GPU_select_cache_begin has been called,
   * future calls to #GPU_select_begin should initialize the cache.
   *
   * \note #GPU_select_cache_begin could perform initialization but doesn't as it's inconvenient
   * for callers making the cache begin/end calls outside lower level selection logic
   * where the `mode` to pass to #GPU_select_begin yet isn't known.
   */
  bool use_cache_needs_init;
};

static GPUSelectState g_select_state = {false};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

static void gpu_select_begin_ex(GPUSelectBuffer *buffer,
                                const rcti *input,
                                GPUSelectMode mode,
                                int oldhits,
                                bool use_select_next)
{
  if (mode == GPU_SELECT_NEAREST_SECOND_PASS) {
    /* In the case hits was '-1',
     * don't start the second pass since it's not going to give useful results.
     * As well as buffer overflow in 'gpu_select_query_load_id'. */
    BLI_assert(oldhits != -1);
  }

  g_select_state.select_is_active = true;
  g_select_state.mode = mode;

  if (use_select_next) {
    g_select_state.algorithm = ALGO_SELECT_NEXT;
  }
  else if (ELEM(g_select_state.mode, GPU_SELECT_PICK_ALL, GPU_SELECT_PICK_NEAREST)) {
    g_select_state.algorithm = ALGO_DEPTH_PICK;
  }
  else {
    g_select_state.algorithm = ALGO_SAMPLE_QUERY;
  }

  /* This function is called when cache has already been initialized,
   * so only manipulate cache values when cache is pending. */
  if (g_select_state.use_cache_needs_init) {
    g_select_state.use_cache_needs_init = false;

    switch (g_select_state.algorithm) {
      case ALGO_SELECT_NEXT:
      case ALGO_SAMPLE_QUERY: {
        g_select_state.use_cache = false;
        break;
      }
      default: {
        g_select_state.use_cache = true;
        gpu_select_pick_cache_begin();
        break;
      }
    }
  }

  switch (g_select_state.algorithm) {
    case ALGO_SELECT_NEXT: {
      gpu_select_next_begin(buffer, input, mode);
      break;
    }
    case ALGO_SAMPLE_QUERY: {
      gpu_select_query_begin(buffer, input, mode, oldhits);
      break;
    }
    default: /* ALGO_DEPTH_PICK */
    {
      gpu_select_pick_begin(buffer, input, mode);
      break;
    }
  }
}

void GPU_select_begin_next(GPUSelectBuffer *buffer,
                           const rcti *input,
                           GPUSelectMode mode,
                           int oldhits)
{
  gpu_select_begin_ex(buffer, input, mode, oldhits, true);
}

void GPU_select_begin(GPUSelectBuffer *buffer, const rcti *input, GPUSelectMode mode, int oldhits)
{
  gpu_select_begin_ex(buffer, input, mode, oldhits, false);
}

bool GPU_select_load_id(uint id)
{
  /* if no selection mode active, ignore */
  if (!g_select_state.select_is_active) {
    return true;
  }

  switch (g_select_state.algorithm) {
    case ALGO_SELECT_NEXT:
      /* This shouldn't use this pipeline. */
      BLI_assert_unreachable();
      return false;

    case ALGO_SAMPLE_QUERY: {
      return gpu_select_query_load_id(id);
    }
    default: /* ALGO_DEPTH_PICK */
    {
      return gpu_select_pick_load_id(id, false);
    }
  }
}

uint GPU_select_end()
{
  uint hits = 0;

  switch (g_select_state.algorithm) {
    case ALGO_SELECT_NEXT: {
      hits = gpu_select_next_end();
      break;
    }
    case ALGO_SAMPLE_QUERY: {
      hits = gpu_select_query_end();
      break;
    }
    default: /* ALGO_DEPTH_PICK */
    {
      hits = gpu_select_pick_end();
      break;
    }
  }

  g_select_state.select_is_active = false;

  return hits;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Caching
 *
 * Support multiple begin/end's as long as they are within the initial region.
 * Currently only used by #ALGO_DEPTH_PICK.
 * \{ */

void GPU_select_cache_begin()
{
  BLI_assert(g_select_state.select_is_active == false);
  /* Ensure #GPU_select_cache_end is always called. */
  BLI_assert(g_select_state.use_cache_needs_init == false);

  /* Signal that cache should be used, instead of calling the algorithms cache-begin function.
   * This is more convenient as the exact method of selection may not be known by the caller. */
  g_select_state.use_cache_needs_init = true;
}

void GPU_select_cache_load_id()
{
  BLI_assert(g_select_state.use_cache == true);
  if (g_select_state.algorithm == ALGO_DEPTH_PICK) {
    gpu_select_pick_cache_load_id();
  }
}

void GPU_select_cache_end()
{
  if (g_select_state.algorithm == ALGO_DEPTH_PICK) {
    BLI_assert(g_select_state.use_cache == true);
    gpu_select_pick_cache_end();
  }
  g_select_state.use_cache = false;
  /* Paranoid assignment, should already be false. */
  g_select_state.use_cache_needs_init = false;
}

bool GPU_select_is_cached()
{
  return g_select_state.use_cache && gpu_select_pick_is_cached();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

const GPUSelectResult *GPU_select_buffer_near(const blender::Span<GPUSelectResult> hit_results)
{
  const GPUSelectResult *hit_result_near = nullptr;
  uint depth_min = uint(-1);
  for (const GPUSelectResult &hit_result : hit_results) {
    if (hit_result.depth < depth_min) {
      BLI_assert(hit_result.id != -1);
      depth_min = hit_result.depth;
      hit_result_near = &hit_result;
    }
  }
  return hit_result_near;
}

uint GPU_select_buffer_remove_by_id(blender::MutableSpan<GPUSelectResult> hit_results,
                                    uint select_id)
{
  uint index_src = 0;
  uint index_dst = 0;
  uint hits_final = 0;
  for (const GPUSelectResult &hit_result : hit_results) {
    if (hit_result.id != select_id) {
      if (index_dst != index_src) {
        hit_results[index_dst] = hit_result;
      }
      index_dst++;
      hits_final++;
    }
    index_src++;
  }
  return hits_final;
}

void GPU_select_buffer_stride_realign(const rcti *src, const rcti *dst, uint *r_buf)
{
  const int x = dst->xmin - src->xmin;
  const int y = dst->ymin - src->ymin;

  BLI_assert(src->xmin <= dst->xmin && src->ymin <= dst->ymin && src->xmax >= dst->xmax &&
             src->ymax >= dst->ymax);
  BLI_assert(x >= 0 && y >= 0);

  const int src_x = BLI_rcti_size_x(src);
  const int src_y = BLI_rcti_size_y(src);
  const int dst_x = BLI_rcti_size_x(dst);
  const int dst_y = BLI_rcti_size_y(dst);

  BLI_assert(dst_x > 0 && dst_y > 0);

  int last_px_id = src_x * (y + dst_y - 1) + (x + dst_x - 1);
  memset(&r_buf[last_px_id + 1], 0, (src_x * src_y - (last_px_id + 1)) * sizeof(*r_buf));

  if (last_px_id < 0) {
    /* Nothing to write. */
    BLI_assert(last_px_id == -1);
    return;
  }

  int last_px_written = dst_x * dst_y - 1;
  const int skip = src_x - dst_x;

  while (true) {
    for (int i = dst_x; i--;) {
      r_buf[last_px_id--] = r_buf[last_px_written--];
    }
    if (last_px_written < 0) {
      break;
    }
    last_px_id -= skip;
    memset(&r_buf[last_px_id + 1], 0, skip * sizeof(*r_buf));
  }
  memset(r_buf, 0, (last_px_id + 1) * sizeof(*r_buf));
}

/** \} */
