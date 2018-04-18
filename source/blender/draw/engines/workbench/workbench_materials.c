/*
 * Copyright 2016, Blender Foundation.
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
 * Contributor(s): Blender Institute
 *
 */

/** \file workbench_materials.c
 *  \ingroup draw_engine
 */
 
#include "workbench_private.h"
#include "GPU_shader.h"

/* *********** STATIC *********** */
static struct {
	struct GPUShader *depth_sh;

	/* Solid flat mode */
	struct GPUShader *solid_flat_sh;
	
	/* Solid studio mode */
	struct GPUShader *solid_studio_sh;

} e_data = {NULL};

/* Shaders */
extern char datatoc_solid_flat_frag_glsl[];
extern char datatoc_solid_studio_frag_glsl[];
extern char datatoc_workbench_vert_glsl[];
extern char datatoc_workbench_studio_vert_glsl[];
extern char datatoc_workbench_diffuse_lib_glsl[];

/* Functions */
static uint get_material_hash(const float color[3]) {
	uint r = (uint)(color[0] * 512);
	uint g = (uint)(color[1] * 512);
	uint b = (uint)(color[2] * 512);
	
	return r + g * 4096 + b * 4096 * 4096;
}

WORKBENCH_MaterialData* workbench_get_or_create_solid_flat_material_data(WORKBENCH_Data *vedata, const float color[3]) {
	WORKBENCH_StorageList *stl = vedata->stl;
	WORKBENCH_PassList *psl = vedata->psl;
	WORKBENCH_PrivateData* wpd = stl->g_data;
	
	uint hash = get_material_hash(color);
	WORKBENCH_MaterialData *material;
	
	material = BLI_ghash_lookup(wpd->material_hash, SET_UINT_IN_POINTER(hash));
	if (material == NULL) {
		material = MEM_mallocN(sizeof(WORKBENCH_MaterialData), "WORKBENCH_MaterialData");
		material->shgrp = DRW_shgroup_create(e_data.solid_flat_sh, psl->solid_pass);
		material->color[0] = color[0];
		material->color[1] = color[1];
		material->color[2] = color[2];
		DRW_shgroup_uniform_vec3(material->shgrp, "color", material->color, 1);
		BLI_ghash_insert(wpd->material_hash, SET_UINT_IN_POINTER(hash), material);
	}
	return material;
}

WORKBENCH_MaterialData* workbench_get_or_create_solid_studio_material_data(WORKBENCH_Data *vedata, const float color[3]) {
	WORKBENCH_StorageList *stl = vedata->stl;
	WORKBENCH_PassList *psl = vedata->psl;
	WORKBENCH_PrivateData* wpd = stl->g_data;
	
	uint hash = get_material_hash(color);
	WORKBENCH_MaterialData *material;
	
	material = BLI_ghash_lookup(wpd->material_hash, SET_UINT_IN_POINTER(hash));
	if (material == NULL) {
		material = MEM_mallocN(sizeof(WORKBENCH_MaterialData), "WORKBENCH_MaterialData");
		material->shgrp = DRW_shgroup_create(e_data.solid_studio_sh, psl->solid_pass);
		material->color[0] = color[0];
		material->color[1] = color[1];
		material->color[2] = color[2];
		DRW_shgroup_uniform_vec3(material->shgrp, "color", material->color, 1);
		BLI_ghash_insert(wpd->material_hash, SET_UINT_IN_POINTER(hash), material);
	}
	return material;
}

void workbench_materials_engine_init(void) {
	if (!e_data.depth_sh) {
		/* Depth pass */
		e_data.depth_sh = DRW_shader_create_3D_depth_only();

		/* Solid flat */
		e_data.solid_flat_sh = DRW_shader_create(datatoc_workbench_vert_glsl, NULL, datatoc_solid_flat_frag_glsl, "\n");
		e_data.solid_studio_sh = DRW_shader_create(datatoc_workbench_studio_vert_glsl, NULL, datatoc_solid_studio_frag_glsl, datatoc_workbench_diffuse_lib_glsl);
	}
}

void workbench_materials_engine_finish(void) {
	DRW_SHADER_FREE_SAFE(e_data.solid_flat_sh);
	DRW_SHADER_FREE_SAFE(e_data.solid_studio_sh);
}

void workbench_materials_cache_init(WORKBENCH_Data *vedata) {
	WORKBENCH_StorageList* stl = vedata->stl;
	WORKBENCH_PassList* psl = vedata->psl;
	WORKBENCH_PrivateData* wpd = stl->g_data;
	
	wpd->depth_shgrp = DRW_shgroup_create(e_data.depth_sh, psl->depth_pass);
	wpd->material_hash = BLI_ghash_ptr_new("Workbench material_hash");
}

void workbench_materials_cache_finish(WORKBENCH_Data *vedata) {
	WORKBENCH_StorageList* stl = vedata->stl;
	WORKBENCH_PrivateData* wpd = stl->g_data;
	BLI_ghash_free(wpd->material_hash, NULL, MEM_freeN);
}
