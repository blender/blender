/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2014 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct rcti;

/** Flags for mode of operation. */
typedef enum eGPUSelectMode {
  GPU_SELECT_ALL = 1,
  /* gpu_select_query */
  GPU_SELECT_NEAREST_FIRST_PASS = 2,
  GPU_SELECT_NEAREST_SECOND_PASS = 3,
  /* gpu_select_pick */
  GPU_SELECT_PICK_ALL = 4,
  GPU_SELECT_PICK_NEAREST = 5,
} eGPUSelectMode;

/**
 * The result of calling #GPU_select_begin & #GPU_select_end.
 */
typedef struct GPUSelectResult {
  /** The selection identifier matching the value passed in by #GPU_select_load_id. */
  unsigned int id;
  /**
   * The nearest depth.
   * - Only supported by picking modes (#GPU_SELECT_PICK_ALL and #GPU_SELECT_PICK_NEAREST)
   *   since occlusion quires don't provide a convenient way of accessing the depth-buffer.
   * - OpenGL's `GL_SELECT` supported both near and far depths,
   *   this has not been included as Blender doesn't need this however support could be added.
   */
  unsigned int depth;
} GPUSelectResult;

/**
 * Initialize and provide buffer for results.
 */
void GPU_select_begin(GPUSelectResult *buffer,
                      unsigned int buffer_len,
                      const struct rcti *input,
                      eGPUSelectMode mode,
                      int oldhits);
/**
 * Initialize and provide buffer for results.
 * Uses the new Select-Next engine if enabled.
 */
void GPU_select_begin_next(GPUSelectResult *buffer,
                           const uint buffer_len,
                           const struct rcti *input,
                           eGPUSelectMode mode,
                           int oldhits);
/**
 * Loads a new selection id and ends previous query, if any.
 * In second pass of selection it also returns
 * if id has been hit on the first pass already.
 * Thus we can skip drawing un-hit objects.
 *
 * \warning We rely on the order of object rendering on passes to be the same for this to work.
 */
bool GPU_select_load_id(unsigned int id);
void GPU_select_finalize(void);
/**
 * Cleanup and flush selection results to buffer.
 * Return number of hits and hits in buffer.
 * if \a dopass is true, we will do a second pass with occlusion queries to get the closest hit.
 */
unsigned int GPU_select_end(void);

/* Cache selection region. */

bool GPU_select_is_cached(void);
void GPU_select_cache_begin(void);
void GPU_select_cache_load_id(void);
void GPU_select_cache_end(void);

/* Utilities. */

/**
 * Helper function, nothing special but avoids doing inline since hits aren't sorted by depth
 * and purpose of 4x buffer indices isn't so clear.
 *
 * Note that comparing depth as uint is fine.
 */
const GPUSelectResult *GPU_select_buffer_near(const GPUSelectResult *buffer, int hits);
uint GPU_select_buffer_remove_by_id(GPUSelectResult *buffer, int hits, uint select_id);
/**
 * Part of the solution copied from `rect_subregion_stride_calc`.
 */
void GPU_select_buffer_stride_realign(const struct rcti *src, const struct rcti *dst, uint *r_buf);

#ifdef __cplusplus
}
#endif
