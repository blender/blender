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

/** \file blender/gpu/intern/gpu_select.c
 *  \ingroup gpu
 *
 * Interface for accessing gpu-related methods for selection. The semantics will be
 * similar to glRenderMode(GL_SELECT) since the goal is to maintain compatibility.
 */
#include <stdlib.h>

#include "GPU_select.h"
#include "GPU_extensions.h"
#include "GPU_glew.h"

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_utildefines.h"

#include "gpu_select_private.h"

/* Internal algorithm used */
enum {
	/** GL_SELECT, legacy OpenGL selection */
	ALGO_GL_LEGACY = 1,
	/** glBegin/EndQuery(GL_SAMPLES_PASSED... ), `gpu_select_query.c`
	 * Only sets 4th component (ID) correctly. */
	ALGO_GL_QUERY = 2,
	/** Read depth buffer for every drawing pass and extract depths, `gpu_select_pick.c`
	 * Only sets 4th component (ID) correctly. */
	ALGO_GL_PICK = 3,
};

typedef struct GPUSelectState {
	/* To ignore selection id calls when not initialized */
	bool select_is_active;
	/* flag to cache user preference for occlusion based selection */
	bool use_gpu_select;
	/* mode of operation */
	char mode;
	/* internal algorithm for selection */
	char algorithm;
	/* allow GPU_select_begin/end without drawing */
	bool use_cache;
} GPUSelectState;

static GPUSelectState g_select_state = {0};

/**
 * initialize and provide buffer for results
 */
void GPU_select_begin(uint *buffer, uint bufsize, const rcti *input, char mode, int oldhits)
{
	if (mode == GPU_SELECT_NEAREST_SECOND_PASS) {
		/* In the case hits was '-1', don't start the second pass since it's not going to give useful results.
		 * As well as buffer overflow in 'gpu_select_query_load_id'. */
		BLI_assert(oldhits != -1);
	}

	g_select_state.select_is_active = true;
	g_select_state.use_gpu_select = GPU_select_query_check_active();
	g_select_state.mode = mode;

	if (ELEM(g_select_state.mode, GPU_SELECT_PICK_ALL, GPU_SELECT_PICK_NEAREST)) {
		g_select_state.algorithm = ALGO_GL_PICK;
	}
	else if (!g_select_state.use_gpu_select) {
		g_select_state.algorithm = ALGO_GL_LEGACY;
	}
	else {
		g_select_state.algorithm = ALGO_GL_QUERY;
	}

	switch (g_select_state.algorithm) {
		case ALGO_GL_LEGACY:
		{
			g_select_state.use_cache = false;
			glSelectBuffer(bufsize, (GLuint *)buffer);
			glRenderMode(GL_SELECT);
			glInitNames();
			glPushName(-1);
			break;
		}
		case ALGO_GL_QUERY:
		{
			g_select_state.use_cache = false;
			gpu_select_query_begin((uint (*)[4])buffer, bufsize / 4, input, mode, oldhits);
			break;
		}
		default:  /* ALGO_GL_PICK */
		{
			gpu_select_pick_begin((uint (*)[4])buffer, bufsize / 4, input, mode);
			break;
		}
	}
}

/**
 * loads a new selection id and ends previous query, if any. In second pass of selection it also returns
 * if id has been hit on the first pass already.
 * Thus we can skip drawing un-hit objects.
 *
 * \warning We rely on the order of object rendering on passes to be the same for this to work.
 */
bool GPU_select_load_id(uint id)
{
	/* if no selection mode active, ignore */
	if (!g_select_state.select_is_active)
		return true;

	switch (g_select_state.algorithm) {
		case ALGO_GL_LEGACY:
		{
			glLoadName(id);
			return true;
		}
		case ALGO_GL_QUERY:
		{
			return gpu_select_query_load_id(id);
		}
		default:  /* ALGO_GL_PICK */
		{
			return gpu_select_pick_load_id(id);
		}
	}
}

/**
 * Cleanup and flush selection results to buffer.
 * Return number of hits and hits in buffer.
 * if \a dopass is true, we will do a second pass with occlusion queries to get the closest hit.
 */
uint GPU_select_end(void)
{
	uint hits = 0;

	switch (g_select_state.algorithm) {
		case ALGO_GL_LEGACY:
		{
			glPopName();
			hits = glRenderMode(GL_RENDER);
			break;
		}
		case ALGO_GL_QUERY:
		{
			hits = gpu_select_query_end();
			break;
		}
		default:  /* ALGO_GL_PICK */
		{
			hits = gpu_select_pick_end();
			break;
		}
	}

	g_select_state.select_is_active = false;

	return hits;
}

/**
 * has user activated?
 */
bool GPU_select_query_check_active(void)
{
	return ELEM(U.gpu_select_method, USER_SELECT_USE_OCCLUSION_QUERY, USER_SELECT_AUTO);
}

/* ----------------------------------------------------------------------------
 * Caching
 *
 * Support multiple begin/end's as long as they are within the initial region.
 * Currently only used by ALGO_GL_PICK.
 */

void GPU_select_cache_begin(void)
{
	/* validate on GPU_select_begin, clear if not supported */
	BLI_assert(g_select_state.use_cache == false);
	g_select_state.use_cache = true;
	if (g_select_state.algorithm == ALGO_GL_PICK) {
		gpu_select_pick_cache_begin();
	}
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
		gpu_select_pick_cache_end();
	}
	g_select_state.use_cache = false;
}

bool GPU_select_is_cached(void)
{
	return g_select_state.use_cache && gpu_select_pick_is_cached();
}


/* ----------------------------------------------------------------------------
 * Utilities
 */

/**
 * Helper function, nothing special but avoids doing inline since hit's aren't sorted by depth
 * and purpose of 4x buffer indices isn't so clear.
 *
 * Note that comparing depth as uint is fine.
 */
const uint *GPU_select_buffer_near(const uint *buffer, int hits)
{
	const uint *buffer_near = NULL;
	uint depth_min = (uint) - 1;
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
