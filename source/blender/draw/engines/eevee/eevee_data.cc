/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * All specific data handler for Objects, Lights, ViewLayers, ...
 */

#include "DRW_render.hh"

#include "BLI_ghash.h"
#include "BLI_memblock.h"

#include "BKE_duplilist.h"
#include "BKE_modifier.hh"
#include "BKE_object.hh"

#include "DEG_depsgraph_query.hh"

#include "GPU_vertex_buffer.h"

#include "eevee_lightcache.h"
#include "eevee_private.h"

/* Motion Blur data. */

static void eevee_motion_blur_mesh_data_free(void *val)
{
  EEVEE_ObjectMotionData *mb_data = (EEVEE_ObjectMotionData *)val;
  if (mb_data->hair_data != nullptr) {
    MEM_freeN(mb_data->hair_data);
  }
  if (mb_data->geometry_data != nullptr) {
    MEM_freeN(mb_data->geometry_data);
  }
  MEM_freeN(val);
}

static uint eevee_object_key_hash(const void *key)
{
  EEVEE_ObjectKey *ob_key = (EEVEE_ObjectKey *)key;
  uint hash = BLI_ghashutil_ptrhash(ob_key->ob);
  hash = BLI_ghashutil_combine_hash(hash, BLI_ghashutil_ptrhash(ob_key->parent));
  for (int i = 0; i < MAX_DUPLI_RECUR; i++) {
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

void EEVEE_motion_hair_step_free(EEVEE_HairMotionStepData *step_data)
{
  GPU_vertbuf_discard(step_data->hair_pos);
  DRW_texture_free(step_data->hair_pos_tx);
  MEM_freeN(step_data);
}

void EEVEE_motion_blur_data_init(EEVEE_MotionBlurData *mb)
{
  if (mb->object == nullptr) {
    mb->object = BLI_ghash_new(eevee_object_key_hash, eevee_object_key_cmp, "EEVEE Object Motion");
  }
  for (int i = 0; i < 2; i++) {
    if (mb->position_vbo_cache[i] == nullptr) {
      mb->position_vbo_cache[i] = BLI_ghash_new(
          BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "EEVEE duplicate vbo cache");
    }
    if (mb->hair_motion_step_cache[i] == nullptr) {
      mb->hair_motion_step_cache[i] = BLI_ghash_new(
          BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "EEVEE hair motion step cache");
    }
  }
}

void EEVEE_motion_blur_data_free(EEVEE_MotionBlurData *mb)
{
  if (mb->object) {
    BLI_ghash_free(mb->object, MEM_freeN, eevee_motion_blur_mesh_data_free);
    mb->object = nullptr;
  }
  for (int i = 0; i < 2; i++) {
    if (mb->position_vbo_cache[i]) {
      BLI_ghash_free(mb->position_vbo_cache[i], nullptr, (GHashValFreeFP)GPU_vertbuf_discard);
      mb->position_vbo_cache[i] = nullptr;
    }
    if (mb->hair_motion_step_cache[i]) {
      BLI_ghash_free(
          mb->hair_motion_step_cache[i], nullptr, (GHashValFreeFP)EEVEE_motion_hair_step_free);
      mb->hair_motion_step_cache[i] = nullptr;
    }
  }
}

EEVEE_ObjectMotionData *EEVEE_motion_blur_object_data_get(EEVEE_MotionBlurData *mb,
                                                          Object *ob,
                                                          bool is_psys)
{
  if (mb->object == nullptr) {
    return nullptr;
  }

  EEVEE_ObjectKey key, *key_p;
  /* Assumes that all instances have the same object pointer. This is currently the case because
   * instance objects are temporary objects on the stack. */
  /* WORKAROUND: Duplicate object key for particle system (hairs) to be able to store dupli offset
   * matrix along with the emitter obmat. (see #97380) */
  key.ob = (void *)((char *)ob + is_psys);
  DupliObject *dup = DRW_object_get_dupli(ob);
  if (dup) {
    key.parent = DRW_object_get_dupli_parent(ob);
    memcpy(key.id, dup->persistent_id, sizeof(key.id));
  }
  else {
    key.parent = ob;
    memset(key.id, 0, sizeof(key.id));
  }

  EEVEE_ObjectMotionData *ob_step = static_cast<EEVEE_ObjectMotionData *>(
      BLI_ghash_lookup(mb->object, &key));
  if (ob_step == nullptr) {
    key_p = static_cast<EEVEE_ObjectKey *>(MEM_mallocN(sizeof(*key_p), __func__));
    memcpy(key_p, &key, sizeof(*key_p));

    ob_step = static_cast<EEVEE_ObjectMotionData *>(
        MEM_callocN(sizeof(EEVEE_ObjectMotionData), __func__));

    BLI_ghash_insert(mb->object, key_p, ob_step);
  }
  return ob_step;
}

EEVEE_GeometryMotionData *EEVEE_motion_blur_geometry_data_get(EEVEE_ObjectMotionData *mb_data)
{
  if (mb_data->geometry_data == nullptr) {
    EEVEE_GeometryMotionData *geom_step = static_cast<EEVEE_GeometryMotionData *>(
        MEM_callocN(sizeof(EEVEE_GeometryMotionData), __func__));
    geom_step->type = EEVEE_MOTION_DATA_MESH;
    mb_data->geometry_data = geom_step;
  }
  return mb_data->geometry_data;
}

EEVEE_HairMotionData *EEVEE_motion_blur_hair_data_get(EEVEE_ObjectMotionData *mb_data, Object *ob)
{
  if (mb_data->hair_data == nullptr) {
    /* Ugly, we allocate for each modifiers and just fill based on modifier index in the list. */
    int psys_len = BLI_listbase_count(&ob->modifiers);
    EEVEE_HairMotionData *hair_step = static_cast<EEVEE_HairMotionData *>(MEM_callocN(
        sizeof(EEVEE_HairMotionData) + sizeof(hair_step->psys[0]) * psys_len, __func__));
    hair_step->psys_len = psys_len;
    hair_step->type = EEVEE_MOTION_DATA_HAIR;
    mb_data->hair_data = hair_step;
  }
  return mb_data->hair_data;
}

EEVEE_HairMotionData *EEVEE_motion_blur_curves_data_get(EEVEE_ObjectMotionData *mb_data)
{
  if (mb_data->hair_data == nullptr) {
    EEVEE_HairMotionData *hair_step = static_cast<EEVEE_HairMotionData *>(
        MEM_callocN(sizeof(EEVEE_HairMotionData) + sizeof(hair_step->psys[0]), __func__));
    hair_step->psys_len = 1;
    hair_step->type = EEVEE_MOTION_DATA_HAIR;
    mb_data->hair_data = hair_step;
  }
  return mb_data->hair_data;
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
    sldata->fallback_lightcache = nullptr;
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
  DRW_UBO_FREE_SAFE(sldata->renderpass_ubo.environment);
  for (int aov_index = 0; aov_index < MAX_AOVS; aov_index++) {
    DRW_UBO_FREE_SAFE(sldata->renderpass_ubo.aovs[aov_index]);
  }

  if (sldata->material_cache) {
    BLI_memblock_destroy(sldata->material_cache, nullptr);
    sldata->material_cache = nullptr;
  }
}

EEVEE_ViewLayerData *EEVEE_view_layer_data_get()
{
  return (EEVEE_ViewLayerData *)DRW_view_layer_engine_data_get(&draw_engine_eevee_type);
}

static void eevee_view_layer_init(EEVEE_ViewLayerData *sldata)
{
  sldata->common_ubo = GPU_uniformbuf_create(sizeof(sldata->common_data));
}

EEVEE_ViewLayerData *EEVEE_view_layer_data_ensure_ex(ViewLayer *view_layer)
{
  EEVEE_ViewLayerData **sldata = (EEVEE_ViewLayerData **)DRW_view_layer_engine_data_ensure_ex(
      view_layer, &draw_engine_eevee_type, &EEVEE_view_layer_data_free);

  if (*sldata == nullptr) {
    *sldata = static_cast<EEVEE_ViewLayerData *>(
        MEM_callocN(sizeof(**sldata), "EEVEE_ViewLayerData"));
    eevee_view_layer_init(*sldata);
  }

  return *sldata;
}

EEVEE_ViewLayerData *EEVEE_view_layer_data_ensure()
{
  EEVEE_ViewLayerData **sldata = (EEVEE_ViewLayerData **)DRW_view_layer_engine_data_ensure(
      &draw_engine_eevee_type, &EEVEE_view_layer_data_free);

  if (*sldata == nullptr) {
    *sldata = static_cast<EEVEE_ViewLayerData *>(
        MEM_callocN(sizeof(**sldata), "EEVEE_ViewLayerData"));
    eevee_view_layer_init(*sldata);
  }

  return *sldata;
}

/* Object data. */

static void eevee_object_data_init(DrawData *dd)
{
  EEVEE_ObjectEngineData *eevee_data = (EEVEE_ObjectEngineData *)dd;
  eevee_data->shadow_caster_id = -1;
  eevee_data->need_update = false;
  eevee_data->geom_update = false;
}

EEVEE_ObjectEngineData *EEVEE_object_data_get(Object *ob)
{
  if (ELEM(ob->type, OB_LIGHTPROBE, OB_LAMP)) {
    return nullptr;
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
                                                       nullptr);
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
    return nullptr;
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
                                                           nullptr);
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
    return nullptr;
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
                                                      nullptr);
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
                                                      nullptr);
}
