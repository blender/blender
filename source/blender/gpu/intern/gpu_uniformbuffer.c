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
#include "gpu_context_private.h"

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

struct GPUUniformBuffer {
	int size;           /* in bytes */
	GLuint bindcode;    /* opengl identifier for UBO */
	int bindpoint;      /* current binding point */
	GPUUniformBufferType type;
};

#define GPUUniformBufferStatic GPUUniformBuffer

typedef struct GPUUniformBufferDynamic {
	GPUUniformBuffer buffer;
	void *data;                  /* Continuous memory block to copy to GPU. */
	char flag;
} GPUUniformBufferDynamic;

/* Prototypes */
static GPUType get_padded_gpu_type(struct LinkData *link);
static void gpu_uniformbuffer_inputs_sort(struct ListBase *inputs);

/* Only support up to this type, if you want to extend it, make sure the
 * padding logic is correct for the new types. */
#define MAX_UBO_GPU_TYPE GPU_VEC4

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
	ubo->bindpoint = -1;

	/* Generate Buffer object */
	ubo->bindcode = GPU_buf_alloc();

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
	ubo->buffer.bindpoint = -1;
	ubo->flag = GPU_UBO_FLAG_DIRTY;

	/* Generate Buffer object. */
	ubo->buffer.bindcode = GPU_buf_alloc();

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
		const GPUType gputype = get_padded_gpu_type(link);
		ubo->buffer.size += gputype * sizeof(float);
	}

	/* Allocate the data. */
	ubo->data = MEM_mallocN(ubo->buffer.size, __func__);

	/* Now that we know the total ubo size we can start populating it. */
	float *offset = ubo->data;
	for (LinkData *link = inputs->first; link; link = link->next) {
		GPUInput *input = link->data;
		const GPUType gputype = get_padded_gpu_type(link);
		memcpy(offset, input->dynamicvec, gputype * sizeof(float));
		offset += gputype;
	}

	/* Note since we may create the UBOs in the CPU in a different thread than the main drawing one,
	 * we don't create the UBO in the GPU here. This will happen when we first bind the UBO.
	 */

	return &ubo->buffer;
}

/**
 * Free the data
 */
static void gpu_uniformbuffer_dynamic_free(GPUUniformBuffer *ubo_)
{
	BLI_assert(ubo_->type == GPU_UBO_DYNAMIC);
	GPUUniformBufferDynamic *ubo = (GPUUniformBufferDynamic *)ubo_;

	ubo->buffer.size = 0;
	if (ubo->data) {
		MEM_freeN(ubo->data);
	}
}

void GPU_uniformbuffer_free(GPUUniformBuffer *ubo)
{
	if (ubo->type == GPU_UBO_DYNAMIC) {
		gpu_uniformbuffer_dynamic_free(ubo);
	}

	GPU_buf_free(ubo->bindcode);
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
 * We need to pad some data types (vec3) on the C side
 * To match the GPU expected memory block alignment.
 */
static GPUType get_padded_gpu_type(LinkData *link)
{
	GPUInput *input = link->data;
	GPUType gputype = input->type;

	/* Unless the vec3 is followed by a float we need to treat it as a vec4. */
	if (gputype == GPU_VEC3 &&
	    (link->next != NULL) &&
	    (((GPUInput *)link->next->data)->type != GPU_FLOAT))
	{
		gputype = GPU_VEC4;
	}

	return gputype;
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
	/* Order them as vec4, vec3, vec2, float. */
	BLI_listbase_sort(inputs, inputs_cmp);

	/* Creates a lookup table for the different types; */
	LinkData *inputs_lookup[MAX_UBO_GPU_TYPE + 1] = {NULL};
	GPUType cur_type = MAX_UBO_GPU_TYPE + 1;

	for (LinkData *link = inputs->first; link; link = link->next) {
		GPUInput *input = link->data;
		if (input->type == cur_type) {
			continue;
		}
		else {
			inputs_lookup[input->type] = link;
			cur_type = input->type;
		}
	}

	/* If there is no GPU_VEC3 there is no need for alignment. */
	if (inputs_lookup[GPU_VEC3] == NULL) {
		return;
	}

	LinkData *link = inputs_lookup[GPU_VEC3];
	while (link != NULL && ((GPUInput *)link->data)->type == GPU_VEC3) {
		LinkData *link_next = link->next;

		/* If GPU_VEC3 is followed by nothing or a GPU_FLOAT, no need for aligment. */
		if ((link_next == NULL) ||
		    ((GPUInput *)link_next->data)->type == GPU_FLOAT)
		{
			break;
		}

		/* If there is a float, move it next to current vec3. */
		if (inputs_lookup[GPU_FLOAT] != NULL) {
			LinkData *float_input = inputs_lookup[GPU_FLOAT];
			inputs_lookup[GPU_FLOAT] = float_input->next;

			BLI_remlink(inputs, float_input);
			BLI_insertlinkafter(inputs, link, float_input);
		}

		link = link_next;
	}
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

void GPU_uniformbuffer_unbind(GPUUniformBuffer *ubo)
{
	ubo->bindpoint = -1;
}

int GPU_uniformbuffer_bindpoint(GPUUniformBuffer *ubo)
{
	return ubo->bindpoint;
}

#undef MAX_UBO_GPU_TYPE
