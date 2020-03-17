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
 * \brief Volume API for render engines
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_math_base.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "DNA_object_types.h"
#include "DNA_volume_types.h"

#include "BKE_global.h"
#include "BKE_volume.h"
#include "BKE_volume_render.h"

#include "GPU_batch.h"
#include "GPU_texture.h"

#include "DRW_render.h"

#include "draw_cache.h"      /* own include */
#include "draw_cache_impl.h" /* own include */

static void volume_batch_cache_clear(Volume *volume);

/* ---------------------------------------------------------------------- */
/* Volume GPUBatch Cache */

typedef struct VolumeBatchCache {
  /* 3D textures */
  ListBase grids;

  /* Wireframe */
  struct {
    GPUVertBuf *pos_nor_in_order;
    GPUBatch *batch;
  } face_wire;

  /* settings to determine if cache is invalid */
  bool is_dirty;
} VolumeBatchCache;

/* GPUBatch cache management. */

static bool volume_batch_cache_valid(Volume *volume)
{
  VolumeBatchCache *cache = volume->batch_cache;
  return (cache && cache->is_dirty == false);
}

static void volume_batch_cache_init(Volume *volume)
{
  VolumeBatchCache *cache = volume->batch_cache;

  if (!cache) {
    cache = volume->batch_cache = MEM_callocN(sizeof(*cache), __func__);
  }
  else {
    memset(cache, 0, sizeof(*cache));
  }

  cache->is_dirty = false;
}

void DRW_volume_batch_cache_validate(Volume *volume)
{
  if (!volume_batch_cache_valid(volume)) {
    volume_batch_cache_clear(volume);
    volume_batch_cache_init(volume);
  }
}

static VolumeBatchCache *volume_batch_cache_get(Volume *volume)
{
  DRW_volume_batch_cache_validate(volume);
  return volume->batch_cache;
}

void DRW_volume_batch_cache_dirty_tag(Volume *volume, int mode)
{
  VolumeBatchCache *cache = volume->batch_cache;
  if (cache == NULL) {
    return;
  }
  switch (mode) {
    case BKE_VOLUME_BATCH_DIRTY_ALL:
      cache->is_dirty = true;
      break;
    default:
      BLI_assert(0);
  }
}

static void volume_batch_cache_clear(Volume *volume)
{
  VolumeBatchCache *cache = volume->batch_cache;
  if (!cache) {
    return;
  }

  for (DRWVolumeGrid *grid = cache->grids.first; grid; grid = grid->next) {
    MEM_SAFE_FREE(grid->name);
    DRW_TEXTURE_FREE_SAFE(grid->texture);
  }
  BLI_freelistN(&cache->grids);

  GPU_VERTBUF_DISCARD_SAFE(cache->face_wire.pos_nor_in_order);
  GPU_BATCH_DISCARD_SAFE(cache->face_wire.batch);
}

void DRW_volume_batch_cache_free(Volume *volume)
{
  volume_batch_cache_clear(volume);
  MEM_SAFE_FREE(volume->batch_cache);
}

static void drw_volume_wireframe_cb(
    void *userdata, float (*verts)[3], int (*edges)[2], int totvert, int totedge)
{
  Volume *volume = userdata;
  VolumeBatchCache *cache = volume->batch_cache;

  /* Create vertex buffer. */
  static GPUVertFormat format = {0};
  static uint pos_id, nor_id;
  if (format.attr_len == 0) {
    pos_id = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    nor_id = GPU_vertformat_attr_add(&format, "nor", GPU_COMP_I10, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }

  static float normal[3] = {1.0f, 0.0f, 0.0f};
  GPUPackedNormal packed_normal = GPU_normal_convert_i10_v3(normal);

  cache->face_wire.pos_nor_in_order = GPU_vertbuf_create_with_format(&format);
  GPU_vertbuf_data_alloc(cache->face_wire.pos_nor_in_order, totvert);
  GPU_vertbuf_attr_fill(cache->face_wire.pos_nor_in_order, pos_id, verts);
  GPU_vertbuf_attr_fill_stride(cache->face_wire.pos_nor_in_order, nor_id, 0, &packed_normal);

  /* Create wiredata. */
  GPUVertBuf *vbo_wiredata = MEM_callocN(sizeof(GPUVertBuf), __func__);
  DRW_vertbuf_create_wiredata(vbo_wiredata, totvert);

  if (volume->display.wireframe_type == VOLUME_WIREFRAME_POINTS) {
    /* Create batch. */
    cache->face_wire.batch = GPU_batch_create(
        GPU_PRIM_POINTS, cache->face_wire.pos_nor_in_order, NULL);
  }
  else {
    /* Create edge index buffer. */
    GPUIndexBufBuilder elb;
    GPU_indexbuf_init(&elb, GPU_PRIM_LINES, totedge, totvert);
    for (int i = 0; i < totedge; i++) {
      GPU_indexbuf_add_line_verts(&elb, edges[i][0], edges[i][1]);
    }
    GPUIndexBuf *ibo = GPU_indexbuf_build(&elb);

    /* Create batch. */
    cache->face_wire.batch = GPU_batch_create_ex(
        GPU_PRIM_LINES, cache->face_wire.pos_nor_in_order, ibo, GPU_BATCH_OWNS_INDEX);
  }

  GPU_batch_vertbuf_add_ex(cache->face_wire.batch, vbo_wiredata, true);
}

GPUBatch *DRW_volume_batch_cache_get_wireframes_face(Volume *volume)
{
  if (volume->display.wireframe_type == VOLUME_WIREFRAME_NONE) {
    return NULL;
  }

  VolumeBatchCache *cache = volume_batch_cache_get(volume);

  if (cache->face_wire.batch == NULL) {
    VolumeGrid *volume_grid = BKE_volume_grid_active_get(volume);
    if (volume_grid == NULL) {
      return NULL;
    }

    /* Create wireframe from OpenVDB tree. */
    BKE_volume_grid_wireframe(volume, volume_grid, drw_volume_wireframe_cb, volume);
  }

  return cache->face_wire.batch;
}

static DRWVolumeGrid *volume_grid_cache_get(Volume *volume,
                                            VolumeGrid *grid,
                                            VolumeBatchCache *cache)
{
  const char *name = BKE_volume_grid_name(grid);

  /* Return cached grid. */
  DRWVolumeGrid *cache_grid;
  for (cache_grid = cache->grids.first; cache_grid; cache_grid = cache_grid->next) {
    if (STREQ(cache_grid->name, name)) {
      return cache_grid;
    }
  }

  /* Allocate new grid. */
  cache_grid = MEM_callocN(sizeof(DRWVolumeGrid), __func__);
  cache_grid->name = BLI_strdup(name);
  BLI_addtail(&cache->grids, cache_grid);

  /* TODO: can we load this earlier, avoid accessing the global and take
   * advantage of dependency graph multithreading? */
  BKE_volume_load(volume, G.main);

  /* Test if we support textures with the number of channels. */
  size_t channels = BKE_volume_grid_channels(grid);
  if (!ELEM(channels, 1, 3)) {
    return cache_grid;
  }

  /* Load grid tree into memory, if not loaded already. */
  const bool was_loaded = BKE_volume_grid_is_loaded(grid);
  BKE_volume_grid_load(volume, grid);

  /* Compute dense voxel grid size. */
  int64_t dense_min[3], dense_max[3], resolution[3] = {0};
  if (BKE_volume_grid_dense_bounds(volume, grid, dense_min, dense_max)) {
    resolution[0] = dense_max[0] - dense_min[0];
    resolution[1] = dense_max[1] - dense_min[1];
    resolution[2] = dense_max[2] - dense_min[2];
  }
  size_t num_voxels = resolution[0] * resolution[1] * resolution[2];
  size_t elem_size = sizeof(float) * channels;

  /* Allocate and load voxels. */
  float *voxels = (num_voxels > 0) ? MEM_malloc_arrayN(num_voxels, elem_size, __func__) : NULL;
  if (voxels != NULL) {
    BKE_volume_grid_dense_voxels(volume, grid, dense_min, dense_max, voxels);

    /* Create GPU texture. */
    cache_grid->texture = GPU_texture_create_3d(resolution[0],
                                                resolution[1],
                                                resolution[2],
                                                (channels == 3) ? GPU_RGB16F : GPU_R16F,
                                                voxels,
                                                NULL);

    GPU_texture_bind(cache_grid->texture, 0);
    GPU_texture_swizzle_channel_auto(cache_grid->texture, channels);
    GPU_texture_unbind(cache_grid->texture);

    MEM_freeN(voxels);

    /* Compute transform matrices. */
    BKE_volume_grid_dense_transform_matrix(
        grid, dense_min, dense_max, cache_grid->texture_to_object);
    invert_m4_m4(cache_grid->object_to_texture, cache_grid->texture_to_object);
  }

  /* Free grid from memory if it wasn't previously loaded. */
  if (!was_loaded) {
    BKE_volume_grid_unload(volume, grid);
  }

  return cache_grid;
}

DRWVolumeGrid *DRW_volume_batch_cache_get_grid(Volume *volume, VolumeGrid *volume_grid)
{
  VolumeBatchCache *cache = volume_batch_cache_get(volume);
  DRWVolumeGrid *grid = volume_grid_cache_get(volume, volume_grid, cache);
  return (grid->texture != NULL) ? grid : NULL;
}

int DRW_volume_material_count_get(Volume *volume)
{
  return max_ii(1, volume->totcol);
}
