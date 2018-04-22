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

/** \file solid_flat_mode.c
 *  \ingroup draw_engine
 *
 * Simple engine for drawing color and/or depth.
 * When we only need simple flat shaders.
 */

#include "DRW_render.h"

#include "GPU_shader.h"

#include "workbench_private.h"
/* Functions */

static void workbench_solid_flat_engine_init(void *UNUSED(vedata))
{
	workbench_materials_engine_init();
}

static void workbench_solid_flat_cache_init(void *vedata)
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
	}

	/* Solid Pass */
	{
		int state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL;
		psl->solid_pass = DRW_pass_create("Solid Pass", state);
	}

	workbench_materials_cache_init(data);
}


static void workbench_solid_flat_cache_populate(void *vedata, Object *ob)
{
	WORKBENCH_Data * data = (WORKBENCH_Data *)vedata;
	workbench_materials_solid_cache_populate(data, ob);
}

static void workbench_solid_flat_cache_finish(void *UNUSED(vedata))
{
}

static void workbench_solid_flat_draw_scene(void *vedata)
{
	WORKBENCH_Data *data = (WORKBENCH_Data *)vedata;
	WORKBENCH_PassList *psl = data->psl;

	DRW_draw_pass(psl->depth_pass);
	DRW_draw_pass(psl->solid_pass);

	workbench_materials_draw_scene_finish(data);
}

static void workbench_solid_flat_engine_free(void)
{
	workbench_materials_engine_free();
}

static const DrawEngineDataSize workbench_data_size = DRW_VIEWPORT_DATA_SIZE(WORKBENCH_Data);

DrawEngineType draw_engine_workbench_solid_flat = {
	NULL, NULL,
	N_("Workbench"),
	&workbench_data_size,
	&workbench_solid_flat_engine_init,
	&workbench_solid_flat_engine_free,
	&workbench_solid_flat_cache_init,
	&workbench_solid_flat_cache_populate,
	&workbench_solid_flat_cache_finish,
	NULL,
	&workbench_solid_flat_draw_scene,
	NULL,
	NULL,
	NULL,
};
