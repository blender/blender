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

/** \file GPU_select.h
 *  \ingroup gpu
 */

#ifndef __GPU_SELECT__
#define __GPU_SELECT__

#include "DNA_vec_types.h"  /* rcft */
#include "BLI_sys_types.h"

/* flags for mode of operation */
enum {
	GPU_SELECT_ALL                      = 1,
	GPU_SELECT_NEAREST_FIRST_PASS       = 2,
	GPU_SELECT_NEAREST_SECOND_PASS      = 3,
};

/* initialize and provide buffer for results */
void GPU_select_begin(unsigned int *buffer, unsigned int bufsize, rctf *input, char mode, int oldhits);

/* loads a new selection id and ends previous query, if any. In second pass of selection it also returns
 * if id has been hit on the first pass already. Thus we can skip drawing un-hit objects IMPORTANT: We rely on the order of object rendering on passes to be
 * the same for this to work */
bool GPU_select_load_id(unsigned int id);

/* cleanup and flush selection results to buffer. Return number of hits and hits in buffer.
 * if dopass is true, we will do a second pass with occlusion queries to get the closest hit */
unsigned int GPU_select_end(void);

/* does the GPU support occlusion queries? */
bool GPU_select_query_check_support(void);

/* is occlusion query supported and user activated? */
bool GPU_select_query_check_active(void);

#endif
