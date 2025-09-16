/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "DNA_scene_types.h"
#include "DNA_vec_types.h"

#include "GPU_framebuffer.hh"
#include "GPU_texture.hh"

#define GLA_PIXEL_OFS 0.375f

struct DRWData;
struct GPUViewport;
struct GPUOffScreen;
namespace blender::gpu {
class FrameBuffer;
}  // namespace blender::gpu

GPUViewport *GPU_viewport_create();
GPUViewport *GPU_viewport_stereo_create();
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
                                 const ColorManagedViewSettings *view_settings,
                                 const ColorManagedDisplaySettings *display_settings,
                                 float dither);

/**
 * Should be called from DRW after DRW_gpu_context_enable.
 */
void GPU_viewport_bind_from_offscreen(GPUViewport *viewport,
                                      GPUOffScreen *ofs,
                                      bool is_xr_surface);
/**
 * Clear vars assigned from offscreen, so we don't free data owned by `GPUOffScreen`.
 */
void GPU_viewport_unbind_from_offscreen(GPUViewport *viewport,
                                        GPUOffScreen *ofs,
                                        bool display_colorspace,
                                        bool do_overlay_merge);

DRWData **GPU_viewport_data_get(GPUViewport *viewport);

/**
 * Merge the stereo textures. `color` and `overlay` texture will be modified.
 */
void GPU_viewport_stereo_composite(GPUViewport *viewport, Stereo3dFormat *stereo_format);

void GPU_viewport_tag_update(GPUViewport *viewport);
bool GPU_viewport_do_update(GPUViewport *viewport);

int GPU_viewport_active_view_get(GPUViewport *viewport);
bool GPU_viewport_is_stereo_get(GPUViewport *viewport);

blender::gpu::Texture *GPU_viewport_color_texture(GPUViewport *viewport, int view);
blender::gpu::Texture *GPU_viewport_overlay_texture(GPUViewport *viewport, int view);
blender::gpu::Texture *GPU_viewport_depth_texture(GPUViewport *viewport);

/**
 * Color render and overlay frame-buffers for drawing outside of DRW module.
 */
blender::gpu::FrameBuffer *GPU_viewport_framebuffer_render_get(GPUViewport *viewport);
blender::gpu::FrameBuffer *GPU_viewport_framebuffer_overlay_get(GPUViewport *viewport);
