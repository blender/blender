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
 * Selection implementations.
 */

#ifndef __GPU_SELECT_PRIVATE_H__
#define __GPU_SELECT_PRIVATE_H__

/* gpu_select_pick */
void gpu_select_pick_begin(uint (*buffer)[4], uint bufsize, const rcti *input, char mode);
bool gpu_select_pick_load_id(uint id, bool end);
uint gpu_select_pick_end(void);

void gpu_select_pick_cache_begin(void);
void gpu_select_pick_cache_end(void);
bool gpu_select_pick_is_cached(void);
void gpu_select_pick_cache_load_id(void);

/* gpu_select_sample_query */
void gpu_select_query_begin(
    uint (*buffer)[4], uint bufsize, const rcti *input, char mode, int oldhits);
bool gpu_select_query_load_id(uint id);
uint gpu_select_query_end(void);

#define SELECT_ID_NONE ((uint)0xffffffff)

#endif /* __GPU_SELECT_PRIVATE_H__ */
