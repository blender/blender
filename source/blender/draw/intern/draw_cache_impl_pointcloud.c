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
 * \brief PointCloud API for render engines
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_math_base.h"
#include "BLI_utildefines.h"

#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"

#include "BKE_pointcloud.h"

#include "GPU_batch.h"

#include "draw_cache_impl.h" /* own include */

static void pointcloud_batch_cache_clear(PointCloud *pointcloud);

/* ---------------------------------------------------------------------- */
/* PointCloud GPUBatch Cache */

typedef struct PointCloudBatchCache {
  GPUVertBuf *pos;
  GPUBatch *batch;

  /* settings to determine if cache is invalid */
  bool is_dirty;
} PointCloudBatchCache;

/* GPUBatch cache management. */

static bool pointcloud_batch_cache_valid(PointCloud *pointcloud)
{
  PointCloudBatchCache *cache = pointcloud->batch_cache;
  return (cache && cache->is_dirty == false);
}

static void pointcloud_batch_cache_init(PointCloud *pointcloud)
{
  PointCloudBatchCache *cache = pointcloud->batch_cache;

  if (!cache) {
    cache = pointcloud->batch_cache = MEM_callocN(sizeof(*cache), __func__);
  }
  else {
    memset(cache, 0, sizeof(*cache));
  }

  cache->is_dirty = false;
}

void DRW_pointcloud_batch_cache_validate(PointCloud *pointcloud)
{
  if (!pointcloud_batch_cache_valid(pointcloud)) {
    pointcloud_batch_cache_clear(pointcloud);
    pointcloud_batch_cache_init(pointcloud);
  }
}

static PointCloudBatchCache *pointcloud_batch_cache_get(PointCloud *pointcloud)
{
  return pointcloud->batch_cache;
}

void DRW_pointcloud_batch_cache_dirty_tag(PointCloud *pointcloud, int mode)
{
  PointCloudBatchCache *cache = pointcloud->batch_cache;
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

static void pointcloud_batch_cache_clear(PointCloud *pointcloud)
{
  PointCloudBatchCache *cache = pointcloud->batch_cache;
  if (!cache) {
    return;
  }

  GPU_BATCH_DISCARD_SAFE(cache->batch);
  GPU_VERTBUF_DISCARD_SAFE(cache->pos);
}

void DRW_pointcloud_batch_cache_free(PointCloud *pointcloud)
{
  pointcloud_batch_cache_clear(pointcloud);
  MEM_SAFE_FREE(pointcloud->batch_cache);
}

static void pointcloud_batch_cache_ensure_pos(Object *ob, PointCloudBatchCache *cache)
{
  if (cache->pos != NULL) {
    return;
  }

  PointCloud *pointcloud = ob->data;

  static GPUVertFormat format = {0};
  static uint pos_id;
  static uint radius_id;
  if (format.attr_len == 0) {
    /* initialize vertex format */
    pos_id = GPU_vertformat_attr_add(&format, "pointcloud_pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    radius_id = GPU_vertformat_attr_add(
        &format, "pointcloud_radius", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  }

  GPU_VERTBUF_DISCARD_SAFE(cache->pos);
  cache->pos = GPU_vertbuf_create_with_format(&format);
  GPU_vertbuf_data_alloc(cache->pos, pointcloud->totpoint);
  GPU_vertbuf_attr_fill(cache->pos, pos_id, pointcloud->co);

  if (pointcloud->radius) {
    GPU_vertbuf_attr_fill(cache->pos, radius_id, pointcloud->radius);
  }
  else if (pointcloud->totpoint) {
    /* TODO: optimize for constant radius by not including in vertex buffer at all? */
    float *radius = MEM_malloc_arrayN(pointcloud->totpoint, sizeof(float), __func__);
    for (int i = 0; i < pointcloud->totpoint; i++) {
      /* TODO: add default radius to PointCloud data structure. */
      radius[i] = 0.01f;
    }
    GPU_vertbuf_attr_fill(cache->pos, radius_id, radius);
    MEM_freeN(radius);
  }
}

GPUBatch *DRW_pointcloud_batch_cache_get_dots(Object *ob)
{
  PointCloud *pointcloud = ob->data;
  PointCloudBatchCache *cache = pointcloud_batch_cache_get(pointcloud);

  if (cache->batch == NULL) {
    pointcloud_batch_cache_ensure_pos(ob, cache);
    cache->batch = GPU_batch_create(GPU_PRIM_POINTS, cache->pos, NULL);
  }

  return cache->batch;
}

int DRW_pointcloud_material_count_get(PointCloud *pointcloud)
{
  return max_ii(1, pointcloud->totcol);
}
