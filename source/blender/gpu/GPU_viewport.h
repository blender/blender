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

struct DRWData;
struct DefaultFramebufferList;
struct DefaultTextureList;
struct GPUFrameBuffer;

GPUViewport *GPU_viewport_create(void);
GPUViewport *GPU_viewport_stereo_create(void);
void GPU_viewport_bind(GPUViewport *viewport, int view, const rcti *rect);
void GPU_viewport_unbind(GPUViewport *viewport);
/**
 * Merge and draw the buffers of \a viewport into the currently active framebuffer, performing
 * color transform to display space.
 *
 * \param rect: Coordinates to draw into. By swapping min and max values, drawing can be done
 * with inversed axis coordinates (upside down or sideways).
 */
void GPU_viewport_draw_to_screen(GPUViewport *viewport, int view, const rcti *rect);
/**
 * Version of #GPU_viewport_draw_to_screen() that lets caller decide if display colorspace
 * transform should be performed.
 */
void GPU_viewport_draw_to_screen_ex(GPUViewport *viewport,
                                    int view,
                                    const rcti *rect,
                                    bool display_colorspace,
                                    bool do_overlay_merge);
/**
 * Must be executed inside Draw-manager OpenGL Context.
 */
void GPU_viewport_free(GPUViewport *viewport);

void GPU_viewport_colorspace_set(GPUViewport *viewport,
                                 ColorManagedViewSettings *view_settings,
                                 const ColorManagedDisplaySettings *display_settings,
                                 float dither);

/**
 * Should be called from DRW after DRW_opengl_context_enable.
 */
void GPU_viewport_bind_from_offscreen(GPUViewport *viewport,
                                      struct GPUOffScreen *ofs,
                                      bool is_xr_surface);
/**
 * Clear vars assigned from offscreen, so we don't free data owned by `GPUOffScreen`.
 */
void GPU_viewport_unbind_from_offscreen(GPUViewport *viewport,
                                        struct GPUOffScreen *ofs,
                                        bool display_colorspace,
                                        bool do_overlay_merge);

struct DRWData **GPU_viewport_data_get(GPUViewport *viewport);

/**
 * Merge the stereo textures. `color` and `overlay` texture will be modified.
 */
void GPU_viewport_stereo_composite(GPUViewport *viewport, Stereo3dFormat *stereo_format);

void GPU_viewport_tag_update(GPUViewport *viewport);
bool GPU_viewport_do_update(GPUViewport *viewport);

int GPU_viewport_active_view_get(GPUViewport *viewport);
bool GPU_viewport_is_stereo_get(GPUViewport *viewport);

GPUTexture *GPU_viewport_color_texture(GPUViewport *viewport, int view);
GPUTexture *GPU_viewport_overlay_texture(GPUViewport *viewport, int view);
GPUTexture *GPU_viewport_depth_texture(GPUViewport *viewport);

/**
 * Overlay frame-buffer for drawing outside of DRW module.
 */
GPUFrameBuffer *GPU_viewport_framebuffer_overlay_get(GPUViewport *viewport);

#ifdef __cplusplus
}
#endif
