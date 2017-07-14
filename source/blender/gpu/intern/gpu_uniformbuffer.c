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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Clement Foucault.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gpu_uniformbuffer.c
 *  \ingroup gpu
 */

#include <string.h>
#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "gpu_codegen.h"

#include "GPU_extensions.h"
#include "GPU_glew.h"
#include "GPU_material.h"
#include "GPU_uniformbuffer.h"

typedef enum GPUUniformBufferFlag {
	GPU_UBO_FLAG_INITIALIZED = (1 << 0),
	GPU_UBO_FLAG_DIRTY = (1 << 1),
} GPUUniformBufferFlag;

typedef enum GPUUniformBufferType {
	GPU_UBO_STATIC = 0,
	GPU_UBO_DYNAMIC = 1,
} GPUUniformBufferType;

typedef struct GPUUniformBuffer {
	int size;           /* in bytes */
	GLuint bindcode;    /* opengl identifier for UBO */
	int bindpoint;      /* current binding point */
	GPUUniformBufferType type;
} GPUUniformBuffer;

#define GPUUniformBufferStatic GPUUniformBuffer

typedef struct GPUUniformBufferDynamic {
	GPUUniformBuffer buffer;
	ListBase items;				/* GPUUniformBufferDynamicItem */
	void *data;
	char flag;
} GPUUniformBufferDynamic;

typedef struct GPUUniformBufferDynamicItem {
	struct GPUUniformBufferDynamicItem *next, *prev;
	GPUType gputype;
	float *data;
	int size;
} GPUUniformBufferDynamicItem;


/* Prototypes */
static void gpu_uniformbuffer_inputs_sort(struct ListBase *inputs);

static GPUUniformBufferDynamicItem *gpu_uniformbuffer_populate(
        GPUUniformBufferDynamic *ubo, const GPUType gputype, float *num);

static void gpu_uniformbuffer_initialize(GPUUniformBuffer *ubo, const void *data)
{
	glBindBuffer(GL_UNIFORM_BUFFER, ubo->bindcode);
	glBufferData(GL_UNIFORM_BUFFER, ubo->size, data, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

GPUUniformBuffer *GPU_uniformbuffer_create(int size, const void *data, char err_out[256])
{
	GPUUniformBuffer *ubo = MEM_callocN(sizeof(GPUUniformBufferStatic), "GPUUniformBufferStatic");
	ubo->size = size;

	/* Generate Buffer object */
	glGenBuffers(1, &ubo->bindcode);
	
	if (!ubo->bindcode) {
		if (err_out)
			BLI_snprintf(err_out, 256, "GPUUniformBuffer: UBO create failed");
		GPU_uniformbuffer_free(ubo);
		return NULL;
	}

	if (ubo->size > GPU_max_ubo_size()) {
		if (err_out)
			BLI_snprintf(err_out, 256, "GPUUniformBuffer: UBO too big");
		GPU_uniformbuffer_free(ubo);
		return NULL;
	}

	gpu_uniformbuffer_initialize(ubo, data);
	return ubo;
}

/**
 * Create dynamic UBO from parameters
 * Return NULL if failed to create or if \param inputs is empty.
 *
 * \param inputs ListBase of BLI_genericNodeN(GPUInput)
 */
GPUUniformBuffer *GPU_uniformbuffer_dynamic_create(ListBase *inputs, char err_out[256])
{
	/* There is no point on creating an UBO if there is no arguments. */
	if (BLI_listbase_is_empty(inputs)) {
		return NULL;
	}

	GPUUniformBufferDynamic *ubo = MEM_callocN(sizeof(GPUUniformBufferDynamic), "GPUUniformBufferDynamic");
	ubo->buffer.type = GPU_UBO_DYNAMIC;
	ubo->flag = GPU_UBO_FLAG_DIRTY;

	/* Generate Buffer object. */
	glGenBuffers(1, &ubo->buffer.bindcode);

	if (!ubo->buffer.bindcode) {
		if (err_out)
			BLI_snprintf(err_out, 256, "GPUUniformBuffer: UBO create failed");
		GPU_uniformbuffer_free(&ubo->buffer);
		return NULL;
	}

	if (ubo->buffer.size > GPU_max_ubo_size()) {
		if (err_out)
			BLI_snprintf(err_out, 256, "GPUUniformBuffer: UBO too big");
		GPU_uniformbuffer_free(&ubo->buffer);
		return NULL;
	}

	/* Make sure we comply to the ubo alignment requirements. */
	gpu_uniformbuffer_inputs_sort(inputs);

	for (LinkData *link = inputs->first; link; link = link->next) {
		GPUInput *input = link->data;
		gpu_uniformbuffer_populate(ubo, input->type, input->dynamicvec);
	}

	ubo->data = MEM_mallocN(ubo->buffer.size, __func__);

	/* Initialize buffer data. */
	GPU_uniformbuffer_dynamic_update(&ubo->buffer);
	return &ubo->buffer;
}

/**
 * Free the data, and clean the items list.
 */
static void gpu_uniformbuffer_dynamic_reset(GPUUniformBufferDynamic *ubo)
{
	ubo->buffer.size = 0;
	if (ubo->data) {
		MEM_freeN(ubo->data);
	}
	BLI_freelistN(&ubo->items);
}

void GPU_uniformbuffer_free(GPUUniformBuffer *ubo)
{
	if (ubo->type == GPU_UBO_DYNAMIC) {
		gpu_uniformbuffer_dynamic_reset((GPUUniformBufferDynamic *)ubo);
	}

	glDeleteBuffers(1, &ubo->bindcode);
	MEM_freeN(ubo);
}

static void gpu_uniformbuffer_update(GPUUniformBuffer *ubo, const void *data)
{
	glBindBuffer(GL_UNIFORM_BUFFER, ubo->bindcode);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, ubo->size, data);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void GPU_uniformbuffer_update(GPUUniformBuffer *ubo, const void *data)
{
	BLI_assert(ubo->type == GPU_UBO_STATIC);
	gpu_uniformbuffer_update(ubo, data);
}

/**
 * We need to recalculate the internal data, and re-generate it
 * from its populated items.
 */
void GPU_uniformbuffer_dynamic_update(GPUUniformBuffer *ubo_)
{
	BLI_assert(ubo_->type == GPU_UBO_DYNAMIC);
	GPUUniformBufferDynamic *ubo = (GPUUniformBufferDynamic *)ubo_;

	float *offset = ubo->data;
	for (GPUUniformBufferDynamicItem *item = ubo->items.first; item; item = item->next) {
		memcpy(offset, item->data, item->size);
		offset += item->gputype;
	}

	if (ubo->flag & GPU_UBO_FLAG_INITIALIZED) {
		gpu_uniformbuffer_update(ubo_, ubo->data);
	}
	else {
		ubo->flag |= GPU_UBO_FLAG_INITIALIZED;
		gpu_uniformbuffer_initialize(ubo_, ubo->data);
	}

	ubo->flag &= ~GPU_UBO_FLAG_DIRTY;
}

/**
 * Returns 1 if the first item shold be after second item.
 * We make sure the vec4 uniforms come first.
 */
static int inputs_cmp(const void *a, const void *b)
{
	const LinkData *link_a = a, *link_b = b;
	const GPUInput *input_a = link_a->data, *input_b = link_b->data;
	return input_a->type < input_b->type ? 1 : 0;
}

/**
 * Make sure we respect the expected alignment of UBOs.
 * vec4, pad vec3 as vec4, then vec2, then floats.
 */
static void gpu_uniformbuffer_inputs_sort(ListBase *inputs)
{
	BLI_listbase_sort(inputs, inputs_cmp);
}

/**
 * This may now happen from the main thread, so we can't update the UBO
 * We simply flag it as dirty
 */
static GPUUniformBufferDynamicItem *gpu_uniformbuffer_populate(
        GPUUniformBufferDynamic *ubo, const GPUType gputype, float *num)
{
	BLI_assert(gputype <= GPU_VEC4);
	GPUUniformBufferDynamicItem *item = MEM_callocN(sizeof(GPUUniformBufferDynamicItem), __func__);

	/* Treat VEC3 as VEC4 because of UBO struct alignment requirements. */
	GPUType type = gputype == GPU_VEC3 ? GPU_VEC4 : gputype;

	item->gputype = type;
	item->data = num;
	item->size = type * sizeof(float);
	ubo->buffer.size += item->size;

	ubo->flag |= GPU_UBO_FLAG_DIRTY;
	BLI_addtail(&ubo->items, item);

	return item;
}

void GPU_uniformbuffer_bind(GPUUniformBuffer *ubo, int number)
{
	if (number >= GPU_max_ubo_binds()) {
		fprintf(stderr, "Not enough UBO slots.\n");
		return;
	}

	if (ubo->type == GPU_UBO_DYNAMIC) {
		GPUUniformBufferDynamic *ubo_dynamic = (GPUUniformBufferDynamic *)ubo;
		if (ubo_dynamic->flag & GPU_UBO_FLAG_DIRTY) {
			GPU_uniformbuffer_dynamic_update(ubo);
		}
	}

	if (ubo->bindcode != 0) {
		glBindBufferBase(GL_UNIFORM_BUFFER, number, ubo->bindcode);
	}

	ubo->bindpoint = number;
}

int GPU_uniformbuffer_bindpoint(GPUUniformBuffer *ubo)
{
	return ubo->bindpoint;
}

void GPU_uniformbuffer_tag_dirty(GPUUniformBuffer *ubo_) {
	BLI_assert(ubo_->type == GPU_UBO_DYNAMIC);
	GPUUniformBufferDynamic *ubo = (GPUUniformBufferDynamic *)ubo_;
	ubo->flag |= GPU_UBO_FLAG_DIRTY;
}
