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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * Interface for accessing GPU-related methods for selection. The semantics are
 * similar to `glRenderMode(GL_SELECT)` from older OpenGL versions.
 */
#include <stdlib.h>
#include <string.h>

#include "GPU_select.h"

#include "MEM_guardedalloc.h"

#include "BLI_rect.h"

#include "DNA_userdef_types.h"

#include "BLI_utildefines.h"

#include "gpu_select_private.h"

/* -------------------------------------------------------------------- */
/** \name Internal Types
 * \{ */

/* Internal algorithm used */
enum {
  /** glBegin/EndQuery(GL_SAMPLES_PASSED... ), `gpu_select_query.c`
   * Only sets 4th component (ID) correctly. */
  ALGO_GL_QUERY = 1,
  /** Read depth buffer for every drawing pass and extract depths, `gpu_select_pick.c`
   * Only sets 4th component (ID) correctly. */
  ALGO_GL_PICK = 2,
};

typedef struct GPUSelectState {
  /* To ignore selection id calls when not initialized */
  bool select_is_active;
  /* mode of operation */
  char mode;
  /* internal algorithm for selection */
  char algorithm;
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
} GPUSelectState;

static GPUSelectState g_select_state = {0};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

void GPU_select_begin(uint *buffer, uint bufsize, const rcti *input, char mode, int oldhits)
{
  if (mode == GPU_SELECT_NEAREST_SECOND_PASS) {
    /* In the case hits was '-1',
     * don't start the second pass since it's not going to give useful results.
     * As well as buffer overflow in 'gpu_select_query_load_id'. */
    BLI_assert(oldhits != -1);
  }

  g_select_state.select_is_active = true;
  g_select_state.mode = mode;

  if (ELEM(g_select_state.mode, GPU_SELECT_PICK_ALL, GPU_SELECT_PICK_NEAREST)) {
    g_select_state.algorithm = ALGO_GL_PICK;
  }
  else {
    g_select_state.algorithm = ALGO_GL_QUERY;
  }

  /* This function is called when cache has already been initialized,
   * so only manipulate cache values when cache is pending. */
  if (g_select_state.use_cache_needs_init) {
    g_select_state.use_cache_needs_init = false;

    switch (g_select_state.algorithm) {
      case ALGO_GL_QUERY: {
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
    case ALGO_GL_QUERY: {
      gpu_select_query_begin((uint(*)[4])buffer, bufsize / 4, input, mode, oldhits);
      break;
    }
    default: /* ALGO_GL_PICK */
    {
      gpu_select_pick_begin((uint(*)[4])buffer, bufsize / 4, input, mode);
      break;
    }
  }
}

bool GPU_select_load_id(uint id)
{
  /* if no selection mode active, ignore */
  if (!g_select_state.select_is_active) {
    return true;
  }

  switch (g_select_state.algorithm) {
    case ALGO_GL_QUERY: {
      return gpu_select_query_load_id(id);
    }
    default: /* ALGO_GL_PICK */
    {
      return gpu_select_pick_load_id(id, false);
    }
  }
}

uint GPU_select_end(void)
{
  uint hits = 0;

  switch (g_select_state.algorithm) {
    case ALGO_GL_QUERY: {
      hits = gpu_select_query_end();
      break;
    }
    default: /* ALGO_GL_PICK */
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
 * Currently only used by #ALGO_GL_PICK.
 * \{ */

void GPU_select_cache_begin(void)
{
  BLI_assert(g_select_state.select_is_active == false);
  /* Ensure #GPU_select_cache_end is always called. */
  BLI_assert(g_select_state.use_cache_needs_init == false);

  /* Signal that cache should be used, instead of calling the algorithms cache-begin function.
   * This is more convenient as the exact method of selection may not be known by the caller. */
  g_select_state.use_cache_needs_init = true;
}

void GPU_select_cache_load_id(void)
{
  BLI_assert(g_select_state.use_cache == true);
  if (g_select_state.algorithm == ALGO_GL_PICK) {
    gpu_select_pick_cache_load_id();
  }
}

void GPU_select_cache_end(void)
{
  if (g_select_state.algorithm == ALGO_GL_PICK) {
    BLI_assert(g_select_state.use_cache == true);
    gpu_select_pick_cache_end();
  }
  g_select_state.use_cache = false;
  /* Paranoid assignment, should already be false. */
  g_select_state.use_cache_needs_init = false;
}

bool GPU_select_is_cached(void)
{
  return g_select_state.use_cache && gpu_select_pick_is_cached();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

const uint *GPU_select_buffer_near(const uint *buffer, int hits)
{
  const uint *buffer_near = NULL;
  uint depth_min = (uint)-1;
  for (int i = 0; i < hits; i++) {
    if (buffer[1] < depth_min) {
      BLI_assert(buffer[3] != -1);
      depth_min = buffer[1];
      buffer_near = buffer;
    }
    buffer += 4;
  }
  return buffer_near;
}

uint GPU_select_buffer_remove_by_id(uint *buffer, int hits, uint select_id)
{
  uint *buffer_src = buffer;
  uint *buffer_dst = buffer;
  int hits_final = 0;
  for (int i = 0; i < hits; i++) {
    if (buffer_src[3] != select_id) {
      if (buffer_dst != buffer_src) {
        memcpy(buffer_dst, buffer_src, sizeof(int[4]));
      }
      buffer_dst += 4;
      hits_final += 1;
    }
    buffer_src += 4;
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
