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

extern char datatoc_solid_frag_glsl[];
extern char datatoc_flat_lighting_frag_glsl[];
extern char datatoc_workbench_vert_glsl[];

/* *********** STATIC *********** */
static struct {
	struct GPUShader *depth_sh;

	/* Shading Pass */
	struct GPUShader *solid_sh;
	
} e_data = {NULL};


void workbench_solid_materials_init() {
	if (!e_data.depth_sh) {
		/* Depth pass */
		e_data.depth_sh = DRW_shader_create_3D_depth_only();

		/* Shading pass */
		e_data.solid_sh = DRW_shader_create(
		        datatoc_workbench_vert_glsl, NULL, datatoc_solid_frag_glsl, "\n");
	}
}

void workbench_solid_materials_cache_init(WORKBENCH_Data* vedata)
{
	WORKBENCH_PassList *psl = vedata->psl;
	WORKBENCH_StorageList *stl = vedata->stl;

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
	}
	
	/* Depth Pass */
	{
		int state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS;
		psl->depth_pass = DRW_pass_create("Depth Pass", state);
		stl->g_data->depth_shgrp = DRW_shgroup_create(e_data.depth_sh, psl->depth_pass);
	}

	/* Solid Pass */
	{
		int state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL;
		psl->solid_pass = DRW_pass_create("Solid Pass", state);
	}
	
	/* Flat Lighting Pass */
	{	
		int state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL;
		psl->lighting_pass = DRW_pass_create("Lighting Pass", state);
	}
}

	void workbench_solid_materials_cache_populate(WORKBENCH_Data* vedata, Object *ob)
{
	WORKBENCH_PassList *psl = vedata->psl;
	WORKBENCH_StorageList *stl = vedata->stl;
	
	IDProperty *props = BKE_layer_collection_engine_evaluated_get(ob, COLLECTION_MODE_NONE, RE_engine_id_BLENDER_WORKBENCH);
	const float* color = BKE_collection_engine_property_value_get_float_array(props, "object_color");

	if (!DRW_object_is_renderable(ob))
		return;
	
	struct Gwn_Batch *geom = DRW_cache_object_surface_get(ob);
	DRWShadingGroup *grp;
	if (geom) {
		/* Depth */
		DRW_shgroup_call_add(stl->g_data->depth_shgrp, geom, ob->obmat);
		
		/* Solid */
		grp = DRW_shgroup_create(e_data.solid_sh, psl->solid_pass);
		DRW_shgroup_uniform_vec3(grp, "color", color, 1);
		DRW_shgroup_call_add(grp, geom, ob->obmat);
		
		/* Lighting */
		// if studio lighting
		
	}
}

void workbench_solid_materials_cache_finish(WORKBENCH_Data *vedata)
{
	WORKBENCH_StorageList *stl = ((WORKBENCH_Data *)vedata)->stl;

	UNUSED_VARS(stl);
}

void workbench_solid_materials_draw_scene(WORKBENCH_Data *vedata)
{

	WORKBENCH_PassList *psl = ((WORKBENCH_Data *)vedata)->psl;
	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
	
	DRW_draw_pass(psl->depth_pass);
//	if (studio lighting) {
//		DRW_draw_pass(psl->lighting_pass);
//		DRW_draw_pass(psl->solid_pass);
// TODO: COMPOSITE
// 	}
	
// 	if (flat lighting) {
		DRW_draw_pass(psl->solid_pass);
// 	}
}

void workbench_solid_materials_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.solid_sh);
}



