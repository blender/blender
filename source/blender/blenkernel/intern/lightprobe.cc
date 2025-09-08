/* SPDX-FileCopyrightText: Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstring>

#include "DNA_collection_types.h"
#include "DNA_defaults.h"
#include "DNA_lightprobe_types.h"
#include "DNA_object_types.h"

#include "BLI_math_base.h"
#include "BLI_utildefines.h"

#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_lightprobe.h"

#include "BLT_translation.hh"

#include "BLO_read_write.hh"

static void lightprobe_init_data(ID *id)
{
  LightProbe *probe = (LightProbe *)id;
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(probe, id));

  MEMCPY_STRUCT_AFTER(probe, DNA_struct_default_get(LightProbe), id);
}

static void lightprobe_foreach_id(ID *id, LibraryForeachIDData *data)
{
  LightProbe *probe = (LightProbe *)id;

  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, probe->visibility_grp, IDWALK_CB_NOP);
}

static void lightprobe_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  LightProbe *prb = (LightProbe *)id;

  /* write LibData */
  BLO_write_id_struct(writer, LightProbe, id_address, &prb->id);
  BKE_id_blend_write(writer, &prb->id);
}

IDTypeInfo IDType_ID_LP = {
    /*id_code*/ LightProbe::id_type,
    /*id_filter*/ FILTER_ID_LP,
    /*dependencies_id_types*/ FILTER_ID_IM,
    /*main_listbase_index*/ INDEX_ID_LP,
    /*struct_size*/ sizeof(LightProbe),
    /*name*/ "LightProbe",
    /*name_plural*/ N_("lightprobes"),
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
    /*foreach_working_space_color*/ nullptr,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ lightprobe_blend_write,
    /*blend_read_data*/ nullptr,
    /*blend_read_after_liblink*/ nullptr,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

void BKE_lightprobe_type_set(LightProbe *probe, const short lightprobe_type)
{
  probe->type = lightprobe_type;

  switch (probe->type) {
    case LIGHTPROBE_TYPE_VOLUME:
      probe->distinf = 0.3f;
      probe->falloff = 1.0f;
      probe->clipsta = 0.01f;
      break;
    case LIGHTPROBE_TYPE_PLANE:
      probe->distinf = 0.1f;
      probe->falloff = 0.5f;
      probe->clipsta = 0.001f;
      break;
    case LIGHTPROBE_TYPE_SPHERE:
      probe->attenuation_type = LIGHTPROBE_SHAPE_ELIPSOID;
      break;
    default:
      BLI_assert_msg(0, "LightProbe type not configured.");
      break;
  }
}

LightProbe *BKE_lightprobe_add(Main *bmain, const char *name)
{
  LightProbe *probe;

  probe = BKE_id_new<LightProbe>(bmain, name);

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

  BLO_write_int8_array(writer, sample_count, (int8_t *)cache->connectivity.validity);
}

static void lightprobe_grid_cache_frame_blend_read(BlendDataReader *reader,
                                                   LightProbeGridCacheFrame *cache)
{
  if (!ELEM(
          cache->data_layout, LIGHTPROBE_CACHE_ADAPTIVE_RESOLUTION, LIGHTPROBE_CACHE_UNIFORM_GRID))
  {
    /* Do not try to read data from incompatible layout. Clear all pointers. */
    *cache = LightProbeGridCacheFrame{};
    return;
  }

  BLO_read_struct_array(reader, LightProbeGridCacheFrame, cache->block_len, &cache->block_infos);

  int64_t sample_count = BKE_lightprobe_grid_cache_frame_sample_count(cache);

  /* Baking data is not stored. */
  cache->baking.L0 = nullptr;
  cache->baking.L1_a = nullptr;
  cache->baking.L1_b = nullptr;
  cache->baking.L1_c = nullptr;
  cache->baking.virtual_offset = nullptr;
  cache->baking.validity = nullptr;
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

  BLO_read_int8_array(reader, sample_count, (int8_t **)&cache->connectivity.validity);
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
    BLO_read_struct(reader, LightProbeGridCacheFrame, &cache->grid_static_cache);
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

template<typename DataT, typename T> static void spherical_harmonic_copy(T &dst, T &src)
{
  dst.L0 = (DataT *)MEM_dupallocN(src.L0);
  dst.L1_a = (DataT *)MEM_dupallocN(src.L1_a);
  dst.L1_b = (DataT *)MEM_dupallocN(src.L1_b);
  dst.L1_c = (DataT *)MEM_dupallocN(src.L1_c);
}

LightProbeGridCacheFrame *BKE_lightprobe_grid_cache_frame_create()
{
  LightProbeGridCacheFrame *cache = MEM_callocN<LightProbeGridCacheFrame>(
      "LightProbeGridCacheFrame");
  return cache;
}

LightProbeGridCacheFrame *BKE_lightprobe_grid_cache_frame_copy(LightProbeGridCacheFrame *src)
{
  LightProbeGridCacheFrame *dst = static_cast<LightProbeGridCacheFrame *>(MEM_dupallocN(src));
  dst->block_infos = static_cast<LightProbeBlockData *>(MEM_dupallocN(src->block_infos));
  spherical_harmonic_copy<float[3]>(dst->irradiance, src->irradiance);
  spherical_harmonic_copy<float>(dst->visibility, src->visibility);
  dst->connectivity.validity = static_cast<uint8_t *>(MEM_dupallocN(src->connectivity.validity));
  /* NOTE: Don't copy baking since it wouldn't be freed nor updated after completion. */
  dst->baking.L0 = nullptr;
  dst->baking.L1_a = nullptr;
  dst->baking.L1_b = nullptr;
  dst->baking.L1_c = nullptr;
  dst->baking.virtual_offset = nullptr;
  dst->baking.validity = nullptr;
  dst->surfels = nullptr;
  return dst;
}

void BKE_lightprobe_grid_cache_frame_free(LightProbeGridCacheFrame *cache)
{
  MEM_SAFE_FREE(cache->block_infos);
  spherical_harmonic_free(cache->baking);
  spherical_harmonic_free(cache->irradiance);
  spherical_harmonic_free(cache->visibility);
  MEM_SAFE_FREE(cache->baking.validity);
  MEM_SAFE_FREE(cache->connectivity.validity);
  MEM_SAFE_FREE(cache->surfels);
  MEM_SAFE_FREE(cache->baking.virtual_offset);

  MEM_SAFE_FREE(cache);
}

void BKE_lightprobe_cache_create(Object *object)
{
  BLI_assert(object->lightprobe_cache == nullptr);

  object->lightprobe_cache = MEM_callocN<LightProbeObjectCache>("LightProbeObjectCache");
}

LightProbeObjectCache *BKE_lightprobe_cache_copy(LightProbeObjectCache *src_cache)
{
  BLI_assert(src_cache != nullptr);

  LightProbeObjectCache *dst_cache = static_cast<LightProbeObjectCache *>(
      MEM_dupallocN(src_cache));

  if (src_cache->grid_static_cache) {
    dst_cache->grid_static_cache = BKE_lightprobe_grid_cache_frame_copy(
        src_cache->grid_static_cache);
  }
  return dst_cache;
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
  return int64_t(cache->size[0]) * cache->size[1] * cache->size[2];
}
