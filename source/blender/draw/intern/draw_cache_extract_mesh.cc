/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Extraction of Mesh data into VBO to feed to GPU.
 */

#include "BKE_attribute.hh"

#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"

#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_task.hh"

#include "GPU_capabilities.hh"
#include "GPU_debug.hh"

#include "GPU_debug.hh"
#include "draw_cache_extract.hh"
#include "draw_subdivision.hh"

#include "mesh_extractors/extract_mesh.hh"

// #define DEBUG_TIME

#ifdef DEBUG_TIME
#  include "BLI_time_utildefines.h"
#endif

namespace blender::draw {

static void ensure_dependency_data(MeshRenderData &mr,
                                   Span<IBOType> ibo_requests,
                                   Span<VBOType> vbo_requests,
                                   MeshBufferCache &cache)
{
  const bool request_face_normals = vbo_requests.contains(VBOType::CornerNormal) ||
                                    vbo_requests.contains(VBOType::FaceDotNormal) ||
                                    vbo_requests.contains(VBOType::EdgeFactor) ||
                                    vbo_requests.contains(VBOType::MeshAnalysis);
  const bool request_corner_normals = vbo_requests.contains(VBOType::CornerNormal);
  const bool force_corner_normals = vbo_requests.contains(VBOType::Tangents);

  if (request_face_normals) {
    mesh_render_data_update_face_normals(mr);
  }
  if ((request_corner_normals && mr.normals_domain == bke::MeshNormalDomain::Corner &&
       !mr.use_simplify_normals) ||
      force_corner_normals)
  {
    mesh_render_data_update_corner_normals(mr);
  }

  const bool calc_loose_geom = ibo_requests.contains(IBOType::Lines) ||
                               ibo_requests.contains(IBOType::LinesLoose) ||
                               ibo_requests.contains(IBOType::Points) ||
                               vbo_requests.contains(VBOType::Position) ||
                               vbo_requests.contains(VBOType::EditData) ||
                               vbo_requests.contains(VBOType::VertexNormal) ||
                               vbo_requests.contains(VBOType::IndexVert) ||
                               vbo_requests.contains(VBOType::IndexEdge) ||
                               vbo_requests.contains(VBOType::EdgeFactor);

  if (calc_loose_geom) {
    mesh_render_data_update_loose_geom(mr, cache);
  }
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Loop
 * \{ */

/**
 * The mesh normals access functions can end up mixing face corner normals calculated with the
 * costly tangent space method. The "Simplify Normals" option is supposed to avoid that, but not
 * the "Free" normals which are actually cheaper than calculating true normals.
 */
static bool use_normals_simplify(const Scene &scene, const MeshRenderData &mr)
{
  if (!(scene.r.mode & R_SIMPLIFY) || !(scene.r.mode & R_SIMPLIFY_NORMALS)) {
    return false;
  }
  if (!mr.mesh) {
    return true;
  }
  const Mesh &mesh = *mr.mesh;
  const std::optional<bke::AttributeMetaData> meta_data = mesh.attributes().lookup_meta_data(
      "custom_normal");
  if (!meta_data) {
    return false;
  }
  if (meta_data->domain == bke::AttrDomain::Corner &&
      meta_data->data_type == bke::AttrType::Int16_2D)
  {
    return true;
  }
  return false;
}

void mesh_buffer_cache_create_requested(TaskGraph & /*task_graph*/,
                                        const Scene &scene,
                                        MeshBatchCache &cache,
                                        MeshBufferCache &mbc,
                                        const Span<IBOType> ibo_requests,
                                        const Span<VBOType> vbo_requests,
                                        Object &object,
                                        Mesh &mesh,
                                        const bool is_editmode,
                                        const bool is_paint_mode,
                                        const bool do_final,
                                        const bool do_uvedit,
                                        const bool use_hide)
{
  if (ibo_requests.is_empty() && vbo_requests.is_empty()) {
    return;
  }

  MeshBufferList &buffers = mbc.buff;

  Vector<IBOType, 16> ibos_to_create;
  for (const IBOType request : ibo_requests) {
    if (!buffers.ibos.contains(request)) {
      ibos_to_create.append(request);
    }
  }

  Vector<VBOType, 16> vbos_to_create;
  for (const VBOType request : vbo_requests) {
    if (!buffers.vbos.contains(request)) {
      vbos_to_create.append(request);
    }
  }

  if (ibos_to_create.is_empty() && vbos_to_create.is_empty()) {
    return;
  }

#ifdef DEBUG_TIME
  SCOPED_TIMER(__func__);
#endif

  MeshRenderData mr = mesh_render_data_create(
      object, mesh, is_editmode, is_paint_mode, do_final, do_uvedit, use_hide, scene.toolsettings);

  mr.use_subsurf_fdots = mr.mesh && !mr.mesh->runtime->subsurf_face_dot_tags.is_empty();
  mr.use_simplify_normals = use_normals_simplify(scene, mr);

  ensure_dependency_data(mr, ibo_requests, vbo_requests, mbc);

  Array<gpu::IndexBufPtr, 16> created_ibos(ibos_to_create.size());

  {
    /* Because lines and loose lines are stored in the same buffer, they're handled separately
     * rather than from potentially multiple threads in the parallel_for_each loop below. */
    const int lines_index = ibos_to_create.as_span().first_index_try(IBOType::Lines);
    const int loose_lines_index = ibos_to_create.as_span().first_index_try(IBOType::LinesLoose);
    if (lines_index != -1 || loose_lines_index != -1) {
      extract_lines(mr,
                    lines_index == -1 ? nullptr : &created_ibos[lines_index],
                    loose_lines_index == -1 ? nullptr : &created_ibos[loose_lines_index],
                    cache.no_loose_wire);
    }
  }

  threading::parallel_for_each(ibos_to_create.index_range(), [&](const int i) {
    switch (ibos_to_create[i]) {
      case IBOType::Tris:
        created_ibos[i] = extract_tris(mr, mesh_render_data_faces_sorted_ensure(mr, mbc));
        break;
      case IBOType::Lines:
      case IBOType::LinesLoose:
        /* Handled as a special case above. */
        break;
      case IBOType::Points:
        created_ibos[i] = extract_points(mr);
        break;
      case IBOType::FaceDots:
        created_ibos[i] = extract_face_dots(mr);
        break;
      case IBOType::LinesPaintMask:
        created_ibos[i] = extract_lines_paint_mask(mr);
        break;
      case IBOType::LinesAdjacency:
        created_ibos[i] = extract_lines_adjacency(mr, cache.is_manifold);
        break;
      case IBOType::UVTris:
        created_ibos[i] = extract_edituv_tris(mr, false);
        break;
      case IBOType::EditUVTris:
        created_ibos[i] = extract_edituv_tris(mr, true);
        break;
      case IBOType::AllUVLines:
        created_ibos[i] = extract_edituv_lines(mr, UvExtractionMode::All);
        break;
      case IBOType::UVLines:
        created_ibos[i] = extract_edituv_lines(mr, UvExtractionMode::Selection);
        break;
      case IBOType::EditUVLines:
        created_ibos[i] = extract_edituv_lines(mr, UvExtractionMode::Edit);
        break;
      case IBOType::EditUVPoints:
        created_ibos[i] = extract_edituv_points(mr);
        break;
      case IBOType::EditUVFaceDots:
        created_ibos[i] = extract_edituv_face_dots(mr);
        break;
    }
  });

  Array<gpu::VertBufPtr, 16> created_vbos(vbos_to_create.size());

  const bool do_hq_normals = (scene.r.perf_flag & SCE_PERF_HQ_NORMALS) != 0 ||
                             GPU_use_hq_normals_workaround();

  threading::parallel_for_each(vbos_to_create.index_range(), [&](const int i) {
    switch (vbos_to_create[i]) {
      case VBOType::Position:
        created_vbos[i] = extract_positions(mr);
        break;
      case VBOType::CornerNormal:
        created_vbos[i] = extract_normals(mr, do_hq_normals);
        break;
      case VBOType::EdgeFactor:
        created_vbos[i] = extract_edge_factor(mr);
        break;
      case VBOType::VertexGroupWeight:
        created_vbos[i] = extract_weights(mr, cache);
        break;
      case VBOType::UVs:
        created_vbos[i] = extract_uv_maps(mr, cache);
        break;
      case VBOType::Tangents:
        created_vbos[i] = extract_tangents(mr, cache, do_hq_normals);
        break;
      case VBOType::SculptData:
        created_vbos[i] = extract_sculpt_data(mr);
        break;
      case VBOType::Orco:
        created_vbos[i] = extract_orco(mr);
        break;
      case VBOType::EditData:
        created_vbos[i] = extract_edit_data(mr);
        break;
      case VBOType::EditUVData:
        created_vbos[i] = extract_edituv_data(mr);
        break;
      case VBOType::EditUVStretchArea:
        created_vbos[i] = extract_edituv_stretch_area(mr, cache.tot_area, cache.tot_uv_area);
        break;
      case VBOType::EditUVStretchAngle:
        created_vbos[i] = extract_edituv_stretch_angle(mr);
        break;
      case VBOType::MeshAnalysis:
        created_vbos[i] = extract_mesh_analysis(mr, object.object_to_world());
        break;
      case VBOType::FaceDotPosition:
        created_vbos[i] = extract_face_dots_position(mr);
        break;
      case VBOType::FaceDotNormal:
        created_vbos[i] = extract_face_dot_normals(mr, do_hq_normals);
        break;
      case VBOType::FaceDotUV:
        created_vbos[i] = extract_face_dots_uv(mr);
        break;
      case VBOType::FaceDotEditUVData:
        created_vbos[i] = extract_face_dots_edituv_data(mr);
        break;
      case VBOType::SkinRoots:
        created_vbos[i] = extract_skin_roots(mr);
        break;
      case VBOType::IndexVert:
        created_vbos[i] = extract_vert_index(mr);
        break;
      case VBOType::IndexEdge:
        created_vbos[i] = extract_edge_index(mr);
        break;
      case VBOType::IndexFace:
        created_vbos[i] = extract_face_index(mr);
        break;
      case VBOType::IndexFaceDot:
        created_vbos[i] = extract_face_dot_index(mr);
        break;
      case VBOType::Attr0:
      case VBOType::Attr1:
      case VBOType::Attr2:
      case VBOType::Attr3:
      case VBOType::Attr5:
      case VBOType::Attr6:
      case VBOType::Attr7:
      case VBOType::Attr8:
      case VBOType::Attr9:
      case VBOType::Attr10:
      case VBOType::Attr11:
      case VBOType::Attr12:
      case VBOType::Attr13:
      case VBOType::Attr14:
      case VBOType::Attr15: {
        const int8_t attr_index = int8_t(vbos_to_create[i]) - int8_t(VBOType::Attr0);
        created_vbos[i] = extract_attribute(mr, cache.attr_used[attr_index]);
        break;
      }
      case VBOType::AttrViewer:
        created_vbos[i] = extract_attr_viewer(mr);
        break;
      case VBOType::VertexNormal:
        created_vbos[i] = extract_vert_normals(mr);
        break;
      case VBOType::PaintOverlayFlag:
        created_vbos[i] = extract_paint_overlay_flags(mr);
        break;
    }
  });

  for (const int i : ibos_to_create.index_range()) {
    buffers.ibos.add_new(ibos_to_create[i], std::move(created_ibos[i]));
  }
  for (const int i : vbos_to_create.index_range()) {
    buffers.vbos.add_new(vbos_to_create[i], std::move(created_vbos[i]));
  }
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Subdivision Extract Loop
 * \{ */

void mesh_buffer_cache_create_requested_subdiv(MeshBatchCache &cache,
                                               MeshBufferCache &mbc,
                                               const Span<IBOType> ibo_requests,
                                               const Span<VBOType> vbo_requests,
                                               DRWSubdivCache &subdiv_cache,
                                               MeshRenderData &mr)
{
  if (ibo_requests.is_empty() && vbo_requests.is_empty()) {
    return;
  }
  MeshBufferList &buffers = mbc.buff;

  mesh_render_data_update_corner_normals(mr);
  mesh_render_data_update_loose_geom(mr, mbc);
  DRW_subdivide_loose_geom(subdiv_cache, mbc);

  Set<IBOType, 16> ibos_to_create;
  for (const IBOType request : ibo_requests) {
    if (!buffers.ibos.contains(request)) {
      ibos_to_create.add_new(request);
    }
  }

  Set<VBOType, 16> vbos_to_create;
  for (const VBOType request : vbo_requests) {
    if (!buffers.vbos.contains(request)) {
      vbos_to_create.add_new(request);
    }
  }

  if (ibos_to_create.is_empty() && vbos_to_create.is_empty()) {
    return;
  }

  static gpu::DebugScope subdiv_extract_scope = {"SubdivExtraction"};
  auto capture = subdiv_extract_scope.scoped_capture();

  if (vbos_to_create.contains(VBOType::Position) || vbos_to_create.contains(VBOType::Orco)) {
    gpu::VertBufPtr orco_vbo;
    /* Don't use `add_new` because #VBOType::Orco might be requested after #VBOType::Position
     * already exists. It's inefficient to build the position VBO a second time but that's the API
     * that GPU subdivision provides. */
    buffers.vbos.add(
        VBOType::Position,
        extract_positions_subdiv(
            subdiv_cache, mr, vbos_to_create.contains(VBOType::Orco) ? &orco_vbo : nullptr));
    if (orco_vbo) {
      buffers.vbos.add_new(VBOType::Orco, std::move(orco_vbo));
    }
  }
  if (vbos_to_create.contains(VBOType::CornerNormal)) {
    /* The corner normals calculation uses positions and normals stored in the `pos` VBO. */
    buffers.vbos.add_new(
        VBOType::CornerNormal,
        extract_normals_subdiv(mr, subdiv_cache, *buffers.vbos.lookup(VBOType::Position)));
  }
  if (vbos_to_create.contains(VBOType::EdgeFactor)) {
    buffers.vbos.add_new(
        VBOType::EdgeFactor,
        extract_edge_factor_subdiv(subdiv_cache, mr, *buffers.vbos.lookup(VBOType::Position)));
  }
  if (ibos_to_create.contains(IBOType::Lines) || ibos_to_create.contains(IBOType::LinesLoose)) {
    gpu::IndexBufPtr lines_ibo;
    gpu::IndexBufPtr lines_loose_ibo;
    extract_lines_subdiv(subdiv_cache,
                         mr,
                         ibos_to_create.contains(IBOType::Lines) ? &lines_ibo : nullptr,
                         ibos_to_create.contains(IBOType::LinesLoose) ? &lines_loose_ibo : nullptr,
                         cache.no_loose_wire);
    if (lines_ibo) {
      buffers.ibos.add_new(IBOType::Lines, std::move(lines_ibo));
    }
    if (lines_loose_ibo) {
      buffers.ibos.add_new(IBOType::LinesLoose, std::move(lines_loose_ibo));
    }
  }
  if (ibos_to_create.contains(IBOType::Tris)) {
    buffers.ibos.add_new(IBOType::Tris, extract_tris_subdiv(subdiv_cache, cache));
  }
  if (ibos_to_create.contains(IBOType::Points)) {
    buffers.ibos.add_new(IBOType::Points, extract_points_subdiv(mr, subdiv_cache));
  }
  if (vbos_to_create.contains(VBOType::EditData)) {
    buffers.vbos.add_new(VBOType::EditData, extract_edit_data_subdiv(mr, subdiv_cache));
  }
  if (vbos_to_create.contains(VBOType::Tangents)) {
    buffers.vbos.add_new(VBOType::Tangents, extract_tangents_subdiv(mr, subdiv_cache, cache));
  }
  if (vbos_to_create.contains(VBOType::IndexVert)) {
    buffers.vbos.add_new(VBOType::IndexVert, extract_vert_index_subdiv(subdiv_cache, mr));
  }
  if (vbos_to_create.contains(VBOType::IndexEdge)) {
    buffers.vbos.add_new(VBOType::IndexEdge, extract_edge_index_subdiv(subdiv_cache, mr));
  }
  if (vbos_to_create.contains(VBOType::IndexFace)) {
    buffers.vbos.add_new(VBOType::IndexFace, extract_face_index_subdiv(subdiv_cache, mr));
  }
  if (vbos_to_create.contains(VBOType::VertexGroupWeight)) {
    buffers.vbos.add_new(VBOType::VertexGroupWeight,
                         extract_weights_subdiv(mr, subdiv_cache, cache));
  }
  if (vbos_to_create.contains(VBOType::FaceDotNormal) ||
      vbos_to_create.contains(VBOType::FaceDotPosition) ||
      ibos_to_create.contains(IBOType::FaceDots))
  {
    gpu::VertBufPtr face_dot_position_vbo;
    gpu::VertBufPtr face_dot_normal_vbo;
    gpu::IndexBufPtr face_dot_ibo;

    /* We use only one extractor for face dots, as the work is done in a single compute shader. */
    extract_face_dots_subdiv(
        subdiv_cache,
        face_dot_position_vbo,
        vbos_to_create.contains(VBOType::FaceDotNormal) ? &face_dot_normal_vbo : nullptr,
        face_dot_ibo);
    if (vbos_to_create.contains(VBOType::FaceDotPosition)) {
      buffers.vbos.add_new(VBOType::FaceDotPosition, std::move(face_dot_position_vbo));
    }
    if (face_dot_normal_vbo) {
      buffers.vbos.add_new(VBOType::FaceDotNormal, std::move(face_dot_normal_vbo));
    }
    if (ibos_to_create.contains(IBOType::FaceDots)) {
      buffers.ibos.add_new(IBOType::FaceDots, std::move(face_dot_ibo));
    }
  }
  if (vbos_to_create.contains(VBOType::PaintOverlayFlag)) {
    buffers.vbos.add_new(VBOType::PaintOverlayFlag,
                         extract_paint_overlay_flags_subdiv(mr, subdiv_cache));
  }
  if (ibos_to_create.contains(IBOType::LinesPaintMask)) {
    buffers.ibos.add_new(IBOType::LinesPaintMask,
                         extract_lines_paint_mask_subdiv(mr, subdiv_cache));
  }
  if (ibos_to_create.contains(IBOType::LinesAdjacency)) {
    buffers.ibos.add_new(IBOType::LinesAdjacency,
                         extract_lines_adjacency_subdiv(subdiv_cache, cache.is_manifold));
  }
  if (vbos_to_create.contains(VBOType::SculptData)) {
    buffers.vbos.add_new(VBOType::SculptData, extract_sculpt_data_subdiv(mr, subdiv_cache));
  }
  if (vbos_to_create.contains(VBOType::UVs)) {
    /* Make sure UVs are computed before edituv stuffs. */
    buffers.vbos.add_new(VBOType::UVs, extract_uv_maps_subdiv(subdiv_cache, cache));
  }
  if (ibos_to_create.contains(IBOType::AllUVLines)) {
    buffers.ibos.add_new(IBOType::AllUVLines,
                         extract_edituv_lines_subdiv(mr, subdiv_cache, UvExtractionMode::All));
  }
  if (ibos_to_create.contains(IBOType::UVLines)) {
    buffers.ibos.add_new(
        IBOType::UVLines,
        extract_edituv_lines_subdiv(mr, subdiv_cache, UvExtractionMode::Selection));
  }
  if (vbos_to_create.contains(VBOType::EditUVStretchArea)) {
    buffers.vbos.add_new(
        VBOType::EditUVStretchArea,
        extract_edituv_stretch_area_subdiv(mr, subdiv_cache, cache.tot_area, cache.tot_uv_area));
  }
  if (vbos_to_create.contains(VBOType::EditUVStretchAngle)) {
    buffers.vbos.add_new(VBOType::EditUVStretchAngle,
                         extract_edituv_stretch_angle_subdiv(mr, subdiv_cache, cache));
  }
  if (vbos_to_create.contains(VBOType::EditUVData)) {
    buffers.vbos.add_new(VBOType::EditUVData, extract_edituv_data_subdiv(mr, subdiv_cache));
  }
  if (ibos_to_create.contains(IBOType::EditUVTris)) {
    buffers.ibos.add_new(IBOType::EditUVTris, extract_edituv_tris_subdiv(mr, subdiv_cache));
  }
  if (ibos_to_create.contains(IBOType::EditUVLines)) {
    buffers.ibos.add_new(IBOType::EditUVLines,
                         extract_edituv_lines_subdiv(mr, subdiv_cache, UvExtractionMode::Edit));
  }
  if (ibos_to_create.contains(IBOType::EditUVPoints)) {
    buffers.ibos.add_new(IBOType::EditUVPoints, extract_edituv_points_subdiv(mr, subdiv_cache));
  }
  for (const int8_t i : IndexRange(GPU_MAX_ATTR)) {
    const VBOType request = VBOType(int8_t(VBOType::Attr0) + i);
    if (vbos_to_create.contains(request)) {
      buffers.vbos.add_new(request,
                           extract_attribute_subdiv(mr, subdiv_cache, cache.attr_used[i]));
    }
  }
}

/** \} */

}  // namespace blender::draw
