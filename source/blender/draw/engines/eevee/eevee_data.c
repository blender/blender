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

/* All specific data handler for Objects, Lights, SceneLayers, ...
 */

/** \file eevee_data.c
 *  \ingroup draw_engine
 */

#include "DRW_render.h"

#include "eevee_private.h"

static void eevee_scene_layer_data_free(void *storage)
{
	EEVEE_SceneLayerData *sldata = (EEVEE_SceneLayerData *)storage;

	/* Lights */
	MEM_SAFE_FREE(sldata->lamps);
	DRW_UBO_FREE_SAFE(sldata->light_ubo);
	DRW_UBO_FREE_SAFE(sldata->shadow_ubo);
	DRW_UBO_FREE_SAFE(sldata->shadow_render_ubo);
	DRW_FRAMEBUFFER_FREE_SAFE(sldata->shadow_cube_target_fb);
	DRW_FRAMEBUFFER_FREE_SAFE(sldata->shadow_cube_fb);
	DRW_FRAMEBUFFER_FREE_SAFE(sldata->shadow_map_fb);
	DRW_FRAMEBUFFER_FREE_SAFE(sldata->shadow_cascade_fb);
	DRW_TEXTURE_FREE_SAFE(sldata->shadow_depth_cube_target);
	DRW_TEXTURE_FREE_SAFE(sldata->shadow_color_cube_target);
	DRW_TEXTURE_FREE_SAFE(sldata->shadow_depth_cube_pool);
	DRW_TEXTURE_FREE_SAFE(sldata->shadow_depth_map_pool);
	DRW_TEXTURE_FREE_SAFE(sldata->shadow_depth_cascade_pool);
	BLI_freelistN(&sldata->shadow_casters);

	/* Probes */
	MEM_SAFE_FREE(sldata->probes);
	DRW_UBO_FREE_SAFE(sldata->probe_ubo);
	DRW_UBO_FREE_SAFE(sldata->grid_ubo);
	DRW_UBO_FREE_SAFE(sldata->planar_ubo);
	DRW_FRAMEBUFFER_FREE_SAFE(sldata->probe_fb);
	DRW_FRAMEBUFFER_FREE_SAFE(sldata->probe_filter_fb);
	DRW_TEXTURE_FREE_SAFE(sldata->probe_rt);
	DRW_TEXTURE_FREE_SAFE(sldata->probe_pool);
	DRW_TEXTURE_FREE_SAFE(sldata->irradiance_pool);
	DRW_TEXTURE_FREE_SAFE(sldata->irradiance_rt);
}

static void eevee_lamp_data_free(void *storage)
{
	EEVEE_LampEngineData *led = (EEVEE_LampEngineData *)storage;

	MEM_SAFE_FREE(led->storage);
	BLI_freelistN(&led->shadow_caster_list);
}

static void eevee_lightprobe_data_free(void *storage)
{
	EEVEE_LightProbeEngineData *ped = (EEVEE_LightProbeEngineData *)storage;

	BLI_freelistN(&ped->captured_object_list);
}

EEVEE_SceneLayerData *EEVEE_scene_layer_data_get(void)
{
	EEVEE_SceneLayerData **sldata = (EEVEE_SceneLayerData **)DRW_scene_layer_engine_data_get(&draw_engine_eevee_type, &eevee_scene_layer_data_free);

	if (*sldata == NULL) {
		*sldata = MEM_callocN(sizeof(**sldata), "EEVEE_SceneLayerData");
	}

	return *sldata;
}

EEVEE_ObjectEngineData *EEVEE_object_data_get(Object *ob)
{
	EEVEE_ObjectEngineData **oedata = (EEVEE_ObjectEngineData **)DRW_object_engine_data_get(ob, &draw_engine_eevee_type, NULL);

	if (*oedata == NULL) {
		*oedata = MEM_callocN(sizeof(**oedata), "EEVEE_ObjectEngineData");
	}

	return *oedata;
}

EEVEE_LightProbeEngineData *EEVEE_lightprobe_data_get(Object *ob)
{
	EEVEE_LightProbeEngineData **pedata = (EEVEE_LightProbeEngineData **)DRW_object_engine_data_get(ob, &draw_engine_eevee_type, &eevee_lightprobe_data_free);

	if (*pedata == NULL) {
		*pedata = MEM_callocN(sizeof(**pedata), "EEVEE_LightProbeEngineData");
		(*pedata)->need_update = true;
	}

	return *pedata;
}

EEVEE_LampEngineData *EEVEE_lamp_data_get(Object *ob)
{
	EEVEE_LampEngineData **ledata = (EEVEE_LampEngineData **)DRW_object_engine_data_get(ob, &draw_engine_eevee_type, &eevee_lamp_data_free);

	if (*ledata == NULL) {
		*ledata = MEM_callocN(sizeof(**ledata), "EEVEE_LampEngineData");
		(*ledata)->need_update = true;
	}

	return *ledata;
}
