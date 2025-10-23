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

#include "BLI_color.hh"
#include "BLI_listbase.h"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_userdef_types.h"

#include "BKE_attribute.hh"
#include "BKE_material.hh"
#include "BKE_pointcloud.hh"

#include "GPU_batch.hh"
#include "GPU_material.hh"

#include "DRW_render.hh"

#include "draw_attributes.hh"
#include "draw_cache_impl.hh"
#include "draw_cache_inline.hh"
#include "draw_pointcloud_private.hh" /* own include */

namespace blender::draw {

/* -------------------------------------------------------------------- */
/** \name gpu::Batch cache management
 * \{ */

struct PointCloudEvalCache {
  /* Dot primitive types. */
  gpu::Batch *dots;
  /* Triangle primitive types. */
  gpu::Batch *surface;
  gpu::Batch **surface_per_mat;

  /* Triangles indices to draw the points. */
  gpu::IndexBuf *geom_indices;

  /* Position and radius. */
  gpu::VertBuf *pos_rad;
  /* Active attribute in 3D view. */
  gpu::VertBuf *attr_viewer;
  /* Requested attributes */
  gpu::VertBuf *attributes_buf[GPU_MAX_ATTR];

  /** Attributes currently being drawn or about to be drawn. */
  VectorSet<std::string> attr_used;
  /**
   * Attributes that were used at some point. This is used for garbage collection, to remove
   * attributes that are not used in shaders anymore due to user edits.
   */
  VectorSet<std::string> attr_used_over_time;

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

  gpu::IndexBuf *edit_selection_indices = nullptr;
  gpu::Batch *edit_selection = nullptr;

  /* settings to determine if cache is invalid */
  bool is_dirty;
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
  if (cache->eval_cache.mat_len != BKE_id_material_used_with_fallback_eval(pointcloud.id)) {
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
    cache->edit_selection = nullptr;
    cache->edit_selection_indices = nullptr;
  }

  cache->eval_cache.mat_len = BKE_id_material_used_with_fallback_eval(pointcloud.id);
  cache->eval_cache.surface_per_mat = static_cast<gpu::Batch **>(
      MEM_callocN(sizeof(gpu::Batch *) * cache->eval_cache.mat_len, __func__));

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

  cache.eval_cache.attr_used.clear();
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

  GPU_INDEXBUF_DISCARD_SAFE(cache->edit_selection_indices);
  GPU_BATCH_DISCARD_SAFE(cache->edit_selection);

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
  MEM_delete(static_cast<PointCloudBatchCache *>(pointcloud->batch_cache));
  pointcloud->batch_cache = nullptr;
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

  cache->eval_cache.attr_used_over_time.clear();

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
  /* Overlap shape and point indices to avoid both having to store the indices into a separate
   * buffer and avoid rendering points as instances. */
  uint32_t vertid_max = pointcloud.totpoint << 3;
  constexpr uint32_t tri_count_per_point = ARRAY_SIZE(half_octahedron_tris);
  uint32_t primitive_len = pointcloud.totpoint * tri_count_per_point;

  GPUIndexBufBuilder builder;

  /* Max allowed points to ensure the size of the index buffer will not overflow.
   * NOTE: pointcloud.totpoint is an int we assume that we can safely use 31 bits. */
  const uint32_t max_totpoint = INT32_MAX / uint32_t(tri_count_per_point *
                                                     GPU_indexbuf_primitive_len(GPU_PRIM_TRIS));
  if (pointcloud.totpoint > max_totpoint) {
    GPU_indexbuf_init(&builder, GPU_PRIM_TRIS, 0, 0);
    GPU_indexbuf_build_in_place_ex(&builder, 0, 0, false, cache.eval_cache.geom_indices);
    return;
  }

  GPU_indexbuf_init(&builder, GPU_PRIM_TRIS, primitive_len, vertid_max);
  MutableSpan<uint3> data = GPU_indexbuf_get_data(&builder).cast<uint3>();

  /* TODO(fclem): Could be build on GPU or not be built at all. */
  threading::parallel_for(IndexRange(pointcloud.totpoint), 1024, [&](const IndexRange range) {
    for (int p : range) {
      for (int i : IndexRange(tri_count_per_point)) {
        data[p * tri_count_per_point + i] = uint3(half_octahedron_tris[i]) | (p << 3);
      }
    }
  });

  GPU_indexbuf_build_in_place_ex(
      &builder, 0, primitive_len * 3, false, cache.eval_cache.geom_indices);
}

static void pointcloud_extract_position_and_radius(const PointCloud &pointcloud,
                                                   PointCloudBatchCache &cache)
{
  const bke::AttributeAccessor attributes = pointcloud.attributes();
  const Span<float3> positions = pointcloud.positions();
  const VArray<float> radii = *attributes.lookup<float>("radius");
  static const GPUVertFormat format = [&]() {
    GPUVertFormat format{};
    GPU_vertformat_attr_add(&format, "pos", gpu::VertAttrType::SFLOAT_32_32_32_32);
    GPU_vertformat_alias_add(&format, "pos_rad");
    return format;
  }();

  GPUUsageType usage_flag = GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY;
  GPU_vertbuf_init_with_format_ex(*cache.eval_cache.pos_rad, format, usage_flag);

  GPU_vertbuf_data_alloc(*cache.eval_cache.pos_rad, positions.size());
  MutableSpan<float4> vbo_data = cache.eval_cache.pos_rad->data<float4>();
  if (radii) {
    const VArraySpan<float> radii_span(std::move(radii));
    threading::parallel_for(vbo_data.index_range(), 4096, [&](IndexRange range) {
      for (const int i : range) {
        vbo_data[i].x = positions[i].x;
        vbo_data[i].y = positions[i].y;
        vbo_data[i].z = positions[i].z;
        vbo_data[i].w = radii_span[i];
      }
    });
  }
  else {
    threading::parallel_for(vbo_data.index_range(), 4096, [&](IndexRange range) {
      for (const int i : range) {
        vbo_data[i].x = positions[i].x;
        vbo_data[i].y = positions[i].y;
        vbo_data[i].z = positions[i].z;
        vbo_data[i].w = 0.01f;
      }
    });
  }
}

static void pointcloud_extract_attribute(const PointCloud &pointcloud,
                                         PointCloudBatchCache &cache,
                                         const StringRef name,
                                         int index)
{
  gpu::VertBuf &attr_buf = *cache.eval_cache.attributes_buf[index];

  const bke::AttributeAccessor attributes = pointcloud.attributes();

  /* TODO(@kevindietrich): float4 is used for scalar attributes as the implicit conversion done
   * by OpenGL to float4 for a scalar `s` will produce a `float4(s, 0, 0, 1)`. However, following
   * the Blender convention, it should be `float4(s, s, s, 1)`. This could be resolved using a
   * similar texture state swizzle to map the attribute correctly as for volume attributes, so we
   * can control the conversion ourselves. */
  bke::AttributeReader<ColorGeometry4f> attribute = attributes.lookup_or_default<ColorGeometry4f>(
      name, bke::AttrDomain::Point, {0.0f, 0.0f, 0.0f, 1.0f});

  static const GPUVertFormat format = [&]() {
    GPUVertFormat format{};
    GPU_vertformat_attr_add(&format, "attr", gpu::VertAttrType::SFLOAT_32_32_32_32);
    return format;
  }();
  GPUUsageType usage_flag = GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY;
  GPU_vertbuf_init_with_format_ex(attr_buf, format, usage_flag);
  GPU_vertbuf_data_alloc(attr_buf, pointcloud.totpoint);

  attribute.varray.materialize(attr_buf.data<ColorGeometry4f>());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Private API
 * \{ */

gpu::VertBuf *pointcloud_position_and_radius_get(PointCloud *pointcloud)
{
  PointCloudBatchCache *cache = pointcloud_batch_cache_get(*pointcloud);
  DRW_vbo_request(nullptr, &cache->eval_cache.pos_rad);
  return cache->eval_cache.pos_rad;
}

gpu::Batch **pointcloud_surface_shaded_get(PointCloud *pointcloud,
                                           GPUMaterial **gpu_materials,
                                           int mat_len)
{
  const bke::AttributeAccessor attributes = pointcloud->attributes();
  PointCloudBatchCache *cache = pointcloud_batch_cache_get(*pointcloud);
  VectorSet<std::string> attrs_needed;

  for (GPUMaterial *gpu_material : Span<GPUMaterial *>(gpu_materials, mat_len)) {
    ListBase gpu_attrs = GPU_material_attributes(gpu_material);
    LISTBASE_FOREACH (GPUMaterialAttribute *, gpu_attr, &gpu_attrs) {
      const StringRef name = gpu_attr->name;
      if (!attributes.contains(name)) {
        continue;
      }
      drw_attributes_add_request(&attrs_needed, name);
    }
  }

  if (!drw_attributes_overlap(&cache->eval_cache.attr_used, &attrs_needed)) {
    /* Some new attributes have been added, free all and start over. */
    for (const int i : IndexRange(GPU_MAX_ATTR)) {
      GPU_VERTBUF_DISCARD_SAFE(cache->eval_cache.attributes_buf[i]);
    }
    drw_attributes_merge(&cache->eval_cache.attr_used, &attrs_needed);
  }
  drw_attributes_merge(&cache->eval_cache.attr_used_over_time, &attrs_needed);

  DRW_batch_request(&cache->eval_cache.surface_per_mat[0]);
  return cache->eval_cache.surface_per_mat;
}

gpu::Batch *pointcloud_surface_get(PointCloud *pointcloud)
{
  PointCloudBatchCache *cache = pointcloud_batch_cache_get(*pointcloud);
  return DRW_batch_request(&cache->eval_cache.surface);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name API
 * \{ */

gpu::Batch *DRW_pointcloud_batch_cache_get_dots(Object *ob)
{
  PointCloud &pointcloud = DRW_object_get_data_for_drawing<PointCloud>(*ob);
  PointCloudBatchCache *cache = pointcloud_batch_cache_get(pointcloud);
  return DRW_batch_request(&cache->eval_cache.dots);
}

gpu::VertBuf *DRW_pointcloud_position_and_radius_buffer_get(Object *ob)
{
  PointCloud &pointcloud = DRW_object_get_data_for_drawing<PointCloud>(*ob);
  return pointcloud_position_and_radius_get(&pointcloud);
}

gpu::VertBuf **DRW_pointcloud_evaluated_attribute(PointCloud *pointcloud, const StringRef name)
{
  const bke::AttributeAccessor attributes = pointcloud->attributes();
  PointCloudBatchCache &cache = *pointcloud_batch_cache_get(*pointcloud);

  if (!attributes.contains(name)) {
    return nullptr;
  }
  {
    VectorSet<std::string> requests{};
    drw_attributes_add_request(&requests, name);
    drw_attributes_merge(&cache.eval_cache.attr_used, &requests);
  }

  int request_i = -1;
  for (const int i : IndexRange(cache.eval_cache.attr_used.index_range())) {
    if (cache.eval_cache.attr_used[i] == name) {
      request_i = i;
      break;
    }
  }
  if (request_i == -1) {
    return nullptr;
  }
  return &cache.eval_cache.attributes_buf[request_i];
}

static void index_mask_to_ibo(const IndexMask &mask, gpu::IndexBuf &ibo)
{
  const int max_index = mask.min_array_size();
  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_POINTS, mask.size(), max_index);
  MutableSpan<uint> data = GPU_indexbuf_get_data(&builder);
  mask.to_indices<int>(data.cast<int>());
  GPU_indexbuf_build_in_place_ex(&builder, 0, max_index, false, &ibo);
}

static void build_edit_selection_indices(const PointCloud &pointcloud, gpu::IndexBuf &ibo)
{
  const VArray selection = *pointcloud.attributes().lookup_or_default<bool>(
      ".selection", bke::AttrDomain::Point, true);
  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_bools(selection, memory);
  if (mask.is_empty()) {
    return;
  }
  index_mask_to_ibo(mask, ibo);
}

void DRW_pointcloud_batch_cache_create_requested(Object *ob)
{
  PointCloud &pointcloud = DRW_object_get_data_for_drawing<PointCloud>(*ob);
  PointCloudBatchCache &cache = *pointcloud_batch_cache_get(pointcloud);

  if (DRW_batch_requested(cache.eval_cache.dots, GPU_PRIM_POINTS)) {
    DRW_vbo_request(cache.eval_cache.dots, &cache.eval_cache.pos_rad);
  }

  if (DRW_batch_requested(cache.edit_selection, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache.edit_selection, &cache.edit_selection_indices);
    DRW_vbo_request(cache.edit_selection, &cache.eval_cache.pos_rad);
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
  for (const int j : cache.eval_cache.attr_used.index_range()) {
    DRW_vbo_request(nullptr, &cache.eval_cache.attributes_buf[j]);

    if (DRW_vbo_requested(cache.eval_cache.attributes_buf[j])) {
      pointcloud_extract_attribute(pointcloud, cache, cache.eval_cache.attr_used[j], j);
    }
  }

  if (DRW_ibo_requested(cache.edit_selection_indices)) {
    build_edit_selection_indices(pointcloud, *cache.edit_selection_indices);
  }

  if (DRW_ibo_requested(cache.eval_cache.geom_indices)) {
    pointcloud_extract_indices(pointcloud, cache);
  }

  if (DRW_vbo_requested(cache.eval_cache.pos_rad)) {
    pointcloud_extract_position_and_radius(pointcloud, cache);
  }
}

gpu::Batch *DRW_pointcloud_batch_cache_get_edit_dots(PointCloud *pointcloud)
{
  PointCloudBatchCache *cache = pointcloud_batch_cache_get(*pointcloud);
  return DRW_batch_request(&cache->edit_selection);
}

/** \} */

}  // namespace blender::draw
