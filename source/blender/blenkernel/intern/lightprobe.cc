/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright Blender Foundation */

/** \file
 * \ingroup bke
 */

#include <string.h>

#include "DNA_collection_types.h"
#include "DNA_defaults.h"
#include "DNA_lightprobe_types.h"
#include "DNA_object_types.h"

#include "BLI_math_base.h"
#include "BLI_span.hh"
#include "BLI_utildefines.h"

#include "BKE_anim_data.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_lightprobe.h"
#include "BKE_main.h"

#include "BLT_translation.h"

#include "BLO_read_write.h"

static void lightprobe_init_data(ID *id)
{
  LightProbe *probe = (LightProbe *)id;
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(probe, id));

  MEMCPY_STRUCT_AFTER(probe, DNA_struct_default_get(LightProbe), id);
}

static void lightprobe_foreach_id(ID *id, LibraryForeachIDData *data)
{
  LightProbe *probe = (LightProbe *)id;

  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, probe->image, IDWALK_CB_USER);
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, probe->visibility_grp, IDWALK_CB_NOP);
}

static void lightprobe_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  LightProbe *prb = (LightProbe *)id;

  /* write LibData */
  BLO_write_id_struct(writer, LightProbe, id_address, &prb->id);
  BKE_id_blend_write(writer, &prb->id);

  if (prb->adt) {
    BKE_animdata_blend_write(writer, prb->adt);
  }
}

static void lightprobe_blend_read_data(BlendDataReader *reader, ID *id)
{
  LightProbe *prb = (LightProbe *)id;
  BLO_read_data_address(reader, &prb->adt);
  BKE_animdata_blend_read_data(reader, prb->adt);
}

static void lightprobe_blend_read_lib(BlendLibReader *reader, ID *id)
{
  LightProbe *prb = (LightProbe *)id;
  BLO_read_id_address(reader, &prb->id, &prb->visibility_grp);
}

IDTypeInfo IDType_ID_LP = {
    /*id_code*/ ID_LP,
    /*id_filter*/ FILTER_ID_LP,
    /*main_listbase_index*/ INDEX_ID_LP,
    /*struct_size*/ sizeof(LightProbe),
    /*name*/ "LightProbe",
    /*name_plural*/ "lightprobes",
    /*translation_context*/ BLT_I18NCONTEXT_ID_LIGHTPROBE,
    /*flags*/ IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    /*asset_type_info*/ nullptr,

    /*init_data*/ lightprobe_init_data,
    /*copy_data*/ nullptr,
    /*free_data*/ nullptr,
    /*make_local*/ nullptr,
    /*foreach_id*/ lightprobe_foreach_id,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ nullptr,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ lightprobe_blend_write,
    /*blend_read_data*/ lightprobe_blend_read_data,
    /*blend_read_lib*/ lightprobe_blend_read_lib,
    /*blend_read_expand*/ nullptr,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

void BKE_lightprobe_type_set(LightProbe *probe, const short lightprobe_type)
{
  probe->type = lightprobe_type;

  switch (probe->type) {
    case LIGHTPROBE_TYPE_GRID:
      probe->distinf = 0.3f;
      probe->falloff = 1.0f;
      probe->clipsta = 0.01f;
      break;
    case LIGHTPROBE_TYPE_PLANAR:
      probe->distinf = 0.1f;
      probe->falloff = 0.5f;
      probe->clipsta = 0.001f;
      break;
    case LIGHTPROBE_TYPE_CUBE:
      probe->attenuation_type = LIGHTPROBE_SHAPE_ELIPSOID;
      break;
    default:
      BLI_assert_msg(0, "LightProbe type not configured.");
      break;
  }
}

void *BKE_lightprobe_add(Main *bmain, const char *name)
{
  LightProbe *probe;

  probe = static_cast<LightProbe *>(BKE_id_new(bmain, ID_LP, name));

  return probe;
}

static void lightprobe_grid_cache_frame_blend_write(BlendWriter *writer,
                                                    const LightProbeGridCacheFrame *cache)
{
  BLO_write_struct_array(writer, LightProbeGridCacheFrame, cache->block_len, cache->block_infos);

  int64_t sample_count = BKE_lightprobe_grid_cache_frame_sample_count(cache);

  BLO_write_float3_array(writer, sample_count, (float *)cache->irradiance.L0);
  BLO_write_float3_array(writer, sample_count, (float *)cache->irradiance.L1_a);
  BLO_write_float3_array(writer, sample_count, (float *)cache->irradiance.L1_b);
  BLO_write_float3_array(writer, sample_count, (float *)cache->irradiance.L1_c);

  BLO_write_float_array(writer, sample_count, cache->visibility.L0);
  BLO_write_float_array(writer, sample_count, cache->visibility.L1_a);
  BLO_write_float_array(writer, sample_count, cache->visibility.L1_b);
  BLO_write_float_array(writer, sample_count, cache->visibility.L1_c);

  BLO_write_struct_array(
      writer, LightProbeGridCacheFrame, sample_count, cache->connectivity.bitmask);
}

static void lightprobe_grid_cache_frame_blend_read(BlendDataReader *reader,
                                                   LightProbeGridCacheFrame *cache)
{
  if (!ELEM(
          cache->data_layout, LIGHTPROBE_CACHE_ADAPTIVE_RESOLUTION, LIGHTPROBE_CACHE_UNIFORM_GRID))
  {
    /* Do not try to read data from incompatible layout. Clear all pointers. */
    memset(cache, 0, sizeof(*cache));
    return;
  }

  BLO_read_data_address(reader, &cache->block_infos);

  int64_t sample_count = BKE_lightprobe_grid_cache_frame_sample_count(cache);

  /* Baking data is not stored. */
  cache->baking.L0 = nullptr;
  cache->baking.L1_a = nullptr;
  cache->baking.L1_b = nullptr;
  cache->baking.L1_c = nullptr;
  cache->surfels = nullptr;
  cache->surfels_len = 0;

  BLO_read_float3_array(reader, sample_count, (float **)&cache->irradiance.L0);
  BLO_read_float3_array(reader, sample_count, (float **)&cache->irradiance.L1_a);
  BLO_read_float3_array(reader, sample_count, (float **)&cache->irradiance.L1_b);
  BLO_read_float3_array(reader, sample_count, (float **)&cache->irradiance.L1_c);

  BLO_read_float_array(reader, sample_count, &cache->visibility.L0);
  BLO_read_float_array(reader, sample_count, &cache->visibility.L1_a);
  BLO_read_float_array(reader, sample_count, &cache->visibility.L1_b);
  BLO_read_float_array(reader, sample_count, &cache->visibility.L1_c);

  BLO_read_data_address(reader, &cache->connectivity.bitmask);
}

void BKE_lightprobe_cache_blend_write(BlendWriter *writer, LightProbeObjectCache *cache)
{
  if (cache->grid_static_cache != nullptr) {
    BLO_write_struct(writer, LightProbeGridCacheFrame, cache->grid_static_cache);
    lightprobe_grid_cache_frame_blend_write(writer, cache->grid_static_cache);
  }
}

void BKE_lightprobe_cache_blend_read(BlendDataReader *reader, LightProbeObjectCache *cache)
{
  if (cache->grid_static_cache != nullptr) {
    BLO_read_data_address(reader, &cache->grid_static_cache);
    lightprobe_grid_cache_frame_blend_read(reader, cache->grid_static_cache);
  }
}

template<typename T> static void spherical_harmonic_free(T &data)
{
  MEM_SAFE_FREE(data.L0);
  MEM_SAFE_FREE(data.L1_a);
  MEM_SAFE_FREE(data.L1_b);
  MEM_SAFE_FREE(data.L1_c);
}

LightProbeGridCacheFrame *BKE_lightprobe_grid_cache_frame_create()
{
  LightProbeGridCacheFrame *cache = static_cast<LightProbeGridCacheFrame *>(
      MEM_callocN(sizeof(LightProbeGridCacheFrame), "LightProbeGridCacheFrame"));
  return cache;
}

void BKE_lightprobe_grid_cache_frame_free(LightProbeGridCacheFrame *cache)
{
  MEM_SAFE_FREE(cache->block_infos);
  spherical_harmonic_free(cache->baking);
  spherical_harmonic_free(cache->irradiance);
  spherical_harmonic_free(cache->visibility);
  MEM_SAFE_FREE(cache->connectivity.bitmask);
  MEM_SAFE_FREE(cache->surfels);

  MEM_SAFE_FREE(cache);
}

void BKE_lightprobe_cache_create(Object *object)
{
  BLI_assert(object->lightprobe_cache == nullptr);

  object->lightprobe_cache = static_cast<LightProbeObjectCache *>(
      MEM_callocN(sizeof(LightProbeObjectCache), "LightProbeObjectCache"));
}

void BKE_lightprobe_cache_free(Object *object)
{
  if (object->lightprobe_cache == nullptr) {
    return;
  }

  LightProbeObjectCache *cache = object->lightprobe_cache;

  if (cache->shared == false) {
    if (cache->grid_static_cache != nullptr) {
      BKE_lightprobe_grid_cache_frame_free(cache->grid_static_cache);
    }
  }

  MEM_SAFE_FREE(object->lightprobe_cache);
}

int64_t BKE_lightprobe_grid_cache_frame_sample_count(const LightProbeGridCacheFrame *cache)
{
  if (cache->data_layout == LIGHTPROBE_CACHE_ADAPTIVE_RESOLUTION) {
    return cache->block_len * cube_i(cache->block_size);
  }
  /* LIGHTPROBE_CACHE_UNIFORM_GRID */
  return cache->size[0] * cache->size[1] * cache->size[2];
}
