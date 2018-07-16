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
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file GPU_draw.h
 *  \ingroup gpu
 */

#ifndef __GPU_DRAW_H__
#define __GPU_DRAW_H__

#ifdef __cplusplus
extern "C" {
#endif

struct ImBuf;
struct Image;
struct ImageUser;
struct Main;
struct Object;
struct Scene;
struct ViewLayer;
struct View3D;
struct RegionView3D;
struct SmokeModifierData;
struct DupliObject;

#include "DNA_object_enums.h"

/* OpenGL drawing functions related to shading. */

/* Initialize
 * - sets the default Blender opengl state, if in doubt, check
 *   the contents of this function
 * - this is called when starting Blender, for opengl rendering. */

void GPU_state_init(void);

/* Programmable point size
 * - shaders set their own point size when enabled
 * - use glPointSize when disabled */

void GPU_enable_program_point_size(void);
void GPU_disable_program_point_size(void);

/* Mipmap settings
 * - these will free textures on changes */

void GPU_set_mipmap(struct Main *bmain, bool mipmap);
bool GPU_get_mipmap(void);
void GPU_set_linear_mipmap(bool linear);
bool GPU_get_linear_mipmap(void);
void GPU_paint_set_mipmap(struct Main *bmain, bool mipmap);

/* Anisotropic filtering settings
 * - these will free textures on changes */
void GPU_set_anisotropic(struct Main *bmain, float value);
float GPU_get_anisotropic(void);

/* enable gpu mipmapping */
void GPU_set_gpu_mipmapping(struct Main *bmain, int gpu_mipmap);

/* Image updates and free
 * - these deal with images bound as opengl textures */

void GPU_paint_update_image(struct Image *ima, struct ImageUser *iuser, int x, int y, int w, int h);
void GPU_create_gl_tex(
        unsigned int *bind, unsigned int *rect, float *frect, int rectw, int recth,
        int textarget, bool mipmap, bool use_hight_bit_depth, struct Image *ima);
void GPU_create_gl_tex_compressed(
        unsigned int *bind, unsigned int *pix, int x, int y, int mipmap,
        int textarget, struct Image *ima, struct ImBuf *ibuf);
bool GPU_upload_dxt_texture(struct ImBuf *ibuf);
void GPU_free_image(struct Image *ima);
void GPU_free_images(struct Main *bmain);
void GPU_free_images_anim(struct Main *bmain);
void GPU_free_images_old(struct Main *bmain);

/* smoke drawing functions */
void GPU_free_smoke(struct SmokeModifierData *smd);
void GPU_free_smoke_velocity(struct SmokeModifierData *smd);
void GPU_create_smoke(struct SmokeModifierData *smd, int highres);
void GPU_create_smoke_velocity(struct SmokeModifierData *smd);

/* Delayed free of OpenGL buffers by main thread */
void GPU_free_unused_buffers(struct Main *bmain);

/* utilities */
void	GPU_select_index_set(int index);
void	GPU_select_index_get(int index, int *r_col);
int		GPU_select_to_index(unsigned int col);
void	GPU_select_to_index_array(unsigned int *col, const unsigned int size);

typedef enum eGPUAttribMask {
	GPU_DEPTH_BUFFER_BIT = (1 << 0),
	GPU_ENABLE_BIT = (1 << 1),
	GPU_SCISSOR_BIT = (1 << 2),
	GPU_VIEWPORT_BIT = (1 << 3),
	GPU_BLEND_BIT = (1 << 4),
} eGPUAttribMask;

void gpuPushAttrib(eGPUAttribMask mask);
void gpuPopAttrib(void);

#ifdef __cplusplus
}
#endif

#endif
