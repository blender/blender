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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_viewport.c
 *  \ingroup gpu
 *
 * System that manages viewport drawing.
 */

#include "GPU_glew.h"
#include "GPU_immediate.h"
#include "GPU_viewport.h"
#include "GPU_texture.h"

#include "MEM_guardedalloc.h"

struct GPUViewport {
	float pad[4];

	/* debug */
	GPUTexture *debug_depth;
	int debug_width, debug_height;
};

GPUViewport *GPU_viewport_create(void)
{
	GPUViewport *viewport = MEM_callocN(sizeof(GPUViewport), "GPUViewport");
	return viewport;
}

void GPU_viewport_free(GPUViewport *viewport)
{
	GPU_viewport_debug_depth_free(viewport);
	MEM_freeN(viewport);
}

/****************** debug ********************/

bool GPU_viewport_debug_depth_create(GPUViewport *viewport, int width, int height, char err_out[256])
{
	viewport->debug_depth = GPU_texture_create_2D_custom(width, height, 4, GPU_RGBA16F, NULL, err_out);
	return (viewport->debug_depth != NULL);
}

void GPU_viewport_debug_depth_free(GPUViewport *viewport)
{
	if (viewport->debug_depth != NULL) {
		MEM_freeN(viewport->debug_depth);
		viewport->debug_depth = NULL;
	}
}

void GPU_viewport_debug_depth_store(GPUViewport *viewport, const int x, const int y)
{
	const int w = GPU_texture_width(viewport->debug_depth);
	const int h = GPU_texture_height(viewport->debug_depth);

	GPU_texture_bind(viewport->debug_depth, 0);
	glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, x, y, w, h, 0);
	GPU_texture_unbind(viewport->debug_depth);
}

void GPU_viewport_debug_depth_draw(GPUViewport *viewport, const float znear, const float zfar)
{
	const float w = (float)GPU_texture_width(viewport->debug_depth);
	const float h = (float)GPU_texture_height(viewport->debug_depth);

	VertexFormat *format = immVertexFormat();
	unsigned texcoord = add_attrib(format, "texCoord", GL_FLOAT, 2, KEEP_FLOAT);
	unsigned pos = add_attrib(format, "pos", GL_FLOAT, 2, KEEP_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_IMAGE_DEPTH);

	GPU_texture_bind(viewport->debug_depth, 0);

	immUniform1f("znear", znear);
	immUniform1f("zfar", zfar);
	immUniform1i("image", 0); /* default GL_TEXTURE0 unit */

	immBegin(GL_QUADS, 4);

	immAttrib2f(texcoord, 0.0f, 0.0f);
	immVertex2f(pos, 0.0f, 0.0f);

	immAttrib2f(texcoord, 1.0f, 0.0f);
	immVertex2f(pos, w, 0.0f);

	immAttrib2f(texcoord, 1.0f, 1.0f);
	immVertex2f(pos, w, h);

	immAttrib2f(texcoord, 0.0f, 1.0f);
	immVertex2f(pos, 0.0f, h);

	immEnd();

	GPU_texture_unbind(viewport->debug_depth);

	immUnbindProgram();
}

int GPU_viewport_debug_depth_width(const GPUViewport *viewport)
{
	return GPU_texture_width(viewport->debug_depth);
}

int GPU_viewport_debug_depth_height(const GPUViewport *viewport)
{
	return GPU_texture_height(viewport->debug_depth);
}

bool GPU_viewport_debug_depth_is_valid(GPUViewport *viewport)
{
	return viewport->debug_depth != NULL;
}
