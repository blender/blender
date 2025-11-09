/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Mesh API for render engines
 */

#include <array>
#include <optional>

#include "MEM_guardedalloc.h"

#include "BLI_index_range.hh"
#include "BLI_listbase.h"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_editmesh.hh"
#include "BKE_material.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"
#include "BKE_object_deform.h"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_subdiv_modifier.hh"

#include "GPU_batch.hh"
#include "GPU_material.hh"

#include "DRW_render.hh"

#include "draw_cache_extract.hh"
#include "draw_cache_inline.hh"
#include "draw_subdivision.hh"

#include "draw_cache_impl.hh" /* own include */
#include "draw_context_private.hh"

#include "mesh_extractors/extract_mesh.hh"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Dependencies between buffer and batch
 * \{ */

#define TRIS_PER_MAT_INDEX BUFFER_LEN

static void mesh_batch_cache_clear(MeshBatchCache &cache);

static void discard_buffers(MeshBatchCache &cache,
                            const Span<VBOType> vbos,
                            const Span<IBOType> ibos)
{
  Set<const void *, 16> buffer_ptrs;
  buffer_ptrs.reserve(vbos.size() + ibos.size());
  FOREACH_MESH_BUFFER_CACHE (cache, mbc) {
    for (const VBOType vbo : vbos) {
      if (const auto *buffer = mbc->buff.vbos.lookup_ptr(vbo)) {
        buffer_ptrs.add(buffer->get());
      }
    }
  }
  FOREACH_MESH_BUFFER_CACHE (cache, mbc) {
    for (const IBOType ibo : ibos) {
      if (const auto *buffer = mbc->buff.ibos.lookup_ptr(ibo)) {
        buffer_ptrs.add(buffer->get());
      }
    }
  }

  const auto batch_contains_data = [&](gpu::Batch &batch) {
    if (buffer_ptrs.contains(batch.elem)) {
      return true;
    }
    if (std::any_of(batch.verts, batch.verts + ARRAY_SIZE(batch.verts), [&](gpu::VertBuf *vbo) {
          return vbo && buffer_ptrs.contains(vbo);
        }))
    {
      return true;
    }
    return false;
  };

  for (const int i : IndexRange(MBC_BATCH_LEN)) {
    gpu::Batch *batch = ((gpu::Batch **)&cache.batch)[i];
    if (batch && batch_contains_data(*batch)) {
      GPU_BATCH_DISCARD_SAFE(((gpu::Batch **)&cache.batch)[i]);
      cache.batch_ready &= ~DRWBatchFlag(uint64_t(1u) << i);
    }
  }

  if (!cache.surface_per_mat.is_empty()) {
    if (cache.surface_per_mat.first() && batch_contains_data(*cache.surface_per_mat.first())) {
      /* The format for all `surface_per_mat` batches is the same, discard them all. */
      for (const int i : cache.surface_per_mat.index_range()) {
        GPU_BATCH_DISCARD_SAFE(cache.surface_per_mat[i]);
      }
      cache.batch_ready &= ~(MBC_SURFACE | MBC_SURFACE_PER_MAT);
    }
  }

  for (const VBOType vbo : vbos) {
    cache.final.buff.vbos.remove(vbo);
    cache.cage.buff.vbos.remove(vbo);
    cache.uv_cage.buff.vbos.remove(vbo);
  }
  for (const IBOType ibo : ibos) {
    cache.final.buff.ibos.remove(ibo);
    cache.cage.buff.ibos.remove(ibo);
    cache.uv_cage.buff.ibos.remove(ibo);
  }
}

BLI_INLINE void mesh_cd_layers_type_merge(DRW_MeshCDMask *a, const DRW_MeshCDMask &b)
{
  drw_attributes_merge(&a->uv, &b.uv);
  drw_attributes_merge(&a->tan, &b.tan);
  a->orco |= b.orco;
  a->tan_orco |= b.tan_orco;
  a->sculpt_overlays |= b.sculpt_overlays;
  a->edit_uv |= b.edit_uv;
}

static void mesh_cd_calc_edit_uv_layer(const Mesh & /*mesh*/, DRW_MeshCDMask *cd_used)
{
  cd_used->edit_uv = 1;
}

static void mesh_cd_calc_active_uv_layer(const Object &object,
                                         const Mesh &mesh,
                                         DRW_MeshCDMask &cd_used)
{
  const Mesh &me_final = editmesh_final_or_this(object, mesh);
  const StringRef active_uv_map = me_final.active_uv_map_name();
  if (!active_uv_map.is_empty()) {
    cd_used.uv.add_as(active_uv_map);
  }
}

static void mesh_cd_calc_active_mask_uv_layer(const Object &object,
                                              const Mesh &mesh,
                                              DRW_MeshCDMask &cd_used)
{
  const Mesh &me_final = editmesh_final_or_this(object, mesh);
  const CustomData &cd_ldata = mesh_cd_ldata_get_from_mesh(me_final);
  int layer = CustomData_get_stencil_layer_index(&cd_ldata, CD_PROP_FLOAT2);
  if (layer != -1) {
    cd_used.uv.add_as(cd_ldata.layers[layer].name);
  }
}

static bool attribute_exists(const Mesh &mesh, const StringRef name)
{
  if (BMEditMesh *em = mesh.runtime->edit_mesh.get()) {
    return bool(BM_data_layer_lookup(*em->bm, name));
  }
  return mesh.attributes().contains(name);
};

static std::optional<bke::AttributeMetaData> lookup_meta_data(const Mesh &mesh,
                                                              const StringRef name)
{
  if (BMEditMesh *em = mesh.runtime->edit_mesh.get()) {
    if (const BMDataLayerLookup attr = BM_data_layer_lookup(*em->bm, name)) {
      return bke::AttributeMetaData{attr.domain, attr.type};
    }
    return std::nullopt;
  }
  return mesh.attributes().lookup_meta_data(name);
}

static void mesh_cd_calc_used_gpu_layers(const Object &object,
                                         const Mesh &mesh,
                                         const Span<const GPUMaterial *> materials,
                                         VectorSet<std::string> *r_attributes,
                                         DRW_MeshCDMask *r_cd_used)
{
  constexpr bke::AttributeMetaData UV_METADATA{bke::AttrDomain::Corner, bke::AttrType::Float2};
  const Mesh &me_final = editmesh_final_or_this(object, mesh);

  for (const GPUMaterial *gpumat : materials) {
    if (gpumat == nullptr) {
      continue;
    }
    ListBase gpu_attrs = GPU_material_attributes(gpumat);
    LISTBASE_FOREACH (GPUMaterialAttribute *, gpu_attr, &gpu_attrs) {

      if (gpu_attr->is_default_color) {
        const StringRef default_color_name = me_final.default_color_attribute;
        if (attribute_exists(me_final, default_color_name)) {
          drw_attributes_add_request(r_attributes, default_color_name);
        }
        continue;
      }

      if (gpu_attr->type == CD_ORCO) {
        r_cd_used->orco = true;
        continue;
      }

      StringRef name = gpu_attr->name;

      if (gpu_attr->type == CD_TANGENT) {
        if (name.is_empty()) {
          const StringRef default_name = me_final.default_uv_map_name();
          if (!default_name.is_empty()) {
            name = default_name;
          }
        }
        if (lookup_meta_data(mesh, name) == UV_METADATA) {
          r_cd_used->tan.add(name);
        }
        else {
          r_cd_used->tan_orco = true;
          r_cd_used->orco = true;
        }

        continue;
      }

      if (name.is_empty()) {
        const StringRef default_name = me_final.default_uv_map_name();
        if (!default_name.is_empty()) {
          if (lookup_meta_data(mesh, default_name) == UV_METADATA) {
            r_cd_used->uv.add(default_name);
          }
        }
        continue;
      }

      const std::optional<bke::AttributeMetaData> meta_data = lookup_meta_data(mesh, name);
      if (!meta_data) {
        continue;
      }
      if (meta_data == UV_METADATA) {
        r_cd_used->uv.add(name);
        continue;
      }
      drw_attributes_add_request(r_attributes, name);
    }
  }
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Vertex Group Selection
 * \{ */

/** Reset the selection structure, deallocating heap memory as appropriate. */
static void drw_mesh_weight_state_clear(DRW_MeshWeightState *wstate)
{
  MEM_SAFE_FREE(wstate->defgroup_sel);
  MEM_SAFE_FREE(wstate->defgroup_locked);
  MEM_SAFE_FREE(wstate->defgroup_unlocked);

  memset(wstate, 0, sizeof(*wstate));

  wstate->defgroup_active = -1;
}

/** Copy selection data from one structure to another, including heap memory. */
static void drw_mesh_weight_state_copy(DRW_MeshWeightState *wstate_dst,
                                       const DRW_MeshWeightState *wstate_src)
{
  MEM_SAFE_FREE(wstate_dst->defgroup_sel);
  MEM_SAFE_FREE(wstate_dst->defgroup_locked);
  MEM_SAFE_FREE(wstate_dst->defgroup_unlocked);

  memcpy(wstate_dst, wstate_src, sizeof(*wstate_dst));

  if (wstate_src->defgroup_sel) {
    wstate_dst->defgroup_sel = static_cast<bool *>(MEM_dupallocN(wstate_src->defgroup_sel));
  }
  if (wstate_src->defgroup_locked) {
    wstate_dst->defgroup_locked = static_cast<bool *>(MEM_dupallocN(wstate_src->defgroup_locked));
  }
  if (wstate_src->defgroup_unlocked) {
    wstate_dst->defgroup_unlocked = static_cast<bool *>(
        MEM_dupallocN(wstate_src->defgroup_unlocked));
  }
}

static bool drw_mesh_flags_equal(const bool *array1, const bool *array2, int size)
{
  return ((!array1 && !array2) ||
          (array1 && array2 && memcmp(array1, array2, size * sizeof(bool)) == 0));
}

/** Compare two selection structures. */
static bool drw_mesh_weight_state_compare(const DRW_MeshWeightState *a,
                                          const DRW_MeshWeightState *b)
{
  return a->defgroup_active == b->defgroup_active && a->defgroup_len == b->defgroup_len &&
         a->flags == b->flags && a->alert_mode == b->alert_mode &&
         a->defgroup_sel_count == b->defgroup_sel_count &&
         drw_mesh_flags_equal(a->defgroup_sel, b->defgroup_sel, a->defgroup_len) &&
         drw_mesh_flags_equal(a->defgroup_locked, b->defgroup_locked, a->defgroup_len) &&
         drw_mesh_flags_equal(a->defgroup_unlocked, b->defgroup_unlocked, a->defgroup_len);
}

static void drw_mesh_weight_state_extract(
    Object &ob, Mesh &mesh, const ToolSettings &ts, bool paint_mode, DRW_MeshWeightState *wstate)
{
  /* Extract complete vertex weight group selection state and mode flags. */
  memset(wstate, 0, sizeof(*wstate));

  wstate->defgroup_active = mesh.vertex_group_active_index - 1;
  wstate->defgroup_len = BLI_listbase_count(&mesh.vertex_group_names);

  wstate->alert_mode = ts.weightuser;

  if (paint_mode && ts.multipaint) {
    /* Multi-paint needs to know all selected bones, not just the active group.
     * This is actually a relatively expensive operation, but caching would be difficult. */
    wstate->defgroup_sel = BKE_object_defgroup_selected_get(
        &ob, wstate->defgroup_len, &wstate->defgroup_sel_count);

    if (wstate->defgroup_sel_count > 1) {
      wstate->flags |= DRW_MESH_WEIGHT_STATE_MULTIPAINT |
                       (ts.auto_normalize ? DRW_MESH_WEIGHT_STATE_AUTO_NORMALIZE : 0);

      if (ME_USING_MIRROR_X_VERTEX_GROUPS(&mesh)) {
        BKE_object_defgroup_mirror_selection(&ob,
                                             wstate->defgroup_len,
                                             wstate->defgroup_sel,
                                             wstate->defgroup_sel,
                                             &wstate->defgroup_sel_count);
      }
    }
    /* With only one selected bone Multi-paint reverts to regular mode. */
    else {
      wstate->defgroup_sel_count = 0;
      MEM_SAFE_FREE(wstate->defgroup_sel);
    }
  }

  if (paint_mode && ts.wpaint_lock_relative) {
    /* Set of locked vertex groups for the lock relative mode. */
    wstate->defgroup_locked = BKE_object_defgroup_lock_flags_get(&ob, wstate->defgroup_len);
    wstate->defgroup_unlocked = BKE_object_defgroup_validmap_get(&ob, wstate->defgroup_len);

    /* Check that a deform group is active, and none of selected groups are locked. */
    if (BKE_object_defgroup_check_lock_relative(
            wstate->defgroup_locked, wstate->defgroup_unlocked, wstate->defgroup_active) &&
        BKE_object_defgroup_check_lock_relative_multi(wstate->defgroup_len,
                                                      wstate->defgroup_locked,
                                                      wstate->defgroup_sel,
                                                      wstate->defgroup_sel_count))
    {
      wstate->flags |= DRW_MESH_WEIGHT_STATE_LOCK_RELATIVE;

      /* Compute the set of locked and unlocked deform vertex groups. */
      BKE_object_defgroup_split_locked_validmap(wstate->defgroup_len,
                                                wstate->defgroup_locked,
                                                wstate->defgroup_unlocked,
                                                wstate->defgroup_locked, /* out */
                                                wstate->defgroup_unlocked);
    }
    else {
      MEM_SAFE_FREE(wstate->defgroup_unlocked);
      MEM_SAFE_FREE(wstate->defgroup_locked);
    }
  }
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Mesh gpu::Batch Cache
 * \{ */

/* gpu::Batch cache management. */

static bool mesh_batch_cache_valid(Mesh &mesh)
{
  MeshBatchCache *cache = static_cast<MeshBatchCache *>(mesh.runtime->batch_cache);

  if (cache == nullptr) {
    return false;
  }

  /* NOTE: bke::pbvh::Tree draw data should not be checked here. */

  if (cache->is_editmode != (mesh.runtime->edit_mesh != nullptr)) {
    return false;
  }

  if (cache->is_dirty) {
    return false;
  }

  if (cache->mat_len != BKE_id_material_used_with_fallback_eval(mesh.id)) {
    return false;
  }

  return true;
}

static void mesh_batch_cache_init(Mesh &mesh)
{
  if (!mesh.runtime->batch_cache) {
    mesh.runtime->batch_cache = MEM_new<MeshBatchCache>(__func__);
  }
  else {
    *static_cast<MeshBatchCache *>(mesh.runtime->batch_cache) = {};
  }
  MeshBatchCache *cache = static_cast<MeshBatchCache *>(mesh.runtime->batch_cache);

  cache->is_editmode = mesh.runtime->edit_mesh != nullptr;

  if (cache->is_editmode == false) {
    // cache->edge_len = mesh_render_edges_len_get(mesh);
    // cache->tri_len = mesh_render_corner_tris_len_get(mesh);
    // cache->face_len = mesh_render_faces_len_get(mesh);
    // cache->vert_len = mesh_render_verts_len_get(mesh);
  }

  cache->mat_len = BKE_id_material_used_with_fallback_eval(mesh.id);
  cache->surface_per_mat = Array<gpu::Batch *>(cache->mat_len, nullptr);
  cache->tris_per_mat.reinitialize(cache->mat_len);

  cache->is_dirty = false;
  cache->batch_ready = (DRWBatchFlag)0;
  cache->batch_requested = (DRWBatchFlag)0;

  drw_mesh_weight_state_clear(&cache->weight_state);
}

void DRW_mesh_batch_cache_validate(Mesh &mesh)
{
  if (!mesh_batch_cache_valid(mesh)) {
    if (mesh.runtime->batch_cache) {
      mesh_batch_cache_clear(*static_cast<MeshBatchCache *>(mesh.runtime->batch_cache));
    }
    mesh_batch_cache_init(mesh);
  }
}

static MeshBatchCache *mesh_batch_cache_get(Mesh &mesh)
{
  return static_cast<MeshBatchCache *>(mesh.runtime->batch_cache);
}

static void mesh_batch_cache_check_vertex_group(MeshBatchCache &cache,
                                                const DRW_MeshWeightState *wstate)
{
  if (!drw_mesh_weight_state_compare(&cache.weight_state, wstate)) {
    FOREACH_MESH_BUFFER_CACHE (cache, mbc) {
      mbc->buff.vbos.remove(VBOType::VertexGroupWeight);
    }
    GPU_BATCH_CLEAR_SAFE(cache.batch.surface_weights);

    cache.batch_ready &= ~MBC_SURFACE_WEIGHTS;

    drw_mesh_weight_state_clear(&cache.weight_state);
  }
}

static void mesh_batch_cache_request_surface_batches(Mesh &mesh, MeshBatchCache &cache)
{
  cache.batch_requested |= (MBC_SURFACE | MBC_SURFACE_PER_MAT);
  DRW_batch_request(&cache.batch.surface);

  /* If there are only a few materials at most, just request batches for everything. However, if
   * the maximum material index is large, detect the actually used material indices first and only
   * request those. This reduces the overhead of dealing with all these batches down the line. */
  if (cache.mat_len < 16) {
    for (int i = 0; i < cache.mat_len; i++) {
      DRW_batch_request(&cache.surface_per_mat[i]);
    }
  }
  else {
    const VectorSet<int> &used_material_indices = mesh.material_indices_used();
    for (const int i : used_material_indices) {
      DRW_batch_request(&cache.surface_per_mat[i]);
    }
  }
}

static void mesh_batch_cache_discard_shaded_tri(MeshBatchCache &cache)
{
  discard_buffers(cache, {VBOType::UVs, VBOType::Tangents, VBOType::Orco}, {});
}

static void mesh_batch_cache_discard_uvedit(MeshBatchCache &cache)
{
  discard_buffers(cache,
                  {VBOType::EditUVStretchAngle,
                   VBOType::EditUVStretchArea,
                   VBOType::UVs,
                   VBOType::EditUVData,
                   VBOType::FaceDotUV,
                   VBOType::FaceDotEditUVData},
                  {IBOType::EditUVTris,
                   IBOType::EditUVLines,
                   IBOType::EditUVPoints,
                   IBOType::EditUVFaceDots,
                   IBOType::UVLines,
                   IBOType::UVTris});

  cache.tot_area = 0.0f;
  cache.tot_uv_area = 0.0f;

  /* We discarded the vbo.uv so we need to reset the cd_used flag. */
  cache.cd_used.uv.clear();
  cache.cd_used.edit_uv = false;
}

static void mesh_batch_cache_discard_uvedit_select(MeshBatchCache &cache)
{
  discard_buffers(cache,
                  {VBOType::EditUVData, VBOType::FaceDotEditUVData},
                  {IBOType::EditUVTris,
                   IBOType::EditUVLines,
                   IBOType::EditUVPoints,
                   IBOType::EditUVFaceDots,
                   IBOType::UVLines,
                   IBOType::UVTris});
}

void DRW_mesh_batch_cache_dirty_tag(Mesh *mesh, eMeshBatchDirtyMode mode)
{
  if (!mesh->runtime->batch_cache) {
    return;
  }
  MeshBatchCache &cache = *static_cast<MeshBatchCache *>(mesh->runtime->batch_cache);
  switch (mode) {
    case BKE_MESH_BATCH_DIRTY_SELECT:
      discard_buffers(cache, {VBOType::EditData, VBOType::FaceDotNormal}, {});

      /* Because visible UVs depends on edit mode selection, discard topology. */
      mesh_batch_cache_discard_uvedit_select(cache);
      break;
    case BKE_MESH_BATCH_DIRTY_SELECT_PAINT:
      discard_buffers(cache, {VBOType::PaintOverlayFlag}, {IBOType::LinesPaintMask});
      break;
    case BKE_MESH_BATCH_DIRTY_ALL:
      cache.is_dirty = true;
      break;
    case BKE_MESH_BATCH_DIRTY_SHADING:
      mesh_batch_cache_discard_shaded_tri(cache);
      mesh_batch_cache_discard_uvedit(cache);
      break;
    case BKE_MESH_BATCH_DIRTY_UVEDIT_ALL:
      mesh_batch_cache_discard_uvedit(cache);
      break;
    case BKE_MESH_BATCH_DIRTY_UVEDIT_SELECT:
      discard_buffers(cache, {VBOType::EditUVData, VBOType::FaceDotEditUVData}, {});
      break;
    default:
      BLI_assert(0);
  }
}

static void mesh_buffer_cache_clear(MeshBufferCache *mbc)
{
  mbc->buff.ibos.clear();
  mbc->buff.vbos.clear();

  mbc->loose_geom = {};
  mbc->face_sorted = {};
}

static void mesh_batch_cache_free_subdiv_cache(MeshBatchCache &cache)
{
  if (cache.subdiv_cache) {
    draw_subdiv_cache_free(*cache.subdiv_cache);
    MEM_delete(cache.subdiv_cache);
    cache.subdiv_cache = nullptr;
  }
}

static void mesh_batch_cache_clear(MeshBatchCache &cache)
{
  FOREACH_MESH_BUFFER_CACHE (cache, mbc) {
    mesh_buffer_cache_clear(mbc);
  }

  cache.tris_per_mat = {};

  for (int i = 0; i < sizeof(cache.batch) / sizeof(void *); i++) {
    gpu::Batch **batch = (gpu::Batch **)&cache.batch;
    GPU_BATCH_DISCARD_SAFE(batch[i]);
  }
  for (const int i : cache.surface_per_mat.index_range()) {
    GPU_BATCH_DISCARD_SAFE(cache.surface_per_mat[i]);
  }

  mesh_batch_cache_discard_shaded_tri(cache);
  mesh_batch_cache_discard_uvedit(cache);
  cache.surface_per_mat = {};
  cache.mat_len = 0;

  cache.batch_ready = (DRWBatchFlag)0;
  drw_mesh_weight_state_clear(&cache.weight_state);

  mesh_batch_cache_free_subdiv_cache(cache);
}

void DRW_mesh_batch_cache_free(void *batch_cache)
{
  MeshBatchCache *cache = static_cast<MeshBatchCache *>(batch_cache);
  mesh_batch_cache_clear(*cache);
  MEM_delete(cache);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Public API
 * \{ */

static void texpaint_request_active_uv(MeshBatchCache &cache, Object &object, Mesh &mesh)
{
  mesh_cd_calc_active_uv_layer(object, mesh, cache.cd_needed);

  BLI_assert(!cache.cd_needed.uv.is_empty() &&
             "No uv layer available in texpaint, but batches requested anyway!");

  mesh_cd_calc_active_mask_uv_layer(object, mesh, cache.cd_needed);
}

static void request_active_and_default_color_attributes(const Object &object,
                                                        const Mesh &mesh,
                                                        VectorSet<std::string> &attributes)
{
  const Mesh &me_final = editmesh_final_or_this(object, mesh);

  auto request_color_attribute = [&](const StringRef name) {
    if (!name.is_empty()) {
      if (attribute_exists(me_final, name)) {
        drw_attributes_add_request(&attributes, name);
      }
    }
  };

  request_color_attribute(me_final.active_color_attribute);
  request_color_attribute(me_final.default_color_attribute);
}

gpu::Batch *DRW_mesh_batch_cache_get_all_verts(Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  cache.batch_requested |= MBC_ALL_VERTS;
  return DRW_batch_request(&cache.batch.all_verts);
}

gpu::Batch *DRW_mesh_batch_cache_get_paint_overlay_verts(Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  cache.batch_requested |= MBC_PAINT_OVERLAY_VERTS;
  return DRW_batch_request(&cache.batch.paint_overlay_verts);
}

gpu::Batch *DRW_mesh_batch_cache_get_all_edges(Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  cache.batch_requested |= MBC_ALL_EDGES;
  return DRW_batch_request(&cache.batch.all_edges);
}

gpu::Batch *DRW_mesh_batch_cache_get_surface(Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  mesh_batch_cache_request_surface_batches(mesh, cache);

  return cache.batch.surface;
}

gpu::Batch *DRW_mesh_batch_cache_get_paint_overlay_surface(Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  cache.batch_requested |= MBC_PAINT_OVERLAY_SURFACE;
  return DRW_batch_request(&cache.batch.paint_overlay_surface);
}

gpu::Batch *DRW_mesh_batch_cache_get_loose_edges(Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  if (cache.no_loose_wire) {
    return nullptr;
  }
  cache.batch_requested |= MBC_LOOSE_EDGES;
  return DRW_batch_request(&cache.batch.loose_edges);
}

gpu::Batch *DRW_mesh_batch_cache_get_surface_weights(Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  cache.batch_requested |= MBC_SURFACE_WEIGHTS;
  return DRW_batch_request(&cache.batch.surface_weights);
}

gpu::Batch *DRW_mesh_batch_cache_get_edge_detection(Mesh &mesh, bool *r_is_manifold)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  cache.batch_requested |= MBC_EDGE_DETECTION;
  /* Even if is_manifold is not correct (not updated),
   * the default (not manifold) is just the worst case. */
  if (r_is_manifold) {
    *r_is_manifold = cache.is_manifold;
  }
  return DRW_batch_request(&cache.batch.edge_detection);
}

gpu::Batch *DRW_mesh_batch_cache_get_wireframes_face(Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  cache.batch_requested |= (MBC_WIRE_EDGES);
  return DRW_batch_request(&cache.batch.wire_edges);
}

gpu::Batch *DRW_mesh_batch_cache_get_edit_mesh_analysis(Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  cache.batch_requested |= MBC_EDIT_MESH_ANALYSIS;
  return DRW_batch_request(&cache.batch.edit_mesh_analysis);
}

void DRW_mesh_get_attributes(const Object &object,
                             const Mesh &mesh,
                             const Span<const GPUMaterial *> materials,
                             VectorSet<std::string> *r_attrs,
                             DRW_MeshCDMask *r_cd_needed)
{
  mesh_cd_calc_used_gpu_layers(object, mesh, materials, r_attrs, r_cd_needed);
}

Span<gpu::Batch *> DRW_mesh_batch_cache_get_surface_shaded(
    Object &object, Mesh &mesh, const Span<const GPUMaterial *> materials)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  mesh_cd_calc_used_gpu_layers(object, mesh, materials, &cache.attr_needed, &cache.cd_needed);

  BLI_assert(materials.size() == cache.mat_len);

  mesh_batch_cache_request_surface_batches(mesh, cache);
  return cache.surface_per_mat;
}

Span<gpu::Batch *> DRW_mesh_batch_cache_get_surface_texpaint(Object &object, Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  texpaint_request_active_uv(cache, object, mesh);
  mesh_batch_cache_request_surface_batches(mesh, cache);
  return cache.surface_per_mat;
}

gpu::Batch *DRW_mesh_batch_cache_get_surface_texpaint_single(Object &object, Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  texpaint_request_active_uv(cache, object, mesh);
  mesh_batch_cache_request_surface_batches(mesh, cache);
  return cache.batch.surface;
}

gpu::Batch *DRW_mesh_batch_cache_get_surface_vertpaint(Object &object, Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);

  VectorSet<std::string> attrs_needed{};
  request_active_and_default_color_attributes(object, mesh, attrs_needed);

  drw_attributes_merge(&cache.attr_needed, &attrs_needed);

  mesh_batch_cache_request_surface_batches(mesh, cache);
  return cache.batch.surface;
}

gpu::Batch *DRW_mesh_batch_cache_get_surface_sculpt(Object &object, Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);

  VectorSet<std::string> attrs_needed{};
  request_active_and_default_color_attributes(object, mesh, attrs_needed);

  drw_attributes_merge(&cache.attr_needed, &attrs_needed);

  mesh_batch_cache_request_surface_batches(mesh, cache);
  return cache.batch.surface;
}

gpu::Batch *DRW_mesh_batch_cache_get_sculpt_overlays(Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);

  cache.cd_needed.sculpt_overlays = 1;
  cache.batch_requested |= (MBC_SCULPT_OVERLAYS);
  DRW_batch_request(&cache.batch.sculpt_overlays);

  return cache.batch.sculpt_overlays;
}

gpu::Batch *DRW_mesh_batch_cache_get_surface_viewer_attribute(Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);

  cache.batch_requested |= (MBC_VIEWER_ATTRIBUTE_OVERLAY);
  DRW_batch_request(&cache.batch.surface_viewer_attribute);

  return cache.batch.surface_viewer_attribute;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Edit Mode API
 * \{ */

gpu::Batch *DRW_mesh_batch_cache_get_edit_triangles(Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  cache.batch_requested |= MBC_EDIT_TRIANGLES;
  return DRW_batch_request(&cache.batch.edit_triangles);
}

gpu::Batch *DRW_mesh_batch_cache_get_edit_edges(Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  cache.batch_requested |= MBC_EDIT_EDGES;
  return DRW_batch_request(&cache.batch.edit_edges);
}

gpu::Batch *DRW_mesh_batch_cache_get_edit_vertices(Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  cache.batch_requested |= MBC_EDIT_VERTICES;
  return DRW_batch_request(&cache.batch.edit_vertices);
}

gpu::Batch *DRW_mesh_batch_cache_get_edit_vert_normals(Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  cache.batch_requested |= MBC_EDIT_VNOR;
  return DRW_batch_request(&cache.batch.edit_vnor);
}

gpu::Batch *DRW_mesh_batch_cache_get_edit_loop_normals(Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  cache.batch_requested |= MBC_EDIT_LNOR;
  return DRW_batch_request(&cache.batch.edit_lnor);
}

gpu::Batch *DRW_mesh_batch_cache_get_edit_facedots(Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  cache.batch_requested |= MBC_EDIT_FACEDOTS;
  return DRW_batch_request(&cache.batch.edit_fdots);
}

gpu::Batch *DRW_mesh_batch_cache_get_edit_skin_roots(Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  cache.batch_requested |= MBC_SKIN_ROOTS;
  return DRW_batch_request(&cache.batch.edit_skin_roots);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Edit Mode selection API
 * \{ */

gpu::Batch *DRW_mesh_batch_cache_get_triangles_with_select_id(Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  cache.batch_requested |= MBC_EDIT_SELECTION_FACES;
  return DRW_batch_request(&cache.batch.edit_selection_faces);
}

gpu::Batch *DRW_mesh_batch_cache_get_facedots_with_select_id(Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  cache.batch_requested |= MBC_EDIT_SELECTION_FACEDOTS;
  return DRW_batch_request(&cache.batch.edit_selection_fdots);
}

gpu::Batch *DRW_mesh_batch_cache_get_edges_with_select_id(Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  cache.batch_requested |= MBC_EDIT_SELECTION_EDGES;
  return DRW_batch_request(&cache.batch.edit_selection_edges);
}

gpu::Batch *DRW_mesh_batch_cache_get_verts_with_select_id(Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  cache.batch_requested |= MBC_EDIT_SELECTION_VERTS;
  return DRW_batch_request(&cache.batch.edit_selection_verts);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name UV Image editor API
 * \{ */

static void edituv_request_active_uv(MeshBatchCache &cache, Object &object, Mesh &mesh)
{
  mesh_cd_calc_active_uv_layer(object, mesh, cache.cd_needed);
  mesh_cd_calc_edit_uv_layer(mesh, &cache.cd_needed);

  mesh_cd_calc_active_mask_uv_layer(object, mesh, cache.cd_needed);
}

gpu::Batch *DRW_mesh_batch_cache_get_edituv_faces_stretch_area(Object &object,
                                                               Mesh &mesh,
                                                               float **tot_area,
                                                               float **tot_uv_area)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  edituv_request_active_uv(cache, object, mesh);
  cache.batch_requested |= MBC_EDITUV_FACES_STRETCH_AREA;

  if (tot_area != nullptr) {
    *tot_area = &cache.tot_area;
  }
  if (tot_uv_area != nullptr) {
    *tot_uv_area = &cache.tot_uv_area;
  }
  return DRW_batch_request(&cache.batch.edituv_faces_stretch_area);
}

gpu::Batch *DRW_mesh_batch_cache_get_edituv_faces_stretch_angle(Object &object, Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  edituv_request_active_uv(cache, object, mesh);
  cache.batch_requested |= MBC_EDITUV_FACES_STRETCH_ANGLE;
  return DRW_batch_request(&cache.batch.edituv_faces_stretch_angle);
}

gpu::Batch *DRW_mesh_batch_cache_get_edituv_faces(Object &object, Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  edituv_request_active_uv(cache, object, mesh);
  cache.batch_requested |= MBC_EDITUV_FACES;
  return DRW_batch_request(&cache.batch.edituv_faces);
}

gpu::Batch *DRW_mesh_batch_cache_get_edituv_edges(Object &object, Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  edituv_request_active_uv(cache, object, mesh);
  cache.batch_requested |= MBC_EDITUV_EDGES;
  return DRW_batch_request(&cache.batch.edituv_edges);
}

gpu::Batch *DRW_mesh_batch_cache_get_edituv_verts(Object &object, Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  edituv_request_active_uv(cache, object, mesh);
  cache.batch_requested |= MBC_EDITUV_VERTS;
  return DRW_batch_request(&cache.batch.edituv_verts);
}

gpu::Batch *DRW_mesh_batch_cache_get_edituv_facedots(Object &object, Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  edituv_request_active_uv(cache, object, mesh);
  cache.batch_requested |= MBC_EDITUV_FACEDOTS;
  return DRW_batch_request(&cache.batch.edituv_fdots);
}

gpu::Batch *DRW_mesh_batch_cache_get_uv_faces(Object &object, Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  edituv_request_active_uv(cache, object, mesh);
  cache.batch_requested |= MBC_UV_FACES;
  return DRW_batch_request(&cache.batch.uv_faces);
}

gpu::Batch *DRW_mesh_batch_cache_get_all_uv_wireframe(Object &object, Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  edituv_request_active_uv(cache, object, mesh);
  cache.batch_requested |= MBC_WIRE_LOOPS_ALL_UVS;
  return DRW_batch_request(&cache.batch.wire_loops_all_uvs);
}

gpu::Batch *DRW_mesh_batch_cache_get_uv_wireframe(Object &object, Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  edituv_request_active_uv(cache, object, mesh);
  cache.batch_requested |= MBC_WIRE_LOOPS_UVS;
  return DRW_batch_request(&cache.batch.wire_loops_uvs);
}

gpu::Batch *DRW_mesh_batch_cache_get_edituv_wireframe(Object &object, Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  edituv_request_active_uv(cache, object, mesh);
  cache.batch_requested |= MBC_WIRE_LOOPS_EDITUVS;
  return DRW_batch_request(&cache.batch.wire_loops_edituvs);
}

gpu::Batch *DRW_mesh_batch_cache_get_paint_overlay_edges(Mesh &mesh)
{
  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  cache.batch_requested |= MBC_PAINT_OVERLAY_WIRE_LOOPS;
  return DRW_batch_request(&cache.batch.paint_overlay_wire_loops);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Grouped batch generation
 * \{ */

void DRW_mesh_batch_cache_free_old(Mesh *mesh, int ctime)
{
  MeshBatchCache *cache = static_cast<MeshBatchCache *>(mesh->runtime->batch_cache);

  if (cache == nullptr) {
    return;
  }

  if (cache->cd_used_over_time == cache->cd_used) {
    cache->lastmatch = ctime;
  }

  if (drw_attributes_overlap(&cache->attr_used_over_time, &cache->attr_used)) {
    cache->lastmatch = ctime;
  }

  if (ctime - cache->lastmatch > U.vbotimeout) {
    mesh_batch_cache_discard_shaded_tri(*cache);
  }

  cache->cd_used_over_time.clear();
  cache->attr_used_over_time.clear();
}

static void init_empty_dummy_batch(gpu::Batch &batch)
{
  /* The dummy batch is only used in cases with invalid edit mode mapping, so the overhead of
   * creating a vertex buffer shouldn't matter. */
  GPUVertFormat format{};
  GPU_vertformat_attr_add(&format, "dummy", gpu::VertAttrType::SFLOAT_32);
  blender::gpu::VertBuf *vbo = GPU_vertbuf_create_with_format(format);
  GPU_vertbuf_data_alloc(*vbo, 1);
  /* Avoid the batch being rendered at all. */
  GPU_vertbuf_data_len_set(*vbo, 0);

  GPU_batch_vertbuf_add(&batch, vbo, true);
}

void DRW_mesh_batch_cache_create_requested(TaskGraph &task_graph,
                                           Object &ob,
                                           Mesh &mesh,
                                           const Scene &scene,
                                           const bool is_paint_mode,
                                           const bool use_hide)
{
  const ToolSettings *ts = scene.toolsettings;

  MeshBatchCache &cache = *mesh_batch_cache_get(mesh);
  bool cd_uv_update = false;

  /* Early out */
  if (cache.batch_requested == 0) {
    return;
  }

  /* Sanity check. */
  if ((mesh.runtime->edit_mesh != nullptr) && (ob.mode & OB_MODE_EDIT)) {
    BLI_assert(BKE_object_get_editmesh_eval_final(&ob) != nullptr);
  }

  const bool is_editmode = ob.mode == OB_MODE_EDIT;

  DRWBatchFlag batch_requested = cache.batch_requested;
  cache.batch_requested = (DRWBatchFlag)0;

  if (batch_requested & MBC_SURFACE_WEIGHTS) {
    /* Check vertex weights. */
    if ((cache.batch.surface_weights != nullptr) && (ts != nullptr)) {
      DRW_MeshWeightState wstate;
      BLI_assert(ob.type == OB_MESH);
      drw_mesh_weight_state_extract(ob, mesh, *ts, is_paint_mode, &wstate);
      mesh_batch_cache_check_vertex_group(cache, &wstate);
      drw_mesh_weight_state_copy(&cache.weight_state, &wstate);
      drw_mesh_weight_state_clear(&wstate);
    }
  }

  if (batch_requested &
      (MBC_SURFACE | MBC_SURFACE_PER_MAT | MBC_WIRE_LOOPS_ALL_UVS | MBC_WIRE_LOOPS_UVS |
       MBC_WIRE_LOOPS_EDITUVS | MBC_UV_FACES | MBC_EDITUV_FACES_STRETCH_AREA |
       MBC_EDITUV_FACES_STRETCH_ANGLE | MBC_EDITUV_FACES | MBC_EDITUV_EDGES | MBC_EDITUV_VERTS))
  {
    /* Modifiers will only generate an orco layer if the mesh is deformed. */
    if (cache.cd_needed.orco != 0) {
      /* Orco is always extracted from final mesh. */
      const Mesh *me_final = (mesh.runtime->edit_mesh) ? BKE_object_get_editmesh_eval_final(&ob) :
                                                         &mesh;
      if (CustomData_get_layer(&me_final->vert_data, CD_ORCO) == nullptr) {
        /* Skip orco calculation */
        cache.cd_needed.orco = 0;
      }
    }

    /* Verify that all surface batches have needed attribute layers.
     */
    /* TODO(fclem): We could be a bit smarter here and only do it per
     * material. */
    const bool uvs_overlap = drw_attributes_overlap(&cache.cd_used.uv, &cache.cd_needed.uv);
    const bool tan_overlap = drw_attributes_overlap(&cache.cd_used.tan, &cache.cd_needed.tan);
    const bool attr_overlap = drw_attributes_overlap(&cache.attr_used, &cache.attr_needed);
    const bool orco_overlap = cache.cd_used.orco == cache.cd_needed.orco;
    const bool tan_orco_overlap = cache.cd_used.tan_orco == cache.cd_needed.tan_orco;
    const bool sculpt_overlap = cache.cd_used.sculpt_overlays == cache.cd_needed.sculpt_overlays;
    if (!uvs_overlap || !tan_overlap || !attr_overlap || !orco_overlap || !tan_orco_overlap ||
        !sculpt_overlap)
    {
      FOREACH_MESH_BUFFER_CACHE (cache, mbc) {
        if (!uvs_overlap) {
          mbc->buff.vbos.remove(VBOType::UVs);
          cd_uv_update = true;
        }
        if (!tan_overlap || !tan_orco_overlap) {
          mbc->buff.vbos.remove(VBOType::Tangents);
        }
        if (!orco_overlap) {
          mbc->buff.vbos.remove(VBOType::Orco);
        }
        if (!sculpt_overlap) {
          mbc->buff.vbos.remove(VBOType::SculptData);
        }
        if (!attr_overlap) {
          for (int i = 0; i < GPU_MAX_ATTR; i++) {
            mbc->buff.vbos.remove(VBOType(int8_t(VBOType::Attr0) + i));
          }
        }
      }
      /* We can't discard batches at this point as they have been
       * referenced for drawing. Just clear them in place. */
      for (int i = 0; i < cache.mat_len; i++) {
        GPU_BATCH_CLEAR_SAFE(cache.surface_per_mat[i]);
      }
      GPU_BATCH_CLEAR_SAFE(cache.batch.surface);
      GPU_BATCH_CLEAR_SAFE(cache.batch.sculpt_overlays);
      cache.batch_ready &= ~(MBC_SURFACE | MBC_SURFACE_PER_MAT | MBC_SCULPT_OVERLAYS);

      mesh_cd_layers_type_merge(&cache.cd_used, cache.cd_needed);
      drw_attributes_merge(&cache.attr_used, &cache.attr_needed);
    }
    mesh_cd_layers_type_merge(&cache.cd_used_over_time, cache.cd_needed);
    cache.cd_needed.clear();

    drw_attributes_merge(&cache.attr_used_over_time, &cache.attr_needed);
    cache.attr_needed.clear();
  }

  if ((batch_requested & MBC_EDITUV) || cd_uv_update) {
    /* Discard UV batches if sync_selection changes */
    const bool is_uvsyncsel = ts && (ts->uv_flag & UV_FLAG_SELECT_SYNC);
    if (cd_uv_update || (cache.is_uvsyncsel != is_uvsyncsel)) {
      cache.is_uvsyncsel = is_uvsyncsel;
      FOREACH_MESH_BUFFER_CACHE (cache, mbc) {
        mbc->buff.vbos.remove(VBOType::EditUVData);
        mbc->buff.vbos.remove(VBOType::FaceDotUV);
        mbc->buff.vbos.remove(VBOType::FaceDotEditUVData);
        mbc->buff.ibos.remove(IBOType::EditUVTris);
        mbc->buff.ibos.remove(IBOType::EditUVLines);
        mbc->buff.ibos.remove(IBOType::EditUVPoints);
        mbc->buff.ibos.remove(IBOType::EditUVFaceDots);
      }
      /* We only clear the batches as they may already have been
       * referenced. */
      GPU_BATCH_CLEAR_SAFE(cache.batch.uv_faces);
      GPU_BATCH_CLEAR_SAFE(cache.batch.wire_loops_all_uvs);
      GPU_BATCH_CLEAR_SAFE(cache.batch.wire_loops_uvs);
      GPU_BATCH_CLEAR_SAFE(cache.batch.wire_loops_edituvs);
      GPU_BATCH_CLEAR_SAFE(cache.batch.edituv_faces_stretch_area);
      GPU_BATCH_CLEAR_SAFE(cache.batch.edituv_faces_stretch_angle);
      GPU_BATCH_CLEAR_SAFE(cache.batch.edituv_faces);
      GPU_BATCH_CLEAR_SAFE(cache.batch.edituv_edges);
      GPU_BATCH_CLEAR_SAFE(cache.batch.edituv_verts);
      GPU_BATCH_CLEAR_SAFE(cache.batch.edituv_fdots);
      cache.batch_ready &= ~MBC_EDITUV;
    }
  }

  /* Second chance to early out */
  if ((batch_requested & ~cache.batch_ready) == 0) {
    return;
  }

  /* TODO(pablodp606): This always updates the sculpt normals for regular drawing (non-pbvh::Tree).
   * This makes tools that sample the surface per step get wrong normals until a redraw happens.
   * Normal updates should be part of the brush loop and only run during the stroke when the
   * brush needs to sample the surface. The drawing code should only update the normals
   * per redraw when smooth shading is enabled. */
  if (bke::pbvh::Tree *pbvh = bke::object::pbvh_get(ob)) {
    bke::pbvh::update_normals_from_eval(ob, *pbvh);
  }

  /* This is the mesh before modifier evaluation, used to test how the mesh changed during
   * evaluation to decide which data is valid to extract. */
  const Mesh *orig_edit_mesh = is_editmode ? BKE_object_get_pre_modified_mesh(&ob) : nullptr;

  bool do_cage = false;
  const Mesh *edit_data_mesh = nullptr;
  if (is_editmode) {
    const Mesh *eval_cage = DRW_object_get_editmesh_cage_for_drawing(ob);
    if (eval_cage && eval_cage != &mesh) {
      /* Extract "cage" data separately when it exists and it's not just the same mesh as the
       * regular evaluated mesh. Otherwise edit data will be extracted from the final evaluated
       * mesh. */
      do_cage = true;
      edit_data_mesh = eval_cage;
    }
    else {
      edit_data_mesh = &mesh;
    }
  }

  bool do_uvcage = false;
  if (is_editmode) {
    /* Currently we don't extract UV data from the evaluated mesh unless it's the same mesh as the
     * original edit mesh. */
    do_uvcage = !(mesh.runtime->is_original_bmesh &&
                  mesh.runtime->wrapper_type == ME_WRAPPER_TYPE_BMESH);
  }

  const DRWBatchFlag batches_to_create = batch_requested & ~cache.batch_ready;

  const bool do_subdivision = BKE_subsurf_modifier_has_gpu_subdiv(&mesh);

  enum class BufferList : int8_t { Final, Cage, UVCage };

  struct BatchCreateData {
    gpu::Batch &batch;
    GPUPrimType prim_type;
    BufferList list;
    std::optional<IBOType> ibo;
    Vector<VBOType> vbos;
  };
  Vector<BatchCreateData> batch_info;

  {
    const BufferList list = BufferList::Final;
    if (batches_to_create & MBC_SURFACE) {
      BatchCreateData batch{*cache.batch.surface,
                            GPU_PRIM_TRIS,
                            list,
                            IBOType::Tris,
                            {VBOType::CornerNormal, VBOType::Position}};
      if (!cache.cd_used.uv.is_empty()) {
        batch.vbos.append(VBOType::UVs);
      }
      for (const int i : cache.attr_used.index_range()) {
        batch.vbos.append(VBOType(int8_t(VBOType::Attr0) + i));
      }
      batch_info.append(std::move(batch));
    }
    if (batches_to_create & MBC_PAINT_OVERLAY_SURFACE) {
      BatchCreateData batch{*cache.batch.paint_overlay_surface,
                            GPU_PRIM_TRIS,
                            list,
                            IBOType::Tris,
                            {VBOType::Position, VBOType::PaintOverlayFlag}};
      batch_info.append(std::move(batch));
    }
    if (batches_to_create & MBC_VIEWER_ATTRIBUTE_OVERLAY) {
      batch_info.append({*cache.batch.surface_viewer_attribute,
                         GPU_PRIM_TRIS,
                         list,
                         IBOType::Tris,
                         {VBOType::Position, VBOType::AttrViewer}});
    }
    if (batches_to_create & MBC_ALL_VERTS) {
      batch_info.append(
          {*cache.batch.all_verts, GPU_PRIM_POINTS, list, std::nullopt, {VBOType::Position}});
    }
    if (batches_to_create & MBC_PAINT_OVERLAY_VERTS) {
      batch_info.append({*cache.batch.paint_overlay_verts,
                         GPU_PRIM_POINTS,
                         list,
                         std::nullopt,
                         {VBOType::Position, VBOType::PaintOverlayFlag}});
    }
    if (batches_to_create & MBC_SCULPT_OVERLAYS) {
      batch_info.append({*cache.batch.sculpt_overlays,
                         GPU_PRIM_TRIS,
                         list,
                         IBOType::Tris,
                         {VBOType::Position, VBOType::SculptData}});
    }
    if (batches_to_create & MBC_ALL_EDGES) {
      batch_info.append(
          {*cache.batch.all_edges, GPU_PRIM_LINES, list, IBOType::Lines, {VBOType::Position}});
    }
    if (batches_to_create & MBC_LOOSE_EDGES) {
      batch_info.append({*cache.batch.loose_edges,
                         GPU_PRIM_LINES,
                         list,
                         IBOType::LinesLoose,
                         {VBOType::Position}});
    }
    if (batches_to_create & MBC_EDGE_DETECTION) {
      batch_info.append({*cache.batch.edge_detection,
                         GPU_PRIM_LINES_ADJ,
                         list,
                         IBOType::LinesAdjacency,
                         {VBOType::Position}});
    }
    if (batches_to_create & MBC_SURFACE_WEIGHTS) {
      batch_info.append({*cache.batch.surface_weights,
                         GPU_PRIM_TRIS,
                         list,
                         IBOType::Tris,
                         {VBOType::Position, VBOType::CornerNormal, VBOType::VertexGroupWeight}});
    }
    if (batches_to_create & MBC_PAINT_OVERLAY_WIRE_LOOPS) {
      batch_info.append({*cache.batch.paint_overlay_wire_loops,
                         GPU_PRIM_LINES,
                         list,
                         IBOType::LinesPaintMask,
                         {VBOType::Position, VBOType::PaintOverlayFlag}});
    }
    if (batches_to_create & MBC_WIRE_EDGES) {
      batch_info.append({*cache.batch.wire_edges,
                         GPU_PRIM_LINES,
                         list,
                         IBOType::Lines,
                         {VBOType::Position, VBOType::CornerNormal, VBOType::EdgeFactor}});
    }
    if (batches_to_create & MBC_WIRE_LOOPS_ALL_UVS) {
      BatchCreateData batch{
          *cache.batch.wire_loops_all_uvs, GPU_PRIM_LINES, list, IBOType::AllUVLines, {}};
      if (!cache.cd_used.uv.is_empty()) {
        batch.vbos.append(VBOType::UVs);
      }
      batch_info.append(std::move(batch));
    }
    if (batches_to_create & MBC_WIRE_LOOPS_UVS) {
      BatchCreateData batch{
          *cache.batch.wire_loops_uvs, GPU_PRIM_LINES, list, IBOType::UVLines, {}};
      if (!cache.cd_used.uv.is_empty()) {
        batch.vbos.append(VBOType::UVs);
      }
      batch_info.append(std::move(batch));
    }
    if (batches_to_create & MBC_WIRE_LOOPS_EDITUVS) {
      BatchCreateData batch{
          *cache.batch.wire_loops_edituvs, GPU_PRIM_LINES, list, IBOType::EditUVLines, {}};
      if (!cache.cd_used.uv.is_empty()) {
        batch.vbos.append(VBOType::UVs);
      }
      batch_info.append(std::move(batch));
    }
    if (batches_to_create & MBC_UV_FACES) {
      const bool use_face_selection = (mesh.editflag & ME_EDIT_PAINT_FACE_SEL);
      /* Sculpt mode does not support selection, therefore the generic `is_paint_mode` check cannot
       * be used */
      const bool is_face_selectable =
          ELEM(ob.mode, OB_MODE_VERTEX_PAINT, OB_MODE_WEIGHT_PAINT, OB_MODE_TEXTURE_PAINT) &&
          use_face_selection;

      const IBOType ibo = is_face_selectable || is_editmode ? IBOType::UVTris : IBOType::Tris;
      BatchCreateData batch{*cache.batch.uv_faces, GPU_PRIM_TRIS, list, ibo, {}};
      if (!cache.cd_used.uv.is_empty()) {
        batch.vbos.append(VBOType::UVs);
      }
      batch_info.append(std::move(batch));
    }
    if (batches_to_create & MBC_EDIT_MESH_ANALYSIS) {
      batch_info.append({*cache.batch.edit_mesh_analysis,
                         GPU_PRIM_TRIS,
                         list,
                         IBOType::Tris,
                         {VBOType::Position, VBOType::MeshAnalysis}});
    }
  }

  /* When the mesh doesn't correspond to the object's original mesh (i.e. the mesh was replaced by
   * another with the object info node during evaluation), don't extract edit mode data for it.
   * That data can be invalid because any original indices (#CD_ORIGINDEX) on the evaluated mesh
   * won't correspond to the correct mesh. */
  const bool edit_mapping_valid = is_editmode && BKE_editmesh_eval_orig_map_available(
                                                     *edit_data_mesh, orig_edit_mesh);

  {
    const BufferList list = do_cage ? BufferList::Cage : BufferList::Final;
    if (batches_to_create & MBC_EDIT_TRIANGLES) {
      if (edit_mapping_valid) {
        batch_info.append({*cache.batch.edit_triangles,
                           GPU_PRIM_TRIS,
                           list,
                           IBOType::Tris,
                           {VBOType::Position, VBOType::EditData}});
      }
      else {
        init_empty_dummy_batch(*cache.batch.edit_triangles);
      }
    }
    if (batches_to_create & MBC_EDIT_VERTICES) {
      if (edit_mapping_valid) {
        BatchCreateData batch{*cache.batch.edit_vertices,
                              GPU_PRIM_POINTS,
                              list,
                              IBOType::Points,
                              {VBOType::Position, VBOType::EditData}};
        if (!do_subdivision || do_cage) {
          batch.vbos.append(VBOType::CornerNormal);
        }
        batch_info.append(std::move(batch));
      }
      else {
        init_empty_dummy_batch(*cache.batch.edit_vertices);
      }
    }
    if (batches_to_create & MBC_EDIT_EDGES) {
      if (edit_mapping_valid) {
        BatchCreateData batch{*cache.batch.edit_edges,
                              GPU_PRIM_LINES,
                              list,
                              IBOType::Lines,
                              {VBOType::CornerNormal, VBOType::Position, VBOType::EditData}};
        batch_info.append(std::move(batch));
      }
      else {
        init_empty_dummy_batch(*cache.batch.edit_edges);
      }
    }
    if (batches_to_create & MBC_EDIT_VNOR) {
      if (edit_mapping_valid) {
        batch_info.append({*cache.batch.edit_vnor,
                           GPU_PRIM_POINTS,
                           list,
                           IBOType::Points,
                           {VBOType::Position, VBOType::VertexNormal}});
      }
      else {
        init_empty_dummy_batch(*cache.batch.edit_vnor);
      }
    }
    if (batches_to_create & MBC_EDIT_LNOR) {
      if (edit_mapping_valid) {
        batch_info.append({*cache.batch.edit_lnor,
                           GPU_PRIM_POINTS,
                           list,
                           IBOType::Tris,
                           {VBOType::Position, VBOType::CornerNormal}});
      }
      else {
        init_empty_dummy_batch(*cache.batch.edit_lnor);
      }
    }
    if (batches_to_create & MBC_EDIT_FACEDOTS) {
      if (edit_mapping_valid) {
        batch_info.append({*cache.batch.edit_fdots,
                           GPU_PRIM_POINTS,
                           list,
                           IBOType::FaceDots,
                           {VBOType::FaceDotPosition, VBOType::FaceDotNormal}});
      }
      else {
        init_empty_dummy_batch(*cache.batch.edit_fdots);
      }
    }
    if (batches_to_create & MBC_SKIN_ROOTS) {
      if (edit_mapping_valid) {
        batch_info.append({*cache.batch.edit_skin_roots,
                           GPU_PRIM_POINTS,
                           list,
                           std::nullopt,
                           {VBOType::SkinRoots}});
      }
      else {
        init_empty_dummy_batch(*cache.batch.edit_skin_roots);
      }
    }
    if (batches_to_create & MBC_EDIT_SELECTION_VERTS) {
      if (is_editmode && !edit_mapping_valid) {
        init_empty_dummy_batch(*cache.batch.edit_selection_verts);
      }
      else {
        batch_info.append({*cache.batch.edit_selection_verts,
                           GPU_PRIM_POINTS,
                           list,
                           IBOType::Points,
                           {VBOType::Position, VBOType::IndexVert}});
      }
    }
    if (batches_to_create & MBC_EDIT_SELECTION_EDGES) {
      if (is_editmode && !edit_mapping_valid) {
        init_empty_dummy_batch(*cache.batch.edit_selection_edges);
      }
      else {
        batch_info.append({*cache.batch.edit_selection_edges,
                           GPU_PRIM_LINES,
                           list,
                           IBOType::Lines,
                           {VBOType::Position, VBOType::IndexEdge}});
      }
    }
    if (batches_to_create & MBC_EDIT_SELECTION_FACES) {
      if (is_editmode && !edit_mapping_valid) {
        init_empty_dummy_batch(*cache.batch.edit_selection_faces);
      }
      else {
        batch_info.append({*cache.batch.edit_selection_faces,
                           GPU_PRIM_TRIS,
                           list,
                           IBOType::Tris,
                           {VBOType::Position, VBOType::IndexFace}});
      }
    }
    if (batches_to_create & MBC_EDIT_SELECTION_FACEDOTS) {
      if (is_editmode && !edit_mapping_valid) {
        init_empty_dummy_batch(*cache.batch.edit_selection_fdots);
      }
      else {
        batch_info.append({*cache.batch.edit_selection_fdots,
                           GPU_PRIM_POINTS,
                           list,
                           IBOType::FaceDots,
                           {VBOType::FaceDotPosition, VBOType::IndexFaceDot}});
      }
    }
  }

  {
    /**
     * TODO: The code and data structure is ready to support modified UV display
     * but the selection code for UVs needs to support it first. So for now, only
     * display the cage in all cases.
     */
    const BufferList list = do_uvcage ? BufferList::UVCage : BufferList::Final;

    if (batches_to_create & MBC_EDITUV_FACES) {
      if (edit_mapping_valid) {
        batch_info.append({*cache.batch.edituv_faces,
                           GPU_PRIM_TRIS,
                           list,
                           IBOType::EditUVTris,
                           {VBOType::UVs, VBOType::EditUVData}});
      }
      else {
        init_empty_dummy_batch(*cache.batch.edituv_faces);
      }
    }
    if (batches_to_create & MBC_EDITUV_FACES_STRETCH_AREA) {
      if (edit_mapping_valid) {
        batch_info.append({*cache.batch.edituv_faces_stretch_area,
                           GPU_PRIM_TRIS,
                           list,
                           IBOType::EditUVTris,
                           {VBOType::UVs, VBOType::EditUVData, VBOType::EditUVStretchArea}});
      }
      else {
        init_empty_dummy_batch(*cache.batch.edituv_faces_stretch_area);
      }
    }
    if (batches_to_create & MBC_EDITUV_FACES_STRETCH_ANGLE) {
      if (edit_mapping_valid) {
        batch_info.append({*cache.batch.edituv_faces_stretch_angle,
                           GPU_PRIM_TRIS,
                           list,
                           IBOType::EditUVTris,
                           {VBOType::UVs, VBOType::EditUVData, VBOType::EditUVStretchAngle}});
      }
      else {
        init_empty_dummy_batch(*cache.batch.edituv_faces_stretch_angle);
      }
    }
    if (batches_to_create & MBC_EDITUV_EDGES) {
      if (edit_mapping_valid) {
        batch_info.append({*cache.batch.edituv_edges,
                           GPU_PRIM_LINES,
                           list,
                           IBOType::EditUVLines,
                           {VBOType::UVs, VBOType::EditUVData}});
      }
      else {
        init_empty_dummy_batch(*cache.batch.edituv_edges);
      }
    }
    if (batches_to_create & MBC_EDITUV_VERTS) {
      if (edit_mapping_valid) {
        batch_info.append({*cache.batch.edituv_verts,
                           GPU_PRIM_POINTS,
                           list,
                           IBOType::EditUVPoints,
                           {VBOType::UVs, VBOType::EditUVData}});
      }
      else {
        init_empty_dummy_batch(*cache.batch.edituv_verts);
      }
    }
    if (batches_to_create & MBC_EDITUV_FACEDOTS) {
      if (edit_mapping_valid) {
        batch_info.append({*cache.batch.edituv_fdots,
                           GPU_PRIM_POINTS,
                           list,
                           IBOType::EditUVFaceDots,
                           {VBOType::FaceDotUV, VBOType::FaceDotEditUVData}});
      }
      else {
        init_empty_dummy_batch(*cache.batch.edituv_fdots);
      }
    }
  }

  std::array<VectorSet<IBOType>, 3> ibo_requests;
  std::array<VectorSet<VBOType>, 3> vbo_requests;
  for (const BatchCreateData &batch : batch_info) {
    if (batch.ibo) {
      ibo_requests[int(batch.list)].add(*batch.ibo);
    }
    vbo_requests[int(batch.list)].add_multiple(batch.vbos);
  }

  if (batches_to_create & MBC_SURFACE_PER_MAT) {
    ibo_requests[int(BufferList::Final)].add(IBOType::Tris);
    vbo_requests[int(BufferList::Final)].add(VBOType::CornerNormal);
    vbo_requests[int(BufferList::Final)].add(VBOType::Position);
    for (const int i : cache.attr_used.index_range()) {
      vbo_requests[int(BufferList::Final)].add(VBOType(int8_t(VBOType::Attr0) + i));
    }
    if (!cache.cd_used.uv.is_empty()) {
      vbo_requests[int(BufferList::Final)].add(VBOType::UVs);
    }
    if (!cache.cd_used.tan.is_empty() || cache.cd_used.tan_orco) {
      vbo_requests[int(BufferList::Final)].add(VBOType::Tangents);
    }
    if (cache.cd_used.orco) {
      vbo_requests[int(BufferList::Final)].add(VBOType::Orco);
    }
  }

  if (do_uvcage) {
    mesh_buffer_cache_create_requested(task_graph,
                                       scene,
                                       cache,
                                       cache.uv_cage,
                                       ibo_requests[int(BufferList::UVCage)],
                                       vbo_requests[int(BufferList::UVCage)],
                                       ob,
                                       mesh,
                                       is_editmode,
                                       is_paint_mode,
                                       false,
                                       true,
                                       true);
  }

  if (do_cage) {
    mesh_buffer_cache_create_requested(task_graph,
                                       scene,
                                       cache,
                                       cache.cage,
                                       ibo_requests[int(BufferList::Cage)],
                                       vbo_requests[int(BufferList::Cage)],
                                       ob,
                                       mesh,
                                       is_editmode,
                                       is_paint_mode,
                                       false,
                                       false,
                                       true);
  }

  if (do_subdivision) {
    DRW_create_subdivision(ob,
                           mesh,
                           cache,
                           cache.final,
                           ibo_requests[int(BufferList::Final)],
                           vbo_requests[int(BufferList::Final)],
                           is_editmode,
                           is_paint_mode,
                           true,
                           false,
                           do_cage,
                           ts,
                           use_hide);
  }
  else {
    /* The subsurf modifier may have been recently removed, or another modifier was added after it,
     * so free any potential subdivision cache as it is not needed anymore. */
    mesh_batch_cache_free_subdiv_cache(cache);
  }

  mesh_buffer_cache_create_requested(task_graph,
                                     scene,
                                     cache,
                                     cache.final,
                                     ibo_requests[int(BufferList::Final)],
                                     vbo_requests[int(BufferList::Final)],
                                     ob,
                                     mesh,
                                     is_editmode,
                                     is_paint_mode,
                                     true,
                                     false,
                                     use_hide);

  std::array<MeshBufferCache *, 3> caches{&cache.final, &cache.cage, &cache.uv_cage};
  for (const BatchCreateData &batch : batch_info) {
    MeshBufferCache &cache_for_batch = *caches[int(batch.list)];
    gpu::IndexBuf *ibo = batch.ibo ? caches[int(batch.list)]->buff.ibos.lookup(*batch.ibo).get() :
                                     nullptr;
    GPU_batch_init(&batch.batch, batch.prim_type, nullptr, ibo);
    for (const VBOType vbo_request : batch.vbos) {
      GPU_batch_vertbuf_add(
          &batch.batch, cache_for_batch.buff.vbos.lookup(vbo_request).get(), false);
    }
  }

  if (batches_to_create & MBC_SURFACE_PER_MAT) {
    MeshBufferList &buffers = cache.final.buff;
    gpu::IndexBuf &tris_ibo = *buffers.ibos.lookup(IBOType::Tris);
    create_material_subranges(cache.final.face_sorted, tris_ibo, cache.tris_per_mat);
    for (const int material : IndexRange(cache.mat_len)) {
      gpu::Batch *batch = cache.surface_per_mat[material];
      if (!batch) {
        continue;
      }
      GPU_batch_init(batch, GPU_PRIM_TRIS, nullptr, cache.tris_per_mat[material].get());
      GPU_batch_vertbuf_add(batch, buffers.vbos.lookup(VBOType::CornerNormal).get(), false);
      GPU_batch_vertbuf_add(batch, buffers.vbos.lookup(VBOType::Position).get(), false);
      if (!cache.cd_used.uv.is_empty()) {
        GPU_batch_vertbuf_add(batch, buffers.vbos.lookup(VBOType::UVs).get(), false);
      }
      if (!cache.cd_used.tan.is_empty() || cache.cd_used.tan_orco) {
        GPU_batch_vertbuf_add(batch, buffers.vbos.lookup(VBOType::Tangents).get(), false);
      }
      if (cache.cd_used.orco) {
        GPU_batch_vertbuf_add(batch, buffers.vbos.lookup(VBOType::Orco).get(), false);
      }
      for (const int i : cache.attr_used.index_range()) {
        GPU_batch_vertbuf_add(
            batch, buffers.vbos.lookup(VBOType(int8_t(VBOType::Attr0) + i)).get(), false);
      }
    }
  }

  cache.batch_ready |= batch_requested;
}

/** \} */

}  // namespace blender::draw
