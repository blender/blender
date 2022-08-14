/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2017 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup draw
 *
 * \brief PointCloud API for render engines
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_math_base.h"
#include "BLI_math_vector.h"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"

#include "BKE_attribute.hh"
#include "BKE_pointcloud.h"

#include "GPU_batch.h"

#include "draw_cache_impl.h" /* own include */

/* ---------------------------------------------------------------------- */
/* PointCloud GPUBatch Cache */

struct PointCloudBatchCache {
  GPUVertBuf *pos;  /* Position and radius. */
  GPUVertBuf *geom; /* Instanced geometry for each point in the cloud (small sphere). */
  GPUIndexBuf *geom_indices;

  GPUBatch *dots;
  GPUBatch *surface;
  GPUBatch **surface_per_mat;

  /* settings to determine if cache is invalid */
  bool is_dirty;

  int mat_len;
};

/* GPUBatch cache management. */

static PointCloudBatchCache *pointcloud_batch_cache_get(PointCloud &pointcloud)
{
  return static_cast<PointCloudBatchCache *>(pointcloud.batch_cache);
}

static bool pointcloud_batch_cache_valid(PointCloud &pointcloud)
{
  PointCloudBatchCache *cache = pointcloud_batch_cache_get(pointcloud);

  if (cache == NULL) {
    return false;
  }
  if (cache->mat_len != DRW_pointcloud_material_count_get(&pointcloud)) {
    return false;
  }
  return cache->is_dirty == false;
}

static void pointcloud_batch_cache_init(PointCloud &pointcloud)
{
  PointCloudBatchCache *cache = pointcloud_batch_cache_get(pointcloud);

  if (!cache) {
    cache = MEM_cnew<PointCloudBatchCache>(__func__);
    pointcloud.batch_cache = cache;
  }
  else {
    memset(cache, 0, sizeof(*cache));
  }

  cache->mat_len = DRW_pointcloud_material_count_get(&pointcloud);
  cache->surface_per_mat = static_cast<GPUBatch **>(
      MEM_callocN(sizeof(GPUBatch *) * cache->mat_len, __func__));

  cache->is_dirty = false;
}

void DRW_pointcloud_batch_cache_dirty_tag(PointCloud *pointcloud, int mode)
{
  PointCloudBatchCache *cache = pointcloud_batch_cache_get(*pointcloud);
  if (cache == NULL) {
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

static void pointcloud_batch_cache_clear(PointCloud &pointcloud)
{
  PointCloudBatchCache *cache = pointcloud_batch_cache_get(pointcloud);
  if (!cache) {
    return;
  }

  GPU_BATCH_DISCARD_SAFE(cache->dots);
  GPU_BATCH_DISCARD_SAFE(cache->surface);
  GPU_VERTBUF_DISCARD_SAFE(cache->pos);
  GPU_VERTBUF_DISCARD_SAFE(cache->geom);
  GPU_INDEXBUF_DISCARD_SAFE(cache->geom_indices);

  if (cache->surface_per_mat) {
    for (int i = 0; i < cache->mat_len; i++) {
      GPU_BATCH_DISCARD_SAFE(cache->surface_per_mat[i]);
    }
  }
  MEM_SAFE_FREE(cache->surface_per_mat);
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

static void pointcloud_batch_cache_ensure_pos(const PointCloud &pointcloud,
                                              PointCloudBatchCache &cache)
{
  using namespace blender;
  if (cache.pos != NULL) {
    return;
  }

  const bke::AttributeAccessor attributes = bke::pointcloud_attributes(pointcloud);
  const VArraySpan<float3> positions = attributes.lookup<float3>("position", ATTR_DOMAIN_POINT);
  const VArray<float> radii = attributes.lookup<float>("radius", ATTR_DOMAIN_POINT);
  /* From the opengl wiki:
   * Note that size does not have to exactly match the size used by the vertex shader. If the
   * vertex shader has fewer components than the attribute provides, then the extras are ignored.
   * If the vertex shader has more components than the array provides, the extras are given
   * values from the vector (0, 0, 0, 1) for the missing XYZW components. */
  if (radii) {
    static GPUVertFormat format = {0};
    if (format.attr_len == 0) {
      GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    }
    cache.pos = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(cache.pos, positions.size());
    const VArraySpan<float> radii_span(radii);
    MutableSpan<float4> vbo_data{static_cast<float4 *>(GPU_vertbuf_get_data(cache.pos)),
                                 pointcloud.totpoint};
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
    static GPUVertFormat format = {0};
    static uint pos;
    if (format.attr_len == 0) {
      pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    }
    cache.pos = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(cache.pos, positions.size());
    GPU_vertbuf_attr_fill(cache.pos, pos, positions.data());
  }
}

static const float half_octahedron_normals[5][3] = {
    {0.0f, 0.0f, 1.0f},
    {1.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {-1.0f, 0.0f, 0.0f},
    {0.0f, -1.0f, 0.0f},
};

static const uint half_octahedron_tris[4][3] = {
    {0, 1, 2},
    {0, 2, 3},
    {0, 3, 4},
    {0, 4, 1},
};

static void pointcloud_batch_cache_ensure_geom(PointCloudBatchCache &cache)
{
  if (cache.geom != NULL) {
    return;
  }

  static GPUVertFormat format = {0};
  static uint pos;
  if (format.attr_len == 0) {
    pos = GPU_vertformat_attr_add(&format, "pos_inst", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    GPU_vertformat_alias_add(&format, "nor");
  }

  cache.geom = GPU_vertbuf_create_with_format(&format);
  GPU_vertbuf_data_alloc(cache.geom, ARRAY_SIZE(half_octahedron_normals));

  GPU_vertbuf_attr_fill(cache.geom, pos, half_octahedron_normals);

  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder,
                    GPU_PRIM_TRIS,
                    ARRAY_SIZE(half_octahedron_tris),
                    ARRAY_SIZE(half_octahedron_normals));

  for (int i = 0; i < ARRAY_SIZE(half_octahedron_tris); i++) {
    GPU_indexbuf_add_tri_verts(&builder, UNPACK3(half_octahedron_tris[i]));
  }

  cache.geom_indices = GPU_indexbuf_build(&builder);
}

GPUBatch *DRW_pointcloud_batch_cache_get_dots(Object *ob)
{
  PointCloud &pointcloud = *static_cast<PointCloud *>(ob->data);
  PointCloudBatchCache *cache = pointcloud_batch_cache_get(pointcloud);

  if (cache->dots == NULL) {
    pointcloud_batch_cache_ensure_pos(pointcloud, *cache);
    cache->dots = GPU_batch_create(GPU_PRIM_POINTS, cache->pos, NULL);
  }

  return cache->dots;
}

GPUBatch *DRW_pointcloud_batch_cache_get_surface(Object *ob)
{
  PointCloud &pointcloud = *static_cast<PointCloud *>(ob->data);
  PointCloudBatchCache *cache = pointcloud_batch_cache_get(pointcloud);

  if (cache->surface == NULL) {
    pointcloud_batch_cache_ensure_pos(pointcloud, *cache);
    pointcloud_batch_cache_ensure_geom(*cache);

    cache->surface = GPU_batch_create(GPU_PRIM_TRIS, cache->geom, cache->geom_indices);
    GPU_batch_instbuf_add_ex(cache->surface, cache->pos, false);
  }

  return cache->surface;
}

GPUBatch **DRW_cache_pointcloud_surface_shaded_get(Object *ob,
                                                   struct GPUMaterial **UNUSED(gpumat_array),
                                                   uint gpumat_array_len)
{
  PointCloud &pointcloud = *static_cast<PointCloud *>(ob->data);
  PointCloudBatchCache *cache = pointcloud_batch_cache_get(pointcloud);
  BLI_assert(cache->mat_len == gpumat_array_len);
  UNUSED_VARS(gpumat_array_len);

  if (cache->surface_per_mat[0] == NULL) {
    pointcloud_batch_cache_ensure_pos(pointcloud, *cache);
    pointcloud_batch_cache_ensure_geom(*cache);

    cache->surface_per_mat[0] = GPU_batch_create(GPU_PRIM_TRIS, cache->geom, cache->geom_indices);
    GPU_batch_instbuf_add_ex(cache->surface_per_mat[0], cache->pos, false);
  }

  return cache->surface_per_mat;
}

int DRW_pointcloud_material_count_get(PointCloud *pointcloud)
{
  return max_ii(1, pointcloud->totcol);
}
