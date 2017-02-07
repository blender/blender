
// Gawain buffer IDs
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.#include "buffer_id.h"

#include "buffer_id.h"
#include <mutex>
#include <vector>

#define ORPHAN_DEBUG 0

#if ORPHAN_DEBUG
	#include <cstdio>
#endif

static std::vector<GLuint> orphaned_buffer_ids;
static std::vector<GLuint> orphaned_vao_ids;

static std::mutex orphan_mutex;

extern "C" {
extern int BLI_thread_is_main(void); // Blender-specific function
}

static bool thread_is_main()
	{
	// "main" here means the GL context's thread
	return BLI_thread_is_main();
	}

GLuint buffer_id_alloc()
	{
#if TRUST_NO_ONE
	assert(thread_is_main());
#endif

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

void buffer_id_free(GLuint buffer_id)
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

GLuint vao_id_alloc()
	{
#if TRUST_NO_ONE
	assert(thread_is_main());
#endif

	// delete orphaned IDs
	orphan_mutex.lock();
	if (!orphaned_vao_ids.empty())
		{
		const auto orphaned_vao_ct = (unsigned)orphaned_vao_ids.size();
#if ORPHAN_DEBUG
		printf("deleting %u orphaned VAO%s\n", orphaned_vao_ct, orphaned_vao_ct == 1 ? "" : "s");
#endif
		glDeleteVertexArrays(orphaned_vao_ct, orphaned_vao_ids.data());
		orphaned_vao_ids.clear();
		}
	orphan_mutex.unlock();

	GLuint new_vao_id = 0;
	glGenVertexArrays(1, &new_vao_id);
	return new_vao_id;
	}

void vao_id_free(GLuint vao_id)
	{
	if (thread_is_main())
		glDeleteVertexArrays(1, &vao_id);
	else
		{
		// add this ID to the orphaned list
		orphan_mutex.lock();
#if ORPHAN_DEBUG
		printf("orphaning VAO %u\n", vao_id);
#endif
		orphaned_vao_ids.emplace_back(vao_id);
		orphan_mutex.unlock();
		}
	}
