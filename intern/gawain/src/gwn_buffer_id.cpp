
// Gawain buffer IDs
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.#include "buffer_id.h"

#include "gwn_buffer_id.h"
#include <mutex>
#include <vector>

#define ORPHAN_DEBUG 0

#if ORPHAN_DEBUG
	#include <cstdio>
#endif

static std::vector<GLuint> orphaned_buffer_ids;

static std::mutex orphan_mutex;

extern "C" {
extern int BLI_thread_is_main(void); // Blender-specific function
}

static bool thread_is_main()
	{
	// "main" here means the GL context's thread
	return BLI_thread_is_main();
	}

GLuint GWN_buf_id_alloc()
	{
	// delete orphaned IDs
	orphan_mutex.lock();
	if (!orphaned_buffer_ids.empty())
		{
		const auto orphaned_buffer_ct = (unsigned)orphaned_buffer_ids.size();
#if ORPHAN_DEBUG
		printf("deleting %u orphaned VBO%s\n", orphaned_buffer_ct, orphaned_buffer_ct == 1 ? "" : "s");
#endif
		glDeleteBuffers(orphaned_buffer_ct, orphaned_buffer_ids.data());
		orphaned_buffer_ids.clear();
		}
	orphan_mutex.unlock();

	GLuint new_buffer_id = 0;
	glGenBuffers(1, &new_buffer_id);
	return new_buffer_id;
	}

void GWN_buf_id_free(GLuint buffer_id)
	{
	if (thread_is_main())
		glDeleteBuffers(1, &buffer_id);
	else
		{
		// add this ID to the orphaned list
		orphan_mutex.lock();
#if ORPHAN_DEBUG
		printf("orphaning VBO %u\n", buffer_id);
#endif
		orphaned_buffer_ids.emplace_back(buffer_id);
		orphan_mutex.unlock();
		}
	}
