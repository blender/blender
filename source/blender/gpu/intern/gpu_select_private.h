/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s): Antony Riakiotakis.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_select_private.h
 *  \ingroup gpu
 *
 * Selection implementations.
 */

#ifndef __GPU_SELECT_PRIVATE_H__
#define __GPU_SELECT_PRIVATE_H__

/* gpu_select_pick */
void gpu_select_pick_begin(unsigned int (*buffer)[4], unsigned int bufsize, const rcti *input, char mode);
bool gpu_select_pick_load_id(unsigned int id);
unsigned int gpu_select_pick_end(void);

void gpu_select_pick_cache_begin(void);
void gpu_select_pick_cache_end(void);
bool gpu_select_pick_is_cached(void);
void gpu_select_pick_cache_load_id(void);

/* gpu_select_sample_query */
void gpu_select_query_begin(unsigned int (*buffer)[4], unsigned int bufsize, const rcti *input, char mode, int oldhits);
bool gpu_select_query_load_id(unsigned int id);
unsigned int gpu_select_query_end(void);


#define SELECT_ID_NONE ((unsigned int)0xffffffff)

#endif  /* __GPU_SELECT_PRIVATE_H__ */
