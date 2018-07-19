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
 * The Original Code is Copyright (C) 2016 by Mike Erwin.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_buffer_id.cpp
 *  \ingroup gpu
 *
 * GPU buffer IDs
 */

#include "GPU_buffer_id.h"

#include <mutex>
#include <vector>

#define ORPHAN_DEBUG 0

#if ORPHAN_DEBUG
#  include <cstdio>
#endif

static std::vector<GLuint> orphaned_buffer_ids;

static std::mutex orphan_mutex;

extern "C" {
extern int BLI_thread_is_main(void); /* Blender-specific function */
}

static bool thread_is_main()
{
	/* "main" here means the GL context's thread */
	return BLI_thread_is_main();
}

GLuint GPU_buf_id_alloc()
{
	/* delete orphaned IDs */
	orphan_mutex.lock();
	if (!orphaned_buffer_ids.empty()) {
		const auto orphaned_buffer_len = (uint)orphaned_buffer_ids.size();
#if ORPHAN_DEBUG
		printf("deleting %u orphaned VBO%s\n", orphaned_buffer_len, orphaned_buffer_len == 1 ? "" : "s");
#endif
		glDeleteBuffers(orphaned_buffer_len, orphaned_buffer_ids.data());
		orphaned_buffer_ids.clear();
	}
	orphan_mutex.unlock();

	GLuint new_buffer_id = 0;
	glGenBuffers(1, &new_buffer_id);
	return new_buffer_id;
}

void GPU_buf_id_free(GLuint buffer_id)
{
	if (thread_is_main()) {
		glDeleteBuffers(1, &buffer_id);
	}
	else {
		/* add this ID to the orphaned list */
		orphan_mutex.lock();
#if ORPHAN_DEBUG
		printf("orphaning VBO %u\n", buffer_id);
#endif
		orphaned_buffer_ids.emplace_back(buffer_id);
		orphan_mutex.unlock();
	}
}
