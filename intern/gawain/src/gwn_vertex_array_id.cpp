
// Gawain vertex array IDs
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016 Mike Erwin, Clément Foucault
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.#include "buffer_id.h"

#include "gwn_vertex_array_id.h"
#include "gwn_context.h"
#include <vector>
#include <string.h>
#include <pthread.h>
#include <mutex>

#if TRUST_NO_ONE
extern "C" {
extern int BLI_thread_is_main(void); // Blender-specific function
}

static bool thread_is_main()
	{
	// "main" here means the GL context's thread
	return BLI_thread_is_main();
	}
#endif

struct Gwn_Context {
	GLuint default_vao;
	std::vector<GLuint> orphaned_vertarray_ids;
	std::mutex orphans_mutex; // todo: try spinlock instead
#if TRUST_NO_ONE
	pthread_t thread; // Thread on which this context is active.
	bool thread_is_used;
#endif
};

static thread_local Gwn_Context* active_ctx = NULL;

static void clear_orphans(Gwn_Context* ctx)
	{
	ctx->orphans_mutex.lock();
	if (!ctx->orphaned_vertarray_ids.empty())
		{
		unsigned orphan_ct = (unsigned)ctx->orphaned_vertarray_ids.size();
		glDeleteVertexArrays(orphan_ct, ctx->orphaned_vertarray_ids.data());
		ctx->orphaned_vertarray_ids.clear();
		}
	ctx->orphans_mutex.unlock();
	}

Gwn_Context* GWN_context_create(void)
	{
#if TRUST_NO_ONE
	assert(thread_is_main());
#endif
	Gwn_Context* ctx = (Gwn_Context*)calloc(1, sizeof(Gwn_Context));
	new (&ctx->orphans_mutex) std::mutex();
	glGenVertexArrays(1, &ctx->default_vao);
	GWN_context_active_set(ctx);
	return ctx;
	}

// to be called after GWN_context_active_set(ctx_to_destroy)
void GWN_context_discard(Gwn_Context* ctx)
	{
#if TRUST_NO_ONE
	// Make sure no other thread has locked it.
	assert(ctx == active_ctx);
	assert(pthread_equal(pthread_self(), ctx->thread));
	assert(ctx->orphaned_vertarray_ids.empty());
#endif
	glDeleteVertexArrays(1, &ctx->default_vao);
	(&ctx->orphans_mutex)->~mutex();
	free(ctx);
	active_ctx = NULL;
	}

// ctx can be NULL
void GWN_context_active_set(Gwn_Context* ctx)
	{
#if TRUST_NO_ONE
	if (active_ctx)
		active_ctx->thread_is_used = false;
	// Make sure no other context is already bound to this thread.
	if (ctx)
		{
		// Make sure no other thread has locked it.
		assert(ctx->thread_is_used == false);
		ctx->thread = pthread_self();
		ctx->thread_is_used = true;
		}
#endif
	if (ctx)
		clear_orphans(ctx);
	active_ctx = ctx;
	}

Gwn_Context* GWN_context_active_get(void)
	{
	return active_ctx;
	}

GLuint GWN_vao_default(void)
	{
#if TRUST_NO_ONE
	assert(active_ctx); // need at least an active context
	assert(pthread_equal(pthread_self(), active_ctx->thread)); // context has been activated by another thread!
#endif
	return active_ctx->default_vao;
	}

GLuint GWN_vao_alloc(void)
	{
#if TRUST_NO_ONE
	assert(active_ctx); // need at least an active context
	assert(pthread_equal(pthread_self(), active_ctx->thread)); // context has been activated by another thread!
#endif
	clear_orphans(active_ctx);

	GLuint new_vao_id = 0;
	glGenVertexArrays(1, &new_vao_id);
	return new_vao_id;
	}

// this can be called from multiple thread
void GWN_vao_free(GLuint vao_id, Gwn_Context* ctx)
	{
#if TRUST_NO_ONE
	assert(ctx);
#endif
	if (ctx == active_ctx)
		glDeleteVertexArrays(1, &vao_id);
	else
		{
		ctx->orphans_mutex.lock();
		ctx->orphaned_vertarray_ids.emplace_back(vao_id);
		ctx->orphans_mutex.unlock();
		}
	}
