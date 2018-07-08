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
 * The Original Code is Copyright (C) 2017 by Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, Mike Erwin, Dalai Felinto
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file draw_hair.c
 *  \ingroup draw
 *
 *  \brief Contains procedural GPU hair drawing methods.
 */

#include "DRW_render.h"

#include "BLI_utildefines.h"
#include "BLI_string_utils.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"
#include "DNA_customdata_types.h"

#include "BKE_mesh.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"

#include "ED_particle.h"

#include "GPU_batch.h"
#include "GPU_shader.h"

#include "draw_hair_private.h"

typedef enum ParticleRefineShader {
	PART_REFINE_CATMULL_ROM = 0,
	PART_REFINE_MAX_SHADER,
} ParticleRefineShader;

static GPUShader *g_refine_shaders[PART_REFINE_MAX_SHADER] = {NULL};
static DRWPass *g_tf_pass; /* XXX can be a problem with mulitple DRWManager in the future */

extern char datatoc_common_hair_lib_glsl[];
extern char datatoc_common_hair_refine_vert_glsl[];

static GPUShader *hair_refine_shader_get(ParticleRefineShader sh)
{
	if (g_refine_shaders[sh]) {
		return g_refine_shaders[sh];
	}

	char *vert_with_lib = BLI_string_joinN(datatoc_common_hair_lib_glsl, datatoc_common_hair_refine_vert_glsl);

	const char *var_names[1] = {"outData"};

	g_refine_shaders[sh] = DRW_shader_create_with_transform_feedback(vert_with_lib, NULL, "#define HAIR_PHASE_SUBDIV\n",
	                                                                 GPU_SHADER_TFB_POINTS, var_names, 1);

	MEM_freeN(vert_with_lib);

	return g_refine_shaders[sh];
}

void DRW_hair_init(void)
{
	g_tf_pass = DRW_pass_create("Update Hair Pass", DRW_STATE_TRANS_FEEDBACK);
}

static DRWShadingGroup *drw_shgroup_create_hair_procedural_ex(
        Object *object, ParticleSystem *psys, ModifierData *md,
        DRWPass *hair_pass,
        struct GPUMaterial *gpu_mat, GPUShader *gpu_shader)
{
	/* TODO(fclem): Pass the scene as parameter */
	const DRWContextState *draw_ctx = DRW_context_state_get();
	Scene *scene = draw_ctx->scene;

	int subdiv = scene->r.hair_subdiv;
	int thickness_res = (scene->r.hair_type == SCE_HAIR_SHAPE_STRAND) ? 1 : 2;

	ParticleHairCache *hair_cache;
	ParticleSettings *part = psys->part;
	bool need_ft_update = particles_ensure_procedural_data(object, psys, md, &hair_cache, subdiv, thickness_res);

	DRWShadingGroup *shgrp;
	if (gpu_mat) {
		shgrp = DRW_shgroup_material_create(gpu_mat, hair_pass);
	}
	else if (gpu_shader) {
		shgrp = DRW_shgroup_create(gpu_shader, hair_pass);
	}
	else {
		shgrp = NULL;
		BLI_assert(0);
	}

	/* TODO optimize this. Only bind the ones GPUMaterial needs. */
	for (int i = 0; i < hair_cache->num_uv_layers; ++i) {
		for (int n = 0; n < MAX_LAYER_NAME_CT && hair_cache->uv_layer_names[i][n][0] != '\0'; ++n) {
			DRW_shgroup_uniform_texture(shgrp, hair_cache->uv_layer_names[i][n], hair_cache->uv_tex[i]);
		}
	}
	for (int i = 0; i < hair_cache->num_col_layers; ++i) {
		for (int n = 0; n < MAX_LAYER_NAME_CT && hair_cache->col_layer_names[i][n][0] != '\0'; ++n) {
			DRW_shgroup_uniform_texture(shgrp, hair_cache->col_layer_names[i][n], hair_cache->col_tex[i]);
		}
	}

	DRW_shgroup_uniform_texture(shgrp, "hairPointBuffer", hair_cache->final[subdiv].proc_tex);
	DRW_shgroup_uniform_int(shgrp, "hairStrandsRes", &hair_cache->final[subdiv].strands_res, 1);
	DRW_shgroup_uniform_int_copy(shgrp, "hairThicknessRes", thickness_res);
	DRW_shgroup_uniform_float(shgrp, "hairRadShape", &part->shape, 1);
	DRW_shgroup_uniform_float_copy(shgrp, "hairRadRoot", part->rad_root * part->rad_scale * 0.5f);
	DRW_shgroup_uniform_float_copy(shgrp, "hairRadTip", part->rad_tip * part->rad_scale * 0.5f);
	DRW_shgroup_uniform_bool_copy(shgrp, "hairCloseTip", (part->shape_flag & PART_SHAPE_CLOSE_TIP) != 0);
	/* TODO(fclem): Until we have a better way to cull the hair and render with orco, bypass culling test. */
	DRW_shgroup_call_object_add_no_cull(shgrp, hair_cache->final[subdiv].proc_hairs[thickness_res - 1], object);

	/* Transform Feedback subdiv. */
	if (need_ft_update) {
		int final_points_len = hair_cache->final[subdiv].strands_res * hair_cache->strands_len;
		GPUShader *tf_shader = hair_refine_shader_get(PART_REFINE_CATMULL_ROM);
		DRWShadingGroup *tf_shgrp = DRW_shgroup_transform_feedback_create(tf_shader, g_tf_pass,
		                                                                  hair_cache->final[subdiv].proc_buf);
		DRW_shgroup_uniform_texture(tf_shgrp, "hairPointBuffer", hair_cache->point_tex);
		DRW_shgroup_uniform_texture(tf_shgrp, "hairStrandBuffer", hair_cache->strand_tex);
		DRW_shgroup_uniform_int(tf_shgrp, "hairStrandsRes", &hair_cache->final[subdiv].strands_res, 1);
		DRW_shgroup_call_procedural_points_add(tf_shgrp, final_points_len, NULL);
	}

	return shgrp;
}

DRWShadingGroup *DRW_shgroup_hair_create(
        Object *object, ParticleSystem *psys, ModifierData *md,
        DRWPass *hair_pass,
        GPUShader *shader)
{
	return drw_shgroup_create_hair_procedural_ex(object, psys, md, hair_pass, NULL, shader);
}

DRWShadingGroup *DRW_shgroup_material_hair_create(
        Object *object, ParticleSystem *psys, ModifierData *md,
        DRWPass *hair_pass,
        struct GPUMaterial *material)
{
	return drw_shgroup_create_hair_procedural_ex(object, psys, md, hair_pass, material, NULL);
}

void DRW_hair_update(void)
{
	DRW_draw_pass(g_tf_pass);
}

void DRW_hair_free(void)
{
	for (int i = 0; i < PART_REFINE_MAX_SHADER; ++i) {
		DRW_SHADER_FREE_SAFE(g_refine_shaders[i]);
	}
}
