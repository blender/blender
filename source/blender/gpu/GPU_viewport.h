/*
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
 */

/** \file
 * \ingroup gpu
 */

#pragma once

#include <stdbool.h>

#include "DNA_scene_types.h"
#include "DNA_vec_types.h"

#include "GPU_framebuffer.h"
#include "GPU_texture.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GLA_PIXEL_OFS 0.375f

typedef struct GHash GHash;
typedef struct GPUViewport GPUViewport;

struct GPUFrameBuffer;
struct DefaultFramebufferList;
struct DefaultTextureList;
struct DRWData;

GPUViewport *GPU_viewport_create(void);
GPUViewport *GPU_viewport_stereo_create(void);
void GPU_viewport_bind(GPUViewport *viewport, int view, const rcti *rect);
void GPU_viewport_unbind(GPUViewport *viewport);
void GPU_viewport_draw_to_screen(GPUViewport *viewport, int view, const rcti *rect);
void GPU_viewport_draw_to_screen_ex(GPUViewport *viewport,
                                    int view,
                                    const rcti *rect,
                                    bool display_colorspace,
                                    bool do_overlay_merge);
void GPU_viewport_free(GPUViewport *viewport);

void GPU_viewport_colorspace_set(GPUViewport *viewport,
                                 ColorManagedViewSettings *view_settings,
                                 const ColorManagedDisplaySettings *display_settings,
                                 float dither);

void GPU_viewport_bind_from_offscreen(GPUViewport *viewport, struct GPUOffScreen *ofs);
void GPU_viewport_unbind_from_offscreen(GPUViewport *viewport,
                                        struct GPUOffScreen *ofs,
                                        bool display_colorspace,
                                        bool do_overlay_merge);

struct DRWData **GPU_viewport_data_get(GPUViewport *viewport);

void GPU_viewport_stereo_composite(GPUViewport *viewport, Stereo3dFormat *stereo_format);

void GPU_viewport_tag_update(GPUViewport *viewport);
bool GPU_viewport_do_update(GPUViewport *viewport);

int GPU_viewport_active_view_get(GPUViewport *viewport);
bool GPU_viewport_is_stereo_get(GPUViewport *viewport);

GPUTexture *GPU_viewport_color_texture(GPUViewport *viewport, int view);
GPUTexture *GPU_viewport_overlay_texture(GPUViewport *viewport, int view);
GPUTexture *GPU_viewport_depth_texture(GPUViewport *viewport);

GPUFrameBuffer *GPU_viewport_framebuffer_overlay_get(GPUViewport *viewport);

#ifdef __cplusplus
}
#endif
