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
 */

#pragma once

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct rcti;

/** Flags for mode of operation. */
enum {
  GPU_SELECT_ALL = 1,
  /* gpu_select_query */
  GPU_SELECT_NEAREST_FIRST_PASS = 2,
  GPU_SELECT_NEAREST_SECOND_PASS = 3,
  /* gpu_select_pick */
  GPU_SELECT_PICK_ALL = 4,
  GPU_SELECT_PICK_NEAREST = 5,
};

/**
 * Initialize and provide buffer for results.
 */
void GPU_select_begin(
    unsigned int *buffer, unsigned int bufsize, const struct rcti *input, char mode, int oldhits);
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
const uint *GPU_select_buffer_near(const uint *buffer, int hits);
uint GPU_select_buffer_remove_by_id(uint *buffer, int hits, uint select_id);
/**
 * Part of the solution copied from `rect_subregion_stride_calc`.
 */
void GPU_select_buffer_stride_realign(const struct rcti *src, const struct rcti *dst, uint *r_buf);

#ifdef __cplusplus
}
#endif
