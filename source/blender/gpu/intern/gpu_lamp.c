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
 * Contributor(s): Brecht Van Lommel, Clément Foucault.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_lamp.c
 *  \ingroup gpu
 *
 * Manages Opengl lights.
 */

#include "MEM_guardedalloc.h"

#include "DNA_lamp_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_group.h"

#include "GPU_framebuffer.h"
#include "GPU_glew.h"
#include "GPU_lamp.h"
#include "GPU_material.h"
#include "GPU_shader.h"
#include "GPU_texture.h"

#include "gpu_lamp_private.h"

bool GPU_lamp_override_visible(GPULamp *lamp, SceneRenderLayer *srl, Material *ma)
{
	if (srl && srl->light_override)
		return BKE_group_object_exists(srl->light_override, lamp->ob);
	else if (ma && ma->group)
		return BKE_group_object_exists(ma->group, lamp->ob);
	else
		return true;
}

static void gpu_lamp_calc_winmat(GPULamp *lamp)
{
	float temp, angle, pixsize, wsize;

	if (lamp->type == LA_SUN) {
		wsize = lamp->la->shadow_frustum_size;
		orthographic_m4(lamp->winmat, -wsize, wsize, -wsize, wsize, lamp->d, lamp->clipend);
	}
	else if (lamp->type == LA_SPOT) {
		angle = saacos(lamp->spotsi);
		temp = 0.5f * lamp->size * cosf(angle) / sinf(angle);
		pixsize = lamp->d / temp;
		wsize = pixsize * 0.5f * lamp->size;
		/* compute shadows according to X and Y scaling factors */
		perspective_m4(
		        lamp->winmat,
		        -wsize * lamp->spotvec[0], wsize * lamp->spotvec[0],
		        -wsize * lamp->spotvec[1], wsize * lamp->spotvec[1],
		        lamp->d, lamp->clipend);
	}
}

void GPU_lamp_update(GPULamp *lamp, int lay, int hide, float obmat[4][4])
{
	float mat[4][4];
	float obmat_scale[3];

	lamp->lay = lay;
	lamp->hide = hide;

	normalize_m4_m4_ex(mat, obmat, obmat_scale);

	copy_v3_v3(lamp->vec, mat[2]);
	copy_v3_v3(lamp->co, mat[3]);
	copy_m4_m4(lamp->obmat, mat);
	invert_m4_m4(lamp->imat, mat);

	if (lamp->type == LA_SPOT) {
		/* update spotlamp scale on X and Y axis */
		lamp->spotvec[0] = obmat_scale[0] / obmat_scale[2];
		lamp->spotvec[1] = obmat_scale[1] / obmat_scale[2];
	}

	if (GPU_lamp_has_shadow_buffer(lamp)) {
		/* makeshadowbuf */
		gpu_lamp_calc_winmat(lamp);
	}
}

void GPU_lamp_update_colors(GPULamp *lamp, float r, float g, float b, float energy)
{
	lamp->energy = energy;
	if (lamp->mode & LA_NEG) lamp->energy = -lamp->energy;

	lamp->col[0] = r;
	lamp->col[1] = g;
	lamp->col[2] = b;
}

void GPU_lamp_update_distance(GPULamp *lamp, float distance, float att1, float att2,
                              float coeff_const, float coeff_lin, float coeff_quad)
{
	lamp->dist = distance;
	lamp->att1 = att1;
	lamp->att2 = att2;
	lamp->coeff_const = coeff_const;
	lamp->coeff_lin = coeff_lin;
	lamp->coeff_quad = coeff_quad;
}

void GPU_lamp_update_spot(GPULamp *lamp, float spotsize, float spotblend)
{
	lamp->spotsi = cosf(spotsize * 0.5f);
	lamp->spotbl = (1.0f - lamp->spotsi) * spotblend;
}

static void gpu_lamp_from_blender(Scene *scene, Object *ob, Object *par, Lamp *la, GPULamp *lamp)
{
	lamp->scene = scene;
	lamp->ob = ob;
	lamp->par = par;
	lamp->la = la;

	/* add_render_lamp */
	lamp->mode = la->mode;
	lamp->type = la->type;

	lamp->energy = la->energy;
	if (lamp->mode & LA_NEG) lamp->energy = -lamp->energy;

	lamp->col[0] = la->r;
	lamp->col[1] = la->g;
	lamp->col[2] = la->b;

	GPU_lamp_update(lamp, ob->lay, (ob->restrictflag & OB_RESTRICT_RENDER), ob->obmat);

	lamp->spotsi = la->spotsize;
	if (lamp->mode & LA_HALO)
		if (lamp->spotsi > DEG2RADF(170.0f))
			lamp->spotsi = DEG2RADF(170.0f);
	lamp->spotsi = cosf(lamp->spotsi * 0.5f);
	lamp->spotbl = (1.0f - lamp->spotsi) * la->spotblend;
	lamp->k = la->k;

	lamp->dist = la->dist;
	lamp->falloff_type = la->falloff_type;
	lamp->att1 = la->att1;
	lamp->att2 = la->att2;
	lamp->coeff_const = la->coeff_const;
	lamp->coeff_lin = la->coeff_lin;
	lamp->coeff_quad = la->coeff_quad;
	lamp->curfalloff = la->curfalloff;

	/* initshadowbuf */
	lamp->bias = 0.02f * la->bias;
	lamp->size = la->bufsize;
	lamp->d = la->clipsta;
	lamp->clipend = la->clipend;

	/* arbitrary correction for the fact we do no soft transition */
	lamp->bias *= 0.25f;
}

static void gpu_lamp_shadow_free(GPULamp *lamp)
{
	if (lamp->tex) {
		GPU_texture_free(lamp->tex);
		lamp->tex = NULL;
	}
	if (lamp->depthtex) {
		GPU_texture_free(lamp->depthtex);
		lamp->depthtex = NULL;
	}
	if (lamp->fb) {
		GPU_framebuffer_free(lamp->fb);
		lamp->fb = NULL;
	}
	if (lamp->blurtex) {
		GPU_texture_free(lamp->blurtex);
		lamp->blurtex = NULL;
	}
	if (lamp->blurfb) {
		GPU_framebuffer_free(lamp->blurfb);
		lamp->blurfb = NULL;
	}
}

static GPUTexture *gpu_lamp_create_vsm_shadow_map(int size)
{
	return GPU_texture_create_2D_custom(size, size, 2, GPU_RG32F, NULL, NULL);
}

GPULamp *GPU_lamp_from_engine(Scene *scene, Object *ob, Object *par, struct RenderEngineType *re)
{
	GPULamp *lamp;
	LinkData *link;

	for (link = ob->gpulamp.first; link; link = link->next) {
		lamp = (GPULamp *)link->data;

		if ((lamp->par == par) && (lamp->scene == scene) && (lamp->re == re))
			return link->data;
	}

	lamp = MEM_callocN(sizeof(GPULamp), "GPULamp");

	link = MEM_callocN(sizeof(LinkData), "GPULampLink");
	link->data = lamp;
	BLI_addtail(&ob->gpulamp, link);

	lamp->scene = scene;
	lamp->ob = ob;
	lamp->par = par;
	lamp->la = ob->data;
	lamp->re = re;

	return lamp;
}

GPULamp *GPU_lamp_from_blender(Scene *scene, Object *ob, Object *par)
{
	Lamp *la;
	GPULamp *lamp;
	LinkData *link;

	for (link = ob->gpulamp.first; link; link = link->next) {
		lamp = (GPULamp *)link->data;

		if (lamp->par == par && lamp->scene == scene)
			return link->data;
	}

	lamp = MEM_callocN(sizeof(GPULamp), "GPULamp");

	link = MEM_callocN(sizeof(LinkData), "GPULampLink");
	link->data = lamp;
	BLI_addtail(&ob->gpulamp, link);

	la = ob->data;
	gpu_lamp_from_blender(scene, ob, par, la, lamp);

	if ((la->type == LA_SPOT && (la->mode & (LA_SHAD_BUF | LA_SHAD_RAY))) ||
	    (la->type == LA_SUN && (la->mode & LA_SHAD_RAY)))
	{
		/* opengl */
		lamp->fb = GPU_framebuffer_create();
		if (!lamp->fb) {
			gpu_lamp_shadow_free(lamp);
			return lamp;
		}

		if (lamp->la->shadowmap_type == LA_SHADMAP_VARIANCE) {
			/* Shadow depth map */
			lamp->depthtex = GPU_texture_create_depth(lamp->size, lamp->size, NULL);
			if (!lamp->depthtex) {
				gpu_lamp_shadow_free(lamp);
				return lamp;
			}

			if (!GPU_framebuffer_texture_attach(lamp->fb, lamp->depthtex, 0)) {
				gpu_lamp_shadow_free(lamp);
				return lamp;
			}

			/* Shadow color map */
			lamp->tex = gpu_lamp_create_vsm_shadow_map(lamp->size);
			if (!lamp->tex) {
				gpu_lamp_shadow_free(lamp);
				return lamp;
			}

			if (!GPU_framebuffer_texture_attach(lamp->fb, lamp->tex, 0)) {
				gpu_lamp_shadow_free(lamp);
				return lamp;
			}

			if (!GPU_framebuffer_check_valid(lamp->fb, NULL)) {
				gpu_lamp_shadow_free(lamp);
				return lamp;
			}

			/* FBO and texture for blurring */
			lamp->blurfb = GPU_framebuffer_create();
			if (!lamp->blurfb) {
				gpu_lamp_shadow_free(lamp);
				return lamp;
			}

			lamp->blurtex = gpu_lamp_create_vsm_shadow_map(lamp->size * 0.5);
			if (!lamp->blurtex) {
				gpu_lamp_shadow_free(lamp);
				return lamp;
			}

			if (!GPU_framebuffer_texture_attach(lamp->blurfb, lamp->blurtex, 0)) {
				gpu_lamp_shadow_free(lamp);
				return lamp;
			}

			/* we need to properly bind to test for completeness */
			GPU_texture_bind_as_framebuffer(lamp->blurtex);

			if (!GPU_framebuffer_check_valid(lamp->blurfb, NULL)) {
				gpu_lamp_shadow_free(lamp);
				return lamp;
			}

			GPU_framebuffer_texture_unbind(lamp->blurfb, lamp->blurtex);
		}
		else {
			lamp->tex = GPU_texture_create_depth(lamp->size, lamp->size, NULL);
			if (!lamp->tex) {
				gpu_lamp_shadow_free(lamp);
				return lamp;
			}

			if (!GPU_framebuffer_texture_attach(lamp->fb, lamp->tex, 0)) {
				gpu_lamp_shadow_free(lamp);
				return lamp;
			}

			if (!GPU_framebuffer_check_valid(lamp->fb, NULL)) {
				gpu_lamp_shadow_free(lamp);
				return lamp;
			}
		}

		GPU_framebuffer_restore();

		lamp->shadow_color[0] = la->shdwr;
		lamp->shadow_color[1] = la->shdwg;
		lamp->shadow_color[2] = la->shdwb;
	}
	else {
		lamp->shadow_color[0] = 1.0;
		lamp->shadow_color[1] = 1.0;
		lamp->shadow_color[2] = 1.0;
	}

	return lamp;
}

void GPU_lamp_free(Object *ob)
{
	GPULamp *lamp;
	LinkData *link;
	LinkData *nlink;
	Material *ma;

	for (link = ob->gpulamp.first; link; link = link->next) {
		lamp = link->data;

		while (lamp->materials.first) {
			nlink = lamp->materials.first;
			ma = nlink->data;
			BLI_freelinkN(&lamp->materials, nlink);

			if (ma->gpumaterial.first)
				GPU_material_free(&ma->gpumaterial);
		}

		gpu_lamp_shadow_free(lamp);

		MEM_freeN(lamp);
	}

	BLI_freelistN(&ob->gpulamp);
}

bool GPU_lamp_has_shadow_buffer(GPULamp *lamp)
{
	return (!(lamp->scene->gm.flag & GAME_GLSL_NO_SHADOWS) &&
	        !(lamp->scene->gm.flag & GAME_GLSL_NO_LIGHTS) &&
	        lamp->tex && lamp->fb);
}

void GPU_lamp_update_buffer_mats(GPULamp *lamp)
{
	float rangemat[4][4], persmat[4][4];

	/* initshadowbuf */
	invert_m4_m4(lamp->viewmat, lamp->obmat);
	normalize_v3(lamp->viewmat[0]);
	normalize_v3(lamp->viewmat[1]);
	normalize_v3(lamp->viewmat[2]);

	/* makeshadowbuf */
	mul_m4_m4m4(persmat, lamp->winmat, lamp->viewmat);

	/* opengl depth buffer is range 0.0..1.0 instead of -1.0..1.0 in blender */
	unit_m4(rangemat);
	rangemat[0][0] = 0.5f;
	rangemat[1][1] = 0.5f;
	rangemat[2][2] = 0.5f;
	rangemat[3][0] = 0.5f;
	rangemat[3][1] = 0.5f;
	rangemat[3][2] = 0.5f;

	mul_m4_m4m4(lamp->persmat, rangemat, persmat);
}

void GPU_lamp_shadow_buffer_bind(GPULamp *lamp, float viewmat[4][4], int *winsize, float winmat[4][4])
{
	GPU_lamp_update_buffer_mats(lamp);

	/* opengl */
	glDisable(GL_SCISSOR_TEST);
	GPU_texture_bind_as_framebuffer(lamp->tex);
	if (lamp->la->shadowmap_type == LA_SHADMAP_VARIANCE)
		GPU_shader_bind(GPU_shader_get_builtin_shader(GPU_SHADER_VSM_STORE));

	/* set matrices */
	copy_m4_m4(viewmat, lamp->viewmat);
	copy_m4_m4(winmat, lamp->winmat);
	*winsize = lamp->size;
}

void GPU_lamp_shadow_buffer_unbind(GPULamp *lamp)
{
	if (lamp->la->shadowmap_type == LA_SHADMAP_VARIANCE) {
		GPU_shader_unbind();
		GPU_framebuffer_blur(lamp->fb, lamp->tex, lamp->blurfb, lamp->blurtex);
	}

	GPU_framebuffer_texture_unbind(lamp->fb, lamp->tex);
	GPU_framebuffer_restore();
	glEnable(GL_SCISSOR_TEST);
}

int GPU_lamp_shadow_buffer_type(GPULamp *lamp)
{
	return lamp->la->shadowmap_type;
}

int GPU_lamp_shadow_bind_code(GPULamp *lamp)
{
	return lamp->tex ? GPU_texture_opengl_bindcode(lamp->tex) : -1;
}

float *GPU_lamp_dynpersmat(GPULamp *lamp)
{
	return &lamp->dynpersmat[0][0];
}

int GPU_lamp_shadow_layer(GPULamp *lamp)
{
	if (lamp->fb && lamp->tex && (lamp->mode & (LA_LAYER | LA_LAYER_SHADOW)))
		return lamp->lay;
	else
		return -1;
}
