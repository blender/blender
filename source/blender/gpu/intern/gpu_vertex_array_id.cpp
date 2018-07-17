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
 * Contributor(s): Blender Foundation, Clément Foucault
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/gpu_vertex_array_id.cpp
 *  \ingroup gpu
 *
 * Manage GL vertex array IDs in a thread-safe way
 * Use these instead of glGenBuffers & its friends
 * - alloc must be called from a thread that is bound
 *   to the context that will be used for drawing with
 *   this vao.
 * - free can be called from any thread
 */

#include "gpu_batch_private.h"
#include "GPU_vertex_array_id.h"
#include "GPU_context.h"
#include <vector>
#include <string.h>
#include <pthread.h>
#include <mutex>
#include <unordered_set>

#if TRUST_NO_ONE
#if 0
extern "C" {
extern int BLI_thread_is_main(void); /* Blender-specific function */
}

static bool thread_is_main() {
	/* "main" here means the GL context's thread */
	return BLI_thread_is_main();
}
#endif
#endif

struct GPUContext {
	GLuint default_vao;
	std::unordered_set<GPUBatch*> batches; /* Batches that have VAOs from this context */
	std::vector<GLuint> orphaned_vertarray_ids;
	std::mutex orphans_mutex; /* todo: try spinlock instead */
#if TRUST_NO_ONE
	pthread_t thread; /* Thread on which this context is active. */
	bool thread_is_used;

	GPUContext() {
		thread_is_used = false;
	}
#endif
};

#if defined(_MSC_VER) && (_MSC_VER == 1800)
#define thread_local __declspec(thread)
thread_local GPUContext* active_ctx = NULL;
#else
static thread_local GPUContext* active_ctx = NULL;
#endif

static void clear_orphans(GPUContext* ctx)
{
	ctx->orphans_mutex.lock();
	if (!ctx->orphaned_vertarray_ids.empty()) {
		uint orphan_len = (uint)ctx->orphaned_vertarray_ids.size();
		glDeleteVertexArrays(orphan_len, ctx->orphaned_vertarray_ids.data());
		ctx->orphaned_vertarray_ids.clear();
	}
	ctx->orphans_mutex.unlock();
}

GPUContext* GPU_context_create(void)
{
#if TRUST_NO_ONE
	/* assert(thread_is_main()); */
#endif
	GPUContext* ctx = new GPUContext;
	glGenVertexArrays(1, &ctx->default_vao);
	GPU_context_active_set(ctx);
	return ctx;
}

/* to be called after GPU_context_active_set(ctx_to_destroy) */
void GPU_context_discard(GPUContext* ctx)
{
#if TRUST_NO_ONE
	/* Make sure no other thread has locked it. */
	assert(ctx == active_ctx);
	assert(pthread_equal(pthread_self(), ctx->thread));
	assert(ctx->orphaned_vertarray_ids.empty());
#endif
	/* delete remaining vaos */
	while (!ctx->batches.empty()) {
		/* this removes the array entry */
		GPU_batch_vao_cache_clear(*ctx->batches.begin());
	}
	glDeleteVertexArrays(1, &ctx->default_vao);
	delete ctx;
	active_ctx = NULL;
}

/* ctx can be NULL */
void GPU_context_active_set(GPUContext* ctx)
{
#if TRUST_NO_ONE
	if (active_ctx) {
		active_ctx->thread_is_used = false;
	}
	/* Make sure no other context is already bound to this thread. */
	if (ctx) {
		/* Make sure no other thread has locked it. */
		assert(ctx->thread_is_used == false);
		ctx->thread = pthread_self();
		ctx->thread_is_used = true;
	}
#endif
	if (ctx) {
		clear_orphans(ctx);
	}
	active_ctx = ctx;
}

GPUContext* GPU_context_active_get(void)
{
	return active_ctx;
}

GLuint GPU_vao_default(void)
{
#if TRUST_NO_ONE
	assert(active_ctx); /* need at least an active context */
	assert(pthread_equal(pthread_self(), active_ctx->thread)); /* context has been activated by another thread! */
#endif
	return active_ctx->default_vao;
}

GLuint GPU_vao_alloc(void)
{
#if TRUST_NO_ONE
	assert(active_ctx); /* need at least an active context */
	assert(pthread_equal(pthread_self(), active_ctx->thread)); /* context has been activated by another thread! */
#endif
	clear_orphans(active_ctx);

	GLuint new_vao_id = 0;
	glGenVertexArrays(1, &new_vao_id);
	return new_vao_id;
}

/* this can be called from multiple thread */
void GPU_vao_free(GLuint vao_id, GPUContext* ctx)
{
#if TRUST_NO_ONE
	assert(ctx);
#endif
	if (ctx == active_ctx) {
		glDeleteVertexArrays(1, &vao_id);
	}
	else {
		ctx->orphans_mutex.lock();
		ctx->orphaned_vertarray_ids.emplace_back(vao_id);
		ctx->orphans_mutex.unlock();
	}
}

void gpu_context_add_batch(GPUContext* ctx, GPUBatch* batch)
{
	ctx->batches.emplace(batch);
}

void gpu_context_remove_batch(GPUContext* ctx, GPUBatch* batch)
{
	ctx->orphans_mutex.lock();
	ctx->batches.erase(batch);
	ctx->orphans_mutex.unlock();
}
