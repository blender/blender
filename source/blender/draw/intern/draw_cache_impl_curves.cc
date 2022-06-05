/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2017 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup draw
 *
 * \brief Curves API for render engines
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
#include "DNA_scene_types.h"

#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"

#include "GPU_batch.h"
#include "GPU_material.h"
#include "GPU_texture.h"

#include "DRW_render.h"

#include "draw_attributes.h"
#include "draw_cache_impl.h" /* own include */
#include "draw_cache_inline.h"
#include "draw_curves_private.h" /* own include */
#include "draw_shader.h"

using blender::ColorGeometry4f;
using blender::float3;
using blender::IndexRange;
using blender::MutableSpan;
using blender::Span;

/* ---------------------------------------------------------------------- */
/* Curves GPUBatch Cache */

struct CurvesBatchCache {
  CurvesEvalCache curves_cache;

  GPUBatch *edit_points;

  /* To determine if cache is invalid. */
  bool is_dirty;

  /** Needed when updating material data (e.g. attributes) as the same curves might be used for
   * multiple objects with different materials. */
  ThreadMutex render_mutex;
};

static bool curves_batch_cache_valid(const Curves &curves)
{
  const CurvesBatchCache *cache = static_cast<CurvesBatchCache *>(curves.batch_cache);
  return (cache && cache->is_dirty == false);
}

static void curves_batch_cache_init(Curves &curves)
{
  CurvesBatchCache *cache = static_cast<CurvesBatchCache *>(curves.batch_cache);

  if (!cache) {
    cache = MEM_cnew<CurvesBatchCache>(__func__);
    BLI_mutex_init(&cache->render_mutex);
    curves.batch_cache = cache;
  }
  else {
    memset(cache, 0, sizeof(*cache));
  }

  cache->is_dirty = false;
}

static void curves_discard_attributes(CurvesEvalCache &curves_cache)
{
  for (int i = 0; i < GPU_MAX_ATTR; i++) {
    GPU_VERTBUF_DISCARD_SAFE(curves_cache.proc_attributes_buf[i]);
    DRW_TEXTURE_FREE_SAFE(curves_cache.proc_attributes_tex[i]);
  }

  for (int i = 0; i < MAX_HAIR_SUBDIV; i++) {
    for (int j = 0; j < GPU_MAX_ATTR; j++) {
      GPU_VERTBUF_DISCARD_SAFE(curves_cache.final[i].attributes_buf[j]);
      DRW_TEXTURE_FREE_SAFE(curves_cache.final[i].attributes_tex[j]);
    }

    drw_attributes_clear(&curves_cache.final[i].attr_used);
  }
}

static void curves_batch_cache_clear_data(CurvesEvalCache &curves_cache)
{
  /* TODO: more granular update tagging. */
  GPU_VERTBUF_DISCARD_SAFE(curves_cache.proc_point_buf);
  GPU_VERTBUF_DISCARD_SAFE(curves_cache.proc_length_buf);
  DRW_TEXTURE_FREE_SAFE(curves_cache.point_tex);
  DRW_TEXTURE_FREE_SAFE(curves_cache.length_tex);

  GPU_VERTBUF_DISCARD_SAFE(curves_cache.proc_strand_buf);
  GPU_VERTBUF_DISCARD_SAFE(curves_cache.proc_strand_seg_buf);
  DRW_TEXTURE_FREE_SAFE(curves_cache.strand_tex);
  DRW_TEXTURE_FREE_SAFE(curves_cache.strand_seg_tex);

  for (int i = 0; i < MAX_HAIR_SUBDIV; i++) {
    GPU_VERTBUF_DISCARD_SAFE(curves_cache.final[i].proc_buf);
    DRW_TEXTURE_FREE_SAFE(curves_cache.final[i].proc_tex);
    for (int j = 0; j < MAX_THICKRES; j++) {
      GPU_BATCH_DISCARD_SAFE(curves_cache.final[i].proc_hairs[j]);
    }
  }

  curves_discard_attributes(curves_cache);
}

static void curves_batch_cache_clear(Curves &curves)
{
  CurvesBatchCache *cache = static_cast<CurvesBatchCache *>(curves.batch_cache);
  if (!cache) {
    return;
  }

  curves_batch_cache_clear_data(cache->curves_cache);

  GPU_BATCH_DISCARD_SAFE(cache->edit_points);
}

void DRW_curves_batch_cache_validate(Curves *curves)
{
  if (!curves_batch_cache_valid(*curves)) {
    curves_batch_cache_clear(*curves);
    curves_batch_cache_init(*curves);
  }
}

static CurvesBatchCache &curves_batch_cache_get(Curves &curves)
{
  DRW_curves_batch_cache_validate(&curves);
  return *static_cast<CurvesBatchCache *>(curves.batch_cache);
}

void DRW_curves_batch_cache_dirty_tag(Curves *curves, int mode)
{
  CurvesBatchCache *cache = static_cast<CurvesBatchCache *>(curves->batch_cache);
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

void DRW_curves_batch_cache_free(Curves *curves)
{
  curves_batch_cache_clear(*curves);
  CurvesBatchCache *cache = static_cast<CurvesBatchCache *>(curves->batch_cache);
  BLI_mutex_end(&cache->render_mutex);
  MEM_SAFE_FREE(curves->batch_cache);
}

void DRW_curves_batch_cache_free_old(Curves *curves, int ctime)
{
  CurvesBatchCache *cache = static_cast<CurvesBatchCache *>(curves->batch_cache);
  if (cache == nullptr) {
    return;
  }

  bool do_discard = false;

  for (int i = 0; i < MAX_HAIR_SUBDIV; i++) {
    CurvesEvalFinalCache &final_cache = cache->curves_cache.final[i];

    if (drw_attributes_overlap(&final_cache.attr_used_over_time, &final_cache.attr_used)) {
      final_cache.last_attr_matching_time = ctime;
    }

    if (ctime - final_cache.last_attr_matching_time > U.vbotimeout) {
      do_discard = true;
    }

    drw_attributes_clear(&final_cache.attr_used_over_time);
  }

  if (do_discard) {
    curves_discard_attributes(cache->curves_cache);
  }
}

static void ensure_seg_pt_count(const Curves &curves, CurvesEvalCache &curves_cache)
{
  if (curves_cache.proc_point_buf != nullptr) {
    return;
  }

  curves_cache.strands_len = curves.geometry.curve_num;
  curves_cache.elems_len = curves.geometry.point_num + curves.geometry.curve_num;
  curves_cache.point_len = curves.geometry.point_num;
}

struct PositionAndParameter {
  float3 position;
  float parameter;
};

static void curves_batch_cache_fill_segments_proc_pos(
    const Curves &curves_id,
    MutableSpan<PositionAndParameter> posTime_data,
    MutableSpan<float> hairLength_data)
{
  /* TODO: use hair radius layer if available. */
  const int curve_num = curves_id.geometry.curve_num;
  const blender::bke::CurvesGeometry &curves = blender::bke::CurvesGeometry::wrap(
      curves_id.geometry);
  Span<float3> positions = curves.positions();

  for (const int i_curve : IndexRange(curve_num)) {
    const IndexRange points = curves.points_for_curve(i_curve);

    Span<float3> curve_positions = positions.slice(points);
    MutableSpan<PositionAndParameter> curve_posTime_data = posTime_data.slice(points);

    float total_len = 0.0f;
    for (const int i_point : curve_positions.index_range()) {
      if (i_point > 0) {
        total_len += blender::math::distance(curve_positions[i_point - 1],
                                             curve_positions[i_point]);
      }
      curve_posTime_data[i_point].position = curve_positions[i_point];
      curve_posTime_data[i_point].parameter = total_len;
    }
    hairLength_data[i_curve] = total_len;

    /* Assign length value. */
    if (total_len > 0.0f) {
      const float factor = 1.0f / total_len;
      /* Divide by total length to have a [0-1] number. */
      for (const int i_point : curve_positions.index_range()) {
        curve_posTime_data[i_point].parameter *= factor;
      }
    }
  }
}

static void curves_batch_cache_ensure_procedural_pos(Curves &curves,
                                                     CurvesEvalCache &cache,
                                                     GPUMaterial *gpu_material)
{
  if (cache.proc_point_buf == nullptr || DRW_vbo_requested(cache.proc_point_buf)) {
    /* Initialize vertex format. */
    GPUVertFormat format = {0};
    GPU_vertformat_attr_add(&format, "posTime", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    GPU_vertformat_alias_add(&format, "pos");

    cache.proc_point_buf = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(cache.proc_point_buf, cache.point_len);

    MutableSpan posTime_data{
        reinterpret_cast<PositionAndParameter *>(GPU_vertbuf_get_data(cache.proc_point_buf)),
        cache.point_len};

    GPUVertFormat length_format = {0};
    GPU_vertformat_attr_add(&length_format, "hairLength", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);

    cache.proc_length_buf = GPU_vertbuf_create_with_format(&length_format);
    GPU_vertbuf_data_alloc(cache.proc_length_buf, cache.strands_len);

    MutableSpan hairLength_data{
        reinterpret_cast<float *>(GPU_vertbuf_get_data(cache.proc_length_buf)), cache.strands_len};

    curves_batch_cache_fill_segments_proc_pos(curves, posTime_data, hairLength_data);

    /* Create vbo immediately to bind to texture buffer. */
    GPU_vertbuf_use(cache.proc_point_buf);
    cache.point_tex = GPU_texture_create_from_vertbuf("hair_point", cache.proc_point_buf);
  }

  if (gpu_material && cache.proc_length_buf != nullptr && cache.length_tex) {
    ListBase gpu_attrs = GPU_material_attributes(gpu_material);
    LISTBASE_FOREACH (GPUMaterialAttribute *, attr, &gpu_attrs) {
      if (attr->type == CD_HAIRLENGTH) {
        GPU_vertbuf_use(cache.proc_length_buf);
        cache.length_tex = GPU_texture_create_from_vertbuf("hair_length", cache.proc_length_buf);
        break;
      }
    }
  }
}

void drw_curves_get_attribute_sampler_name(const char *layer_name, char r_sampler_name[32])
{
  char attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
  GPU_vertformat_safe_attr_name(layer_name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);
  /* Attributes use auto-name. */
  BLI_snprintf(r_sampler_name, 32, "a%s", attr_safe_name);
}

static void curves_batch_cache_ensure_procedural_final_attr(
    CurvesEvalCache &cache, GPUVertFormat *format, int subdiv, int index, const char *name)
{
  CurvesEvalFinalCache &final_cache = cache.final[subdiv];
  final_cache.attributes_buf[index] = GPU_vertbuf_create_with_format_ex(format,
                                                                        GPU_USAGE_DEVICE_ONLY);

  /* Create a destination buffer for the transform feedback. Sized appropriately */
  /* Those are points! not line segments. */
  GPU_vertbuf_data_alloc(final_cache.attributes_buf[index],
                         final_cache.strands_res * cache.strands_len);

  /* Create vbo immediately to bind to texture buffer. */
  GPU_vertbuf_use(final_cache.attributes_buf[index]);

  final_cache.attributes_tex[index] = GPU_texture_create_from_vertbuf(
      name, final_cache.attributes_buf[index]);
}

static void curves_batch_ensure_attribute(const Curves &curves,
                                          CurvesEvalCache &cache,
                                          const DRW_AttributeRequest &request,
                                          int subdiv,
                                          int index)
{
  GPU_VERTBUF_DISCARD_SAFE(cache.proc_attributes_buf[index]);
  DRW_TEXTURE_FREE_SAFE(cache.proc_attributes_tex[index]);

  char sampler_name[32];
  drw_curves_get_attribute_sampler_name(request.attribute_name, sampler_name);

  GPUVertFormat format = {0};
  GPU_vertformat_deinterleave(&format);
  /* All attributes use vec4, see comment below. */
  GPU_vertformat_attr_add(&format, sampler_name, GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

  cache.proc_attributes_buf[index] = GPU_vertbuf_create_with_format(&format);
  GPUVertBuf *attr_vbo = cache.proc_attributes_buf[index];

  GPU_vertbuf_data_alloc(attr_vbo,
                         request.domain == ATTR_DOMAIN_POINT ? curves.geometry.point_num :
                                                               curves.geometry.curve_num);

  CurveComponent component;
  component.replace(const_cast<Curves *>(&curves), GeometryOwnershipType::ReadOnly);

  /* TODO(@kevindietrich): float4 is used for scalar attributes as the implicit conversion done
   * by OpenGL to vec4 for a scalar `s` will produce a `vec4(s, 0, 0, 1)`. However, following
   * the Blender convention, it should be `vec4(s, s, s, 1)`. This could be resolved using a
   * similar texture state swizzle to map the attribute correctly as for volume attributes, so we
   * can control the conversion ourselves. */
  blender::VArray<ColorGeometry4f> attribute = component.attribute_get_for_read<ColorGeometry4f>(
      request.attribute_name, request.domain, {0.0f, 0.0f, 0.0f, 1.0f});

  MutableSpan<ColorGeometry4f> vbo_span{
      static_cast<ColorGeometry4f *>(GPU_vertbuf_get_data(attr_vbo)),
      component.attribute_domain_num(request.domain)};

  attribute.materialize(vbo_span);

  GPU_vertbuf_use(attr_vbo);
  cache.proc_attributes_tex[index] = GPU_texture_create_from_vertbuf(sampler_name, attr_vbo);

  /* Existing final data may have been for a different attribute (with a different name or domain),
   * free the data. */
  GPU_VERTBUF_DISCARD_SAFE(cache.final[subdiv].attributes_buf[index]);
  DRW_TEXTURE_FREE_SAFE(cache.final[subdiv].attributes_tex[index]);

  /* Ensure final data for points. */
  if (request.domain == ATTR_DOMAIN_POINT) {
    curves_batch_cache_ensure_procedural_final_attr(cache, &format, subdiv, index, sampler_name);
  }
}

static void curves_batch_cache_fill_strands_data(const Curves &curves_id,
                                                 GPUVertBufRaw &data_step,
                                                 GPUVertBufRaw &seg_step)
{
  const blender::bke::CurvesGeometry &curves = blender::bke::CurvesGeometry::wrap(
      curves_id.geometry);

  for (const int i : IndexRange(curves.curves_num())) {
    const IndexRange curve_range = curves.points_for_curve(i);

    *(uint *)GPU_vertbuf_raw_step(&data_step) = curve_range.start();
    *(ushort *)GPU_vertbuf_raw_step(&seg_step) = curve_range.size() - 1;
  }
}

static void curves_batch_cache_ensure_procedural_strand_data(Curves &curves,
                                                             CurvesEvalCache &cache)
{
  GPUVertBufRaw data_step, seg_step;

  GPUVertFormat format_data = {0};
  uint data_id = GPU_vertformat_attr_add(&format_data, "data", GPU_COMP_U32, 1, GPU_FETCH_INT);

  GPUVertFormat format_seg = {0};
  uint seg_id = GPU_vertformat_attr_add(&format_seg, "data", GPU_COMP_U16, 1, GPU_FETCH_INT);

  /* Curve Data. */
  cache.proc_strand_buf = GPU_vertbuf_create_with_format(&format_data);
  GPU_vertbuf_data_alloc(cache.proc_strand_buf, cache.strands_len);
  GPU_vertbuf_attr_get_raw_data(cache.proc_strand_buf, data_id, &data_step);

  cache.proc_strand_seg_buf = GPU_vertbuf_create_with_format(&format_seg);
  GPU_vertbuf_data_alloc(cache.proc_strand_seg_buf, cache.strands_len);
  GPU_vertbuf_attr_get_raw_data(cache.proc_strand_seg_buf, seg_id, &seg_step);

  curves_batch_cache_fill_strands_data(curves, data_step, seg_step);

  /* Create vbo immediately to bind to texture buffer. */
  GPU_vertbuf_use(cache.proc_strand_buf);
  cache.strand_tex = GPU_texture_create_from_vertbuf("curves_strand", cache.proc_strand_buf);

  GPU_vertbuf_use(cache.proc_strand_seg_buf);
  cache.strand_seg_tex = GPU_texture_create_from_vertbuf("curves_strand_seg",
                                                         cache.proc_strand_seg_buf);
}

static void curves_batch_cache_ensure_procedural_final_points(CurvesEvalCache &cache, int subdiv)
{
  /* Same format as point_tex. */
  GPUVertFormat format = {0};
  GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

  cache.final[subdiv].proc_buf = GPU_vertbuf_create_with_format_ex(&format, GPU_USAGE_DEVICE_ONLY);

  /* Create a destination buffer for the transform feedback. Sized appropriately */
  /* Those are points! not line segments. */
  GPU_vertbuf_data_alloc(cache.final[subdiv].proc_buf,
                         cache.final[subdiv].strands_res * cache.strands_len);

  /* Create vbo immediately to bind to texture buffer. */
  GPU_vertbuf_use(cache.final[subdiv].proc_buf);

  cache.final[subdiv].proc_tex = GPU_texture_create_from_vertbuf("hair_proc",
                                                                 cache.final[subdiv].proc_buf);
}

static void curves_batch_cache_fill_segments_indices(const Curves &curves,
                                                     const int res,
                                                     GPUIndexBufBuilder &elb)
{
  const int curves_num = curves.geometry.curve_num;

  uint curr_point = 0;

  for ([[maybe_unused]] const int i : IndexRange(curves_num)) {
    for (int k = 0; k < res; k++) {
      GPU_indexbuf_add_generic_vert(&elb, curr_point++);
    }
    GPU_indexbuf_add_primitive_restart(&elb);
  }
}

static void curves_batch_cache_ensure_procedural_indices(Curves &curves,
                                                         CurvesEvalCache &cache,
                                                         const int thickness_res,
                                                         const int subdiv)
{
  BLI_assert(thickness_res <= MAX_THICKRES); /* Cylinder strip not currently supported. */

  if (cache.final[subdiv].proc_hairs[thickness_res - 1] != nullptr) {
    return;
  }

  int verts_per_curve = cache.final[subdiv].strands_res * thickness_res;
  /* +1 for primitive restart */
  int element_count = (verts_per_curve + 1) * cache.strands_len;
  GPUPrimType prim_type = (thickness_res == 1) ? GPU_PRIM_LINE_STRIP : GPU_PRIM_TRI_STRIP;

  static GPUVertFormat format = {0};
  GPU_vertformat_clear(&format);

  /* initialize vertex format */
  GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_U8, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);

  GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
  GPU_vertbuf_data_alloc(vbo, 1);

  GPUIndexBufBuilder elb;
  GPU_indexbuf_init_ex(&elb, prim_type, element_count, element_count);

  curves_batch_cache_fill_segments_indices(curves, verts_per_curve, elb);

  cache.final[subdiv].proc_hairs[thickness_res - 1] = GPU_batch_create_ex(
      prim_type, vbo, GPU_indexbuf_build(&elb), GPU_BATCH_OWNS_VBO | GPU_BATCH_OWNS_INDEX);
}

static bool curves_ensure_attributes(const Curves &curves,
                                     CurvesBatchCache &cache,
                                     GPUMaterial *gpu_material,
                                     int subdiv)
{
  ThreadMutex *render_mutex = &cache.render_mutex;
  const CustomData *cd_curve = &curves.geometry.curve_data;
  const CustomData *cd_point = &curves.geometry.point_data;

  DRW_Attributes attrs_needed;
  drw_attributes_clear(&attrs_needed);
  ListBase gpu_attrs = GPU_material_attributes(gpu_material);
  LISTBASE_FOREACH (GPUMaterialAttribute *, gpu_attr, &gpu_attrs) {
    const char *name = gpu_attr->name;
    eCustomDataType type = static_cast<eCustomDataType>(gpu_attr->type);
    int layer = -1;
    eAttrDomain domain;

    if (drw_custom_data_match_attribute(cd_curve, name, &layer, &type)) {
      domain = ATTR_DOMAIN_CURVE;
    }
    else if (drw_custom_data_match_attribute(cd_point, name, &layer, &type)) {
      domain = ATTR_DOMAIN_POINT;
    }
    else {
      continue;
    }

    switch (type) {
      default:
        break;
      case CD_PROP_BOOL:
      case CD_PROP_INT8:
      case CD_PROP_INT32:
      case CD_PROP_FLOAT:
      case CD_PROP_FLOAT2:
      case CD_PROP_FLOAT3:
      case CD_PROP_COLOR: {
        if (layer != -1) {
          DRW_AttributeRequest *req = drw_attributes_add_request(
              &attrs_needed, (eCustomDataType)type, layer, domain);
          if (req) {
            BLI_strncpy(req->attribute_name, name, sizeof(req->attribute_name));
          }
        }
        break;
      }
    }
  }

  CurvesEvalFinalCache &final_cache = cache.curves_cache.final[subdiv];

  const bool attr_overlap = drw_attributes_overlap(&final_cache.attr_used, &attrs_needed);
  if (attr_overlap == false) {
    /* Some new attributes have been added, free all and start over. */
    for (int i = 0; i < GPU_MAX_ATTR; i++) {
      GPU_VERTBUF_DISCARD_SAFE(cache.curves_cache.proc_attributes_buf[i]);
      DRW_TEXTURE_FREE_SAFE(cache.curves_cache.proc_attributes_tex[i]);
    }
    drw_attributes_merge(&final_cache.attr_used, &attrs_needed, render_mutex);
  }
  drw_attributes_merge(&final_cache.attr_used_over_time, &attrs_needed, render_mutex);

  bool need_tf_update = false;

  for (int i = 0; i < final_cache.attr_used.num_requests; i++) {
    const DRW_AttributeRequest &request = final_cache.attr_used.requests[i];

    if (cache.curves_cache.proc_attributes_buf[i] != nullptr) {
      continue;
    }

    if (request.domain == ATTR_DOMAIN_POINT) {
      need_tf_update = true;
    }

    curves_batch_ensure_attribute(curves, cache.curves_cache, request, subdiv, i);
  }

  return need_tf_update;
}

bool curves_ensure_procedural_data(Object *object,
                                   CurvesEvalCache **r_hair_cache,
                                   GPUMaterial *gpu_material,
                                   const int subdiv,
                                   const int thickness_res)
{
  bool need_ft_update = false;
  Curves &curves = *static_cast<Curves *>(object->data);

  CurvesBatchCache &cache = curves_batch_cache_get(curves);
  *r_hair_cache = &cache.curves_cache;

  const int steps = 3; /* TODO: don't hard-code? */
  (*r_hair_cache)->final[subdiv].strands_res = 1 << (steps + subdiv);

  /* Refreshed on combing and simulation. */
  if ((*r_hair_cache)->proc_point_buf == nullptr) {
    ensure_seg_pt_count(curves, cache.curves_cache);
    curves_batch_cache_ensure_procedural_pos(curves, cache.curves_cache, gpu_material);
    need_ft_update = true;
  }

  /* Refreshed if active layer or custom data changes. */
  if ((*r_hair_cache)->strand_tex == nullptr) {
    curves_batch_cache_ensure_procedural_strand_data(curves, cache.curves_cache);
  }

  /* Refreshed only on subdiv count change. */
  if ((*r_hair_cache)->final[subdiv].proc_buf == nullptr) {
    curves_batch_cache_ensure_procedural_final_points(cache.curves_cache, subdiv);
    need_ft_update = true;
  }
  if ((*r_hair_cache)->final[subdiv].proc_hairs[thickness_res - 1] == nullptr) {
    curves_batch_cache_ensure_procedural_indices(
        curves, cache.curves_cache, thickness_res, subdiv);
  }

  if (gpu_material) {
    need_ft_update |= curves_ensure_attributes(curves, cache, gpu_material, subdiv);
  }

  return need_ft_update;
}

int DRW_curves_material_count_get(Curves *curves)
{
  return max_ii(1, curves->totcol);
}

GPUBatch *DRW_curves_batch_cache_get_edit_points(Curves *curves)
{
  CurvesBatchCache &cache = curves_batch_cache_get(*curves);
  return DRW_batch_request(&cache.edit_points);
}

void DRW_curves_batch_cache_create_requested(const Object *ob)
{
  Curves *curves = static_cast<Curves *>(ob->data);
  CurvesBatchCache &cache = curves_batch_cache_get(*curves);

  if (DRW_batch_requested(cache.edit_points, GPU_PRIM_POINTS)) {
    DRW_vbo_request(cache.edit_points, &cache.curves_cache.proc_point_buf);
  }

  if (DRW_vbo_requested(cache.curves_cache.proc_point_buf)) {
    curves_batch_cache_ensure_procedural_pos(*curves, cache.curves_cache, nullptr);
  }
}
