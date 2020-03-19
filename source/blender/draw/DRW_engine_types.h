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
 * Copyright 2016, Blender Foundation.
 */

/** \file
 * \ingroup draw
 */

#ifndef __DRW_ENGINE_TYPES_H__
#define __DRW_ENGINE_TYPES_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Buffer and textures used by the viewport by default */
typedef struct DefaultFramebufferList {
  struct GPUFrameBuffer *default_fb;
  struct GPUFrameBuffer *overlay_fb;
  struct GPUFrameBuffer *in_front_fb;
  struct GPUFrameBuffer *color_only_fb;
  struct GPUFrameBuffer *depth_only_fb;
  struct GPUFrameBuffer *overlay_only_fb;
  struct GPUFrameBuffer *stereo_comp_fb;
} DefaultFramebufferList;

typedef struct DefaultTextureList {
  struct GPUTexture *color;
  struct GPUTexture *color_overlay;
  struct GPUTexture *color_stereo;
  struct GPUTexture *color_overlay_stereo;
  struct GPUTexture *depth;
  struct GPUTexture *depth_in_front;
} DefaultTextureList;

#ifdef __cplusplus
}
#endif

#endif /* __DRW_ENGINE_H__ */
