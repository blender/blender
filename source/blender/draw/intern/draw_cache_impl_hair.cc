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
 * The Original Code is Copyright (C) 2017 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup draw
 *
 * \brief Hair API for render engines
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "DNA_hair_types.h"
#include "DNA_object_types.h"

#include "BKE_hair.h"

#include "GPU_batch.h"
#include "GPU_material.h"
#include "GPU_texture.h"

#include "draw_cache_impl.h"   /* own include */
#include "draw_hair_private.h" /* own include */

static void hair_batch_cache_clear(Hair *hair);

/* ---------------------------------------------------------------------- */
/* Hair GPUBatch Cache */

struct HairBatchCache {
  ParticleHairCache hair;

  /* settings to determine if cache is invalid */
  bool is_dirty;
};

/* GPUBatch cache management. */

static bool hair_batch_cache_valid(Hair *hair)
{
  HairBatchCache *cache = static_cast<HairBatchCache *>(hair->batch_cache);
  return (cache && cache->is_dirty == false);
}

static void hair_batch_cache_init(Hair *hair)
{
  HairBatchCache *cache = static_cast<HairBatchCache *>(hair->batch_cache);

  if (!cache) {
    cache = MEM_cnew<HairBatchCache>(__func__);
    hair->batch_cache = cache;
  }
  else {
    memset(cache, 0, sizeof(*cache));
  }

  cache->is_dirty = false;
}

void DRW_hair_batch_cache_validate(Hair *hair)
{
  if (!hair_batch_cache_valid(hair)) {
    hair_batch_cache_clear(hair);
    hair_batch_cache_init(hair);
  }
}

static HairBatchCache *hair_batch_cache_get(Hair *hair)
{
  DRW_hair_batch_cache_validate(hair);
  return static_cast<HairBatchCache *>(hair->batch_cache);
}

void DRW_hair_batch_cache_dirty_tag(Hair *hair, int mode)
{
  HairBatchCache *cache = static_cast<HairBatchCache *>(hair->batch_cache);
  if (cache == nullptr) {
    return;
  }
  switch (mode) {
    case BKE_HAIR_BATCH_DIRTY_ALL:
      cache->is_dirty = true;
      break;
    default:
      BLI_assert(0);
  }
}

static void hair_batch_cache_clear(Hair *hair)
{
  HairBatchCache *cache = static_cast<HairBatchCache *>(hair->batch_cache);
  if (!cache) {
    return;
  }

  particle_batch_cache_clear_hair(&cache->hair);
}

void DRW_hair_batch_cache_free(Hair *hair)
{
  hair_batch_cache_clear(hair);
  MEM_SAFE_FREE(hair->batch_cache);
}

static void ensure_seg_pt_count(Hair *hair, ParticleHairCache *hair_cache)
{
  if ((hair_cache->pos != nullptr && hair_cache->indices != nullptr) ||
      (hair_cache->proc_point_buf != nullptr)) {
    return;
  }

  hair_cache->strands_len = 0;
  hair_cache->elems_len = 0;
  hair_cache->point_len = 0;

  HairCurve *curve = hair->curves;
  int num_curves = hair->totcurve;
  for (int i = 0; i < num_curves; i++, curve++) {
    hair_cache->strands_len++;
    hair_cache->elems_len += curve->numpoints + 1;
    hair_cache->point_len += curve->numpoints;
  }
}

static void hair_batch_cache_fill_segments_proc_pos(Hair *hair,
                                                    GPUVertBufRaw *attr_step,
                                                    GPUVertBufRaw *length_step)
{
  /* TODO: use hair radius layer if available. */
  HairCurve *curve = hair->curves;
  int num_curves = hair->totcurve;
  for (int i = 0; i < num_curves; i++, curve++) {
    float(*curve_co)[3] = hair->co + curve->firstpoint;
    float total_len = 0.0f;
    float *co_prev = nullptr, *seg_data_first;
    for (int j = 0; j < curve->numpoints; j++) {
      float *seg_data = (float *)GPU_vertbuf_raw_step(attr_step);
      copy_v3_v3(seg_data, curve_co[j]);
      if (co_prev) {
        total_len += len_v3v3(co_prev, curve_co[j]);
      }
      else {
        seg_data_first = seg_data;
      }
      seg_data[3] = total_len;
      co_prev = curve_co[j];
    }
    /* Assign length value*/
    *(float *)GPU_vertbuf_raw_step(length_step) = total_len;
    if (total_len > 0.0f) {
      /* Divide by total length to have a [0-1] number. */
      for (int j = 0; j < curve->numpoints; j++, seg_data_first += 4) {
        seg_data_first[3] /= total_len;
      }
    }
  }
}

static void hair_batch_cache_ensure_procedural_pos(Hair *hair,
                                                   ParticleHairCache *cache,
                                                   GPUMaterial *gpu_material)
{
  if (cache->proc_point_buf == nullptr) {
    /* initialize vertex format */
    GPUVertFormat format = {0};
    uint pos_id = GPU_vertformat_attr_add(&format, "posTime", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

    cache->proc_point_buf = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(cache->proc_point_buf, cache->point_len);

    GPUVertBufRaw point_step;
    GPU_vertbuf_attr_get_raw_data(cache->proc_point_buf, pos_id, &point_step);

    GPUVertFormat length_format = {0};
    uint length_id = GPU_vertformat_attr_add(
        &length_format, "hairLength", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);

    cache->proc_length_buf = GPU_vertbuf_create_with_format(&length_format);
    GPU_vertbuf_data_alloc(cache->proc_length_buf, cache->strands_len);

    GPUVertBufRaw length_step;
    GPU_vertbuf_attr_get_raw_data(cache->proc_length_buf, length_id, &length_step);

    hair_batch_cache_fill_segments_proc_pos(hair, &point_step, &length_step);

    /* Create vbo immediately to bind to texture buffer. */
    GPU_vertbuf_use(cache->proc_point_buf);
    cache->point_tex = GPU_texture_create_from_vertbuf("hair_point", cache->proc_point_buf);
  }

  if (gpu_material && cache->proc_length_buf != nullptr && cache->length_tex) {
    ListBase gpu_attrs = GPU_material_attributes(gpu_material);
    LISTBASE_FOREACH (GPUMaterialAttribute *, attr, &gpu_attrs) {
      if (attr->type == CD_HAIRLENGTH) {
        GPU_vertbuf_use(cache->proc_length_buf);
        cache->length_tex = GPU_texture_create_from_vertbuf("hair_length", cache->proc_length_buf);
        break;
      }
    }
  }
}

static void hair_batch_cache_fill_strands_data(Hair *hair,
                                               GPUVertBufRaw *data_step,
                                               GPUVertBufRaw *seg_step)
{
  HairCurve *curve = hair->curves;
  int num_curves = hair->totcurve;
  for (int i = 0; i < num_curves; i++, curve++) {
    *(uint *)GPU_vertbuf_raw_step(data_step) = curve->firstpoint;
    *(ushort *)GPU_vertbuf_raw_step(seg_step) = curve->numpoints - 1;
  }
}

static void hair_batch_cache_ensure_procedural_strand_data(Hair *hair, ParticleHairCache *cache)
{
  GPUVertBufRaw data_step, seg_step;

  GPUVertFormat format_data = {0};
  uint data_id = GPU_vertformat_attr_add(&format_data, "data", GPU_COMP_U32, 1, GPU_FETCH_INT);

  GPUVertFormat format_seg = {0};
  uint seg_id = GPU_vertformat_attr_add(&format_seg, "data", GPU_COMP_U16, 1, GPU_FETCH_INT);

  /* Strand Data */
  cache->proc_strand_buf = GPU_vertbuf_create_with_format(&format_data);
  GPU_vertbuf_data_alloc(cache->proc_strand_buf, cache->strands_len);
  GPU_vertbuf_attr_get_raw_data(cache->proc_strand_buf, data_id, &data_step);

  cache->proc_strand_seg_buf = GPU_vertbuf_create_with_format(&format_seg);
  GPU_vertbuf_data_alloc(cache->proc_strand_seg_buf, cache->strands_len);
  GPU_vertbuf_attr_get_raw_data(cache->proc_strand_seg_buf, seg_id, &seg_step);

  hair_batch_cache_fill_strands_data(hair, &data_step, &seg_step);

  /* Create vbo immediately to bind to texture buffer. */
  GPU_vertbuf_use(cache->proc_strand_buf);
  cache->strand_tex = GPU_texture_create_from_vertbuf("hair_strand", cache->proc_strand_buf);

  GPU_vertbuf_use(cache->proc_strand_seg_buf);
  cache->strand_seg_tex = GPU_texture_create_from_vertbuf("hair_strand_seg",
                                                          cache->proc_strand_seg_buf);
}

static void hair_batch_cache_ensure_procedural_final_points(ParticleHairCache *cache, int subdiv)
{
  /* Same format as point_tex. */
  GPUVertFormat format = {0};
  GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

  cache->final[subdiv].proc_buf = GPU_vertbuf_create_with_format_ex(&format,
                                                                    GPU_USAGE_DEVICE_ONLY);

  /* Create a destination buffer for the transform feedback. Sized appropriately */
  /* Those are points! not line segments. */
  GPU_vertbuf_data_alloc(cache->final[subdiv].proc_buf,
                         cache->final[subdiv].strands_res * cache->strands_len);

  /* Create vbo immediately to bind to texture buffer. */
  GPU_vertbuf_use(cache->final[subdiv].proc_buf);

  cache->final[subdiv].proc_tex = GPU_texture_create_from_vertbuf("hair_proc",
                                                                  cache->final[subdiv].proc_buf);
}

static void hair_batch_cache_fill_segments_indices(Hair *hair,
                                                   const int res,
                                                   GPUIndexBufBuilder *elb)
{
  HairCurve *curve = hair->curves;
  int num_curves = hair->totcurve;
  uint curr_point = 0;
  for (int i = 0; i < num_curves; i++, curve++) {
    for (int k = 0; k < res; k++) {
      GPU_indexbuf_add_generic_vert(elb, curr_point++);
    }
    GPU_indexbuf_add_primitive_restart(elb);
  }
}

static void hair_batch_cache_ensure_procedural_indices(Hair *hair,
                                                       ParticleHairCache *cache,
                                                       int thickness_res,
                                                       int subdiv)
{
  BLI_assert(thickness_res <= MAX_THICKRES); /* Cylinder strip not currently supported. */

  if (cache->final[subdiv].proc_hairs[thickness_res - 1] != nullptr) {
    return;
  }

  int verts_per_hair = cache->final[subdiv].strands_res * thickness_res;
  /* +1 for primitive restart */
  int element_count = (verts_per_hair + 1) * cache->strands_len;
  GPUPrimType prim_type = (thickness_res == 1) ? GPU_PRIM_LINE_STRIP : GPU_PRIM_TRI_STRIP;

  static GPUVertFormat format = {0};
  GPU_vertformat_clear(&format);

  /* initialize vertex format */
  GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_U8, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);

  GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
  GPU_vertbuf_data_alloc(vbo, 1);

  GPUIndexBufBuilder elb;
  GPU_indexbuf_init_ex(&elb, prim_type, element_count, element_count);

  hair_batch_cache_fill_segments_indices(hair, verts_per_hair, &elb);

  cache->final[subdiv].proc_hairs[thickness_res - 1] = GPU_batch_create_ex(
      prim_type, vbo, GPU_indexbuf_build(&elb), GPU_BATCH_OWNS_VBO | GPU_BATCH_OWNS_INDEX);
}

bool hair_ensure_procedural_data(Object *object,
                                 ParticleHairCache **r_hair_cache,
                                 GPUMaterial *gpu_material,
                                 int subdiv,
                                 int thickness_res)
{
  bool need_ft_update = false;
  Hair *hair = static_cast<Hair *>(object->data);

  HairBatchCache *cache = hair_batch_cache_get(hair);
  *r_hair_cache = &cache->hair;

  const int steps = 2; /* TODO: don't hard-code? */
  (*r_hair_cache)->final[subdiv].strands_res = 1 << (steps + subdiv);

  /* Refreshed on combing and simulation. */
  if ((*r_hair_cache)->proc_point_buf == nullptr) {
    ensure_seg_pt_count(hair, &cache->hair);
    hair_batch_cache_ensure_procedural_pos(hair, &cache->hair, gpu_material);
    need_ft_update = true;
  }

  /* Refreshed if active layer or custom data changes. */
  if ((*r_hair_cache)->strand_tex == nullptr) {
    hair_batch_cache_ensure_procedural_strand_data(hair, &cache->hair);
  }

  /* Refreshed only on subdiv count change. */
  if ((*r_hair_cache)->final[subdiv].proc_buf == nullptr) {
    hair_batch_cache_ensure_procedural_final_points(&cache->hair, subdiv);
    need_ft_update = true;
  }
  if ((*r_hair_cache)->final[subdiv].proc_hairs[thickness_res - 1] == nullptr) {
    hair_batch_cache_ensure_procedural_indices(hair, &cache->hair, thickness_res, subdiv);
  }

  return need_ft_update;
}

int DRW_hair_material_count_get(Hair *hair)
{
  return max_ii(1, hair->totcol);
}
