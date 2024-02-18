/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief PointCloud API for render engines
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_math_color.hh"
#include "BLI_math_vector.h"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"

#include "BKE_attribute.hh"
#include "BKE_pointcloud.hh"

#include "GPU_batch.h"
#include "GPU_material.hh"

#include "draw_attributes.hh"
#include "draw_cache_impl.hh"
#include "draw_cache_inline.hh"
#include "draw_pointcloud_private.hh" /* own include */

namespace blender::draw {

/* -------------------------------------------------------------------- */
/** \name GPUBatch cache management
 * \{ */

struct PointCloudEvalCache {
  /* Dot primitive types. */
  GPUBatch *dots;
  /* Triangle primitive types. */
  GPUBatch *surface;
  GPUBatch **surface_per_mat;

  /* Triangles indices to draw the points. */
  GPUIndexBuf *geom_indices;

  /* Position and radius. */
  GPUVertBuf *pos_rad;
  /* Active attribute in 3D view. */
  GPUVertBuf *attr_viewer;
  /* Requested attributes */
  GPUVertBuf *attributes_buf[GPU_MAX_ATTR];

  /** Attributes currently being drawn or about to be drawn. */
  DRW_Attributes attr_used;
  /**
   * Attributes that were used at some point. This is used for garbage collection, to remove
   * attributes that are not used in shaders anymore due to user edits.
   */
  DRW_Attributes attr_used_over_time;

  /**
   * The last time in seconds that the `attr_used` and `attr_used_over_time` were exactly the same.
   * If the delta between this time and the current scene time is greater than the timeout set in
   * user preferences (`U.vbotimeout`) then garbage collection is performed.
   */
  int last_attr_matching_time;

  int mat_len;
};

struct PointCloudBatchCache {
  PointCloudEvalCache eval_cache;

  /* settings to determine if cache is invalid */
  bool is_dirty;

  /**
   * The draw cache extraction is currently not multi-threaded for multiple objects, but if it was,
   * some locking would be necessary because multiple objects can use the same object data with
   * different materials, etc. This is a placeholder to make multi-threading easier in the future.
   */
  std::mutex render_mutex;
};

static PointCloudBatchCache *pointcloud_batch_cache_get(PointCloud &pointcloud)
{
  return static_cast<PointCloudBatchCache *>(pointcloud.batch_cache);
}

static bool pointcloud_batch_cache_valid(PointCloud &pointcloud)
{
  PointCloudBatchCache *cache = pointcloud_batch_cache_get(pointcloud);

  if (cache == nullptr) {
    return false;
  }
  if (cache->eval_cache.mat_len != DRW_pointcloud_material_count_get(&pointcloud)) {
    return false;
  }
  return cache->is_dirty == false;
}

static void pointcloud_batch_cache_init(PointCloud &pointcloud)
{
  PointCloudBatchCache *cache = pointcloud_batch_cache_get(pointcloud);

  if (!cache) {
    cache = MEM_new<PointCloudBatchCache>(__func__);
    pointcloud.batch_cache = cache;
  }
  else {
    cache->eval_cache = {};
  }

  cache->eval_cache.mat_len = DRW_pointcloud_material_count_get(&pointcloud);
  cache->eval_cache.surface_per_mat = static_cast<GPUBatch **>(
      MEM_callocN(sizeof(GPUBatch *) * cache->eval_cache.mat_len, __func__));

  cache->is_dirty = false;
}

void DRW_pointcloud_batch_cache_dirty_tag(PointCloud *pointcloud, int mode)
{
  PointCloudBatchCache *cache = pointcloud_batch_cache_get(*pointcloud);
  if (cache == nullptr) {
    return;
  }
  switch (mode) {
    case BKE_POINTCLOUD_BATCH_DIRTY_ALL:
      cache->is_dirty = true;
      break;
    default:
      BLI_assert(0);
  }
}

static void pointcloud_discard_attributes(PointCloudBatchCache &cache)
{
  for (const int j : IndexRange(GPU_MAX_ATTR)) {
    GPU_VERTBUF_DISCARD_SAFE(cache.eval_cache.attributes_buf[j]);
  }

  drw_attributes_clear(&cache.eval_cache.attr_used);
}

static void pointcloud_batch_cache_clear(PointCloud &pointcloud)
{
  PointCloudBatchCache *cache = pointcloud_batch_cache_get(pointcloud);
  if (!cache) {
    return;
  }

  GPU_BATCH_DISCARD_SAFE(cache->eval_cache.dots);
  GPU_BATCH_DISCARD_SAFE(cache->eval_cache.surface);
  GPU_VERTBUF_DISCARD_SAFE(cache->eval_cache.pos_rad);
  GPU_VERTBUF_DISCARD_SAFE(cache->eval_cache.attr_viewer);
  GPU_INDEXBUF_DISCARD_SAFE(cache->eval_cache.geom_indices);

  if (cache->eval_cache.surface_per_mat) {
    for (int i = 0; i < cache->eval_cache.mat_len; i++) {
      GPU_BATCH_DISCARD_SAFE(cache->eval_cache.surface_per_mat[i]);
    }
  }
  MEM_SAFE_FREE(cache->eval_cache.surface_per_mat);

  pointcloud_discard_attributes(*cache);
}

void DRW_pointcloud_batch_cache_validate(PointCloud *pointcloud)
{
  if (!pointcloud_batch_cache_valid(*pointcloud)) {
    pointcloud_batch_cache_clear(*pointcloud);
    pointcloud_batch_cache_init(*pointcloud);
  }
}

void DRW_pointcloud_batch_cache_free(PointCloud *pointcloud)
{
  pointcloud_batch_cache_clear(*pointcloud);
  MEM_SAFE_FREE(pointcloud->batch_cache);
}

void DRW_pointcloud_batch_cache_free_old(PointCloud *pointcloud, int ctime)
{
  PointCloudBatchCache *cache = pointcloud_batch_cache_get(*pointcloud);
  if (!cache) {
    return;
  }

  bool do_discard = false;

  if (drw_attributes_overlap(&cache->eval_cache.attr_used_over_time, &cache->eval_cache.attr_used))
  {
    cache->eval_cache.last_attr_matching_time = ctime;
  }

  if (ctime - cache->eval_cache.last_attr_matching_time > U.vbotimeout) {
    do_discard = true;
  }

  drw_attributes_clear(&cache->eval_cache.attr_used_over_time);

  if (do_discard) {
    pointcloud_discard_attributes(*cache);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name PointCloud extraction
 * \{ */

static const uint half_octahedron_tris[4][3] = {
    {0, 1, 2},
    {0, 2, 3},
    {0, 3, 4},
    {0, 4, 1},
};

static void pointcloud_extract_indices(const PointCloud &pointcloud, PointCloudBatchCache &cache)
{
  /** \note Avoid modulo by non-power-of-two in shader. */
  uint32_t vertid_max = pointcloud.totpoint * 32;
  uint32_t index_len = pointcloud.totpoint * ARRAY_SIZE(half_octahedron_tris);

  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_TRIS, index_len, vertid_max);

  for (int p = 0; p < pointcloud.totpoint; p++) {
    for (int i = 0; i < ARRAY_SIZE(half_octahedron_tris); i++) {
      GPU_indexbuf_add_tri_verts(&builder,
                                 half_octahedron_tris[i][0] + p * 32,
                                 half_octahedron_tris[i][1] + p * 32,
                                 half_octahedron_tris[i][2] + p * 32);
    }
  }

  GPU_indexbuf_build_in_place(&builder, cache.eval_cache.geom_indices);
}

static void pointcloud_extract_position_and_radius(const PointCloud &pointcloud,
                                                   PointCloudBatchCache &cache)
{
  const bke::AttributeAccessor attributes = pointcloud.attributes();
  const Span<float3> positions = pointcloud.positions();
  const VArray<float> radii = *attributes.lookup<float>("radius");
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  }

  GPUUsageType usage_flag = GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY;
  GPU_vertbuf_init_with_format_ex(cache.eval_cache.pos_rad, &format, usage_flag);

  GPU_vertbuf_data_alloc(cache.eval_cache.pos_rad, positions.size());
  MutableSpan<float4> vbo_data{
      static_cast<float4 *>(GPU_vertbuf_get_data(cache.eval_cache.pos_rad)), pointcloud.totpoint};
  if (radii) {
    const VArraySpan<float> radii_span(radii);
    threading::parallel_for(vbo_data.index_range(), 4096, [&](IndexRange range) {
      for (const int i : range) {
        vbo_data[i].x = positions[i].x;
        vbo_data[i].y = positions[i].y;
        vbo_data[i].z = positions[i].z;
        /* TODO(fclem): remove multiplication. Here only for keeping the size correct for now. */
        vbo_data[i].w = radii_span[i] * 100.0f;
      }
    });
  }
  else {
    threading::parallel_for(vbo_data.index_range(), 4096, [&](IndexRange range) {
      for (const int i : range) {
        vbo_data[i].x = positions[i].x;
        vbo_data[i].y = positions[i].y;
        vbo_data[i].z = positions[i].z;
        vbo_data[i].w = 1.0f;
      }
    });
  }
}

static void pointcloud_extract_attribute(const PointCloud &pointcloud,
                                         PointCloudBatchCache &cache,
                                         const DRW_AttributeRequest &request,
                                         int index)
{
  GPUVertBuf *&attr_buf = cache.eval_cache.attributes_buf[index];

  const bke::AttributeAccessor attributes = pointcloud.attributes();

  /* TODO(@kevindietrich): float4 is used for scalar attributes as the implicit conversion done
   * by OpenGL to vec4 for a scalar `s` will produce a `vec4(s, 0, 0, 1)`. However, following
   * the Blender convention, it should be `vec4(s, s, s, 1)`. This could be resolved using a
   * similar texture state swizzle to map the attribute correctly as for volume attributes, so we
   * can control the conversion ourselves. */
  bke::AttributeReader<ColorGeometry4f> attribute = attributes.lookup_or_default<ColorGeometry4f>(
      request.attribute_name, request.domain, {0.0f, 0.0f, 0.0f, 1.0f});

  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "attr", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  }
  GPUUsageType usage_flag = GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY;
  GPU_vertbuf_init_with_format_ex(attr_buf, &format, usage_flag);
  GPU_vertbuf_data_alloc(attr_buf, pointcloud.totpoint);

  MutableSpan<ColorGeometry4f> vbo_data{
      static_cast<ColorGeometry4f *>(GPU_vertbuf_get_data(attr_buf)), pointcloud.totpoint};
  attribute.varray.materialize(vbo_data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Private API
 * \{ */

GPUVertBuf *pointcloud_position_and_radius_get(PointCloud *pointcloud)
{
  PointCloudBatchCache *cache = pointcloud_batch_cache_get(*pointcloud);
  DRW_vbo_request(nullptr, &cache->eval_cache.pos_rad);
  return cache->eval_cache.pos_rad;
}

GPUBatch **pointcloud_surface_shaded_get(PointCloud *pointcloud,
                                         GPUMaterial **gpu_materials,
                                         int mat_len)
{
  PointCloudBatchCache *cache = pointcloud_batch_cache_get(*pointcloud);
  DRW_Attributes attrs_needed;
  drw_attributes_clear(&attrs_needed);

  for (GPUMaterial *gpu_material : Span<GPUMaterial *>(gpu_materials, mat_len)) {
    ListBase gpu_attrs = GPU_material_attributes(gpu_material);
    LISTBASE_FOREACH (GPUMaterialAttribute *, gpu_attr, &gpu_attrs) {
      const char *name = gpu_attr->name;

      int layer_index;
      eCustomDataType type;
      bke::AttrDomain domain = bke::AttrDomain::Point;
      if (!drw_custom_data_match_attribute(&pointcloud->pdata, name, &layer_index, &type)) {
        continue;
      }

      drw_attributes_add_request(&attrs_needed, name, type, layer_index, domain);
    }
  }

  if (!drw_attributes_overlap(&cache->eval_cache.attr_used, &attrs_needed)) {
    /* Some new attributes have been added, free all and start over. */
    for (const int i : IndexRange(GPU_MAX_ATTR)) {
      GPU_VERTBUF_DISCARD_SAFE(cache->eval_cache.attributes_buf[i]);
    }
    drw_attributes_merge(&cache->eval_cache.attr_used, &attrs_needed, cache->render_mutex);
  }
  drw_attributes_merge(&cache->eval_cache.attr_used_over_time, &attrs_needed, cache->render_mutex);

  DRW_batch_request(&cache->eval_cache.surface_per_mat[0]);
  return cache->eval_cache.surface_per_mat;
}

GPUBatch *pointcloud_surface_get(PointCloud *pointcloud)
{
  PointCloudBatchCache *cache = pointcloud_batch_cache_get(*pointcloud);
  return DRW_batch_request(&cache->eval_cache.surface);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name API
 * \{ */

GPUBatch *DRW_pointcloud_batch_cache_get_dots(Object *ob)
{
  PointCloud &pointcloud = *static_cast<PointCloud *>(ob->data);
  PointCloudBatchCache *cache = pointcloud_batch_cache_get(pointcloud);
  return DRW_batch_request(&cache->eval_cache.dots);
}

GPUVertBuf *DRW_pointcloud_position_and_radius_buffer_get(Object *ob)
{
  PointCloud &pointcloud = *static_cast<PointCloud *>(ob->data);
  return pointcloud_position_and_radius_get(&pointcloud);
}

GPUVertBuf **DRW_pointcloud_evaluated_attribute(PointCloud *pointcloud, const char *name)
{
  PointCloudBatchCache &cache = *pointcloud_batch_cache_get(*pointcloud);

  int layer_index;
  eCustomDataType type;
  bke::AttrDomain domain = bke::AttrDomain::Point;
  if (drw_custom_data_match_attribute(&pointcloud->pdata, name, &layer_index, &type)) {
    DRW_Attributes attributes{};
    drw_attributes_add_request(&attributes, name, type, layer_index, domain);
    drw_attributes_merge(&cache.eval_cache.attr_used, &attributes, cache.render_mutex);
  }

  int request_i = -1;
  for (const int i : IndexRange(cache.eval_cache.attr_used.num_requests)) {
    if (STREQ(cache.eval_cache.attr_used.requests[i].attribute_name, name)) {
      request_i = i;
      break;
    }
  }
  if (request_i == -1) {
    return nullptr;
  }
  return &cache.eval_cache.attributes_buf[request_i];
}

int DRW_pointcloud_material_count_get(const PointCloud *pointcloud)
{
  return max_ii(1, pointcloud->totcol);
}

void DRW_pointcloud_batch_cache_create_requested(Object *ob)
{
  PointCloud *pointcloud = static_cast<PointCloud *>(ob->data);
  PointCloudBatchCache &cache = *pointcloud_batch_cache_get(*pointcloud);

  if (DRW_batch_requested(cache.eval_cache.dots, GPU_PRIM_POINTS)) {
    DRW_vbo_request(cache.eval_cache.dots, &cache.eval_cache.pos_rad);
  }

  if (DRW_batch_requested(cache.eval_cache.surface, GPU_PRIM_TRIS)) {
    DRW_ibo_request(cache.eval_cache.surface, &cache.eval_cache.geom_indices);
    DRW_vbo_request(cache.eval_cache.surface, &cache.eval_cache.pos_rad);
  }
  for (int i = 0; i < cache.eval_cache.mat_len; i++) {
    if (DRW_batch_requested(cache.eval_cache.surface_per_mat[i], GPU_PRIM_TRIS)) {
      /* TODO(fclem): Per material ranges. */
      DRW_ibo_request(cache.eval_cache.surface_per_mat[i], &cache.eval_cache.geom_indices);
    }
  }
  for (int j = 0; j < cache.eval_cache.attr_used.num_requests; j++) {
    DRW_vbo_request(nullptr, &cache.eval_cache.attributes_buf[j]);

    if (DRW_vbo_requested(cache.eval_cache.attributes_buf[j])) {
      pointcloud_extract_attribute(*pointcloud, cache, cache.eval_cache.attr_used.requests[j], j);
    }
  }

  if (DRW_ibo_requested(cache.eval_cache.geom_indices)) {
    pointcloud_extract_indices(*pointcloud, cache);
  }

  if (DRW_vbo_requested(cache.eval_cache.pos_rad)) {
    pointcloud_extract_position_and_radius(*pointcloud, cache);
  }
}

/** \} */

}  // namespace blender::draw
