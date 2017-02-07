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

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "GPU_extensions.h"
#include "GPU_glew.h"
#include "GPU_uniformbuffer.h"

struct GPUUniformBuffer {
	int size;           /* in bytes */
	GLuint bindcode;    /* opengl identifier for UBO */
	int bindpoint;      /* current binding point */
};

GPUUniformBuffer *GPU_uniformbuffer_create(int size, const void *data, char err_out[256])
{
	GPUUniformBuffer *ubo = MEM_callocN(sizeof(GPUUniformBuffer), "GPUUniformBuffer");
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

	glBindBuffer(GL_UNIFORM_BUFFER, ubo->bindcode);
	glBufferData(GL_UNIFORM_BUFFER, ubo->size, data, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	return ubo;
}

void GPU_uniformbuffer_free(GPUUniformBuffer *ubo)
{
	glDeleteBuffers(1, &ubo->bindcode);
	MEM_freeN(ubo);
}

void GPU_uniformbuffer_update(GPUUniformBuffer *ubo, const void *data)
{
	glBindBuffer(GL_UNIFORM_BUFFER, ubo->bindcode);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, ubo->size, data);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void GPU_uniformbuffer_bind(GPUUniformBuffer *ubo, int number)
{
	if (number >= GPU_max_ubo_binds()) {
		fprintf(stderr, "Not enough UBO slots.\n");
		return;
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