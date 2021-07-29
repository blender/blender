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
 * Contributor(s): Antony Riakiotakis.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file GPU_compositing.h
 *  \ingroup gpu
 */

#ifndef __GPU_COMPOSITING_H__
#define __GPU_COMPOSITING_H__

#ifdef __cplusplus
extern "C" {
#endif

/* opaque handle for framebuffer compositing effects (defined in gpu_compositing.c )*/
typedef struct GPUFX GPUFX;
struct GPUDOFSettings;
struct GPUSSAOSettings;
struct GPUOffScreen;
struct GPUFXSettings;
struct rcti;
struct Scene;
struct GPUShader;
enum eGPUFXFlags;

/**** Public API *****/

typedef enum GPUFXShaderEffect {
	/* Screen space ambient occlusion shader */
	GPU_SHADER_FX_SSAO = 1,

	/* depth of field passes. Yep, quite a complex effect */
	GPU_SHADER_FX_DEPTH_OF_FIELD_PASS_ONE = 2,
	GPU_SHADER_FX_DEPTH_OF_FIELD_PASS_TWO = 3,
	GPU_SHADER_FX_DEPTH_OF_FIELD_PASS_THREE = 4,
	GPU_SHADER_FX_DEPTH_OF_FIELD_PASS_FOUR = 5,
	GPU_SHADER_FX_DEPTH_OF_FIELD_PASS_FIVE = 6,

	/* high quality */
	GPU_SHADER_FX_DEPTH_OF_FIELD_HQ_PASS_ONE = 7,
	GPU_SHADER_FX_DEPTH_OF_FIELD_HQ_PASS_TWO = 8,
	GPU_SHADER_FX_DEPTH_OF_FIELD_HQ_PASS_THREE = 9,

	GPU_SHADER_FX_DEPTH_RESOLVE = 10,
} GPUFXShaderEffect;

/* keep in synch with enum above! */
#define MAX_FX_SHADERS 11

/* generate a new FX compositor */
GPUFX *GPU_fx_compositor_create(void);

/* destroy a text compositor */
void GPU_fx_compositor_destroy(GPUFX *fx);

/* initialize a framebuffer with size taken from the viewport */
bool GPU_fx_compositor_initialize_passes(
        GPUFX *fx, const struct rcti *rect, const struct rcti *scissor_rect,
        const struct GPUFXSettings *fx_settings);

/* do compositing on the fx passes that have been initialized */
bool GPU_fx_do_composite_pass(
        GPUFX *fx, float projmat[4][4], bool is_persp,
        struct Scene *scene, struct GPUOffScreen *ofs);

/* bind new depth buffer for XRay pass */
void GPU_fx_compositor_setup_XRay_pass(GPUFX *fx, bool do_xray);

/* resolve a final depth buffer by compositing the XRay and normal depth buffers */
void GPU_fx_compositor_XRay_resolve(GPUFX *fx);

void GPU_fx_compositor_init_dof_settings(struct GPUDOFSettings *dof);
void GPU_fx_compositor_init_ssao_settings(struct GPUSSAOSettings *ssao);


/* initialize and cache the shader unform interface for effects */
void GPU_fx_shader_init_interface(struct GPUShader *shader, GPUFXShaderEffect effect);
#ifdef __cplusplus
}
#endif

#endif // __GPU_COMPOSITING_H__
