/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "draw_context_private.hh"
#include "draw_subdivision.hh"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.hh"
#include "BKE_editmesh.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_object.hh"
#include "BKE_subdiv.hh"
#include "BKE_subdiv_eval.hh"
#include "BKE_subdiv_foreach.hh"
#include "BKE_subdiv_mesh.hh"
#include "BKE_subdiv_modifier.hh"

#include "BLI_linklist.h"
#include "BLI_mutex.hh"
#include "BLI_virtual_array.hh"

#include "DRW_engine.hh"
#include "DRW_render.hh"

#include "GPU_capabilities.hh"
#include "GPU_compute.hh"
#include "GPU_index_buffer.hh"
#include "GPU_state.hh"
#include "GPU_uniform_buffer.hh"
#include "GPU_vertex_buffer.hh"

#include "opensubdiv_capi_type.hh"
#include "opensubdiv_evaluator_capi.hh"
#ifdef WITH_OPENSUBDIV
#  include "opensubdiv_evaluator.hh"
#  include "opensubdiv_topology_refiner.hh"
#endif

#include "draw_cache_extract.hh"
#include "draw_cache_impl.hh"
#include "draw_cache_inline.hh"
#include "draw_common_c.hh"
#include "draw_shader.hh"
#include "draw_subdiv_shader_shared.hh"
#include "mesh_extractors/extract_mesh.hh"

namespace blender::draw {

/* -------------------------------------------------------------------- */
/** \name Vertex Formats
 *
 * Used for data transfer from OpenSubdiv, and for data processing on our side.
 * \{ */

#ifdef WITH_OPENSUBDIV
/* Vertex format used for the `PatchTable::PatchHandle`. */
static const GPUVertFormat &get_patch_handle_format()
{
  static const GPUVertFormat format = [&]() {
    GPUVertFormat format{};
    GPU_vertformat_attr_add(&format, "vert_index", gpu::VertAttrType::SINT_32);
    GPU_vertformat_attr_add(&format, "array_index", gpu::VertAttrType::SINT_32);
    GPU_vertformat_attr_add(&format, "patch_index", gpu::VertAttrType::SINT_32);
    return format;
  }();
  return format;
}

/* Vertex format used for the quad-tree nodes of the PatchMap. */
static const GPUVertFormat &get_quadtree_format()
{
  static const GPUVertFormat format = [&]() {
    GPUVertFormat format{};
    GPU_vertformat_attr_add(&format, "child", gpu::VertAttrType::UINT_32_32_32_32);
    return format;
  }();
  return format;
}

struct CompressedPatchCoord {
  int ptex_face_index;
  /* UV coordinate encoded as u << 16 | v, where u and v are quantized on 16-bits. */
  uint encoded_uv;
};

MINLINE CompressedPatchCoord make_patch_coord(int ptex_face_index, float u, float v)
{
  CompressedPatchCoord patch_coord = {
      ptex_face_index,
      (uint(u * 65535.0f) << 16) | uint(v * 65535.0f),
  };
  return patch_coord;
}

/* Vertex format used for the #CompressedPatchCoord. */
static const GPUVertFormat &get_blender_patch_coords_format()
{
  static const GPUVertFormat format = [&]() {
    GPUVertFormat format{};
    /* WARNING! Adjust #CompressedPatchCoord accordingly. */
    GPU_vertformat_attr_add(&format, "ptex_face_index", gpu::VertAttrType::UINT_32);
    GPU_vertformat_attr_add(&format, "uv", gpu::VertAttrType::UINT_32);
    return format;
  }();
  return format;
}

#endif

static const GPUVertFormat &get_origindex_format()
{
  static const GPUVertFormat format = [&]() {
    GPUVertFormat format{};
    GPU_vertformat_attr_add(&format, "index", gpu::VertAttrType::SINT_32);
    return format;
  }();
  return format;
}

/** \} */

// --------------------------------------------------------

static uint tris_count_from_number_of_loops(const uint number_of_loops)
{
  const uint32_t number_of_quads = number_of_loops / 4;
  return number_of_quads * 2;
}

/* -------------------------------------------------------------------- */
/** \name Utilities to build a gpu::VertBuf from an origindex buffer.
 * \{ */

gpu::VertBufPtr draw_subdiv_init_origindex_buffer(int32_t *vert_origindex,
                                                  uint num_loops,
                                                  uint loose_len)
{
  gpu::VertBufPtr buffer = gpu::VertBufPtr(
      GPU_vertbuf_create_with_format_ex(get_origindex_format(), GPU_USAGE_STATIC));
  GPU_vertbuf_data_alloc(*buffer, num_loops + loose_len);

  buffer->data<int32_t>().take_front(num_loops).copy_from({vert_origindex, num_loops});
  return buffer;
}

gpu::VertBuf *draw_subdiv_build_origindex_buffer(int *vert_origindex, uint num_loops)
{
  return draw_subdiv_init_origindex_buffer(vert_origindex, num_loops, 0).release();
}

gpu::VertBufPtr draw_subdiv_init_origindex_buffer(Span<int32_t> vert_origindex, uint loose_len)
{
  gpu::VertBufPtr buffer = gpu::VertBufPtr(
      GPU_vertbuf_create_with_format_ex(get_origindex_format(), GPU_USAGE_STATIC));
  GPU_vertbuf_data_alloc(*buffer, vert_origindex.size() + loose_len);

  buffer->data<int32_t>().take_front(vert_origindex.size()).copy_from(vert_origindex);
  return buffer;
}

gpu::VertBuf *draw_subdiv_build_origindex_buffer(Span<int> vert_origindex)
{
  return draw_subdiv_init_origindex_buffer(vert_origindex, 0).release();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities for DRWPatchMap.
 * \{ */

#ifdef WITH_OPENSUBDIV

static void draw_patch_map_build(DRWPatchMap *gpu_patch_map, bke::subdiv::Subdiv *subdiv)
{
  gpu::VertBuf *patch_map_handles = GPU_vertbuf_calloc();
  GPU_vertbuf_init_with_format_ex(*patch_map_handles, get_patch_handle_format(), GPU_USAGE_STATIC);

  gpu::VertBuf *patch_map_quadtree = GPU_vertbuf_calloc();
  GPU_vertbuf_init_with_format_ex(*patch_map_quadtree, get_quadtree_format(), GPU_USAGE_STATIC);

  int min_patch_face = 0;
  int max_patch_face = 0;
  int max_depth = 0;
  int patches_are_triangular = 0;

  OpenSubdiv_Evaluator *evaluator = subdiv->evaluator;
  evaluator->eval_output->getPatchMap(patch_map_handles,
                                      patch_map_quadtree,
                                      &min_patch_face,
                                      &max_patch_face,
                                      &max_depth,
                                      &patches_are_triangular);

  gpu_patch_map->patch_map_handles = patch_map_handles;
  gpu_patch_map->patch_map_quadtree = patch_map_quadtree;
  gpu_patch_map->min_patch_face = min_patch_face;
  gpu_patch_map->max_patch_face = max_patch_face;
  gpu_patch_map->max_depth = max_depth;
  gpu_patch_map->patches_are_triangular = patches_are_triangular;
}

#endif

static void draw_patch_map_free(DRWPatchMap *gpu_patch_map)
{
  GPU_VERTBUF_DISCARD_SAFE(gpu_patch_map->patch_map_handles);
  GPU_VERTBUF_DISCARD_SAFE(gpu_patch_map->patch_map_quadtree);
  gpu_patch_map->min_patch_face = 0;
  gpu_patch_map->max_patch_face = 0;
  gpu_patch_map->max_depth = 0;
  gpu_patch_map->patches_are_triangular = false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name DRWSubdivCache
 * \{ */

static bool draw_subdiv_cache_need_face_data(const DRWSubdivCache &cache)
{
  return cache.subdiv && cache.subdiv->evaluator && cache.num_subdiv_loops != 0;
}

static void draw_subdiv_cache_free_material_data(DRWSubdivCache &cache)
{
  GPU_VERTBUF_DISCARD_SAFE(cache.face_mat_offset);
  MEM_SAFE_FREE(cache.mat_start);
  MEM_SAFE_FREE(cache.mat_end);
}

static void draw_subdiv_free_edit_mode_cache(DRWSubdivCache &cache)
{
  GPU_VERTBUF_DISCARD_SAFE(cache.verts_orig_index);
  GPU_VERTBUF_DISCARD_SAFE(cache.edges_orig_index);
  GPU_VERTBUF_DISCARD_SAFE(cache.edges_draw_flag);
  GPU_VERTBUF_DISCARD_SAFE(cache.fdots_patch_coords);
}

void draw_subdiv_cache_free(DRWSubdivCache &cache)
{
  GPU_VERTBUF_DISCARD_SAFE(cache.patch_coords);
  GPU_VERTBUF_DISCARD_SAFE(cache.corner_patch_coords);
  GPU_VERTBUF_DISCARD_SAFE(cache.face_ptex_offset_buffer);
  GPU_VERTBUF_DISCARD_SAFE(cache.subdiv_face_offset_buffer);
  GPU_VERTBUF_DISCARD_SAFE(cache.extra_coarse_face_data);
  MEM_SAFE_FREE(cache.subdiv_loop_subdiv_vert_index);
  MEM_SAFE_FREE(cache.subdiv_loop_subdiv_edge_index);
  MEM_SAFE_FREE(cache.subdiv_loop_face_index);
  MEM_SAFE_FREE(cache.subdiv_face_offset);
  GPU_VERTBUF_DISCARD_SAFE(cache.subdiv_vert_face_adjacency_offsets);
  GPU_VERTBUF_DISCARD_SAFE(cache.subdiv_vert_face_adjacency);
  cache.resolution = 0;
  cache.num_subdiv_loops = 0;
  cache.num_subdiv_edges = 0;
  cache.num_subdiv_verts = 0;
  cache.num_subdiv_triangles = 0;
  cache.num_coarse_faces = 0;
  cache.num_subdiv_quads = 0;
  cache.may_have_loose_geom = false;
  draw_subdiv_free_edit_mode_cache(cache);
  draw_subdiv_cache_free_material_data(cache);
  draw_patch_map_free(&cache.gpu_patch_map);
  if (cache.ubo) {
    GPU_uniformbuf_free(cache.ubo);
    cache.ubo = nullptr;
  }
  cache.loose_edge_positions = {};
}

/* Flags used in #DRWSubdivCache.extra_coarse_face_data. The flags are packed in the upper bits of
 * each uint (one per coarse face), #SUBDIV_COARSE_FACE_FLAG_OFFSET tells where they are in the
 * packed bits. */
#define SUBDIV_COARSE_FACE_FLAG_SMOOTH 1u
#define SUBDIV_COARSE_FACE_FLAG_SELECT 2u
#define SUBDIV_COARSE_FACE_FLAG_ACTIVE 4u
#define SUBDIV_COARSE_FACE_FLAG_HIDDEN 8u

#define SUBDIV_COARSE_FACE_FLAG_OFFSET 28u

#define SUBDIV_COARSE_FACE_FLAG_SMOOTH_MASK \
  (SUBDIV_COARSE_FACE_FLAG_SMOOTH << SUBDIV_COARSE_FACE_FLAG_OFFSET)
#define SUBDIV_COARSE_FACE_FLAG_SELECT_MASK \
  (SUBDIV_COARSE_FACE_FLAG_SELECT << SUBDIV_COARSE_FACE_FLAG_OFFSET)
#define SUBDIV_COARSE_FACE_FLAG_ACTIVE_MASK \
  (SUBDIV_COARSE_FACE_FLAG_ACTIVE << SUBDIV_COARSE_FACE_FLAG_OFFSET)
#define SUBDIV_COARSE_FACE_FLAG_HIDDEN_MASK \
  (SUBDIV_COARSE_FACE_FLAG_HIDDEN << SUBDIV_COARSE_FACE_FLAG_OFFSET)

#define SUBDIV_COARSE_FACE_LOOP_START_MASK \
  ~((SUBDIV_COARSE_FACE_FLAG_SMOOTH | SUBDIV_COARSE_FACE_FLAG_SELECT | \
     SUBDIV_COARSE_FACE_FLAG_ACTIVE | SUBDIV_COARSE_FACE_FLAG_HIDDEN) \
    << SUBDIV_COARSE_FACE_FLAG_OFFSET)

static uint32_t compute_coarse_face_flag_bm(BMFace *f, BMFace *efa_act)
{
  uint32_t flag = 0;
  if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
    flag |= SUBDIV_COARSE_FACE_FLAG_SELECT;
  }
  if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
    flag |= SUBDIV_COARSE_FACE_FLAG_HIDDEN;
  }
  if (f == efa_act) {
    flag |= SUBDIV_COARSE_FACE_FLAG_ACTIVE;
  }
  return flag;
}

static void draw_subdiv_cache_extra_coarse_face_data_bm(BMesh *bm,
                                                        BMFace *efa_act,
                                                        MutableSpan<uint32_t> flags_data)
{
  BMFace *f;
  BMIter iter;

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    const int index = BM_elem_index_get(f);
    uint32_t flag = compute_coarse_face_flag_bm(f, efa_act);
    if (BM_elem_flag_test(f, BM_ELEM_SMOOTH)) {
      flag |= SUBDIV_COARSE_FACE_FLAG_SMOOTH;
    }
    const int loopstart = BM_elem_index_get(f->l_first);
    flags_data[index] = uint(loopstart) | (flag << SUBDIV_COARSE_FACE_FLAG_OFFSET);
  }
}

static void draw_subdiv_cache_extra_coarse_face_data_mesh(const MeshRenderData &mr,
                                                          const Mesh *mesh,
                                                          MutableSpan<uint32_t> flags_data)
{
  const OffsetIndices faces = mesh->faces();
  for (const int i : faces.index_range()) {
    uint32_t flag = 0;
    if (!(mr.normals_domain == bke::MeshNormalDomain::Face ||
          (!mr.sharp_faces.is_empty() && mr.sharp_faces[i])))
    {
      flag |= SUBDIV_COARSE_FACE_FLAG_SMOOTH;
    }
    if (!mr.select_poly.is_empty() && mr.select_poly[i]) {
      flag |= SUBDIV_COARSE_FACE_FLAG_SELECT;
    }
    if (!mr.hide_poly.is_empty() && mr.hide_poly[i]) {
      flag |= SUBDIV_COARSE_FACE_FLAG_HIDDEN;
    }
    flags_data[i] = uint(faces[i].start()) | (flag << SUBDIV_COARSE_FACE_FLAG_OFFSET);
  }
}

static void draw_subdiv_cache_extra_coarse_face_data_mapped(const Mesh *mesh,
                                                            BMesh *bm,
                                                            MeshRenderData &mr,
                                                            MutableSpan<uint32_t> flags_data)
{
  if (bm == nullptr) {
    draw_subdiv_cache_extra_coarse_face_data_mesh(mr, mesh, flags_data);
    return;
  }

  const OffsetIndices faces = mesh->faces();
  for (const int i : faces.index_range()) {
    BMFace *f = bm_original_face_get(mr, i);
    /* Selection and hiding from bmesh. */
    uint32_t flag = (f) ? compute_coarse_face_flag_bm(f, mr.efa_act) : 0;
    /* Smooth from mesh. */
    if (!(mr.normals_domain == bke::MeshNormalDomain::Face ||
          (!mr.sharp_faces.is_empty() && mr.sharp_faces[i])))
    {
      flag |= SUBDIV_COARSE_FACE_FLAG_SMOOTH;
    }
    flags_data[i] = uint(faces[i].start()) | (flag << SUBDIV_COARSE_FACE_FLAG_OFFSET);
  }
}

static void draw_subdiv_cache_update_extra_coarse_face_data(DRWSubdivCache &cache,
                                                            const Mesh *mesh,
                                                            MeshRenderData &mr)
{
  if (cache.extra_coarse_face_data == nullptr) {
    cache.extra_coarse_face_data = GPU_vertbuf_calloc();
    static const GPUVertFormat format = []() {
      GPUVertFormat format{};
      GPU_vertformat_attr_add(&format, "data", gpu::VertAttrType::UINT_32);
      return format;
    }();
    GPU_vertbuf_init_with_format_ex(*cache.extra_coarse_face_data, format, GPU_USAGE_DYNAMIC);
    GPU_vertbuf_data_alloc(*cache.extra_coarse_face_data,
                           mr.extract_type == MeshExtractType::BMesh ? cache.bm->totface :
                                                                       mesh->faces_num);
  }

  MutableSpan<uint32_t> flags_data = cache.extra_coarse_face_data->data<uint32_t>();

  if (mr.extract_type == MeshExtractType::BMesh) {
    draw_subdiv_cache_extra_coarse_face_data_bm(cache.bm, mr.efa_act, flags_data);
  }
  else if (mr.orig_index_face != nullptr) {
    draw_subdiv_cache_extra_coarse_face_data_mapped(mesh, cache.bm, mr, flags_data);
  }
  else {
    draw_subdiv_cache_extra_coarse_face_data_mesh(mr, mesh, flags_data);
  }

  /* Make sure updated data is re-uploaded. */
  GPU_vertbuf_tag_dirty(cache.extra_coarse_face_data);
}

static DRWSubdivCache &mesh_batch_cache_ensure_subdiv_cache(MeshBatchCache &mbc)
{
  DRWSubdivCache *subdiv_cache = mbc.subdiv_cache;
  if (subdiv_cache == nullptr) {
    subdiv_cache = MEM_new<DRWSubdivCache>(__func__);
  }
  mbc.subdiv_cache = subdiv_cache;
  return *subdiv_cache;
}

#ifdef WITH_OPENSUBDIV

static void draw_subdiv_invalidate_evaluator_for_orco(bke::subdiv::Subdiv *subdiv,
                                                      const Mesh *mesh)
{
  if (!(subdiv && subdiv->evaluator)) {
    return;
  }

  const bool has_orco = CustomData_has_layer(&mesh->vert_data, CD_ORCO);
  if (has_orco && !subdiv->evaluator->eval_output->hasVertexData()) {
    /* If we suddenly have/need original coordinates, recreate the evaluator if the extra
     * source was not created yet. The refiner also has to be recreated as refinement for source
     * and vertex data is done only once. */
    delete subdiv->evaluator;
    subdiv->evaluator = nullptr;

    delete subdiv->topology_refiner;
    subdiv->topology_refiner = nullptr;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Subdivision grid traversal.
 *
 * Traverse the uniform subdivision grid over coarse faces and gather useful information for
 * building the draw buffers on the GPU. We primarily gather the patch coordinates for all
 * subdivision faces, as well as the original coarse indices for each subdivision element (vertex,
 * face, or edge) which directly maps to its coarse counterpart (note that all subdivision faces
 * map to a coarse face). This information will then be cached in #DRWSubdivCache for subsequent
 * reevaluations, as long as the topology does not change.
 * \{ */

struct DRWCacheBuildingContext {
  const Mesh *coarse_mesh;
  const bke::subdiv::Subdiv *subdiv;
  const bke::subdiv::ToMeshSettings *settings;

  DRWSubdivCache *cache;

  /* Pointers into #DRWSubdivCache buffers for easier access during traversal. */
  CompressedPatchCoord *patch_coords;
  int *subdiv_loop_vert_index;
  int *subdiv_loop_subdiv_vert_index;
  int *subdiv_loop_edge_index;
  int *subdiv_loop_edge_draw_flag;
  int *subdiv_loop_subdiv_edge_index;
  int *subdiv_loop_face_index;

  /* Temporary buffers used during traversal. */
  int *vert_origindex_map;
  int *edge_draw_flag_map;
  int *edge_origindex_map;

  /* #CD_ORIGINDEX layers from the mesh to directly look up during traversal the original-index
   * from the base mesh for edit data so that we do not have to handle yet another GPU buffer and
   * do this in the shaders. */
  const int *orig_index_vert;
  const int *orig_index_edge;
};

static bool draw_subdiv_topology_info_cb(const bke::subdiv::ForeachContext *foreach_context,
                                         const int num_verts,
                                         const int num_edges,
                                         const int num_loops,
                                         const int num_faces,
                                         const int *subdiv_face_offset)
{
  /* num_loops does not take into account meshes with only loose geometry, which might be meshes
   * used as custom bone shapes, so let's check the num_verts also. */
  if (num_verts == 0 && num_loops == 0) {
    return false;
  }

  DRWCacheBuildingContext *ctx = (DRWCacheBuildingContext *)(foreach_context->user_data);
  DRWSubdivCache *cache = ctx->cache;

  /* Set topology information only if we have loops. */
  if (num_loops != 0) {
    cache->num_subdiv_edges = uint(num_edges);
    cache->num_subdiv_loops = uint(num_loops);
    cache->num_subdiv_verts = uint(num_verts);
    cache->num_subdiv_quads = uint(num_faces);
    cache->subdiv_face_offset = static_cast<int *>(MEM_dupallocN(subdiv_face_offset));
  }

  cache->may_have_loose_geom = num_verts != 0 || num_edges != 0;

  /* Initialize cache buffers, prefer dynamic usage so we can reuse memory on the host even after
   * it was sent to the device, since we may use the data while building other buffers on the CPU
   * side.
   *
   * These VBOs are created even when there are no faces and only loose geometry. This avoids the
   * need for many null checks. Binding them must be avoided if they are empty though. */
  cache->patch_coords = GPU_vertbuf_calloc();
  GPU_vertbuf_init_with_format_ex(
      *cache->patch_coords, get_blender_patch_coords_format(), GPU_USAGE_DYNAMIC);
  GPU_vertbuf_data_alloc(*cache->patch_coords, cache->num_subdiv_loops);

  cache->corner_patch_coords = GPU_vertbuf_calloc();
  GPU_vertbuf_init_with_format_ex(
      *cache->corner_patch_coords, get_blender_patch_coords_format(), GPU_USAGE_DYNAMIC);
  GPU_vertbuf_data_alloc(*cache->corner_patch_coords, cache->num_subdiv_loops);

  cache->verts_orig_index = GPU_vertbuf_calloc();
  GPU_vertbuf_init_with_format_ex(
      *cache->verts_orig_index, get_origindex_format(), GPU_USAGE_DYNAMIC);
  GPU_vertbuf_data_alloc(*cache->verts_orig_index, cache->num_subdiv_loops);

  cache->edges_orig_index = GPU_vertbuf_calloc();
  GPU_vertbuf_init_with_format_ex(
      *cache->edges_orig_index, get_origindex_format(), GPU_USAGE_DYNAMIC);
  GPU_vertbuf_data_alloc(*cache->edges_orig_index, cache->num_subdiv_loops);

  cache->edges_draw_flag = GPU_vertbuf_calloc();
  GPU_vertbuf_init_with_format_ex(
      *cache->edges_draw_flag, get_origindex_format(), GPU_USAGE_DYNAMIC);
  GPU_vertbuf_data_alloc(*cache->edges_draw_flag, cache->num_subdiv_loops);

  cache->subdiv_loop_subdiv_vert_index = MEM_malloc_arrayN<int>(cache->num_subdiv_loops,
                                                                "subdiv_loop_subdiv_vert_index");

  cache->subdiv_loop_subdiv_edge_index = MEM_malloc_arrayN<int>(cache->num_subdiv_loops,
                                                                "subdiv_loop_subdiv_edge_index");

  cache->subdiv_loop_face_index = MEM_malloc_arrayN<int>(cache->num_subdiv_loops,
                                                         "subdiv_loop_face_index");

  /* Initialize context pointers and temporary buffers. */
  ctx->patch_coords = cache->patch_coords->data<CompressedPatchCoord>().data();
  ctx->subdiv_loop_vert_index = cache->verts_orig_index->data<int>().data();
  ctx->subdiv_loop_edge_index = cache->edges_orig_index->data<int>().data();
  ctx->subdiv_loop_edge_draw_flag = cache->edges_draw_flag->data<int>().data();
  ctx->subdiv_loop_subdiv_vert_index = cache->subdiv_loop_subdiv_vert_index;
  ctx->subdiv_loop_subdiv_edge_index = cache->subdiv_loop_subdiv_edge_index;
  ctx->subdiv_loop_face_index = cache->subdiv_loop_face_index;

  ctx->orig_index_vert = static_cast<const int *>(
      CustomData_get_layer(&ctx->coarse_mesh->vert_data, CD_ORIGINDEX));

  ctx->orig_index_edge = static_cast<const int *>(
      CustomData_get_layer(&ctx->coarse_mesh->edge_data, CD_ORIGINDEX));

  if (cache->num_subdiv_verts) {
    ctx->vert_origindex_map = MEM_malloc_arrayN<int>(cache->num_subdiv_verts,
                                                     "subdiv_vert_origindex_map");
    for (int i = 0; i < num_verts; i++) {
      ctx->vert_origindex_map[i] = -1;
    }
  }

  if (cache->num_subdiv_edges) {
    ctx->edge_origindex_map = MEM_malloc_arrayN<int>(cache->num_subdiv_edges,
                                                     "subdiv_edge_origindex_map");
    for (int i = 0; i < num_edges; i++) {
      ctx->edge_origindex_map[i] = -1;
    }
    ctx->edge_draw_flag_map = MEM_calloc_arrayN<int>(cache->num_subdiv_edges,
                                                     "subdiv_edge_draw_flag_map");
  }

  return true;
}

static void draw_subdiv_vert_corner_cb(const bke::subdiv::ForeachContext *foreach_context,
                                       void * /*tls*/,
                                       const int /*ptex_face_index*/,
                                       const float /*u*/,
                                       const float /*v*/,
                                       const int coarse_vert_index,
                                       const int /*coarse_face_index*/,
                                       const int /*coarse_corner*/,
                                       const int subdiv_vert_index)
{
  BLI_assert(coarse_vert_index != ORIGINDEX_NONE);
  DRWCacheBuildingContext *ctx = (DRWCacheBuildingContext *)(foreach_context->user_data);
  ctx->vert_origindex_map[subdiv_vert_index] = coarse_vert_index;
}

static void draw_subdiv_vert_edge_cb(const bke::subdiv::ForeachContext * /*foreach_context*/,
                                     void * /*tls_v*/,
                                     const int /*ptex_face_index*/,
                                     const float /*u*/,
                                     const float /*v*/,
                                     const int /*coarse_edge_index*/,
                                     const int /*coarse_face_index*/,
                                     const int /*coarse_corner*/,
                                     const int /*subdiv_vert_index*/)
{
  /* Required if bke::subdiv::ForeachContext.vert_corner is also set. */
}

static void draw_subdiv_edge_cb(const bke::subdiv::ForeachContext *foreach_context,
                                void * /*tls*/,
                                const int coarse_edge_index,
                                const int subdiv_edge_index,
                                const bool /*is_loose*/,
                                const int /*subdiv_v1*/,
                                const int /*subdiv_v2*/)
{
  DRWCacheBuildingContext *ctx = (DRWCacheBuildingContext *)(foreach_context->user_data);

  if (!ctx->edge_origindex_map) {
    return;
  }

  if (coarse_edge_index == ORIGINDEX_NONE) {
    /* Not mapped to edge in the subdivision base mesh. */
    ctx->edge_origindex_map[subdiv_edge_index] = ORIGINDEX_NONE;
    if (!ctx->cache->optimal_display) {
      ctx->edge_draw_flag_map[subdiv_edge_index] = 1;
    }
  }
  else {
    if (ctx->orig_index_edge) {
      const int origindex = ctx->orig_index_edge[coarse_edge_index];
      ctx->edge_origindex_map[subdiv_edge_index] = origindex;
      if (!(origindex == ORIGINDEX_NONE && ctx->cache->hide_unmapped_edges)) {
        /* Not mapped to edge in original mesh (generated by a preceding modifier). */
        ctx->edge_draw_flag_map[subdiv_edge_index] = 1;
      }
    }
    else {
      ctx->edge_origindex_map[subdiv_edge_index] = coarse_edge_index;
      ctx->edge_draw_flag_map[subdiv_edge_index] = 1;
    }
  }
}

static void draw_subdiv_loop_cb(const bke::subdiv::ForeachContext *foreach_context,
                                void * /*tls_v*/,
                                const int ptex_face_index,
                                const float u,
                                const float v,
                                const int /*coarse_loop_index*/,
                                const int coarse_face_index,
                                const int /*coarse_corner*/,
                                const int subdiv_loop_index,
                                const int subdiv_vert_index,
                                const int subdiv_edge_index)
{
  DRWCacheBuildingContext *ctx = (DRWCacheBuildingContext *)(foreach_context->user_data);
  ctx->patch_coords[subdiv_loop_index] = make_patch_coord(ptex_face_index, u, v);

  int coarse_vert_index = ctx->vert_origindex_map[subdiv_vert_index];

  ctx->subdiv_loop_subdiv_vert_index[subdiv_loop_index] = subdiv_vert_index;
  ctx->subdiv_loop_subdiv_edge_index[subdiv_loop_index] = subdiv_edge_index;
  ctx->subdiv_loop_face_index[subdiv_loop_index] = coarse_face_index;
  ctx->subdiv_loop_vert_index[subdiv_loop_index] = coarse_vert_index;
}

static void draw_subdiv_foreach_callbacks(bke::subdiv::ForeachContext *foreach_context)
{
  *foreach_context = {};
  foreach_context->topology_info = draw_subdiv_topology_info_cb;
  foreach_context->loop = draw_subdiv_loop_cb;
  foreach_context->edge = draw_subdiv_edge_cb;
  foreach_context->vert_corner = draw_subdiv_vert_corner_cb;
  foreach_context->vert_edge = draw_subdiv_vert_edge_cb;
}

static void do_subdiv_traversal(DRWCacheBuildingContext *cache_building_context,
                                bke::subdiv::Subdiv *subdiv)
{
  bke::subdiv::ForeachContext foreach_context;
  draw_subdiv_foreach_callbacks(&foreach_context);
  foreach_context.user_data = cache_building_context;

  bke::subdiv::foreach_subdiv_geometry(subdiv,
                                       &foreach_context,
                                       cache_building_context->settings,
                                       cache_building_context->coarse_mesh);

  /* Now that traversal is done, we can set up the right original indices for the
   * subdiv-loop-to-coarse-edge map.
   */
  for (int i = 0; i < cache_building_context->cache->num_subdiv_loops; i++) {
    const int edge_index = cache_building_context->subdiv_loop_subdiv_edge_index[i];
    cache_building_context->subdiv_loop_edge_index[i] =
        cache_building_context->edge_origindex_map[edge_index];
    cache_building_context->subdiv_loop_edge_draw_flag[i] =
        cache_building_context->edge_draw_flag_map[edge_index];
  }
}

static gpu::VertBuf *gpu_vertbuf_create_from_format(const GPUVertFormat &format, uint len)
{
  gpu::VertBuf *verts = GPU_vertbuf_calloc();
  GPU_vertbuf_init_with_format(*verts, format);
  GPU_vertbuf_data_alloc(*verts, len);
  return verts;
}

/* Build maps to hold enough information to tell which face is adjacent to which vertex; those will
 * be used for computing normals if limit surfaces are unavailable. */
static void build_vert_face_adjacency_maps(DRWSubdivCache &cache)
{
  /* +1 so that we do not require a special case for the last vertex, this extra offset will
   * contain the total number of adjacent faces. */
  cache.subdiv_vert_face_adjacency_offsets = gpu_vertbuf_create_from_format(
      get_origindex_format(), cache.num_subdiv_verts + 1);

  MutableSpan<int> vert_offsets = cache.subdiv_vert_face_adjacency_offsets->data<int>();
  vert_offsets.fill(0);

  offset_indices::build_reverse_offsets(
      {cache.subdiv_loop_subdiv_vert_index, cache.num_subdiv_loops}, vert_offsets);

  cache.subdiv_vert_face_adjacency = gpu_vertbuf_create_from_format(get_origindex_format(),
                                                                    cache.num_subdiv_loops);
  MutableSpan<int> adjacent_faces = cache.subdiv_vert_face_adjacency->data<int>();
  int *tmp_set_faces = MEM_calloc_arrayN<int>(cache.num_subdiv_verts, "tmp subdiv vertex offset");

  for (int i = 0; i < cache.num_subdiv_loops / 4; i++) {
    for (int j = 0; j < 4; j++) {
      const int subdiv_vert = cache.subdiv_loop_subdiv_vert_index[i * 4 + j];
      int first_face_offset = vert_offsets[subdiv_vert] + tmp_set_faces[subdiv_vert];
      adjacent_faces[first_face_offset] = i;
      tmp_set_faces[subdiv_vert] += 1;
    }
  }

  MEM_freeN(tmp_set_faces);
}

static bool draw_subdiv_build_cache(DRWSubdivCache &cache,
                                    bke::subdiv::Subdiv *subdiv,
                                    const Mesh *mesh_eval,
                                    const SubsurfRuntimeData *runtime_data)
{
  bke::subdiv::ToMeshSettings to_mesh_settings;
  to_mesh_settings.resolution = runtime_data->resolution;
  to_mesh_settings.use_optimal_display = false;

  if (cache.resolution != to_mesh_settings.resolution) {
    /* Resolution changed, we need to rebuild, free any existing cached data. */
    draw_subdiv_cache_free(cache);
  }

  /* If the resolution between the cache and the settings match for some reason, check if the patch
   * coordinates were not already generated. Those coordinates are specific to the resolution, so
   * they should be null either after initialization, or after freeing if the resolution (or some
   * other subdivision setting) changed.
   */
  if (cache.patch_coords != nullptr) {
    return true;
  }

  DRWCacheBuildingContext cache_building_context;
  memset(&cache_building_context, 0, sizeof(DRWCacheBuildingContext));
  cache_building_context.coarse_mesh = mesh_eval;
  cache_building_context.settings = &to_mesh_settings;
  cache_building_context.cache = &cache;

  do_subdiv_traversal(&cache_building_context, subdiv);
  if (cache.num_subdiv_loops == 0 && cache.num_subdiv_verts == 0 && !cache.may_have_loose_geom) {
    /* Either the traversal failed, or we have an empty mesh, either way we cannot go any further.
     * The subdiv_face_offset cannot then be reliably stored in the cache, so free it directly.
     */
    MEM_SAFE_FREE(cache.subdiv_face_offset);
    return false;
  }

  /* Only build face related data if we have polygons. */
  const OffsetIndices faces = mesh_eval->faces();
  if (cache.num_subdiv_loops != 0) {
    /* Build buffers for the PatchMap. */
    draw_patch_map_build(&cache.gpu_patch_map, subdiv);

    cache.face_ptex_offset = bke::subdiv::face_ptex_offset_get(subdiv);

    /* Build patch coordinates for all the face dots. */
    cache.fdots_patch_coords = gpu_vertbuf_create_from_format(get_blender_patch_coords_format(),
                                                              mesh_eval->faces_num);
    CompressedPatchCoord *blender_fdots_patch_coords =
        cache.fdots_patch_coords->data<CompressedPatchCoord>().data();
    for (int i = 0; i < mesh_eval->faces_num; i++) {
      const int ptex_face_index = cache.face_ptex_offset[i];
      if (faces[i].size() == 4) {
        /* For quads, the center coordinate of the coarse face has `u = v = 0.5`. */
        blender_fdots_patch_coords[i] = make_patch_coord(ptex_face_index, 0.5f, 0.5f);
      }
      else {
        /* For N-gons, since they are split into quads from the center, and since the center is
         * chosen to be the top right corner of each quad, the center coordinate of the coarse face
         * is any one of those top right corners with `u = v = 1.0`. */
        blender_fdots_patch_coords[i] = make_patch_coord(ptex_face_index, 1.0f, 1.0f);
      }
    }

    cache.subdiv_face_offset_buffer = draw_subdiv_build_origindex_buffer(cache.subdiv_face_offset,
                                                                         faces.size());

    cache.face_ptex_offset_buffer = draw_subdiv_build_origindex_buffer(cache.face_ptex_offset);

    build_vert_face_adjacency_maps(cache);
  }

  cache.resolution = to_mesh_settings.resolution;
  cache.num_coarse_faces = faces.size();

  /* To avoid floating point precision issues when evaluating patches at patch boundaries,
   * ensure that all loops sharing a vertex use the same patch coordinate. This could cause
   * the mesh to not be watertight, leading to shadowing artifacts (see #97877). */
  Vector<int> first_loop_index(cache.num_subdiv_verts, -1);

  /* Save coordinates for corners, as attributes may vary for each loop connected to the same
   * vertex. */
  if (cache.num_subdiv_loops > 0) {
    memcpy(cache.corner_patch_coords->data<CompressedPatchCoord>().data(),
           cache_building_context.patch_coords,
           sizeof(CompressedPatchCoord) * cache.num_subdiv_loops);

    for (int i = 0; i < cache.num_subdiv_loops; i++) {
      const int vert = cache_building_context.subdiv_loop_subdiv_vert_index[i];
      if (first_loop_index[vert] != -1) {
        continue;
      }
      first_loop_index[vert] = i;
    }

    for (int i = 0; i < cache.num_subdiv_loops; i++) {
      const int vert = cache_building_context.subdiv_loop_subdiv_vert_index[i];
      cache_building_context.patch_coords[i] =
          cache_building_context.patch_coords[first_loop_index[vert]];
    }
  }

  /* Cleanup. */
  MEM_SAFE_FREE(cache_building_context.vert_origindex_map);
  MEM_SAFE_FREE(cache_building_context.edge_origindex_map);
  MEM_SAFE_FREE(cache_building_context.edge_draw_flag_map);

  return true;
}

#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name DRWSubdivUboStorage.
 *
 * Common uniforms for the various shaders.
 * \{ */

static void draw_subdiv_init_ubo_storage(const DRWSubdivCache &cache,
                                         DRWSubdivUboStorage *ubo,
                                         const int src_offset,
                                         const int dst_offset,
                                         const uint total_dispatch_size,
                                         const bool has_sculpt_mask,
                                         const uint edge_loose_offset)
{
  ubo->src_offset = src_offset;
  ubo->dst_offset = dst_offset;
  ubo->min_patch_face = cache.gpu_patch_map.min_patch_face;
  ubo->max_patch_face = cache.gpu_patch_map.max_patch_face;
  ubo->max_depth = cache.gpu_patch_map.max_depth;
  ubo->patches_are_triangular = cache.gpu_patch_map.patches_are_triangular;
  ubo->coarse_face_count = cache.num_coarse_faces;
  ubo->num_subdiv_loops = cache.num_subdiv_loops;
  ubo->edge_loose_offset = edge_loose_offset;
  ubo->has_sculpt_mask = has_sculpt_mask;
  ubo->coarse_face_smooth_mask = SUBDIV_COARSE_FACE_FLAG_SMOOTH_MASK;
  ubo->coarse_face_select_mask = SUBDIV_COARSE_FACE_FLAG_SELECT_MASK;
  ubo->coarse_face_active_mask = SUBDIV_COARSE_FACE_FLAG_ACTIVE_MASK;
  ubo->coarse_face_hidden_mask = SUBDIV_COARSE_FACE_FLAG_HIDDEN_MASK;
  ubo->coarse_face_loopstart_mask = SUBDIV_COARSE_FACE_LOOP_START_MASK;
  ubo->total_dispatch_size = total_dispatch_size;
  ubo->is_edit_mode = cache.is_edit_mode;
  ubo->use_hide = cache.use_hide;
}

static void draw_subdiv_ubo_update_and_bind(const DRWSubdivCache &cache,
                                            const int src_offset,
                                            const int dst_offset,
                                            const uint total_dispatch_size,
                                            const bool has_sculpt_mask = false,
                                            const uint edge_loose_offset = 0)
{
  DRWSubdivUboStorage storage;
  draw_subdiv_init_ubo_storage(cache,
                               &storage,
                               src_offset,
                               dst_offset,
                               total_dispatch_size,
                               has_sculpt_mask,
                               edge_loose_offset);

  if (!cache.ubo) {
    const_cast<DRWSubdivCache *>(&cache)->ubo = GPU_uniformbuf_create_ex(
        sizeof(DRWSubdivUboStorage), &storage, "DRWSubdivUboStorage");
  }

  GPU_uniformbuf_update(cache.ubo, &storage);
  GPU_uniformbuf_bind(cache.ubo, SHADER_DATA_BUF_SLOT);
}

/** \} */

// --------------------------------------------------------

#define SUBDIV_LOCAL_WORK_GROUP_SIZE 64
static uint get_dispatch_size(uint elements)
{
  return divide_ceil_u(elements, SUBDIV_LOCAL_WORK_GROUP_SIZE);
}

/**
 * Helper to ensure that the UBO is always initialized before dispatching computes and that the
 * same number of elements that need to be processed is used for the UBO and the dispatch size.
 * Use this instead of a raw call to #GPU_compute_dispatch.
 */
static void drw_subdiv_compute_dispatch(const DRWSubdivCache &cache,
                                        gpu::Shader *shader,
                                        const int src_offset,
                                        const int dst_offset,
                                        uint total_dispatch_size,
                                        const bool has_sculpt_mask = false,
                                        const uint edge_loose_offset = 0)
{
  const uint max_res_x = uint(GPU_max_work_group_count(0));

  const uint dispatch_size = get_dispatch_size(total_dispatch_size);
  uint dispatch_rx = dispatch_size;
  uint dispatch_ry = 1u;
  if (dispatch_rx > max_res_x) {
    /* Since there are some limitations with regards to the maximum work group size (could be as
     * low as 64k elements per call), we split the number elements into a "2d" number, with the
     * final index being computed as `res_x + res_y * max_work_group_size`. Even with a maximum
     * work group size of 64k, that still leaves us with roughly `64k * 64k = 4` billion elements
     * total, which should be enough. If not, we could also use the 3rd dimension. */
    /* TODO(fclem): We could dispatch fewer groups if we compute the prime factorization and
     * get the smallest rect fitting the requirements. */
    dispatch_rx = dispatch_ry = ceilf(sqrtf(dispatch_size));
    /* Avoid a completely empty dispatch line caused by rounding. */
    if ((dispatch_rx * (dispatch_ry - 1)) >= dispatch_size) {
      dispatch_ry -= 1;
    }
  }

  /* X and Y dimensions may have different limits so the above computation may not be right, but
   * even with the standard 64k minimum on all dimensions we still have a lot of room. Therefore,
   * we presume it all fits. */
  BLI_assert(dispatch_ry < uint(GPU_max_work_group_count(1)));

  draw_subdiv_ubo_update_and_bind(
      cache, src_offset, dst_offset, total_dispatch_size, has_sculpt_mask, edge_loose_offset);

  GPU_compute_dispatch(shader, dispatch_rx, dispatch_ry, 1);
}

void draw_subdiv_extract_pos(const DRWSubdivCache &cache, gpu::VertBuf *pos, gpu::VertBuf *orco)
{
#ifdef WITH_OPENSUBDIV
  if (!draw_subdiv_cache_need_face_data(cache)) {
    /* Happens on meshes with only loose geometry. */
    return;
  }

  bke::subdiv::Subdiv *subdiv = cache.subdiv;
  OpenSubdiv_Evaluator *evaluator = subdiv->evaluator;

  gpu::VertBuf *src_buffer = evaluator->eval_output->get_source_buf();
  gpu::VertBuf *src_extra_buffer = nullptr;
  if (orco) {
    src_extra_buffer = evaluator->eval_output->get_source_data_buf();
  }

  gpu::StorageBuf *patch_arrays_buffer = evaluator->eval_output->create_patch_arrays_buf();
  gpu::StorageBuf *patch_index_buffer = evaluator->eval_output->get_patch_index_buf();
  gpu::StorageBuf *patch_param_buffer = evaluator->eval_output->get_patch_param_buf();

  gpu::Shader *shader = DRW_shader_subdiv_get(orco ? SubdivShaderType::PATCH_EVALUATION_ORCO :
                                                     SubdivShaderType::PATCH_EVALUATION);
  GPU_shader_bind(shader);

  GPU_vertbuf_bind_as_ssbo(src_buffer, PATCH_EVALUATION_SOURCE_VERTEX_BUFFER_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(cache.gpu_patch_map.patch_map_handles,
                           PATCH_EVALUATION_INPUT_PATCH_HANDLES_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(cache.gpu_patch_map.patch_map_quadtree,
                           PATCH_EVALUATION_QUAD_NODES_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(cache.patch_coords, PATCH_EVALUATION_PATCH_COORDS_BUF_SLOT);
  GPU_storagebuf_bind(patch_arrays_buffer, PATCH_EVALUATION_PATCH_ARRAY_BUFFER_BUF_SLOT);
  GPU_storagebuf_bind(patch_index_buffer, PATCH_EVALUATION_PATCH_INDEX_BUFFER_BUF_SLOT);
  GPU_storagebuf_bind(patch_param_buffer, PATCH_EVALUATION_PATCH_PARAM_BUFFER_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(pos, PATCH_EVALUATION_OUTPUT_POS_BUF_SLOT);
  if (orco) {
    GPU_vertbuf_bind_as_ssbo(src_extra_buffer,
                             PATCH_EVALUATION_SOURCE_EXTRA_VERTEX_BUFFER_BUF_SLOT);
    GPU_vertbuf_bind_as_ssbo(orco, PATCH_EVALUATION_OUTPUT_ORCOS_BUF_SLOT);
  }

  drw_subdiv_compute_dispatch(cache, shader, 0, 0, cache.num_subdiv_quads);

  /* This generates a vertex buffer, so we need to put a barrier on the vertex attribute array.
   * We also need it for subsequent compute shaders, so a barrier on the shader storage is also
   * needed. */
  GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE | GPU_BARRIER_VERTEX_ATTRIB_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();

  GPU_storagebuf_free(patch_arrays_buffer);
#else
  UNUSED_VARS(cache, pos, orco);
#endif
}

void draw_subdiv_extract_uvs(const DRWSubdivCache &cache,
                             gpu::VertBuf *uvs,
                             const int face_varying_channel,
                             const int dst_offset)
{
#ifdef WITH_OPENSUBDIV
  if (!draw_subdiv_cache_need_face_data(cache)) {
    /* Happens on meshes with only loose geometry. */
    return;
  }

  bke::subdiv::Subdiv *subdiv = cache.subdiv;
  OpenSubdiv_Evaluator *evaluator = subdiv->evaluator;

  gpu::VertBuf *src_buffer = evaluator->eval_output->get_face_varying_source_buf(
      face_varying_channel);
  int src_buffer_offset = evaluator->eval_output->get_face_varying_source_offset(
      face_varying_channel);

  gpu::StorageBuf *patch_arrays_buffer =
      evaluator->eval_output->create_face_varying_patch_array_buf(face_varying_channel);
  gpu::StorageBuf *patch_index_buffer = evaluator->eval_output->get_face_varying_patch_index_buf(
      face_varying_channel);
  gpu::StorageBuf *patch_param_buffer = evaluator->eval_output->get_face_varying_patch_param_buf(
      face_varying_channel);

  gpu::Shader *shader = DRW_shader_subdiv_get(SubdivShaderType::PATCH_EVALUATION_FVAR);
  GPU_shader_bind(shader);

  GPU_vertbuf_bind_as_ssbo(src_buffer, PATCH_EVALUATION_SOURCE_VERTEX_BUFFER_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(cache.gpu_patch_map.patch_map_handles,
                           PATCH_EVALUATION_INPUT_PATCH_HANDLES_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(cache.gpu_patch_map.patch_map_quadtree,
                           PATCH_EVALUATION_QUAD_NODES_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(cache.corner_patch_coords, PATCH_EVALUATION_PATCH_COORDS_BUF_SLOT);
  GPU_storagebuf_bind(patch_arrays_buffer, PATCH_EVALUATION_PATCH_ARRAY_BUFFER_BUF_SLOT);
  GPU_storagebuf_bind(patch_index_buffer, PATCH_EVALUATION_PATCH_INDEX_BUFFER_BUF_SLOT);
  GPU_storagebuf_bind(patch_param_buffer, PATCH_EVALUATION_PATCH_PARAM_BUFFER_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(uvs, PATCH_EVALUATION_OUTPUT_FVAR_BUF_SLOT);

  /* The buffer offset has the stride baked in (which is 2 as we have UVs) so remove the stride by
   * dividing by 2 */
  drw_subdiv_compute_dispatch(
      cache, shader, src_buffer_offset / 2, dst_offset, cache.num_subdiv_quads);

  /* This generates a vertex buffer, so we need to put a barrier on the vertex attribute array.
   * Since it may also be used for computing UV stretches, we also need a barrier on the shader
   * storage. */
  GPU_memory_barrier(GPU_BARRIER_VERTEX_ATTRIB_ARRAY | GPU_BARRIER_SHADER_STORAGE);

  /* Cleanup. */
  GPU_shader_unbind();

  GPU_storagebuf_free(patch_arrays_buffer);
#else
  UNUSED_VARS(cache, uvs, face_varying_channel, dst_offset);
#endif
}

void draw_subdiv_interp_custom_data(const DRWSubdivCache &cache,
                                    gpu::VertBuf &src_data,
                                    gpu::VertBuf &dst_data,
                                    GPUVertCompType comp_type,
                                    int dimensions,
                                    int dst_offset)
{
  if (!draw_subdiv_cache_need_face_data(cache)) {
    /* Happens on meshes with only loose geometry. */
    return;
  }

  gpu::Shader *shader = DRW_shader_subdiv_custom_data_get(comp_type, dimensions);
  GPU_shader_bind(shader);

  /* subdiv_face_offset is always at binding point 0 for each shader using it. */
  GPU_vertbuf_bind_as_ssbo(cache.subdiv_face_offset_buffer, SUBDIV_FACE_OFFSET_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(&src_data, CUSTOM_DATA_SOURCE_DATA_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(cache.face_ptex_offset_buffer, CUSTOM_DATA_FACE_PTEX_OFFSET_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(cache.corner_patch_coords, CUSTOM_DATA_PATCH_COORDS_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(cache.extra_coarse_face_data,
                           CUSTOM_DATA_EXTRA_COARSE_FACE_DATA_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(&dst_data, CUSTOM_DATA_DESTINATION_DATA_BUF_SLOT);

  drw_subdiv_compute_dispatch(cache, shader, 0, dst_offset, cache.num_subdiv_quads);

  /* This generates a vertex buffer, so we need to put a barrier on the vertex attribute array. Put
   * a barrier on the shader storage as we may use the result in another compute shader. */
  GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE | GPU_BARRIER_VERTEX_ATTRIB_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();
}

void draw_subdiv_interp_corner_normals(const DRWSubdivCache &cache,
                                       gpu::VertBuf &src_data,
                                       gpu::VertBuf &dst_data)
{
  if (!draw_subdiv_cache_need_face_data(cache)) {
    /* Happens on meshes with only loose geometry. */
    return;
  }

  gpu::Shader *shader = DRW_shader_subdiv_interp_corner_normals_get();
  GPU_shader_bind(shader);

  /* subdiv_face_offset is always at binding point 0 for each shader using it. */
  GPU_vertbuf_bind_as_ssbo(cache.subdiv_face_offset_buffer, SUBDIV_FACE_OFFSET_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(&src_data, CUSTOM_DATA_SOURCE_DATA_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(cache.face_ptex_offset_buffer, CUSTOM_DATA_FACE_PTEX_OFFSET_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(cache.corner_patch_coords, CUSTOM_DATA_PATCH_COORDS_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(cache.extra_coarse_face_data,
                           CUSTOM_DATA_EXTRA_COARSE_FACE_DATA_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(&dst_data, CUSTOM_DATA_DESTINATION_DATA_BUF_SLOT);

  drw_subdiv_compute_dispatch(cache, shader, 0, 0, cache.num_subdiv_quads);

  /* This generates a vertex buffer, so we need to put a barrier on the vertex attribute array. Put
   * a barrier on the shader storage as we may use the result in another compute shader. */
  GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE | GPU_BARRIER_VERTEX_ATTRIB_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();
}

void draw_subdiv_build_sculpt_data_buffer(const DRWSubdivCache &cache,
                                          gpu::VertBuf *mask_vbo,
                                          gpu::VertBuf *face_set_vbo,
                                          gpu::VertBuf *sculpt_data)
{
  gpu::Shader *shader = DRW_shader_subdiv_get(SubdivShaderType::BUFFER_SCULPT_DATA);
  GPU_shader_bind(shader);

  /* Mask VBO is always at binding point 0. */
  if (mask_vbo) {
    GPU_vertbuf_bind_as_ssbo(mask_vbo, SCULPT_DATA_SCULPT_MASK_BUF_SLOT);
  }
  GPU_vertbuf_bind_as_ssbo(face_set_vbo, SCULPT_DATA_SCULPT_FACE_SET_COLOR_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(sculpt_data, SCULPT_DATA_SCULPT_DATA_BUF_SLOT);

  drw_subdiv_compute_dispatch(cache, shader, 0, 0, cache.num_subdiv_quads, mask_vbo != nullptr);

  /* This generates a vertex buffer, so we need to put a barrier on the vertex attribute array. */
  GPU_memory_barrier(GPU_BARRIER_VERTEX_ATTRIB_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();
}

void draw_subdiv_accumulate_normals(const DRWSubdivCache &cache,
                                    gpu::VertBuf *pos,
                                    gpu::VertBuf *face_adjacency_offsets,
                                    gpu::VertBuf *face_adjacency_lists,
                                    gpu::VertBuf *vert_loop_map,
                                    gpu::VertBuf *vert_normals)
{
  gpu::Shader *shader = DRW_shader_subdiv_get(SubdivShaderType::BUFFER_NORMALS_ACCUMULATE);
  GPU_shader_bind(shader);

  GPU_vertbuf_bind_as_ssbo(pos, NORMALS_ACCUMULATE_POS_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(face_adjacency_offsets,
                           NORMALS_ACCUMULATE_FACE_ADJACENCY_OFFSETS_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(face_adjacency_lists, NORMALS_ACCUMULATE_FACE_ADJACENCY_LISTS_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(vert_loop_map, NORMALS_ACCUMULATE_VERTEX_LOOP_MAP_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(vert_normals, NORMALS_ACCUMULATE_NORMALS_BUF_SLOT);

  drw_subdiv_compute_dispatch(cache, shader, 0, 0, cache.num_subdiv_verts);

  /* This generates a vertex buffer, so we need to put a barrier on the vertex attribute array.
   * We also need it for subsequent compute shaders, so a barrier on the shader storage is also
   * needed. */
  GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE | GPU_BARRIER_VERTEX_ATTRIB_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();
}

void draw_subdiv_build_tris_buffer(const DRWSubdivCache &cache,
                                   gpu::IndexBuf *subdiv_tris,
                                   const int material_count)
{
  if (!draw_subdiv_cache_need_face_data(cache)) {
    /* Happens on meshes with only loose geometry. */
    return;
  }

  const bool do_single_material = material_count <= 1;

  gpu::Shader *shader = DRW_shader_subdiv_get(
      do_single_material ? SubdivShaderType::BUFFER_TRIS :
                           SubdivShaderType::BUFFER_TRIS_MULTIPLE_MATERIALS);
  GPU_shader_bind(shader);

  /* subdiv_face_offset is always at binding point 0 for each shader using it. */
  GPU_vertbuf_bind_as_ssbo(cache.subdiv_face_offset_buffer, SUBDIV_FACE_OFFSET_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(cache.extra_coarse_face_data, TRIS_EXTRA_COARSE_FACE_DATA_BUF_SLOT);
  if (!do_single_material) {
    GPU_vertbuf_bind_as_ssbo(cache.face_mat_offset, TRIS_FACE_MAT_OFFSET);
  }

  /* Outputs */
  GPU_indexbuf_bind_as_ssbo(subdiv_tris, TRIS_OUTPUT_TRIS_BUF_SLOT);

  drw_subdiv_compute_dispatch(cache, shader, 0, 0, cache.num_subdiv_quads);

  /* This generates an index buffer, so we need to put a barrier on the element array. */
  GPU_memory_barrier(GPU_BARRIER_ELEMENT_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();
}

void draw_subdiv_build_fdots_buffers(const DRWSubdivCache &cache,
                                     gpu::VertBuf *fdots_pos,
                                     gpu::VertBuf *fdots_nor,
                                     gpu::IndexBuf *fdots_indices)
{
#ifdef WITH_OPENSUBDIV
  if (!draw_subdiv_cache_need_face_data(cache)) {
    /* Happens on meshes with only loose geometry. */
    return;
  }

  bke::subdiv::Subdiv *subdiv = cache.subdiv;
  OpenSubdiv_Evaluator *evaluator = subdiv->evaluator;

  gpu::VertBuf *src_buffer = evaluator->eval_output->get_source_buf();
  gpu::StorageBuf *patch_arrays_buffer = evaluator->eval_output->create_patch_arrays_buf();
  gpu::StorageBuf *patch_index_buffer = evaluator->eval_output->get_patch_index_buf();
  gpu::StorageBuf *patch_param_buffer = evaluator->eval_output->get_patch_param_buf();

  gpu::Shader *shader = DRW_shader_subdiv_get(
      fdots_nor ? SubdivShaderType::PATCH_EVALUATION_FACE_DOTS_WITH_NORMALS :
                  SubdivShaderType::PATCH_EVALUATION_FACE_DOTS);
  GPU_shader_bind(shader);

  GPU_vertbuf_bind_as_ssbo(src_buffer, PATCH_EVALUATION_SOURCE_VERTEX_BUFFER_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(cache.gpu_patch_map.patch_map_handles,
                           PATCH_EVALUATION_INPUT_PATCH_HANDLES_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(cache.gpu_patch_map.patch_map_quadtree,
                           PATCH_EVALUATION_QUAD_NODES_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(cache.fdots_patch_coords, PATCH_EVALUATION_PATCH_COORDS_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(cache.verts_orig_index,
                           PATCH_EVALUATION_INPUT_VERTEX_ORIG_INDEX_BUF_SLOT);
  GPU_storagebuf_bind(patch_arrays_buffer, PATCH_EVALUATION_PATCH_ARRAY_BUFFER_BUF_SLOT);
  GPU_storagebuf_bind(patch_index_buffer, PATCH_EVALUATION_PATCH_INDEX_BUFFER_BUF_SLOT);
  GPU_storagebuf_bind(patch_param_buffer, PATCH_EVALUATION_PATCH_PARAM_BUFFER_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(fdots_pos, PATCH_EVALUATION_OUTPUT_FDOTS_VERTEX_BUFFER_BUF_SLOT);
  /* F-dots normals may not be requested, still reserve the binding point. */
  if (fdots_nor) {
    GPU_vertbuf_bind_as_ssbo(fdots_nor, PATCH_EVALUATION_OUTPUT_NORMALS_BUF_SLOT);
  }
  GPU_indexbuf_bind_as_ssbo(fdots_indices, PATCH_EVALUATION_OUTPUT_INDICES_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(cache.extra_coarse_face_data,
                           PATCH_EVALUATION_EXTRA_COARSE_FACE_DATA_BUF_SLOT);

  drw_subdiv_compute_dispatch(cache, shader, 0, 0, cache.num_coarse_faces);

  /* This generates two vertex buffers and an index buffer, so we need to put a barrier on the
   * vertex attributes and element arrays. */
  GPU_memory_barrier(GPU_BARRIER_VERTEX_ATTRIB_ARRAY | GPU_BARRIER_ELEMENT_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();

  GPU_storagebuf_free(patch_arrays_buffer);
#else
  UNUSED_VARS(cache, fdots_pos, fdots_nor, fdots_indices);
#endif
}

void draw_subdiv_build_lines_buffer(const DRWSubdivCache &cache, gpu::IndexBuf *lines_indices)
{
  gpu::Shader *shader = DRW_shader_subdiv_get(SubdivShaderType::BUFFER_LINES);
  GPU_shader_bind(shader);

  GPU_vertbuf_bind_as_ssbo(cache.subdiv_face_offset_buffer, SUBDIV_FACE_OFFSET_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(cache.edges_draw_flag, LINES_INPUT_EDGE_DRAW_FLAG_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(cache.extra_coarse_face_data, LINES_EXTRA_COARSE_FACE_DATA_BUF_SLOT);
  GPU_indexbuf_bind_as_ssbo(lines_indices, LINES_OUTPUT_LINES_BUF_SLOT);

  drw_subdiv_compute_dispatch(cache, shader, 0, 0, cache.num_subdiv_quads);

  /* This generates an index buffer, so we need to put a barrier on the element array. */
  GPU_memory_barrier(GPU_BARRIER_ELEMENT_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();
}

void draw_subdiv_build_lines_loose_buffer(const DRWSubdivCache &cache,
                                          gpu::IndexBuf *lines_indices,
                                          gpu::VertBuf *lines_flags,
                                          uint edge_loose_offset,
                                          uint num_loose_edges)
{
  gpu::Shader *shader = DRW_shader_subdiv_get(SubdivShaderType::BUFFER_LINES_LOOSE);
  GPU_shader_bind(shader);

  GPU_indexbuf_bind_as_ssbo(lines_indices, LINES_OUTPUT_LINES_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(lines_flags, LINES_LINES_LOOSE_FLAGS);

  drw_subdiv_compute_dispatch(cache, shader, 0, 0, num_loose_edges, false, edge_loose_offset);

  /* This generates an index buffer, so we need to put a barrier on the element array. */
  GPU_memory_barrier(GPU_BARRIER_ELEMENT_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();
}

void draw_subdiv_build_edge_fac_buffer(const DRWSubdivCache &cache,
                                       gpu::VertBuf *pos,
                                       gpu::VertBuf *edge_draw_flag,
                                       gpu::VertBuf *poly_other_map,
                                       gpu::VertBuf *edge_fac)
{
  gpu::Shader *shader = DRW_shader_subdiv_get(SubdivShaderType::BUFFER_EDGE_FAC);
  GPU_shader_bind(shader);

  GPU_vertbuf_bind_as_ssbo(pos, EDGE_FAC_POS_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(edge_draw_flag, EDGE_FAC_EDGE_DRAW_FLAG_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(poly_other_map, EDGE_FAC_POLY_OTHER_MAP_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(edge_fac, EDGE_FAC_EDGE_FAC_BUF_SLOT);

  drw_subdiv_compute_dispatch(cache, shader, 0, 0, cache.num_subdiv_quads);

  /* This generates a vertex buffer, so we need to put a barrier on the vertex attribute array. */
  GPU_memory_barrier(GPU_BARRIER_VERTEX_ATTRIB_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();
}

void draw_subdiv_build_lnor_buffer(const DRWSubdivCache &cache,
                                   gpu::VertBuf *pos,
                                   gpu::VertBuf *vert_normals,
                                   gpu::VertBuf *subdiv_corner_verts,
                                   gpu::VertBuf *lnor)
{
  if (!draw_subdiv_cache_need_face_data(cache)) {
    /* Happens on meshes with only loose geometry. */
    return;
  }

  gpu::Shader *shader = DRW_shader_subdiv_get(SubdivShaderType::BUFFER_LNOR);
  GPU_shader_bind(shader);

  /* Inputs */
  GPU_vertbuf_bind_as_ssbo(cache.subdiv_face_offset_buffer, SUBDIV_FACE_OFFSET_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(pos, LOOP_NORMALS_POS_SLOT);
  GPU_vertbuf_bind_as_ssbo(cache.extra_coarse_face_data,
                           LOOP_NORMALS_EXTRA_COARSE_FACE_DATA_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(vert_normals, LOOP_NORMALS_VERT_NORMALS_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(subdiv_corner_verts, LOOP_NORMALS_VERTEX_LOOP_MAP_BUF_SLOT);

  /* Outputs */
  GPU_vertbuf_bind_as_ssbo(lnor, LOOP_NORMALS_OUTPUT_LNOR_BUF_SLOT);

  drw_subdiv_compute_dispatch(cache, shader, 0, 0, cache.num_subdiv_quads);

  /* This generates a vertex buffer, so we need to put a barrier on the vertex attribute array. */
  GPU_memory_barrier(GPU_BARRIER_VERTEX_ATTRIB_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();
}

void draw_subdiv_build_paint_overlay_flag_buffer(const DRWSubdivCache &cache, gpu::VertBuf &flags)
{
  if (!draw_subdiv_cache_need_face_data(cache)) {
    /* Happens on meshes with only loose geometry. */
    return;
  }

  gpu::Shader *shader = DRW_shader_subdiv_get(SubdivShaderType::BUFFER_PAINT_OVERLAY_FLAG);
  GPU_shader_bind(shader);

  /* Inputs */
  GPU_vertbuf_bind_as_ssbo(cache.subdiv_face_offset_buffer, SUBDIV_FACE_OFFSET_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(cache.extra_coarse_face_data,
                           PAINT_OVERLAY_EXTRA_COARSE_FACE_DATA_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(cache.verts_orig_index, PAINT_OVERLAY_EXTRA_INPUT_VERT_ORIG_INDEX_SLOT);

  /* Outputs */
  GPU_vertbuf_bind_as_ssbo(&flags, PAINT_OVERLAY_OUTPUT_FLAG_SLOT);

  drw_subdiv_compute_dispatch(cache, shader, 0, 0, cache.num_subdiv_quads);

  /* This generates a vertex buffer, so we need to put a barrier on the vertex attribute array. */
  GPU_memory_barrier(GPU_BARRIER_VERTEX_ATTRIB_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();
}

void draw_subdiv_build_edituv_stretch_area_buffer(const DRWSubdivCache &cache,
                                                  gpu::VertBuf *coarse_data,
                                                  gpu::VertBuf *subdiv_data)
{
  gpu::Shader *shader = DRW_shader_subdiv_get(SubdivShaderType::BUFFER_UV_STRETCH_AREA);
  GPU_shader_bind(shader);

  /* Inputs */
  /* subdiv_face_offset is always at binding point 0 for each shader using it. */
  GPU_vertbuf_bind_as_ssbo(cache.subdiv_face_offset_buffer, SUBDIV_FACE_OFFSET_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(coarse_data, STRETCH_AREA_COARSE_STRETCH_AREA_BUF_SLOT);
  /* Outputs */
  GPU_vertbuf_bind_as_ssbo(subdiv_data, STRETCH_AREA_SUBDIV_STRETCH_AREA_BUF_SLOT);

  drw_subdiv_compute_dispatch(cache, shader, 0, 0, cache.num_subdiv_quads);

  /* This generates a vertex buffer, so we need to put a barrier on the vertex attribute array. */
  GPU_memory_barrier(GPU_BARRIER_VERTEX_ATTRIB_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();
}

void draw_subdiv_build_edituv_stretch_angle_buffer(const DRWSubdivCache &cache,
                                                   gpu::VertBuf *pos,
                                                   gpu::VertBuf *uvs,
                                                   int uvs_offset,
                                                   gpu::VertBuf *stretch_angles)
{
  gpu::Shader *shader = DRW_shader_subdiv_get(SubdivShaderType::BUFFER_UV_STRETCH_ANGLE);
  GPU_shader_bind(shader);

  /* Inputs */
  GPU_vertbuf_bind_as_ssbo(pos, STRETCH_ANGLE_POS_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(uvs, STRETCH_ANGLE_UVS_BUF_SLOT);
  /* Outputs */
  GPU_vertbuf_bind_as_ssbo(stretch_angles, STRETCH_ANGLE_UV_STRETCHES_BUF_SLOT);

  drw_subdiv_compute_dispatch(cache, shader, uvs_offset, 0, cache.num_subdiv_quads);

  /* This generates a vertex buffer, so we need to put a barrier on the vertex attribute array. */
  GPU_memory_barrier(GPU_BARRIER_VERTEX_ATTRIB_ARRAY);

  /* Cleanup. */
  GPU_shader_unbind();
}

/* -------------------------------------------------------------------- */

/**
 * For material assignments we want indices for triangles that share a common material to be laid
 * out contiguously in memory. To achieve this, we sort the indices based on which material the
 * coarse face was assigned. The sort is performed by offsetting the loops indices so that they
 * are directly assigned to the right sorted indices.
 *
 * \code{.unparsed}
 * Here is a visual representation, considering four quads:
 * +---------+---------+---------+---------+
 * | 3     2 | 7     6 | 11   10 | 15   14 |
 * |         |         |         |         |
 * | 0     1 | 4     5 | 8     9 | 12   13 |
 * +---------+---------+---------+---------+
 *
 * If the first and third quads have the same material, we should have:
 * +---------+---------+---------+---------+
 * | 3     2 | 11   10 | 7     6 | 15   14 |
 * |         |         |         |         |
 * | 0     1 | 8     9 | 4     5 | 12   13 |
 * +---------+---------+---------+---------+
 *
 * So the offsets would be:
 * +---------+---------+---------+---------+
 * | 0     0 | 4     4 | -4   -4 | 0     0 |
 * |         |         |         |         |
 * | 0     0 | 4     4 | -4   -4 | 0     0 |
 * +---------+---------+---------+---------+
 * \endcode
 *
 * The offsets are computed not based on the loops indices, but on the number of subdivided
 * polygons for each coarse face. We then only store a single offset for each coarse face,
 * since all sub-faces are contiguous, they all share the same offset.
 */
static void draw_subdiv_cache_ensure_mat_offsets(DRWSubdivCache &cache,
                                                 const Mesh *mesh_eval,
                                                 uint mat_len)
{
  draw_subdiv_cache_free_material_data(cache);

  const int number_of_quads = cache.num_subdiv_loops / 4;

  if (mat_len == 1) {
    cache.mat_start = MEM_callocN<int>("subdiv mat_end");
    cache.mat_end = MEM_callocN<int>("subdiv mat_end");
    cache.mat_start[0] = 0;
    cache.mat_end[0] = number_of_quads;
    return;
  }

  const bke::AttributeAccessor attributes = mesh_eval->attributes();
  const VArraySpan<int> material_indices = *attributes.lookup_or_default<int>(
      "material_index", bke::AttrDomain::Face, 0);

  /* Count number of subdivided polygons for each material. */
  int *mat_start = MEM_calloc_arrayN<int>(mat_len, "subdiv mat_start");
  int *subdiv_face_offset = cache.subdiv_face_offset;

  /* TODO: parallel_reduce? */
  for (int i = 0; i < mesh_eval->faces_num; i++) {
    const int next_offset = (i == mesh_eval->faces_num - 1) ? number_of_quads :
                                                              subdiv_face_offset[i + 1];
    const int quad_count = next_offset - subdiv_face_offset[i];
    const uint mat_index = uint(material_indices[i]) < mat_len ? uint(material_indices[i]) : 0;
    mat_start[mat_index] += quad_count;
  }

  /* Accumulate offsets. */
  int ofs = mat_start[0];
  mat_start[0] = 0;
  for (uint i = 1; i < mat_len; i++) {
    int tmp = mat_start[i];
    mat_start[i] = ofs;
    ofs += tmp;
  }

  /* Compute per face offsets. */
  int *mat_end = static_cast<int *>(MEM_dupallocN(mat_start));
  int *per_face_mat_offset = MEM_malloc_arrayN<int>(mesh_eval->faces_num, "per_face_mat_offset");

  for (int i = 0; i < mesh_eval->faces_num; i++) {
    const uint mat_index = uint(material_indices[i]) < mat_len ? uint(material_indices[i]) : 0;
    const int single_material_index = subdiv_face_offset[i];
    const int material_offset = mat_end[mat_index];
    const int next_offset = (i == mesh_eval->faces_num - 1) ? number_of_quads :
                                                              subdiv_face_offset[i + 1];
    const int quad_count = next_offset - subdiv_face_offset[i];
    mat_end[mat_index] += quad_count;

    per_face_mat_offset[i] = material_offset - single_material_index;
  }

  cache.face_mat_offset = draw_subdiv_build_origindex_buffer(per_face_mat_offset,
                                                             mesh_eval->faces_num);
  cache.mat_start = mat_start;
  cache.mat_end = mat_end;

  MEM_freeN(per_face_mat_offset);
}

/**
 * The evaluators are owned by the `OpenSubdiv_EvaluatorCache` which is being referenced by
 * `bke::subdiv::Subdiv->evaluator`. So the evaluator cache cannot be freed until all references
 * are gone. The user counting allows to free the evaluator when there is no more subdiv.
 */
static OpenSubdiv_EvaluatorCache *g_subdiv_evaluator_cache = nullptr;
static uint64_t g_subdiv_evaluator_users = 0;
/* The evaluator cache is global, so we cannot allow concurrent usage and need synchronization. */
static Mutex g_subdiv_eval_mutex;

static bool draw_subdiv_create_requested_buffers(Object &ob,
                                                 Mesh &mesh,
                                                 MeshBatchCache &batch_cache,
                                                 MeshBufferCache &mbc,
                                                 const Span<IBOType> ibo_requests,
                                                 const Span<VBOType> vbo_requests,
                                                 const bool is_editmode,
                                                 const bool is_paint_mode,
                                                 const bool do_final,
                                                 const bool do_uvedit,
                                                 const bool do_cage,
                                                 const ToolSettings *ts,
                                                 const bool use_hide)
{
  SubsurfRuntimeData *runtime_data = mesh.runtime->subsurf_runtime_data;
  BLI_assert(runtime_data && runtime_data->has_gpu_subdiv);

  if (runtime_data->settings.level == 0) {
    return false;
  }

  const Mesh *mesh_eval = &mesh;
  BMesh *bm = nullptr;
  if (mesh.runtime->edit_mesh) {
    mesh_eval = BKE_object_get_editmesh_eval_final(&ob);
    bm = mesh.runtime->edit_mesh->bm;
  }

#ifdef WITH_OPENSUBDIV
  draw_subdiv_invalidate_evaluator_for_orco(runtime_data->subdiv_gpu, mesh_eval);
#endif

  bke::subdiv::Subdiv *subdiv = BKE_subsurf_modifier_subdiv_descriptor_ensure(
      runtime_data, mesh_eval, true);
  if (!subdiv) {
    return false;
  }

  /* Lock the entire evaluation to avoid concurrent usage of shader objects in evaluator cache. */
  std::scoped_lock lock(g_subdiv_eval_mutex);

  if (g_subdiv_evaluator_cache == nullptr) {
    g_subdiv_evaluator_cache = openSubdiv_createEvaluatorCache(OPENSUBDIV_EVALUATOR_GPU);
  }

  /* Increment evaluator cache reference if an evaluator has been assigned to it. */
  bool evaluator_might_be_assigned = subdiv->evaluator == nullptr;
  auto maybe_increment_cache_ref = [evaluator_might_be_assigned](bke::subdiv::Subdiv *subdiv) {
    if (evaluator_might_be_assigned && subdiv->evaluator != nullptr) {
      /* An evaluator was assigned. */
      g_subdiv_evaluator_users++;
    }
  };

  if (!bke::subdiv::eval_begin_from_mesh(
          subdiv, mesh_eval, bke::subdiv::SUBDIV_EVALUATOR_TYPE_GPU, {}, g_subdiv_evaluator_cache))
  {
    /* This could happen in two situations:
     * - OpenSubdiv is disabled.
     * - Something totally bad happened, and OpenSubdiv rejected our topology.
     * In either way, we can't safely continue. However, we still have to handle potential loose
     * geometry, which is done separately. */
    if (mesh_eval->faces_num) {
      maybe_increment_cache_ref(subdiv);
      return false;
    }
  }

  DRWSubdivCache &draw_cache = mesh_batch_cache_ensure_subdiv_cache(batch_cache);

  draw_cache.optimal_display = runtime_data->use_optimal_display;
  /* If there is no distinct cage, hide unmapped edges that can't be selected. */
  draw_cache.hide_unmapped_edges = is_editmode && !do_cage;
  draw_cache.bm = bm;
  draw_cache.mesh = mesh_eval;
  draw_cache.subdiv = subdiv;

#ifdef WITH_OPENSUBDIV
  if (!draw_subdiv_build_cache(draw_cache, subdiv, mesh_eval, runtime_data)) {
    maybe_increment_cache_ref(subdiv);
    return false;
  }
#endif

  draw_cache.num_subdiv_triangles = tris_count_from_number_of_loops(draw_cache.num_subdiv_loops);

  /* Copy topology information for stats display. */
  runtime_data->stats_totvert = draw_cache.num_subdiv_verts;
  runtime_data->stats_totedge = draw_cache.num_subdiv_edges;
  runtime_data->stats_faces_num = draw_cache.num_subdiv_quads;
  runtime_data->stats_totloop = draw_cache.num_subdiv_loops;

  draw_cache.use_custom_loop_normals = (runtime_data->use_loop_normals) &&
                                       mesh_eval->attributes().contains("custom_normal");

  if (ibo_requests.contains(IBOType::Tris)) {
    draw_subdiv_cache_ensure_mat_offsets(draw_cache, mesh_eval, batch_cache.mat_len);
  }

  MeshRenderData mr = mesh_render_data_create(
      ob, mesh, is_editmode, is_paint_mode, do_final, do_uvedit, use_hide, ts);
  draw_cache.use_hide = use_hide;

  /* Used for setting loop normals flags. Mapped extraction is only used during edit mode.
   * See comments in #extract_lnor_iter_face_mesh.
   */
  draw_cache.is_edit_mode = mr.edit_bmesh != nullptr;

  draw_subdiv_cache_update_extra_coarse_face_data(draw_cache, mesh_eval, mr);

  mesh_buffer_cache_create_requested_subdiv(
      batch_cache, mbc, ibo_requests, vbo_requests, draw_cache, mr);

  maybe_increment_cache_ref(subdiv);
  return true;
}

void DRW_subdivide_loose_geom(DRWSubdivCache &subdiv_cache, const MeshBufferCache &cache)
{
  const Span<int> loose_edges = cache.loose_geom.edges;
  if (loose_edges.is_empty()) {
    return;
  }

  if (!subdiv_cache.loose_edge_positions.is_empty()) {
    /* Already processed. */
    return;
  }

  const Mesh *coarse_mesh = subdiv_cache.mesh;
  const bool is_simple = subdiv_cache.subdiv->settings.is_simple;
  const int resolution = subdiv_cache.resolution;
  const int resolution_1 = resolution - 1;
  const float inv_resolution_1 = 1.0f / float(resolution_1);

  const Span<float3> coarse_positions = coarse_mesh->vert_positions();
  const Span<int2> coarse_edges = coarse_mesh->edges();

  Array<int> vert_to_edge_offsets;
  Array<int> vert_to_edge_indices;
  const GroupedSpan<int> vert_to_edge_map = bke::mesh::build_vert_to_edge_map(
      coarse_edges, coarse_mesh->verts_num, vert_to_edge_offsets, vert_to_edge_indices);

  /* Also store the last vertex to simplify copying the positions to the VBO. */
  subdiv_cache.loose_edge_positions.reinitialize(loose_edges.size() * resolution);
  MutableSpan<float3> edge_positions = subdiv_cache.loose_edge_positions;

  threading::parallel_for(loose_edges.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      const int coarse_edge = loose_edges[i];
      MutableSpan positions = edge_positions.slice(i * resolution, resolution);
      for (const int j : positions.index_range()) {
        positions[j] = bke::subdiv::mesh_interpolate_position_on_edge(coarse_positions,
                                                                      coarse_edges,
                                                                      vert_to_edge_map,
                                                                      coarse_edge,
                                                                      is_simple,
                                                                      j * inv_resolution_1);
      }
    }
  });
}

/**
 * The #bke::subdiv::Subdiv data is being owned the modifier.
 * Since the modifier can be freed from any thread (e.g. from depsgraph multi-threaded update)
 * which may not have a valid #GPUContext active, we move the data to discard to this free list
 * until a code-path with a active GPUContext is hit.
 * This is kind of garbage collection.
 */
static LinkNode *gpu_subdiv_free_queue = nullptr;
static blender::Mutex gpu_subdiv_queue_mutex;

void DRW_create_subdivision(Object &ob,
                            Mesh &mesh,
                            MeshBatchCache &batch_cache,
                            MeshBufferCache &mbc,
                            const Span<IBOType> ibo_requests,
                            const Span<VBOType> vbo_requests,
                            const bool is_editmode,
                            const bool is_paint_mode,
                            const bool do_final,
                            const bool do_uvedit,
                            const bool do_cage,
                            const ToolSettings *ts,
                            const bool use_hide)
{

#undef TIME_SUBDIV

#ifdef TIME_SUBDIV
  const double begin_time = BLI_time_now_seconds();
#endif

  if (!draw_subdiv_create_requested_buffers(ob,
                                            mesh,
                                            batch_cache,
                                            mbc,
                                            ibo_requests,
                                            vbo_requests,
                                            is_editmode,
                                            is_paint_mode,
                                            do_final,
                                            do_uvedit,
                                            do_cage,
                                            ts,
                                            use_hide))
  {
    /* Did not run. */
    return;
  }

#ifdef TIME_SUBDIV
  const double end_time = BLI_time_now_seconds();
  fprintf(stderr, "Time to update subdivision: %f\n", end_time - begin_time);
  fprintf(stderr, "Maximum FPS: %f\n", 1.0 / (end_time - begin_time));
#endif
}

void DRW_subdiv_cache_free(bke::subdiv::Subdiv *subdiv)
{
  std::scoped_lock lock(gpu_subdiv_queue_mutex);
  BLI_linklist_prepend(&gpu_subdiv_free_queue, subdiv);
}

void DRW_cache_free_old_subdiv()
{
  {
    std::scoped_lock lock(gpu_subdiv_queue_mutex);

    while (gpu_subdiv_free_queue != nullptr) {
      bke::subdiv::Subdiv *subdiv = static_cast<bke::subdiv::Subdiv *>(
          BLI_linklist_pop(&gpu_subdiv_free_queue));

      {
        std::scoped_lock lock(g_subdiv_eval_mutex);
        if (subdiv->evaluator != nullptr) {
          g_subdiv_evaluator_users--;
        }
      }
#ifdef WITH_OPENSUBDIV
      /* Set the type to CPU so that we do actually free the cache. */
      subdiv->evaluator->type = OPENSUBDIV_EVALUATOR_CPU;
#endif
      bke::subdiv::free(subdiv);
    }
  }

  {
    std::scoped_lock lock(g_subdiv_eval_mutex);
    /* Free evaluator cache if there is no more reference to it.. */
    if (g_subdiv_evaluator_users == 0) {
      openSubdiv_deleteEvaluatorCache(g_subdiv_evaluator_cache);
      g_subdiv_evaluator_cache = nullptr;
    }
  }
}

}  // namespace blender::draw
