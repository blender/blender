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
 * Contributor(s): Brecht Van Lommel, Clément Foucault.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file GPU_lamp.h
 *  \ingroup gpu
 */

#ifndef __GPU_LAMP_H__
#define __GPU_LAMP_H__

#ifdef __cplusplus
extern "C" {
#endif

struct Scene;
struct Object;
struct RenderEngineType;

typedef struct GPULamp GPULamp;

typedef struct LampEngineData {
	struct GPUFrameBuffer *framebuffers[4];
	struct GPUTexture *textures[4];
	void *storage[4];
} LampEngineData;

GPULamp *GPU_lamp_from_engine(struct Scene *scene, struct Object *ob, Object *par, struct RenderEngineType *re);
GPULamp *GPU_lamp_from_blender(struct Scene *scene, struct Object *ob, struct Object *par);
void GPU_lamp_free(struct Object *ob);

bool GPU_lamp_override_visible(GPULamp *lamp, struct SceneRenderLayer *srl, struct Material *ma);
bool GPU_lamp_has_shadow_buffer(GPULamp *lamp);
void GPU_lamp_update_buffer_mats(GPULamp *lamp);
void GPU_lamp_shadow_buffer_bind(GPULamp *lamp, float viewmat[4][4], int *winsize, float winmat[4][4]);
void GPU_lamp_shadow_buffer_unbind(GPULamp *lamp);
int GPU_lamp_shadow_buffer_type(GPULamp *lamp);
int GPU_lamp_shadow_bind_code(GPULamp *lamp);
float *GPU_lamp_dynpersmat(GPULamp *lamp);

void GPU_lamp_update(GPULamp *lamp, int lay, int hide, float obmat[4][4]);
void GPU_lamp_update_colors(GPULamp *lamp, float r, float g, float b, float energy);
void GPU_lamp_update_distance(GPULamp *lamp, float distance, float att1, float att2,
                              float coeff_const, float coeff_lin, float coeff_quad);
void GPU_lamp_update_spot(GPULamp *lamp, float spotsize, float spotblend);
int GPU_lamp_shadow_layer(GPULamp *lamp);

#ifdef __cplusplus
}
#endif

#endif  /* __GPU_LAMP_H__ */
