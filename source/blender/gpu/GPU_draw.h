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

#ifndef __GPU_DRAW_H__
#define __GPU_DRAW_H__

#include "BLI_utildefines.h"
#include "DNA_object_enums.h"

#ifdef __cplusplus
extern "C" {
#endif

struct FluidModifierData;
struct ImBuf;
struct Image;
struct ImageUser;
struct Main;

/* OpenGL drawing functions related to shading. */

/* Mipmap settings
 * - these will free textures on changes */

void GPU_set_mipmap(struct Main *bmain, bool mipmap);
bool GPU_get_mipmap(void);
void GPU_set_linear_mipmap(bool linear);
bool GPU_get_linear_mipmap(void);
void GPU_paint_set_mipmap(struct Main *bmain, bool mipmap);

/* Anisotropic filtering settings
 * - these will free textures on changes */
void GPU_set_anisotropic(float value);
float GPU_get_anisotropic(void);

/* Image updates and free
 * - these deal with images bound as opengl textures */

void GPU_paint_update_image(
    struct Image *ima, struct ImageUser *iuser, int x, int y, int w, int h);
void GPU_create_gl_tex(unsigned int *bind,
                       unsigned int *rect,
                       float *frect,
                       int rectw,
                       int recth,
                       int textarget,
                       bool mipmap,
                       bool half_float,
                       bool use_srgb,
                       struct Image *ima);
void GPU_create_gl_tex_compressed(unsigned int *bind,
                                  int textarget,
                                  struct Image *ima,
                                  struct ImBuf *ibuf);
bool GPU_upload_dxt_texture(struct ImBuf *ibuf, bool use_srgb);
void GPU_free_image(struct Image *ima);
void GPU_free_images(struct Main *bmain);
void GPU_free_images_anim(struct Main *bmain);
void GPU_free_images_old(struct Main *bmain);

/* gpu_draw_smoke.c  */
void GPU_free_smoke(struct FluidModifierData *mmd);
void GPU_free_smoke_velocity(struct FluidModifierData *mmd);
void GPU_create_smoke(struct FluidModifierData *mmd, int highres);
void GPU_create_smoke_coba_field(struct FluidModifierData *mmd);
void GPU_create_smoke_velocity(struct FluidModifierData *mmd);

/* Delayed free of OpenGL buffers by main thread */
void GPU_free_unused_buffers(struct Main *bmain);

#ifdef __cplusplus
}
#endif

#endif
