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

/** \file solid_mode.c
 *  \ingroup draw_engine
 *
 * Simple engine for drawing color and/or depth.
 * When we only need simple studio shaders.
 */

#include "DRW_render.h"

#include "GPU_shader.h"

#include "RE_pipeline.h"

#include "workbench_private.h"

/* Functions */

static void workbench_solid_engine_init(void *vedata)
{
	WORKBENCH_Data *data = vedata;
	workbench_deferred_engine_init(data);
}

static void workbench_solid_cache_init(void *vedata)
{

	WORKBENCH_Data *data = vedata;
	workbench_deferred_cache_init(data);
}

static void workbench_solid_cache_populate(void *vedata, Object *ob)
{
	WORKBENCH_Data *data = vedata;
	workbench_deferred_solid_cache_populate(data, ob);
}

static void workbench_solid_cache_finish(void *vedata)
{
	WORKBENCH_Data *data = vedata;
	workbench_deferred_cache_finish(data);
}

static void workbench_solid_draw_background(void *vedata)
{
	WORKBENCH_Data *data = vedata;
	workbench_deferred_draw_background(data);
}

static void workbench_solid_draw_scene(void *vedata)
{
	WORKBENCH_Data *data = vedata;
	workbench_deferred_draw_scene(data);
	workbench_deferred_draw_finish(data);
}

static void workbench_solid_engine_free(void)
{
	workbench_deferred_engine_free();
}

static void workbench_solid_view_update(void *vedata)
{
	WORKBENCH_Data *data = vedata;
	workbench_taa_view_updated(data);
}

static void workbench_solid_id_update(void *UNUSED(vedata), struct ID *id)
{
	if (GS(id->name) == ID_OB) {
		WORKBENCH_ObjectData *oed = (WORKBENCH_ObjectData *)DRW_drawdata_get(id, &draw_engine_workbench_solid);
		if (oed != NULL && oed->dd.recalc != 0) {
			oed->shadow_bbox_dirty = (oed->dd.recalc & ID_RECALC_ALL) != 0;
			oed->dd.recalc = 0;
		}
	}
}

static void workbench_render_to_image(void *vedata, RenderEngine *engine, RenderLayer *render_layer, const rcti *rect)
{
	workbench_render(vedata, engine, render_layer, rect);
}

static const DrawEngineDataSize workbench_data_size = DRW_VIEWPORT_DATA_SIZE(WORKBENCH_Data);

DrawEngineType draw_engine_workbench_solid = {
	NULL, NULL,
	N_("Workbench"),
	&workbench_data_size,
	&workbench_solid_engine_init,
	&workbench_solid_engine_free,
	&workbench_solid_cache_init,
	&workbench_solid_cache_populate,
	&workbench_solid_cache_finish,
	&workbench_solid_draw_background,
	&workbench_solid_draw_scene,
	&workbench_solid_view_update,
	&workbench_solid_id_update,
	&workbench_render_to_image,
};
