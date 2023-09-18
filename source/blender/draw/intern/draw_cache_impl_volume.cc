/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Volume API for render engines
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "DNA_object_types.h"
#include "DNA_volume_types.h"

#include "BKE_global.h"
#include "BKE_volume.h"
#include "BKE_volume_render.h"

#include "GPU_batch.h"
#include "GPU_capabilities.h"
#include "GPU_texture.h"

#include "DEG_depsgraph_query.h"

#include "DRW_render.h"

#include "draw_cache.h"       /* own include */
#include "draw_cache_impl.hh" /* own include */

static void volume_batch_cache_clear(Volume *volume);

/* ---------------------------------------------------------------------- */
/* Volume GPUBatch Cache */

struct VolumeBatchCache {
  /* 3D textures */
  ListBase grids;

  /* Wireframe */
  struct {
    GPUVertBuf *pos_nor_in_order;
    GPUBatch *batch;
  } face_wire;

  /* Surface for selection */
  GPUBatch *selection_surface;

  /* settings to determine if cache is invalid */
  bool is_dirty;
};

/* GPUBatch cache management. */

static bool volume_batch_cache_valid(Volume *volume)
{
  VolumeBatchCache *cache = static_cast<VolumeBatchCache *>(volume->batch_cache);
  return (cache && cache->is_dirty == false);
}

static void volume_batch_cache_init(Volume *volume)
{
  VolumeBatchCache *cache = static_cast<VolumeBatchCache *>(volume->batch_cache);

  if (!cache) {
    volume->batch_cache = cache = MEM_cnew<VolumeBatchCache>(__func__);
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
  return static_cast<VolumeBatchCache *>(volume->batch_cache);
}

void DRW_volume_batch_cache_dirty_tag(Volume *volume, int mode)
{
  VolumeBatchCache *cache = static_cast<VolumeBatchCache *>(volume->batch_cache);
  if (cache == nullptr) {
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
  VolumeBatchCache *cache = static_cast<VolumeBatchCache *>(volume->batch_cache);
  if (!cache) {
    return;
  }

  LISTBASE_FOREACH (DRWVolumeGrid *, grid, &cache->grids) {
    MEM_SAFE_FREE(grid->name);
    DRW_TEXTURE_FREE_SAFE(grid->texture);
  }
  BLI_freelistN(&cache->grids);

  GPU_VERTBUF_DISCARD_SAFE(cache->face_wire.pos_nor_in_order);
  GPU_BATCH_DISCARD_SAFE(cache->face_wire.batch);
  GPU_BATCH_DISCARD_SAFE(cache->selection_surface);
}

void DRW_volume_batch_cache_free(Volume *volume)
{
  volume_batch_cache_clear(volume);
  MEM_SAFE_FREE(volume->batch_cache);
}
struct VolumeWireframeUserData {
  Volume *volume;
  Scene *scene;
};

static void drw_volume_wireframe_cb(
    void *userdata, const float (*verts)[3], const int (*edges)[2], int totvert, int totedge)
{
  VolumeWireframeUserData *data = static_cast<VolumeWireframeUserData *>(userdata);
  Scene *scene = data->scene;
  Volume *volume = data->volume;
  VolumeBatchCache *cache = static_cast<VolumeBatchCache *>(volume->batch_cache);
  const bool do_hq_normals = (scene->r.perf_flag & SCE_PERF_HQ_NORMALS) != 0 ||
                             GPU_use_hq_normals_workaround();

  /* Create vertex buffer. */
  static GPUVertFormat format = {0};
  static GPUVertFormat format_hq = {0};
  static struct {
    uint pos_id, nor_id;
    uint pos_hq_id, nor_hq_id;
  } attr_id;

  if (format.attr_len == 0) {
    attr_id.pos_id = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    attr_id.nor_id = GPU_vertformat_attr_add(
        &format, "nor", GPU_COMP_I10, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
    attr_id.pos_id = GPU_vertformat_attr_add(&format_hq, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    attr_id.nor_id = GPU_vertformat_attr_add(
        &format_hq, "nor", GPU_COMP_I16, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }

  static float normal[3] = {1.0f, 0.0f, 0.0f};
  GPUNormal packed_normal;
  GPU_normal_convert_v3(&packed_normal, normal, do_hq_normals);
  uint pos_id = do_hq_normals ? attr_id.pos_hq_id : attr_id.pos_id;
  uint nor_id = do_hq_normals ? attr_id.nor_hq_id : attr_id.nor_id;

  cache->face_wire.pos_nor_in_order = GPU_vertbuf_create_with_format(do_hq_normals ? &format_hq :
                                                                                     &format);
  GPU_vertbuf_data_alloc(cache->face_wire.pos_nor_in_order, totvert);
  GPU_vertbuf_attr_fill(cache->face_wire.pos_nor_in_order, pos_id, verts);
  GPU_vertbuf_attr_fill_stride(cache->face_wire.pos_nor_in_order, nor_id, 0, &packed_normal);

  /* Create wiredata. */
  GPUVertBuf *vbo_wiredata = GPU_vertbuf_calloc();
  DRW_vertbuf_create_wiredata(vbo_wiredata, totvert);

  if (volume->display.wireframe_type == VOLUME_WIREFRAME_POINTS) {
    /* Create batch. */
    cache->face_wire.batch = GPU_batch_create(
        GPU_PRIM_POINTS, cache->face_wire.pos_nor_in_order, nullptr);
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

  GPU_batch_vertbuf_add(cache->face_wire.batch, vbo_wiredata, true);
}

GPUBatch *DRW_volume_batch_cache_get_wireframes_face(Volume *volume)
{
  if (volume->display.wireframe_type == VOLUME_WIREFRAME_NONE) {
    return nullptr;
  }

  VolumeBatchCache *cache = volume_batch_cache_get(volume);

  if (cache->face_wire.batch == nullptr) {
    const VolumeGrid *volume_grid = BKE_volume_grid_active_get_for_read(volume);
    if (volume_grid == nullptr) {
      return nullptr;
    }

    /* Create wireframe from OpenVDB tree. */
    const DRWContextState *draw_ctx = DRW_context_state_get();
    VolumeWireframeUserData userdata;
    userdata.volume = volume;
    userdata.scene = draw_ctx->scene;
    BKE_volume_grid_wireframe(volume, volume_grid, drw_volume_wireframe_cb, &userdata);
  }

  return cache->face_wire.batch;
}

static void drw_volume_selection_surface_cb(
    void *userdata, float (*verts)[3], int (*tris)[3], int totvert, int tottris)
{
  Volume *volume = static_cast<Volume *>(userdata);
  VolumeBatchCache *cache = static_cast<VolumeBatchCache *>(volume->batch_cache);

  static GPUVertFormat format = {0};
  static uint pos_id;
  if (format.attr_len == 0) {
    pos_id = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  }

  /* Create vertex buffer. */
  GPUVertBuf *vbo_surface = GPU_vertbuf_create_with_format(&format);
  GPU_vertbuf_data_alloc(vbo_surface, totvert);
  GPU_vertbuf_attr_fill(vbo_surface, pos_id, verts);

  /* Create index buffer. */
  GPUIndexBufBuilder elb;
  GPU_indexbuf_init(&elb, GPU_PRIM_TRIS, tottris, totvert);
  for (int i = 0; i < tottris; i++) {
    GPU_indexbuf_add_tri_verts(&elb, UNPACK3(tris[i]));
  }
  GPUIndexBuf *ibo_surface = GPU_indexbuf_build(&elb);

  cache->selection_surface = GPU_batch_create_ex(
      GPU_PRIM_TRIS, vbo_surface, ibo_surface, GPU_BATCH_OWNS_VBO | GPU_BATCH_OWNS_INDEX);
}

GPUBatch *DRW_volume_batch_cache_get_selection_surface(Volume *volume)
{
  VolumeBatchCache *cache = volume_batch_cache_get(volume);
  if (cache->selection_surface == nullptr) {
    const VolumeGrid *volume_grid = BKE_volume_grid_active_get_for_read(volume);
    if (volume_grid == nullptr) {
      return nullptr;
    }
    BKE_volume_grid_selection_surface(
        volume, volume_grid, drw_volume_selection_surface_cb, volume);
  }
  return cache->selection_surface;
}

static DRWVolumeGrid *volume_grid_cache_get(const Volume *volume,
                                            const VolumeGrid *grid,
                                            VolumeBatchCache *cache)
{
  const char *name = BKE_volume_grid_name(grid);

  /* Return cached grid. */
  LISTBASE_FOREACH (DRWVolumeGrid *, cache_grid, &cache->grids) {
    if (STREQ(cache_grid->name, name)) {
      return cache_grid;
    }
  }

  /* Allocate new grid. */
  DRWVolumeGrid *cache_grid = MEM_cnew<DRWVolumeGrid>(__func__);
  cache_grid->name = BLI_strdup(name);
  BLI_addtail(&cache->grids, cache_grid);

  /* TODO: can we load this earlier, avoid accessing the global and take
   * advantage of dependency graph multi-threading? */
  BKE_volume_load(volume, G.main);

  /* Test if we support textures with the number of channels. */
  size_t channels = BKE_volume_grid_channels(grid);
  if (!ELEM(channels, 1, 3)) {
    return cache_grid;
  }

  /* Remember if grid was loaded. If it was not, we want to unload it after the GPUTexture has been
   * created. */
  const bool was_loaded = BKE_volume_grid_is_loaded(grid);

  DenseFloatVolumeGrid dense_grid;
  if (BKE_volume_grid_dense_floats(volume, grid, &dense_grid)) {
    copy_m4_m4(cache_grid->texture_to_object, dense_grid.texture_to_object);
    invert_m4_m4(cache_grid->object_to_texture, dense_grid.texture_to_object);

    /* Create GPU texture. */
    eGPUTextureFormat format = (channels == 3) ? GPU_RGB16F : GPU_R16F;
    cache_grid->texture = GPU_texture_create_3d("volume_grid",
                                                UNPACK3(dense_grid.resolution),
                                                1,
                                                format,
                                                GPU_TEXTURE_USAGE_SHADER_READ |
                                                    GPU_TEXTURE_USAGE_MIP_SWIZZLE_VIEW,
                                                dense_grid.voxels);
    /* The texture can be null if the resolution along one axis is larger than
     * GL_MAX_3D_TEXTURE_SIZE. */
    if (cache_grid->texture != nullptr) {
      GPU_texture_swizzle_set(cache_grid->texture, (channels == 3) ? "rgb1" : "rrr1");
      GPU_texture_extend_mode(cache_grid->texture, GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER);
      BKE_volume_dense_float_grid_clear(&dense_grid);
    }
    else {
      MEM_freeN(dense_grid.voxels);
      printf("Error: Could not allocate 3D texture for volume.\n");
    }
  }

  /* Free grid from memory if it wasn't previously loaded. */
  if (!was_loaded) {
    BKE_volume_grid_unload(volume, grid);
  }

  return cache_grid;
}

DRWVolumeGrid *DRW_volume_batch_cache_get_grid(Volume *volume, const VolumeGrid *volume_grid)
{
  VolumeBatchCache *cache = volume_batch_cache_get(volume);
  DRWVolumeGrid *grid = volume_grid_cache_get(volume, volume_grid, cache);
  return (grid->texture != nullptr) ? grid : nullptr;
}

int DRW_volume_material_count_get(Volume *volume)
{
  return max_ii(1, volume->totcol);
}
