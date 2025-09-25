/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_span.hh"
#include "BLI_sys_types.h"
#include "BLI_vector.hh"

struct rcti;

/** Flags for mode of operation. */
enum GPUSelectMode {
  GPU_SELECT_INVALID = 0,
  GPU_SELECT_ALL = 1,
  /* gpu_select_query */
  GPU_SELECT_NEAREST_FIRST_PASS = 2,
  GPU_SELECT_NEAREST_SECOND_PASS = 3,
  /* gpu_select_pick */
  GPU_SELECT_PICK_ALL = 4,
  GPU_SELECT_PICK_NEAREST = 5,
};

/**
 * The result of calling #GPU_select_begin & #GPU_select_end.
 */
struct GPUSelectResult {
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
};

using GPUSelectStorage = blender::Vector<GPUSelectResult, 2500>;
struct GPUSelectBuffer {
  GPUSelectStorage storage;
};

/**
 * Initialize and provide buffer for results.
 */
void GPU_select_begin(GPUSelectBuffer *buffer, const rcti *input, GPUSelectMode mode, int oldhits);
/**
 * Initialize and provide buffer for results.
 * Uses the new Select-Next engine if enabled.
 */
void GPU_select_begin_next(GPUSelectBuffer *buffer,
                           const rcti *input,
                           GPUSelectMode mode,
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
void GPU_select_finalize();
/**
 * Cleanup and flush selection results to buffer.
 * Return number of hits and hits in buffer.
 * if \a dopass is true, we will do a second pass with occlusion queries to get the closest hit.
 */
unsigned int GPU_select_end();

/* Cache selection region. */

bool GPU_select_is_cached();
void GPU_select_cache_begin();
void GPU_select_cache_load_id();
void GPU_select_cache_end();

/* Utilities. */

/**
 * Helper function, nothing special but avoids doing inline since hits aren't sorted by depth
 * and purpose of 4x buffer indices isn't so clear.
 *
 * Note that comparing depth as uint is fine.
 */
const GPUSelectResult *GPU_select_buffer_near(const blender::Span<GPUSelectResult> hit_results);
uint GPU_select_buffer_remove_by_id(blender::MutableSpan<GPUSelectResult> hit_results,
                                    uint select_id);
/**
 * Part of the solution copied from `rect_subregion_stride_calc`.
 */
void GPU_select_buffer_stride_realign(const rcti *src, const rcti *dst, uint *r_buf);
