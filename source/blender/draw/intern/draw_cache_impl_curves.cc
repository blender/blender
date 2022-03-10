/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2017 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup draw
 *
 * \brief Hair API for render engines
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_math_vec_types.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_span.hh"
#include "BLI_utildefines.h"

#include "DNA_curves_types.h"
#include "DNA_object_types.h"

#include "BKE_curves.hh"

#include "GPU_batch.h"
#include "GPU_material.h"
#include "GPU_texture.h"

#include "draw_cache_impl.h"   /* own include */
#include "draw_hair_private.h" /* own include */

using blender::float3;
using blender::IndexRange;
using blender::Span;

static void curves_batch_cache_clear(Curves *curves);

/* ---------------------------------------------------------------------- */
/* Hair GPUBatch Cache */

struct HairBatchCache {
  ParticleHairCache hair;

  /* settings to determine if cache is invalid */
  bool is_dirty;
};

/* GPUBatch cache management. */

static bool curves_batch_cache_valid(Curves *curves)
{
  HairBatchCache *cache = static_cast<HairBatchCache *>(curves->batch_cache);
  return (cache && cache->is_dirty == false);
}

static void curves_batch_cache_init(Curves *curves)
{
  HairBatchCache *cache = static_cast<HairBatchCache *>(curves->batch_cache);

  if (!cache) {
    cache = MEM_cnew<HairBatchCache>(__func__);
    curves->batch_cache = cache;
  }
  else {
    memset(cache, 0, sizeof(*cache));
  }

  cache->is_dirty = false;
}

void DRW_curves_batch_cache_validate(Curves *curves)
{
  if (!curves_batch_cache_valid(curves)) {
    curves_batch_cache_clear(curves);
    curves_batch_cache_init(curves);
  }
}

static HairBatchCache *curves_batch_cache_get(Curves *curves)
{
  DRW_curves_batch_cache_validate(curves);
  return static_cast<HairBatchCache *>(curves->batch_cache);
}

void DRW_curves_batch_cache_dirty_tag(Curves *curves, int mode)
{
  HairBatchCache *cache = static_cast<HairBatchCache *>(curves->batch_cache);
  if (cache == nullptr) {
    return;
  }
  switch (mode) {
    case BKE_CURVES_BATCH_DIRTY_ALL:
      cache->is_dirty = true;
      break;
    default:
      BLI_assert(0);
  }
}

static void curves_batch_cache_clear(Curves *curves)
{
  HairBatchCache *cache = static_cast<HairBatchCache *>(curves->batch_cache);
  if (!cache) {
    return;
  }

  particle_batch_cache_clear_hair(&cache->hair);
}

void DRW_curves_batch_cache_free(Curves *curves)
{
  curves_batch_cache_clear(curves);
  MEM_SAFE_FREE(curves->batch_cache);
}

static void ensure_seg_pt_count(Curves *curves, ParticleHairCache *curves_cache)
{
  if ((curves_cache->pos != nullptr && curves_cache->indices != nullptr) ||
      (curves_cache->proc_point_buf != nullptr)) {
    return;
  }

  curves_cache->strands_len = curves->geometry.curve_size;
  curves_cache->elems_len = curves->geometry.point_size + curves->geometry.curve_size;
  curves_cache->point_len = curves->geometry.point_size;
}

static void curves_batch_cache_fill_segments_proc_pos(Curves *curves,
                                                      GPUVertBufRaw *attr_step,
                                                      GPUVertBufRaw *length_step)
{
  /* TODO: use hair radius layer if available. */
  const int curve_size = curves->geometry.curve_size;
  const blender::bke::CurvesGeometry &geometry = blender::bke::CurvesGeometry::wrap(
      curves->geometry);
  Span<float3> positions = geometry.positions();

  for (const int i : IndexRange(curve_size)) {
    const IndexRange curve_range = geometry.range_for_curve(i);

    Span<float3> spline_positions = positions.slice(curve_range);
    float total_len = 0.0f;
    float *seg_data_first;
    for (const int i_spline : spline_positions.index_range()) {
      float *seg_data = (float *)GPU_vertbuf_raw_step(attr_step);
      copy_v3_v3(seg_data, spline_positions[i_spline]);
      if (i_spline == 0) {
        seg_data_first = seg_data;
      }
      else {
        total_len += blender::math::distance(spline_positions[i_spline - 1],
                                             spline_positions[i_spline]);
      }
      seg_data[3] = total_len;
    }
    /* Assign length value. */
    *(float *)GPU_vertbuf_raw_step(length_step) = total_len;
    if (total_len > 0.0f) {
      /* Divide by total length to have a [0-1] number. */
      for ([[maybe_unused]] const int i_spline : spline_positions.index_range()) {
        seg_data_first[3] /= total_len;
        seg_data_first += 4;
      }
    }
  }
}

static void curves_batch_cache_ensure_procedural_pos(Curves *curves,
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

    curves_batch_cache_fill_segments_proc_pos(curves, &point_step, &length_step);

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

static void curves_batch_cache_fill_strands_data(Curves *curves,
                                                 GPUVertBufRaw *data_step,
                                                 GPUVertBufRaw *seg_step)
{
  const blender::bke::CurvesGeometry &geometry = blender::bke::CurvesGeometry::wrap(
      curves->geometry);

  for (const int i : IndexRange(geometry.curves_size())) {
    const IndexRange curve_range = geometry.range_for_curve(i);

    *(uint *)GPU_vertbuf_raw_step(data_step) = curve_range.start();
    *(ushort *)GPU_vertbuf_raw_step(seg_step) = curve_range.size() - 1;
  }
}

static void curves_batch_cache_ensure_procedural_strand_data(Curves *curves,
                                                             ParticleHairCache *cache)
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

  curves_batch_cache_fill_strands_data(curves, &data_step, &seg_step);

  /* Create vbo immediately to bind to texture buffer. */
  GPU_vertbuf_use(cache->proc_strand_buf);
  cache->strand_tex = GPU_texture_create_from_vertbuf("curves_strand", cache->proc_strand_buf);

  GPU_vertbuf_use(cache->proc_strand_seg_buf);
  cache->strand_seg_tex = GPU_texture_create_from_vertbuf("curves_strand_seg",
                                                          cache->proc_strand_seg_buf);
}

static void curves_batch_cache_ensure_procedural_final_points(ParticleHairCache *cache, int subdiv)
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

static void curves_batch_cache_fill_segments_indices(Curves *curves,
                                                     const int res,
                                                     GPUIndexBufBuilder *elb)
{
  const int curve_size = curves->geometry.curve_size;

  uint curr_point = 0;

  for ([[maybe_unused]] const int i : IndexRange(curve_size)) {
    for (int k = 0; k < res; k++) {
      GPU_indexbuf_add_generic_vert(elb, curr_point++);
    }
    GPU_indexbuf_add_primitive_restart(elb);
  }
}

static void curves_batch_cache_ensure_procedural_indices(Curves *curves,
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

  curves_batch_cache_fill_segments_indices(curves, verts_per_hair, &elb);

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
  Curves *curves = static_cast<Curves *>(object->data);

  HairBatchCache *cache = curves_batch_cache_get(curves);
  *r_hair_cache = &cache->hair;

  const int steps = 3; /* TODO: don't hard-code? */
  (*r_hair_cache)->final[subdiv].strands_res = 1 << (steps + subdiv);

  /* Refreshed on combing and simulation. */
  if ((*r_hair_cache)->proc_point_buf == nullptr) {
    ensure_seg_pt_count(curves, &cache->hair);
    curves_batch_cache_ensure_procedural_pos(curves, &cache->hair, gpu_material);
    need_ft_update = true;
  }

  /* Refreshed if active layer or custom data changes. */
  if ((*r_hair_cache)->strand_tex == nullptr) {
    curves_batch_cache_ensure_procedural_strand_data(curves, &cache->hair);
  }

  /* Refreshed only on subdiv count change. */
  if ((*r_hair_cache)->final[subdiv].proc_buf == nullptr) {
    curves_batch_cache_ensure_procedural_final_points(&cache->hair, subdiv);
    need_ft_update = true;
  }
  if ((*r_hair_cache)->final[subdiv].proc_hairs[thickness_res - 1] == nullptr) {
    curves_batch_cache_ensure_procedural_indices(curves, &cache->hair, thickness_res, subdiv);
  }

  return need_ft_update;
}

int DRW_curves_material_count_get(Curves *curves)
{
  return max_ii(1, curves->totcol);
}
