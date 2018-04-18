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

/** \file solid_studio_mode.c
 *  \ingroup draw_engine
 *
 * Simple engine for drawing color and/or depth.
 * When we only need simple studio shaders.
 */

#include "DRW_render.h"

#include "GPU_shader.h"

#include "workbench_private.h"
/* Shaders */

extern char datatoc_solid_studio_frag_glsl[];
extern char datatoc_workbench_studio_vert_glsl[];

/* *********** STATIC *********** */
static struct {
	struct GPUShader *depth_sh;

	/* Shading Pass */
	struct GPUShader *solid_sh;

} e_data = {NULL};


/* Functions */

static void workbench_solid_studio_engine_init(void *UNUSED(vedata))
{
	if (!e_data.depth_sh) {
		/* Depth pass */
		e_data.depth_sh = DRW_shader_create_3D_depth_only();

		/* Shading pass */
		e_data.solid_sh = DRW_shader_create(
						datatoc_workbench_studio_vert_glsl, NULL, datatoc_solid_studio_frag_glsl, "\n");
	}
}

static void workbench_solid_studio_cache_init(void *vedata)
{

	WORKBENCH_Data * data = (WORKBENCH_Data *)vedata;
	WORKBENCH_PassList *psl = data->psl;
	WORKBENCH_StorageList *stl = data->stl;

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
}

static void workbench_solid_studio_cache_populate(void *vedata, Object *ob)
{
	WORKBENCH_Data * data = (WORKBENCH_Data *)vedata;

	WORKBENCH_PassList *psl = data->psl;
	WORKBENCH_StorageList *stl = data->stl;

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
	}
}

static void workbench_solid_studio_cache_finish(void *UNUSED(vedata))
{
}

static void workbench_solid_studio_draw_scene(void *vedata)
{
	// WORKBENCH_Data *data = (WORKBENCH_Data *)vedata;
	WORKBENCH_PassList *psl = ((WORKBENCH_Data *)vedata)->psl;

	DRW_draw_pass(psl->depth_pass);
	DRW_draw_pass(psl->solid_pass);
}

static void workbench_solid_studio_engine_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.solid_sh);
}

static const DrawEngineDataSize workbench_data_size = DRW_VIEWPORT_DATA_SIZE(WORKBENCH_Data);

DrawEngineType draw_engine_workbench_solid_studio = {
	NULL, NULL,
	N_("Workbench"),
	&workbench_data_size,
	&workbench_solid_studio_engine_init,
	&workbench_solid_studio_engine_free,
	&workbench_solid_studio_cache_init,
	&workbench_solid_studio_cache_populate,
	&workbench_solid_studio_cache_finish,
	NULL,
	&workbench_solid_studio_draw_scene,
	NULL,
	NULL,
	NULL,
};
