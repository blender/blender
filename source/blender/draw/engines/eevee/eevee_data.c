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

#include "BLI_ghash.h"
#include "BLI_memblock.h"

#include "BKE_duplilist.h"

#include "DEG_depsgraph_query.h"

#include "GPU_vertex_buffer.h"

#include "eevee_lightcache.h"
#include "eevee_private.h"

/* Motion Blur data. */

static void eevee_motion_blur_mesh_data_free(void *val)
{
  EEVEE_GeometryMotionData *geom_mb = (EEVEE_GeometryMotionData *)val;
  for (int i = 0; i < ARRAY_SIZE(geom_mb->vbo); i++) {
    GPU_VERTBUF_DISCARD_SAFE(geom_mb->vbo[i]);
  }
  MEM_freeN(val);
}

static uint eevee_object_key_hash(const void *key)
{
  EEVEE_ObjectKey *ob_key = (EEVEE_ObjectKey *)key;
  uint hash = BLI_ghashutil_ptrhash(ob_key->ob);
  hash = BLI_ghashutil_combine_hash(hash, BLI_ghashutil_ptrhash(ob_key->parent));
  for (int i = 0; i < 16; i++) {
    if (ob_key->id[i] != 0) {
      hash = BLI_ghashutil_combine_hash(hash, BLI_ghashutil_inthash(ob_key->id[i]));
    }
    else {
      break;
    }
  }
  return hash;
}

/* Return false if equal. */
static bool eevee_object_key_cmp(const void *a, const void *b)
{
  EEVEE_ObjectKey *key_a = (EEVEE_ObjectKey *)a;
  EEVEE_ObjectKey *key_b = (EEVEE_ObjectKey *)b;

  if (key_a->ob != key_b->ob) {
    return true;
  }
  if (key_a->parent != key_b->parent) {
    return true;
  }
  if (memcmp(key_a->id, key_b->id, sizeof(key_a->id)) != 0) {
    return true;
  }
  return false;
}

void EEVEE_motion_blur_data_init(EEVEE_MotionBlurData *mb)
{
  if (mb->object == NULL) {
    mb->object = BLI_ghash_new(eevee_object_key_hash, eevee_object_key_cmp, "EEVEE Object Motion");
  }
  if (mb->geom == NULL) {
    mb->geom = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "EEVEE Mesh Motion");
  }
}

void EEVEE_motion_blur_data_free(EEVEE_MotionBlurData *mb)
{
  if (mb->object) {
    BLI_ghash_free(mb->object, MEM_freeN, MEM_freeN);
    mb->object = NULL;
  }
  if (mb->geom) {
    BLI_ghash_free(mb->geom, NULL, eevee_motion_blur_mesh_data_free);
    mb->geom = NULL;
  }
}

EEVEE_ObjectMotionData *EEVEE_motion_blur_object_data_get(EEVEE_MotionBlurData *mb, Object *ob)
{
  if (mb->object == NULL) {
    return NULL;
  }

  EEVEE_ObjectKey key, *key_p;
  key.ob = ob;
  DupliObject *dup = DRW_object_get_dupli(ob);
  if (dup) {
    key.parent = DRW_object_get_dupli_parent(ob);
    memcpy(key.id, dup->persistent_id, sizeof(key.id));
  }
  else {
    key.parent = key.ob;
    memset(key.id, 0, sizeof(key.id));
  }

  EEVEE_ObjectMotionData *ob_step = BLI_ghash_lookup(mb->object, &key);
  if (ob_step == NULL) {
    key_p = MEM_mallocN(sizeof(*key_p), __func__);
    memcpy(key_p, &key, sizeof(*key_p));

    ob_step = MEM_callocN(sizeof(EEVEE_ObjectMotionData), __func__);

    BLI_ghash_insert(mb->object, key_p, ob_step);
  }
  return ob_step;
}

EEVEE_GeometryMotionData *EEVEE_motion_blur_geometry_data_get(EEVEE_MotionBlurData *mb, Object *ob)
{
  if (mb->geom == NULL) {
    return NULL;
  }

  /* Use original data as key to ensure matching accross update. */
  Object *ob_orig = DEG_get_original_object(ob);

  EEVEE_GeometryMotionData *geom_step = BLI_ghash_lookup(mb->geom, ob_orig->data);
  if (geom_step == NULL) {
    geom_step = MEM_callocN(sizeof(EEVEE_GeometryMotionData), __func__);
    BLI_ghash_insert(mb->geom, ob_orig->data, geom_step);
  }

  return geom_step;
}

/* View Layer data. */

void EEVEE_view_layer_data_free(void *storage)
{
  EEVEE_ViewLayerData *sldata = (EEVEE_ViewLayerData *)storage;

  /* Lights */
  MEM_SAFE_FREE(sldata->lights);
  DRW_UBO_FREE_SAFE(sldata->light_ubo);
  DRW_UBO_FREE_SAFE(sldata->shadow_ubo);
  GPU_FRAMEBUFFER_FREE_SAFE(sldata->shadow_fb);
  DRW_TEXTURE_FREE_SAFE(sldata->shadow_cube_pool);
  DRW_TEXTURE_FREE_SAFE(sldata->shadow_cascade_pool);
  for (int i = 0; i < 2; i++) {
    MEM_SAFE_FREE(sldata->shcasters_buffers[i].bbox);
    MEM_SAFE_FREE(sldata->shcasters_buffers[i].update);
  }

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

  DRW_UBO_FREE_SAFE(sldata->renderpass_ubo.combined);
  DRW_UBO_FREE_SAFE(sldata->renderpass_ubo.diff_color);
  DRW_UBO_FREE_SAFE(sldata->renderpass_ubo.diff_light);
  DRW_UBO_FREE_SAFE(sldata->renderpass_ubo.spec_color);
  DRW_UBO_FREE_SAFE(sldata->renderpass_ubo.spec_light);
  DRW_UBO_FREE_SAFE(sldata->renderpass_ubo.emit);

  if (sldata->material_cache) {
    BLI_memblock_destroy(sldata->material_cache, NULL);
    sldata->material_cache = NULL;
  }
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
