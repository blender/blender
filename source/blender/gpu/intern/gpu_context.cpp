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

/** \file blender/gpu/intern/gpu_context.cpp
 *  \ingroup gpu
 *
 * Manage GL vertex array IDs in a thread-safe way
 * Use these instead of glGenBuffers & its friends
 * - alloc must be called from a thread that is bound
 *   to the context that will be used for drawing with
 *   this vao.
 * - free can be called from any thread
 */

#include "BLI_assert.h"
#include "BLI_utildefines.h"

#include "GPU_context.h"
#include "GPU_framebuffer.h"

#include "gpu_batch_private.h"
#include "gpu_context_private.h"

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

static std::vector<GLuint> orphaned_buffer_ids;
static std::vector<GLuint> orphaned_texture_ids;

static std::mutex orphans_mutex;

struct GPUContext {
	GLuint default_vao;
	std::unordered_set<GPUBatch *> batches; /* Batches that have VAOs from this context */
#ifdef DEBUG
	std::unordered_set<GPUFrameBuffer *> framebuffers; /* Framebuffers that have FBO from this context */
#endif
	std::vector<GLuint> orphaned_vertarray_ids;
	std::vector<GLuint> orphaned_framebuffer_ids;
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
thread_local GPUContext *active_ctx = NULL;
#else
static thread_local GPUContext *active_ctx = NULL;
#endif

static void orphans_add(GPUContext *ctx, std::vector<GLuint> *orphan_list, GLuint id)
{
	std::mutex *mutex = (ctx) ? &ctx->orphans_mutex : &orphans_mutex;

	mutex->lock();
	orphan_list->emplace_back(id);
	mutex->unlock();
}

static void orphans_clear(GPUContext *ctx)
{
	BLI_assert(ctx); /* need at least an active context */
	BLI_assert(pthread_equal(pthread_self(), ctx->thread)); /* context has been activated by another thread! */

	ctx->orphans_mutex.lock();
	if (!ctx->orphaned_vertarray_ids.empty()) {
		uint orphan_len = (uint)ctx->orphaned_vertarray_ids.size();
		glDeleteVertexArrays(orphan_len, ctx->orphaned_vertarray_ids.data());
		ctx->orphaned_vertarray_ids.clear();
	}
	if (!ctx->orphaned_framebuffer_ids.empty()) {
		uint orphan_len = (uint)ctx->orphaned_framebuffer_ids.size();
		glDeleteFramebuffers(orphan_len, ctx->orphaned_framebuffer_ids.data());
		ctx->orphaned_framebuffer_ids.clear();
	}

	ctx->orphans_mutex.unlock();

	orphans_mutex.lock();
	if (!orphaned_buffer_ids.empty()) {
		uint orphan_len = (uint)orphaned_buffer_ids.size();
		glDeleteBuffers(orphan_len, orphaned_buffer_ids.data());
		orphaned_buffer_ids.clear();
	}
	if (!orphaned_texture_ids.empty()) {
		uint orphan_len = (uint)orphaned_texture_ids.size();
		glDeleteTextures(orphan_len, orphaned_texture_ids.data());
		orphaned_texture_ids.clear();
	}
	orphans_mutex.unlock();
}

GPUContext *GPU_context_create(void)
{
	/* BLI_assert(thread_is_main()); */
	GPUContext *ctx = new GPUContext;
	glGenVertexArrays(1, &ctx->default_vao);
	GPU_context_active_set(ctx);
	return ctx;
}

/* to be called after GPU_context_active_set(ctx_to_destroy) */
void GPU_context_discard(GPUContext *ctx)
{
	/* Make sure no other thread has locked it. */
	BLI_assert(ctx == active_ctx);
	BLI_assert(pthread_equal(pthread_self(), ctx->thread));
	BLI_assert(ctx->orphaned_vertarray_ids.empty());
#ifdef DEBUG
	/* For now don't allow GPUFrameBuffers to be reuse in another ctx. */
	BLI_assert(ctx->framebuffers.empty());
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
void GPU_context_active_set(GPUContext *ctx)
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
		orphans_clear(ctx);
	}
	active_ctx = ctx;
}

GPUContext *GPU_context_active_get(void)
{
	return active_ctx;
}

GLuint GPU_vao_default(void)
{
	BLI_assert(active_ctx); /* need at least an active context */
	BLI_assert(pthread_equal(pthread_self(), active_ctx->thread)); /* context has been activated by another thread! */
	return active_ctx->default_vao;
}

GLuint GPU_vao_alloc(void)
{
	GLuint new_vao_id = 0;
	orphans_clear(active_ctx);
	glGenVertexArrays(1, &new_vao_id);
	return new_vao_id;
}

GLuint GPU_fbo_alloc(void)
{
	GLuint new_fbo_id = 0;
	orphans_clear(active_ctx);
	glGenFramebuffers(1, &new_fbo_id);
	return new_fbo_id;
}

GLuint GPU_buf_alloc(void)
{
	GLuint new_buffer_id = 0;
	orphans_clear(active_ctx);
	glGenBuffers(1, &new_buffer_id);
	return new_buffer_id;
}

GLuint GPU_tex_alloc(void)
{
	GLuint new_texture_id = 0;
	orphans_clear(active_ctx);
	glGenTextures(1, &new_texture_id);
	return new_texture_id;
}

void GPU_vao_free(GLuint vao_id, GPUContext *ctx)
{
	BLI_assert(ctx);
	if (ctx == active_ctx) {
		glDeleteVertexArrays(1, &vao_id);
	}
	else {
		orphans_add(ctx, &ctx->orphaned_vertarray_ids, vao_id);
	}
}

void GPU_fbo_free(GLuint fbo_id, GPUContext *ctx)
{
	BLI_assert(ctx);
	if (ctx == active_ctx) {
		glDeleteFramebuffers(1, &fbo_id);
	}
	else {
		orphans_add(ctx, &ctx->orphaned_framebuffer_ids, fbo_id);
	}
}

void GPU_buf_free(GLuint buf_id)
{
	if (active_ctx) {
		glDeleteBuffers(1, &buf_id);
	}
	else {
		orphans_add(NULL, &orphaned_buffer_ids, buf_id);
	}
}

void GPU_tex_free(GLuint tex_id)
{
	if (active_ctx) {
		glDeleteTextures(1, &tex_id);
	}
	else {
		orphans_add(NULL, &orphaned_texture_ids, tex_id);
	}
}

/* GPUBatch & GPUFrameBuffer contains respectively VAO & FBO indices
 * which are not shared across contexts. So we need to keep track of
 * ownership. */

void gpu_context_add_batch(GPUContext *ctx, GPUBatch *batch)
{
	BLI_assert(ctx);
	ctx->orphans_mutex.lock();
	ctx->batches.emplace(batch);
	ctx->orphans_mutex.unlock();
}

void gpu_context_remove_batch(GPUContext *ctx, GPUBatch *batch)
{
	BLI_assert(ctx);
	ctx->orphans_mutex.lock();
	ctx->batches.erase(batch);
	ctx->orphans_mutex.unlock();
}

void gpu_context_add_framebuffer(GPUContext *ctx, GPUFrameBuffer *fb)
{
#ifdef DEBUG
	BLI_assert(ctx);
	ctx->orphans_mutex.lock();
	ctx->framebuffers.emplace(fb);
	ctx->orphans_mutex.unlock();
#else
	UNUSED_VARS(ctx, fb);
#endif
}

void gpu_context_remove_framebuffer(GPUContext *ctx, GPUFrameBuffer *fb)
{
#ifdef DEBUG
	BLI_assert(ctx);
	ctx->orphans_mutex.lock();
	ctx->framebuffers.erase(fb);
	ctx->orphans_mutex.unlock();
#else
	UNUSED_VARS(ctx, fb);
#endif
}
