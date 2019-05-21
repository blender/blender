/*
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
 * Copyright 2016, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 *
 * All specific data handler for Objects, Lights, ViewLayers, ...
 */

#include "DRW_render.h"

#include "eevee_private.h"
#include "eevee_lightcache.h"

void EEVEE_view_layer_data_free(void *storage)
{
  EEVEE_ViewLayerData *sldata = (EEVEE_ViewLayerData *)storage;

  /* Lights */
  MEM_SAFE_FREE(sldata->lights);
  DRW_UBO_FREE_SAFE(sldata->light_ubo);
  DRW_UBO_FREE_SAFE(sldata->shadow_ubo);
  DRW_UBO_FREE_SAFE(sldata->shadow_render_ubo);
  GPU_FRAMEBUFFER_FREE_SAFE(sldata->shadow_cube_target_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(sldata->shadow_cube_store_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(sldata->shadow_cascade_target_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(sldata->shadow_cascade_store_fb);
  DRW_TEXTURE_FREE_SAFE(sldata->shadow_cube_target);
  DRW_TEXTURE_FREE_SAFE(sldata->shadow_cube_blur);
  DRW_TEXTURE_FREE_SAFE(sldata->shadow_cube_pool);
  DRW_TEXTURE_FREE_SAFE(sldata->shadow_cascade_target);
  DRW_TEXTURE_FREE_SAFE(sldata->shadow_cascade_blur);
  DRW_TEXTURE_FREE_SAFE(sldata->shadow_cascade_pool);
  MEM_SAFE_FREE(sldata->shcasters_buffers[0].shadow_casters);
  MEM_SAFE_FREE(sldata->shcasters_buffers[0].flags);
  MEM_SAFE_FREE(sldata->shcasters_buffers[1].shadow_casters);
  MEM_SAFE_FREE(sldata->shcasters_buffers[1].flags);

  if (sldata->fallback_lightcache) {
    EEVEE_lightcache_free(sldata->fallback_lightcache);
    sldata->fallback_lightcache = NULL;
  }

  /* Probes */
  MEM_SAFE_FREE(sldata->probes);
  DRW_UBO_FREE_SAFE(sldata->probe_ubo);
  DRW_UBO_FREE_SAFE(sldata->grid_ubo);
  DRW_UBO_FREE_SAFE(sldata->planar_ubo);
  DRW_UBO_FREE_SAFE(sldata->common_ubo);
}

EEVEE_ViewLayerData *EEVEE_view_layer_data_get(void)
{
  return (EEVEE_ViewLayerData *)DRW_view_layer_engine_data_get(&draw_engine_eevee_type);
}

EEVEE_ViewLayerData *EEVEE_view_layer_data_ensure_ex(struct ViewLayer *view_layer)
{
  EEVEE_ViewLayerData **sldata = (EEVEE_ViewLayerData **)DRW_view_layer_engine_data_ensure_ex(
      view_layer, &draw_engine_eevee_type, &EEVEE_view_layer_data_free);

  if (*sldata == NULL) {
    *sldata = MEM_callocN(sizeof(**sldata), "EEVEE_ViewLayerData");
  }

  return *sldata;
}

EEVEE_ViewLayerData *EEVEE_view_layer_data_ensure(void)
{
  EEVEE_ViewLayerData **sldata = (EEVEE_ViewLayerData **)DRW_view_layer_engine_data_ensure(
      &draw_engine_eevee_type, &EEVEE_view_layer_data_free);

  if (*sldata == NULL) {
    *sldata = MEM_callocN(sizeof(**sldata), "EEVEE_ViewLayerData");
  }

  return *sldata;
}

/* Object data. */

static void eevee_object_data_init(DrawData *dd)
{
  EEVEE_ObjectEngineData *eevee_data = (EEVEE_ObjectEngineData *)dd;
  eevee_data->shadow_caster_id = -1;
}

EEVEE_ObjectEngineData *EEVEE_object_data_get(Object *ob)
{
  if (ELEM(ob->type, OB_LIGHTPROBE, OB_LAMP)) {
    return NULL;
  }
  return (EEVEE_ObjectEngineData *)DRW_drawdata_get(&ob->id, &draw_engine_eevee_type);
}

EEVEE_ObjectEngineData *EEVEE_object_data_ensure(Object *ob)
{
  BLI_assert(!ELEM(ob->type, OB_LIGHTPROBE, OB_LAMP));
  return (EEVEE_ObjectEngineData *)DRW_drawdata_ensure(&ob->id,
                                                       &draw_engine_eevee_type,
                                                       sizeof(EEVEE_ObjectEngineData),
                                                       eevee_object_data_init,
                                                       NULL);
}

/* Light probe data. */

static void eevee_lightprobe_data_init(DrawData *dd)
{
  EEVEE_LightProbeEngineData *ped = (EEVEE_LightProbeEngineData *)dd;
  ped->need_update = false;
}

EEVEE_LightProbeEngineData *EEVEE_lightprobe_data_get(Object *ob)
{
  if (ob->type != OB_LIGHTPROBE) {
    return NULL;
  }
  return (EEVEE_LightProbeEngineData *)DRW_drawdata_get(&ob->id, &draw_engine_eevee_type);
}

EEVEE_LightProbeEngineData *EEVEE_lightprobe_data_ensure(Object *ob)
{
  BLI_assert(ob->type == OB_LIGHTPROBE);
  return (EEVEE_LightProbeEngineData *)DRW_drawdata_ensure(&ob->id,
                                                           &draw_engine_eevee_type,
                                                           sizeof(EEVEE_LightProbeEngineData),
                                                           eevee_lightprobe_data_init,
                                                           NULL);
}

/* Light data. */

static void eevee_light_data_init(DrawData *dd)
{
  EEVEE_LightEngineData *led = (EEVEE_LightEngineData *)dd;
  led->need_update = true;
  led->prev_cube_shadow_id = -1;
}

EEVEE_LightEngineData *EEVEE_light_data_get(Object *ob)
{
  if (ob->type != OB_LAMP) {
    return NULL;
  }
  return (EEVEE_LightEngineData *)DRW_drawdata_get(&ob->id, &draw_engine_eevee_type);
}

EEVEE_LightEngineData *EEVEE_light_data_ensure(Object *ob)
{
  BLI_assert(ob->type == OB_LAMP);
  return (EEVEE_LightEngineData *)DRW_drawdata_ensure(&ob->id,
                                                      &draw_engine_eevee_type,
                                                      sizeof(EEVEE_LightEngineData),
                                                      eevee_light_data_init,
                                                      NULL);
}

/* World data. */

static void eevee_world_data_init(DrawData *dd)
{
  EEVEE_WorldEngineData *wed = (EEVEE_WorldEngineData *)dd;
  wed->dd.recalc |= 1;
}

EEVEE_WorldEngineData *EEVEE_world_data_get(World *wo)
{
  return (EEVEE_WorldEngineData *)DRW_drawdata_get(&wo->id, &draw_engine_eevee_type);
}

EEVEE_WorldEngineData *EEVEE_world_data_ensure(World *wo)
{
  return (EEVEE_WorldEngineData *)DRW_drawdata_ensure(&wo->id,
                                                      &draw_engine_eevee_type,
                                                      sizeof(EEVEE_WorldEngineData),
                                                      eevee_world_data_init,
                                                      NULL);
}
