/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Extraction of Mesh data into VBO to feed to GPU.
 */

#include "BLI_array.hh"
#include "BLI_array_utils.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_geom.h"
#include "BLI_task.hh"
#include "BLI_virtual_array.hh"

#include "BKE_attribute.hh"
#include "BKE_editmesh.hh"
#include "BKE_editmesh_cache.hh"
#include "BKE_material.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"

#include "ED_mesh.hh"

#include "DRW_render.hh"

#include "mesh_extractors/extract_mesh.hh"

/* ---------------------------------------------------------------------- */
/** \name Update Loose Geometry
 * \{ */

namespace blender::draw {

static void extract_set_bits(const BitSpan bits, MutableSpan<int> indices)
{
  int count = 0;
  for (const int64_t i : bits.index_range()) {
    if (bits[i]) {
      indices[count] = int(i);
      count++;
    }
  }
  BLI_assert(count == indices.size());
}

static void mesh_render_data_loose_geom_mesh(const MeshRenderData &mr, MeshBufferCache &cache)
{
  const Mesh &mesh = *mr.mesh;
  const bool no_loose_vert_hint = mesh.runtime->loose_verts_cache.is_cached() &&
                                  mesh.runtime->loose_verts_cache.data().count == 0;
  const bool no_loose_edge_hint = mesh.runtime->loose_edges_cache.is_cached() &&
                                  mesh.runtime->loose_edges_cache.data().count == 0;
  threading::parallel_invoke(
      mesh.edges_num > 4096 && !no_loose_vert_hint && !no_loose_edge_hint,
      [&]() {
        const bke::LooseEdgeCache &loose_edges = mesh.loose_edges();
        if (loose_edges.count > 0) {
          cache.loose_geom.edges.reinitialize(loose_edges.count);
          extract_set_bits(loose_edges.is_loose_bits, cache.loose_geom.edges);
        }
      },
      [&]() {
        const bke::LooseVertCache &loose_verts = mesh.loose_verts();
        if (loose_verts.count > 0) {
          cache.loose_geom.verts.reinitialize(loose_verts.count);
          extract_set_bits(loose_verts.is_loose_bits, cache.loose_geom.verts);
        }
      });
}

static void mesh_render_data_loose_verts_bm(const MeshRenderData &mr,
                                            MeshBufferCache &cache,
                                            BMesh &bm)
{
  int i;
  BMIter iter;
  BMVert *vert;
  int count = 0;
  Array<int> loose_verts(mr.verts_num);
  BM_ITER_MESH_INDEX (vert, &iter, &bm, BM_VERTS_OF_MESH, i) {
    if (vert->e == nullptr) {
      loose_verts[count] = i;
      count++;
    }
  }
  if (count < mr.verts_num) {
    cache.loose_geom.verts = loose_verts.as_span().take_front(count);
  }
  else {
    cache.loose_geom.verts = std::move(loose_verts);
  }
}

static void mesh_render_data_loose_edges_bm(const MeshRenderData &mr,
                                            MeshBufferCache &cache,
                                            BMesh &bm)
{
  int i;
  BMIter iter;
  BMEdge *edge;
  int count = 0;
  Array<int> loose_edges(mr.edges_num);
  BM_ITER_MESH_INDEX (edge, &iter, &bm, BM_EDGES_OF_MESH, i) {
    if (edge->l == nullptr) {
      loose_edges[count] = i;
      count++;
    }
  }
  if (count < mr.edges_num) {
    cache.loose_geom.edges = loose_edges.as_span().take_front(count);
  }
  else {
    cache.loose_geom.edges = std::move(loose_edges);
  }
}

static void mesh_render_data_loose_geom_build(const MeshRenderData &mr, MeshBufferCache &cache)
{
  if (mr.extract_type == MeshExtractType::Mesh) {
    mesh_render_data_loose_geom_mesh(mr, cache);
  }
  else {
    BMesh &bm = *mr.bm;
    mesh_render_data_loose_verts_bm(mr, cache, bm);
    mesh_render_data_loose_edges_bm(mr, cache, bm);
  }
}

static void mesh_render_data_loose_geom_ensure(const MeshRenderData &mr, MeshBufferCache &cache)
{
  /* Early exit: Are loose geometry already available.
   * Only checking for loose verts as loose edges and verts are calculated at the same time. */
  if (!cache.loose_geom.verts.is_empty()) {
    return;
  }
  mesh_render_data_loose_geom_build(mr, cache);
}

void mesh_render_data_update_loose_geom(MeshRenderData &mr, MeshBufferCache &cache)
{
  mesh_render_data_loose_geom_ensure(mr, cache);
  mr.loose_edges = cache.loose_geom.edges;
  mr.loose_verts = cache.loose_geom.verts;
  mr.loose_verts_num = cache.loose_geom.verts.size();
  mr.loose_edges_num = cache.loose_geom.edges.size();

  mr.loose_indices_num = mr.loose_verts_num + (mr.loose_edges_num * 2);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Polygons sorted per material
 *
 * Contains face indices sorted based on their material.
 * \{ */

static void accumululate_material_counts_bm(
    const BMesh &bm, threading::EnumerableThreadSpecific<Array<int>> &all_tri_counts)
{
  threading::parallel_for(IndexRange(bm.totface), 1024, [&](const IndexRange range) {
    Array<int> &tri_counts = all_tri_counts.local();
    const short last_index = tri_counts.size() - 1;
    for (const int i : range) {
      const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), i);
      if (!BM_elem_flag_test(&face, BM_ELEM_HIDDEN)) {
        const short mat = std::clamp<short>(face.mat_nr, 0, last_index);
        tri_counts[mat] += face.len - 2;
      }
    }
  });
}

static void accumululate_material_counts_mesh(
    const MeshRenderData &mr, threading::EnumerableThreadSpecific<Array<int>> &all_tri_counts)
{
  const OffsetIndices faces = mr.faces;
  const Span<bool> hide_poly = mr.hide_poly;
  const Span material_indices = mr.material_indices;
  if (material_indices.is_empty()) {
    if (!hide_poly.is_empty()) {
      all_tri_counts.local().first() = threading::parallel_reduce(
          faces.index_range(),
          4096,
          0,
          [&](const IndexRange range, int count) {
            for (const int face : range) {
              if (!hide_poly[face]) {
                count += bke::mesh::face_triangles_num(faces[face].size());
              }
            }
            return count;
          },
          std::plus<>());
    }
    else {
      all_tri_counts.local().first() = poly_to_tri_count(mr.faces_num, mr.corners_num);
    }
    return;
  }

  threading::parallel_for(material_indices.index_range(), 1024, [&](const IndexRange range) {
    Array<int> &tri_counts = all_tri_counts.local();
    const int last_index = tri_counts.size() - 1;
    if (!hide_poly.is_empty()) {
      for (const int i : range) {
        if (!hide_poly[i]) {
          const int mat = std::clamp(material_indices[i], 0, last_index);
          tri_counts[mat] += bke::mesh::face_triangles_num(faces[i].size());
        }
      }
    }
    else {
      for (const int i : range) {
        const int mat = std::clamp(material_indices[i], 0, last_index);
        tri_counts[mat] += bke::mesh::face_triangles_num(faces[i].size());
      }
    }
  });
}

/* Count how many triangles for each material. */
static Array<int> mesh_render_data_mat_tri_len_build(const MeshRenderData &mr)
{
  threading::EnumerableThreadSpecific<Array<int>> all_tri_counts(
      [&]() { return Array<int>(mr.materials_num, 0); });

  if (mr.extract_type == MeshExtractType::BMesh) {
    accumululate_material_counts_bm(*mr.bm, all_tri_counts);
  }
  else {
    accumululate_material_counts_mesh(mr, all_tri_counts);
  }

  Array<int> &tris_num_by_material = all_tri_counts.local();
  for (const Array<int> &counts : all_tri_counts) {
    if (&counts != &tris_num_by_material) {
      for (const int i : tris_num_by_material.index_range()) {
        tris_num_by_material[i] += counts[i];
      }
    }
  }
  return std::move(tris_num_by_material);
}

static Array<int> calc_face_tri_starts_bmesh(const MeshRenderData &mr,
                                             MutableSpan<int> material_tri_starts)
{
  BMesh &bm = *mr.bm;
  Array<int> face_tri_offsets(bm.totface);
#ifndef NDEBUG
  face_tri_offsets.fill(-1);
#endif

  const int mat_last = mr.materials_num - 1;
  BMIter iter;
  BMFace *face;
  int i;
  BM_ITER_MESH_INDEX (face, &iter, &bm, BM_FACES_OF_MESH, i) {
    if (BM_elem_flag_test(face, BM_ELEM_HIDDEN)) {
      continue;
    }
    const int mat = std::clamp(int(face->mat_nr), 0, mat_last);
    face_tri_offsets[i] = material_tri_starts[mat];
    material_tri_starts[mat] += face->len - 2;
  }

  return face_tri_offsets;
}

static bool mesh_is_single_material(const OffsetIndices<int> material_tri_starts)
{
  const int used_materials = std::count_if(
      material_tri_starts.index_range().begin(),
      material_tri_starts.index_range().end(),
      [&](const int i) { return material_tri_starts[i].size() > 0; });
  return used_materials == 1;
}

static std::optional<Array<int>> calc_face_tri_starts_mesh(const MeshRenderData &mr,
                                                           MutableSpan<int> material_tri_starts)
{
  const bool single_material = mesh_is_single_material(material_tri_starts.as_span());
  if (single_material && mr.hide_poly.is_empty()) {
    return std::nullopt;
  }

  const OffsetIndices faces = mr.faces;
  const Span<bool> hide_poly = mr.hide_poly;

  Array<int> face_tri_offsets(faces.size());
#ifndef NDEBUG
  face_tri_offsets.fill(-1);
#endif

  if (single_material) {
    int offset = 0;
    for (const int face : faces.index_range()) {
      if (hide_poly[face]) {
        continue;
      }
      face_tri_offsets[face] = offset;
      offset += bke::mesh::face_triangles_num(faces[face].size());
    }
    return face_tri_offsets;
  }

  const Span<int> material_indices = mr.material_indices;
  const int mat_last = mr.materials_num - 1;
  for (const int face : faces.index_range()) {
    if (!hide_poly.is_empty() && hide_poly[face]) {
      continue;
    }
    const int mat = std::clamp(material_indices[face], 0, mat_last);
    face_tri_offsets[face] = material_tri_starts[mat];
    material_tri_starts[mat] += bke::mesh::face_triangles_num(faces[face].size());
  }

  return face_tri_offsets;
}

static SortedFaceData mesh_render_data_faces_sorted_build(const MeshRenderData &mr)
{
  SortedFaceData cache;
  cache.tris_num_by_material = mesh_render_data_mat_tri_len_build(mr);
  const Span<int> tris_num_by_material = cache.tris_num_by_material;

  Array<int, 32> material_tri_starts(mr.materials_num + 1);
  material_tri_starts.as_mutable_span().drop_back(1).copy_from(tris_num_by_material);
  offset_indices::accumulate_counts_to_offsets(material_tri_starts);
  cache.visible_tris_num = material_tri_starts.last();

  /* Sort per material. */
  if (mr.extract_type == MeshExtractType::BMesh) {
    cache.face_tri_offsets = calc_face_tri_starts_bmesh(mr, material_tri_starts);
  }
  else {
    cache.face_tri_offsets = calc_face_tri_starts_mesh(mr, material_tri_starts);
  }
  return cache;
}

const SortedFaceData &mesh_render_data_faces_sorted_ensure(const MeshRenderData &mr,
                                                           MeshBufferCache &cache)
{
  if (cache.face_sorted.visible_tris_num == 0) {
    cache.face_sorted = mesh_render_data_faces_sorted_build(mr);
  }
  return cache.face_sorted;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Mesh/BMesh Interface (indirect, partially cached access to complex data).
 * \{ */

const CustomData &mesh_cd_ldata_get_from_mesh(const Mesh &mesh)
{
  switch (mesh.runtime->wrapper_type) {
    case ME_WRAPPER_TYPE_SUBD:
    case ME_WRAPPER_TYPE_MDATA:
      return mesh.corner_data;
      break;
    case ME_WRAPPER_TYPE_BMESH:
      return mesh.runtime->edit_mesh->bm->ldata;
      break;
  }

  BLI_assert(0);
  return mesh.corner_data;
}

static bool bm_edge_is_sharp(const BMEdge *const &edge)
{
  return !BM_elem_flag_test(edge, BM_ELEM_SMOOTH);
}

static bool bm_face_is_sharp(const BMFace *const &face)
{
  return !BM_elem_flag_test(face, BM_ELEM_SMOOTH);
}

/**
 * Returns which domain of normals is required because of sharp and smooth flags.
 * Similar to #Mesh::normals_domain().
 */
static bke::MeshNormalDomain bmesh_normals_domain(BMesh *bm)
{
  if (bm->totface == 0) {
    return bke::MeshNormalDomain::Point;
  }

  if (CustomData_has_layer_named(&bm->vdata, CD_PROP_FLOAT3, "custom_normal")) {
    return bke::MeshNormalDomain::Point;
  }
  if (CustomData_has_layer_named(&bm->pdata, CD_PROP_FLOAT3, "custom_normal")) {
    return bke::MeshNormalDomain::Face;
  }
  if (CustomData_has_layer_named(&bm->ldata, CD_PROP_FLOAT3, "custom_normal")) {
    return bke::MeshNormalDomain::Corner;
  }
  if (CustomData_has_layer_named(&bm->ldata, CD_PROP_INT16_2D, "custom_normal")) {
    return bke::MeshNormalDomain::Corner;
  }

  BM_mesh_elem_table_ensure(bm, BM_FACE);
  const VArray<bool> sharp_faces =
      VArray<bool>::from_derived_span<const BMFace *, bm_face_is_sharp>(
          Span(bm->ftable, bm->totface));
  const array_utils::BooleanMix face_mix = array_utils::booleans_mix_calc(sharp_faces);
  if (face_mix == array_utils::BooleanMix::AllTrue) {
    return bke::MeshNormalDomain::Face;
  }

  BM_mesh_elem_table_ensure(bm, BM_EDGE);
  const VArray<bool> sharp_edges =
      VArray<bool>::from_derived_span<const BMEdge *, bm_edge_is_sharp>(
          Span(bm->etable, bm->totedge));
  const array_utils::BooleanMix edge_mix = array_utils::booleans_mix_calc(sharp_edges);
  if (edge_mix == array_utils::BooleanMix::AllTrue) {
    return bke::MeshNormalDomain::Face;
  }

  if (edge_mix == array_utils::BooleanMix::AllFalse &&
      face_mix == array_utils::BooleanMix::AllFalse)
  {
    return bke::MeshNormalDomain::Point;
  }

  return bke::MeshNormalDomain::Corner;
}

void mesh_render_data_update_corner_normals(MeshRenderData &mr)
{
  if (mr.extract_type == MeshExtractType::Mesh) {
    mr.corner_normals = mr.mesh->corner_normals();
  }
  else {
    if (mr.bm_free_normal_offset_vert != -1 || mr.bm_free_normal_offset_face != -1 ||
        mr.bm_free_normal_offset_corner != -1)
    {
      /* If there are free custom normals they should be used directly. */
      mr.bm_loop_normals = {};
      return;
    }
    mr.bm_loop_normals.reinitialize(mr.corners_num);
    const int clnors_offset = CustomData_get_offset_named(
        &mr.bm->ldata, CD_PROP_INT16_2D, "custom_normal");
    BM_loops_calc_normal_vcos(mr.bm,
                              mr.bm_vert_coords,
                              mr.bm_vert_normals,
                              mr.bm_face_normals,
                              true,
                              mr.bm_loop_normals,
                              nullptr,
                              nullptr,
                              clnors_offset,
                              false);
  }
}

void mesh_render_data_update_face_normals(MeshRenderData &mr)
{
  if (mr.extract_type == MeshExtractType::Mesh) {
    /* Eager calculation of face normals can reduce waiting on the lazy cache's lock. */
    mr.face_normals = mr.mesh->face_normals();
  }
  else {
    /* Use #BMFace.no. */
  }
}

static void retrieve_active_attribute_names(MeshRenderData &mr,
                                            const Object &object,
                                            const Mesh &mesh)
{
  const Mesh &mesh_final = editmesh_final_or_this(object, mesh);
  mr.active_color_name = mesh_final.active_color_attribute;
  mr.default_color_name = mesh_final.default_color_attribute;
}

MeshRenderData mesh_render_data_create(Object &object,
                                       Mesh &mesh,
                                       const bool is_editmode,
                                       const bool is_paint_mode,
                                       const bool do_final,
                                       const bool do_uvedit,
                                       const bool use_hide,
                                       const ToolSettings *ts)
{
  MeshRenderData mr{};
  mr.toolsettings = ts;
  mr.materials_num = BKE_object_material_used_with_fallback_eval(object);

  mr.use_hide = use_hide;

  const Mesh *editmesh_orig = BKE_object_get_pre_modified_mesh(&object);
  if (editmesh_orig && editmesh_orig->runtime->edit_mesh) {
    const Mesh *eval_cage = DRW_object_get_editmesh_cage_for_drawing(object);

    mr.bm = editmesh_orig->runtime->edit_mesh->bm;
    mr.edit_bmesh = editmesh_orig->runtime->edit_mesh.get();
    mr.mesh = (do_final) ? &mesh : eval_cage;
    mr.edit_data = is_editmode ? mr.mesh->runtime->edit_data.get() : nullptr;

    /* If there is no distinct cage, hide unmapped edges that can't be selected. */
    mr.hide_unmapped_edges = !do_final || &mesh == eval_cage;

    mr.bm_free_normal_offset_vert = CustomData_get_offset_named(
        &mr.bm->vdata, CD_PROP_FLOAT3, "custom_normal");
    mr.bm_free_normal_offset_face = CustomData_get_offset_named(
        &mr.bm->pdata, CD_PROP_FLOAT3, "custom_normal");
    mr.bm_free_normal_offset_corner = CustomData_get_offset_named(
        &mr.bm->ldata, CD_PROP_FLOAT3, "custom_normal");

    if (bke::EditMeshData *emd = mr.edit_data) {
      if (!emd->vert_positions.is_empty()) {
        mr.bm_vert_coords = mr.edit_data->vert_positions;
        if (mr.bm_free_normal_offset_vert == -1) {
          mr.bm_vert_normals = BKE_editmesh_cache_ensure_vert_normals(*mr.edit_bmesh, *emd);
        }
        if (mr.bm_free_normal_offset_face == -1) {
          mr.bm_face_normals = BKE_editmesh_cache_ensure_face_normals(*mr.edit_bmesh, *emd);
        }
      }
    }

    int bm_ensure_types = BM_VERT | BM_EDGE | BM_LOOP | BM_FACE;

    BM_mesh_elem_index_ensure(mr.bm, bm_ensure_types);
    BM_mesh_elem_table_ensure(mr.bm, bm_ensure_types & ~BM_LOOP);

    mr.efa_act_uv = EDBM_uv_active_face_get(mr.edit_bmesh, false, false);
    mr.efa_act = BM_mesh_active_face_get(mr.bm, false, true);
    mr.eed_act = BM_mesh_active_edge_get(mr.bm);
    mr.eve_act = BM_mesh_active_vert_get(mr.bm);

    mr.vert_crease_ofs = CustomData_get_offset_named(&mr.bm->vdata, CD_PROP_FLOAT, "crease_vert");
    mr.edge_crease_ofs = CustomData_get_offset_named(&mr.bm->edata, CD_PROP_FLOAT, "crease_edge");
    mr.bweight_ofs = CustomData_get_offset_named(
        &mr.bm->edata, CD_PROP_FLOAT, "bevel_weight_edge");
#ifdef WITH_FREESTYLE
    mr.freestyle_edge_ofs = CustomData_get_offset_named(
        &mr.bm->edata, CD_PROP_BOOL, "freestyle_edge");
    mr.freestyle_face_ofs = CustomData_get_offset_named(
        &mr.bm->pdata, CD_PROP_BOOL, "freestyle_face");
#endif

    /* Use bmesh directly when the object is unchanged by any modifiers. For non-final UVs, always
     * use original bmesh since the UV editor does not support using the cage mesh with deformed
     * coordinates. */
    if ((mr.mesh->runtime->is_original_bmesh &&
         mr.mesh->runtime->wrapper_type == ME_WRAPPER_TYPE_BMESH) ||
        (do_uvedit && !do_final))
    {
      mr.extract_type = MeshExtractType::BMesh;
    }
    else {
      mr.extract_type = MeshExtractType::Mesh;

      /* Use mapping from final to original mesh when the object is in edit mode. */
      if (is_editmode && do_final) {
        mr.orig_index_vert = static_cast<const int *>(
            CustomData_get_layer(&mr.mesh->vert_data, CD_ORIGINDEX));
        mr.orig_index_edge = static_cast<const int *>(
            CustomData_get_layer(&mr.mesh->edge_data, CD_ORIGINDEX));
        mr.orig_index_face = static_cast<const int *>(
            CustomData_get_layer(&mr.mesh->face_data, CD_ORIGINDEX));
      }
      else {
        mr.orig_index_vert = nullptr;
        mr.orig_index_edge = nullptr;
        mr.orig_index_face = nullptr;
      }
    }
  }
  else {
    mr.mesh = &mesh;
    mr.edit_bmesh = nullptr;
    mr.extract_type = MeshExtractType::Mesh;
    mr.hide_unmapped_edges = false;

    if (is_paint_mode && mr.mesh) {
      mr.orig_index_vert = static_cast<const int *>(
          CustomData_get_layer(&mr.mesh->vert_data, CD_ORIGINDEX));
      mr.orig_index_edge = static_cast<const int *>(
          CustomData_get_layer(&mr.mesh->edge_data, CD_ORIGINDEX));
      mr.orig_index_face = static_cast<const int *>(
          CustomData_get_layer(&mr.mesh->face_data, CD_ORIGINDEX));
    }
    else {
      mr.orig_index_vert = nullptr;
      mr.orig_index_edge = nullptr;
      mr.orig_index_face = nullptr;
    }
  }

  if (mr.extract_type == MeshExtractType::Mesh) {
    mr.verts_num = mr.mesh->verts_num;
    mr.edges_num = mr.mesh->edges_num;
    mr.faces_num = mr.mesh->faces_num;
    mr.corners_num = mr.mesh->corners_num;
    mr.corner_tris_num = poly_to_tri_count(mr.faces_num, mr.corners_num);

    mr.vert_positions = mr.mesh->vert_positions();
    mr.edges = mr.mesh->edges();
    mr.faces = mr.mesh->faces();
    mr.corner_verts = mr.mesh->corner_verts();
    mr.corner_edges = mr.mesh->corner_edges();

    mr.orig_index_vert = static_cast<const int *>(
        CustomData_get_layer(&mr.mesh->vert_data, CD_ORIGINDEX));
    mr.orig_index_edge = static_cast<const int *>(
        CustomData_get_layer(&mr.mesh->edge_data, CD_ORIGINDEX));
    mr.orig_index_face = static_cast<const int *>(
        CustomData_get_layer(&mr.mesh->face_data, CD_ORIGINDEX));

    mr.normals_domain = mr.mesh->normals_domain();

    const bke::AttributeAccessor attributes = mr.mesh->attributes();

    mr.material_indices = *attributes.lookup<int>("material_index", bke::AttrDomain::Face);

    if (is_editmode || is_paint_mode) {
      if (use_hide) {
        mr.hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);
        mr.hide_edge = *attributes.lookup<bool>(".hide_edge", bke::AttrDomain::Edge);
        mr.hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
      }

      mr.select_vert = *attributes.lookup<bool>(".select_vert", bke::AttrDomain::Point);
      mr.select_edge = *attributes.lookup<bool>(".select_edge", bke::AttrDomain::Edge);
      mr.select_poly = *attributes.lookup<bool>(".select_poly", bke::AttrDomain::Face);
    }

    mr.sharp_faces = *attributes.lookup<bool>("sharp_face", bke::AttrDomain::Face);
  }
  else {
    BMesh *bm = mr.bm;

    mr.verts_num = bm->totvert;
    mr.edges_num = bm->totedge;
    mr.faces_num = bm->totface;
    mr.corners_num = bm->totloop;
    mr.corner_tris_num = poly_to_tri_count(mr.faces_num, mr.corners_num);

    mr.normals_domain = bmesh_normals_domain(bm);
  }

  retrieve_active_attribute_names(mr, object, *mr.mesh);

  return mr;
}

/** \} */

}  // namespace blender::draw
