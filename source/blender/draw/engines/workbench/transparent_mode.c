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

/** \file transparent_mode.c
 *  \ingroup draw_engine
 *
 * Simple engine for drawing color and/or depth.
 * When we only need simple studio shaders.
 */

#include "DRW_render.h"

#include "GPU_shader.h"

#include "workbench_private.h"

/* Functions */

static void workbench_transparent_engine_init(void *vedata)
{
	WORKBENCH_Data *data = vedata;
	workbench_forward_engine_init(data);
}

static void workbench_transparent_cache_init(void *vedata)
{

	WORKBENCH_Data *data = vedata;
	workbench_forward_cache_init(data);
}

static void workbench_transparent_cache_populate(void *vedata, Object *ob)
{
	WORKBENCH_Data *data = vedata;
	workbench_forward_cache_populate(data, ob);
}

static void workbench_transparent_cache_finish(void *vedata)
{
	WORKBENCH_Data *data = vedata;
	workbench_forward_cache_finish(data);
}

static void workbench_transparent_draw_background(void *vedata)
{
	WORKBENCH_Data *data = vedata;
	workbench_forward_draw_background(data);
}

static void workbench_transparent_draw_scene(void *vedata)
{
	WORKBENCH_Data *data = vedata;
	workbench_forward_draw_scene(data);
	workbench_forward_draw_finish(data);
}

static void workbench_transparent_engine_free(void)
{
	workbench_forward_engine_free();
}

static void workbench_transparent_view_update(void *vedata)
{
	WORKBENCH_Data *data = vedata;
	workbench_taa_view_updated(data);
}

static const DrawEngineDataSize workbench_data_size = DRW_VIEWPORT_DATA_SIZE(WORKBENCH_Data);

DrawEngineType draw_engine_workbench_transparent = {
	NULL, NULL,
	N_("Workbench"),
	&workbench_data_size,
	&workbench_transparent_engine_init,
	&workbench_transparent_engine_free,
	&workbench_transparent_cache_init,
	&workbench_transparent_cache_populate,
	&workbench_transparent_cache_finish,
	&workbench_transparent_draw_background,
	&workbench_transparent_draw_scene,
	&workbench_transparent_view_update,
	NULL,
	NULL,
};
