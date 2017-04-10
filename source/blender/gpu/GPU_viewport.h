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
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file GPU_viewport.h
 *  \ingroup gpu
 */

#ifndef __GPU_VIEWPORT_H__
#define __GPU_VIEWPORT_H__

#include <stdbool.h>

#include "DNA_vec_types.h"

#include "GPU_framebuffer.h"
#include "GPU_texture.h"

typedef struct GPUViewport GPUViewport;

#define MAX_BUFFERS 8
#define MAX_TEXTURES 16
#define MAX_PASSES 16
#define MAX_STORAGE 9 /* extend if needed */

/* All FramebufferLists are just the same pointers with different names */
typedef struct FramebufferList {
	struct GPUFrameBuffer *framebuffers[MAX_BUFFERS];
} FramebufferList;

typedef struct TextureList {
	struct GPUTexture *textures[MAX_TEXTURES];
} TextureList;

typedef struct PassList {
	struct DRWPass *passes[MAX_PASSES];
} PassList;

typedef struct StorageList {
	void *storage[MAX_STORAGE]; /* custom structs from the engine */
} StorageList;

typedef struct ViewportEngineData {
	void *engine_type;

	FramebufferList *fbl;
	TextureList *txl;
	PassList *psl;
	StorageList *stl;

	/* Profiling data */
	double init_time;
	double cache_time;
	double render_time;
	double background_time;
} ViewportEngineData;

GPUViewport *GPU_viewport_create(void);
void GPU_viewport_bind(GPUViewport *viewport, const rcti *rect);
void GPU_viewport_unbind(GPUViewport *viewport);
void GPU_viewport_free(GPUViewport *viewport);

void *GPU_viewport_engine_data_create(GPUViewport *viewport, void *engine_type);
void *GPU_viewport_engine_data_get(GPUViewport *viewport, void *engine_type);
void *GPU_viewport_framebuffer_list_get(GPUViewport *viewport);
void *GPU_viewport_texture_list_get(GPUViewport *viewport);
void  GPU_viewport_size_get(GPUViewport *viewport, int *size);

bool GPU_viewport_cache_validate(GPUViewport *viewport, unsigned int hash);

/* debug */
bool GPU_viewport_debug_depth_create(GPUViewport *viewport, int width, int height, char err_out[256]);
void GPU_viewport_debug_depth_free(GPUViewport *viewport);
void GPU_viewport_debug_depth_store(GPUViewport *viewport, const int x, const int y);
void GPU_viewport_debug_depth_draw(GPUViewport *viewport, const float znear, const float zfar);
bool GPU_viewport_debug_depth_is_valid(GPUViewport *viewport);
int GPU_viewport_debug_depth_width(const GPUViewport *viewport);
int GPU_viewport_debug_depth_height(const GPUViewport *viewport);

#endif // __GPU_VIEWPORT_H__
