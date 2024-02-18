/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_object_types.h"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_index_mask.hh"
#include "BLI_listbase.h"

#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_geometry_fields.hh"
#include "BKE_mesh.hh"

#include "GEO_mesh_copy_selection.hh"
#include "GEO_mesh_selection.hh"

namespace blender::geometry {

static void remap_verts(const OffsetIndices<int> src_faces,
                        const OffsetIndices<int> dst_faces,
                        const int src_verts_num,
                        const IndexMask &vert_mask,
                        const IndexMask &edge_mask,
                        const IndexMask &face_mask,
                        const Span<int2> src_edges,
                        const Span<int> src_corner_verts,
                        MutableSpan<int2> dst_edges,
                        MutableSpan<int> dst_corner_verts)
{
  Array<int> map(src_verts_num);
  index_mask::build_reverse_map<int>(vert_mask, map);
  threading::parallel_invoke(
      vert_mask.size() > 1024,
      [&]() {
        face_mask.foreach_index(GrainSize(512), [&](const int64_t src_i, const int64_t dst_i) {
          const IndexRange src_face = src_faces[src_i];
          const IndexRange dst_face = dst_faces[dst_i];
          for (const int i : src_face.index_range()) {
            dst_corner_verts[dst_face[i]] = map[src_corner_verts[src_face[i]]];
          }
        });
      },
      [&]() {
        edge_mask.foreach_index(GrainSize(512), [&](const int64_t src_i, const int64_t dst_i) {
          dst_edges[dst_i][0] = map[src_edges[src_i][0]];
          dst_edges[dst_i][1] = map[src_edges[src_i][1]];
        });
      });
}

static void remap_edges(const OffsetIndices<int> src_faces,
                        const OffsetIndices<int> dst_faces,
                        const int src_edges_num,
                        const IndexMask &edge_mask,
                        const IndexMask &face_mask,
                        const Span<int> src_corner_edges,
                        MutableSpan<int> dst_corner_edges)
{
  Array<int> map(src_edges_num);
  index_mask::build_reverse_map<int>(edge_mask, map);
  face_mask.foreach_index(GrainSize(512), [&](const int64_t src_i, const int64_t dst_i) {
    const IndexRange src_face = src_faces[src_i];
    const IndexRange dst_face = dst_faces[dst_i];
    for (const int i : src_face.index_range()) {
      dst_corner_edges[dst_face[i]] = map[src_corner_edges[src_face[i]]];
    }
  });
}

static void copy_loose_vert_hint(const Mesh &src, Mesh &dst)
{
  const auto &src_cache = src.runtime->loose_verts_cache;
  if (src_cache.is_cached() && src_cache.data().count == 0) {
    dst.tag_loose_verts_none();
  }
}

static void copy_loose_edge_hint(const Mesh &src, Mesh &dst)
{
  const auto &src_cache = src.runtime->loose_edges_cache;
  if (src_cache.is_cached() && src_cache.data().count == 0) {
    dst.tag_loose_edges_none();
  }
}

static void copy_overlapping_hint(const Mesh &src, Mesh &dst)
{
  if (src.no_overlapping_topology()) {
    dst.tag_overlapping_none();
  }
}

/** Gather vertex group data and array attributes in separate loops. */
static void gather_vert_attributes(const Mesh &mesh_src,
                                   const bke::AnonymousAttributePropagationInfo &propagation_info,
                                   const IndexMask &vert_mask,
                                   Mesh &mesh_dst)
{
  Set<std::string> vertex_group_names;
  LISTBASE_FOREACH (bDeformGroup *, group, &mesh_src.vertex_group_names) {
    vertex_group_names.add(group->name);
  }

  const Span<MDeformVert> src = mesh_src.deform_verts();
  if (!vertex_group_names.is_empty() && !src.is_empty()) {
    MutableSpan<MDeformVert> dst = mesh_dst.deform_verts_for_write();
    bke::gather_deform_verts(src, vert_mask, dst);
  }

  bke::gather_attributes(mesh_src.attributes(),
                         bke::AttrDomain::Point,
                         propagation_info,
                         vertex_group_names,
                         vert_mask,
                         mesh_dst.attributes_for_write());
}

std::optional<Mesh *> mesh_copy_selection(
    const Mesh &src_mesh,
    const VArray<bool> &selection,
    const bke::AttrDomain selection_domain,
    const bke::AnonymousAttributePropagationInfo &propagation_info)
{
  const Span<int2> src_edges = src_mesh.edges();
  const OffsetIndices src_faces = src_mesh.faces();
  const Span<int> src_corner_verts = src_mesh.corner_verts();
  const Span<int> src_corner_edges = src_mesh.corner_edges();
  const bke::AttributeAccessor src_attributes = src_mesh.attributes();

  if (selection.is_empty()) {
    return std::nullopt;
  }
  if (const std::optional<bool> single = selection.get_if_single()) {
    return *single ? std::nullopt : std::make_optional<Mesh *>(nullptr);
  }

  threading::EnumerableThreadSpecific<IndexMaskMemory> memory;
  IndexMask vert_mask;
  IndexMask edge_mask;
  IndexMask face_mask;
  switch (selection_domain) {
    case bke::AttrDomain::Point: {
      const VArraySpan<bool> span(selection);
      threading::parallel_invoke(
          src_mesh.verts_num > 1024,
          [&]() { vert_mask = IndexMask::from_bools(span, memory.local()); },
          [&]() { edge_mask = edge_selection_from_vert(src_edges, span, memory.local()); },
          [&]() {
            face_mask = face_selection_from_vert(
                src_faces, src_corner_verts, span, memory.local());
          });
      break;
    }
    case bke::AttrDomain::Edge: {
      const VArraySpan<bool> span(selection);
      threading::parallel_invoke(
          src_edges.size() > 1024,
          [&]() {
            edge_mask = IndexMask::from_bools(span, memory.local());
            vert_mask = vert_selection_from_edge(
                src_edges, edge_mask, src_mesh.verts_num, memory.local());
          },
          [&]() {
            face_mask = face_selection_from_edge(
                src_faces, src_corner_edges, span, memory.local());
          });
      break;
    }
    case bke::AttrDomain::Face: {
      const VArraySpan<bool> span(selection);
      face_mask = IndexMask::from_bools(span, memory.local());
      threading::parallel_invoke(
          face_mask.size() > 1024,
          [&]() {
            vert_mask = vert_selection_from_face(
                src_faces, face_mask, src_corner_verts, src_mesh.verts_num, memory.local());
          },
          [&]() {
            edge_mask = edge_selection_from_face(
                src_faces, face_mask, src_corner_edges, src_mesh.edges_num, memory.local());
          });
      break;
    }
    default:
      BLI_assert_unreachable();
      break;
  }

  if (vert_mask.is_empty()) {
    return nullptr;
  }
  const bool same_verts = vert_mask.size() == src_mesh.verts_num;
  const bool same_edges = edge_mask.size() == src_mesh.edges_num;
  const bool same_faces = face_mask.size() == src_mesh.faces_num;
  if (same_verts && same_edges && same_faces) {
    return std::nullopt;
  }

  Mesh *dst_mesh = bke::mesh_new_no_attributes(
      vert_mask.size(), edge_mask.size(), face_mask.size(), 0);
  BKE_mesh_copy_parameters_for_eval(dst_mesh, &src_mesh);
  bke::MutableAttributeAccessor dst_attributes = dst_mesh->attributes_for_write();
  dst_attributes.add<int2>(".edge_verts", bke::AttrDomain::Edge, bke::AttributeInitConstruct());
  MutableSpan<int2> dst_edges = dst_mesh->edges_for_write();

  const OffsetIndices<int> dst_faces = offset_indices::gather_selected_offsets(
      src_faces, face_mask, dst_mesh->face_offsets_for_write());
  dst_mesh->corners_num = dst_faces.total_size();
  dst_attributes.add<int>(".corner_vert", bke::AttrDomain::Corner, bke::AttributeInitConstruct());
  dst_attributes.add<int>(".corner_edge", bke::AttrDomain::Corner, bke::AttributeInitConstruct());
  MutableSpan<int> dst_corner_verts = dst_mesh->corner_verts_for_write();
  MutableSpan<int> dst_corner_edges = dst_mesh->corner_edges_for_write();

  threading::parallel_invoke(
      vert_mask.size() > 1024,
      [&]() {
        remap_verts(src_faces,
                    dst_faces,
                    src_mesh.verts_num,
                    vert_mask,
                    edge_mask,
                    face_mask,
                    src_edges,
                    src_corner_verts,
                    dst_edges,
                    dst_corner_verts);
      },
      [&]() {
        remap_edges(src_faces,
                    dst_faces,
                    src_edges.size(),
                    edge_mask,
                    face_mask,
                    src_corner_edges,
                    dst_corner_edges);
      },
      [&]() {
        gather_vert_attributes(src_mesh, propagation_info, vert_mask, *dst_mesh);
        bke::gather_attributes(src_attributes,
                               bke::AttrDomain::Edge,
                               propagation_info,
                               {".edge_verts"},
                               edge_mask,
                               dst_attributes);
        bke::gather_attributes(src_attributes,
                               bke::AttrDomain::Face,
                               propagation_info,
                               {},
                               face_mask,
                               dst_attributes);
        bke::gather_attributes_group_to_group(src_attributes,
                                              bke::AttrDomain::Corner,
                                              propagation_info,
                                              {".corner_edge", ".corner_vert"},
                                              src_faces,
                                              dst_faces,
                                              face_mask,
                                              dst_attributes);
      });

  if (selection_domain == bke::AttrDomain::Edge) {
    copy_loose_vert_hint(src_mesh, *dst_mesh);
  }
  else if (selection_domain == bke::AttrDomain::Face) {
    copy_loose_vert_hint(src_mesh, *dst_mesh);
    copy_loose_edge_hint(src_mesh, *dst_mesh);
  }
  copy_overlapping_hint(src_mesh, *dst_mesh);

  return dst_mesh;
}

std::optional<Mesh *> mesh_copy_selection_keep_verts(
    const Mesh &src_mesh,
    const VArray<bool> &selection,
    const bke::AttrDomain selection_domain,
    const bke::AnonymousAttributePropagationInfo &propagation_info)
{
  const Span<int2> src_edges = src_mesh.edges();
  const OffsetIndices src_faces = src_mesh.faces();
  const Span<int> src_corner_verts = src_mesh.corner_verts();
  const Span<int> src_corner_edges = src_mesh.corner_edges();
  const bke::AttributeAccessor src_attributes = src_mesh.attributes();

  if (selection.is_empty()) {
    return std::nullopt;
  }

  threading::EnumerableThreadSpecific<IndexMaskMemory> memory;
  IndexMask edge_mask;
  IndexMask face_mask;
  switch (selection_domain) {
    case bke::AttrDomain::Point: {
      const VArraySpan<bool> span(selection);
      threading::parallel_invoke(
          src_edges.size() > 1024,
          [&]() { edge_mask = edge_selection_from_vert(src_edges, span, memory.local()); },
          [&]() {
            face_mask = face_selection_from_vert(
                src_faces, src_corner_verts, span, memory.local());
          });
      break;
    }
    case bke::AttrDomain::Edge: {
      const VArraySpan<bool> span(selection);
      threading::parallel_invoke(
          src_edges.size() > 1024,
          [&]() { edge_mask = IndexMask::from_bools(span, memory.local()); },
          [&]() {
            face_mask = face_selection_from_edge(
                src_faces, src_corner_edges, span, memory.local());
          });
      break;
    }
    case bke::AttrDomain::Face: {
      const VArraySpan<bool> span(selection);
      face_mask = IndexMask::from_bools(span, memory.local());
      edge_mask = edge_selection_from_face(
          src_faces, face_mask, src_corner_edges, src_edges.size(), memory.local());
      break;
    }
    default:
      BLI_assert_unreachable();
      break;
  }

  const bool same_edges = edge_mask.size() == src_mesh.edges_num;
  const bool same_faces = face_mask.size() == src_mesh.faces_num;
  if (same_edges && same_faces) {
    return std::nullopt;
  }

  Mesh *dst_mesh = bke::mesh_new_no_attributes(
      src_mesh.verts_num, edge_mask.size(), face_mask.size(), 0);
  BKE_mesh_copy_parameters_for_eval(dst_mesh, &src_mesh);
  bke::MutableAttributeAccessor dst_attributes = dst_mesh->attributes_for_write();

  const OffsetIndices<int> dst_faces = offset_indices::gather_selected_offsets(
      src_faces, face_mask, dst_mesh->face_offsets_for_write());
  dst_mesh->corners_num = dst_faces.total_size();
  dst_attributes.add<int>(".corner_edge", bke::AttrDomain::Corner, bke::AttributeInitConstruct());
  MutableSpan<int> dst_corner_edges = dst_mesh->corner_edges_for_write();

  threading::parallel_invoke(
      [&]() {
        remap_edges(src_faces,
                    dst_faces,
                    src_edges.size(),
                    edge_mask,
                    face_mask,
                    src_corner_edges,
                    dst_corner_edges);
      },
      [&]() {
        bke::copy_attributes(
            src_attributes, bke::AttrDomain::Point, propagation_info, {}, dst_attributes);
        bke::gather_attributes(src_attributes,
                               bke::AttrDomain::Edge,
                               propagation_info,
                               {},
                               edge_mask,
                               dst_attributes);
        bke::gather_attributes(src_attributes,
                               bke::AttrDomain::Face,
                               propagation_info,
                               {},
                               face_mask,
                               dst_attributes);
        bke::gather_attributes_group_to_group(src_attributes,
                                              bke::AttrDomain::Corner,
                                              propagation_info,
                                              {".corner_edge"},
                                              src_faces,
                                              dst_faces,
                                              face_mask,
                                              dst_attributes);
      });

  /* Positions are not changed by the operation, so the bounds are the same. */
  dst_mesh->runtime->bounds_cache = src_mesh.runtime->bounds_cache;
  if (selection_domain == bke::AttrDomain::Face) {
    copy_loose_edge_hint(src_mesh, *dst_mesh);
  }
  copy_overlapping_hint(src_mesh, *dst_mesh);

  return dst_mesh;
}

std::optional<Mesh *> mesh_copy_selection_keep_edges(
    const Mesh &src_mesh,
    const VArray<bool> &selection,
    const bke::AttrDomain selection_domain,
    const bke::AnonymousAttributePropagationInfo &propagation_info)
{
  const OffsetIndices src_faces = src_mesh.faces();
  const bke::AttributeAccessor src_attributes = src_mesh.attributes();

  if (selection.is_empty()) {
    return std::nullopt;
  }

  IndexMaskMemory memory;
  IndexMask face_mask;
  switch (selection_domain) {
    case bke::AttrDomain::Point:
      face_mask = face_selection_from_vert(
          src_faces, src_mesh.corner_verts(), VArraySpan(selection), memory);
      break;
    case bke::AttrDomain::Edge:
      face_mask = face_selection_from_edge(
          src_faces, src_mesh.corner_edges(), VArraySpan(selection), memory);
      break;
    case bke::AttrDomain::Face:
      face_mask = IndexMask::from_bools(selection, memory);
      break;
    default:
      BLI_assert_unreachable();
      break;
  }

  const bool same_faces = face_mask.size() == src_mesh.faces_num;
  if (same_faces) {
    return std::nullopt;
  }

  Mesh *dst_mesh = bke::mesh_new_no_attributes(
      src_mesh.verts_num, src_mesh.edges_num, face_mask.size(), 0);
  BKE_mesh_copy_parameters_for_eval(dst_mesh, &src_mesh);
  bke::MutableAttributeAccessor dst_attributes = dst_mesh->attributes_for_write();

  const OffsetIndices<int> dst_faces = offset_indices::gather_selected_offsets(
      src_faces, face_mask, dst_mesh->face_offsets_for_write());
  dst_mesh->corners_num = dst_faces.total_size();
  dst_attributes.add<int>(".corner_vert", bke::AttrDomain::Corner, bke::AttributeInitConstruct());
  dst_attributes.add<int>(".corner_edge", bke::AttrDomain::Corner, bke::AttributeInitConstruct());

  bke::copy_attributes(
      src_attributes, bke::AttrDomain::Point, propagation_info, {}, dst_attributes);
  bke::copy_attributes(
      src_attributes, bke::AttrDomain::Edge, propagation_info, {}, dst_attributes);
  bke::gather_attributes(
      src_attributes, bke::AttrDomain::Face, propagation_info, {}, face_mask, dst_attributes);
  bke::gather_attributes_group_to_group(src_attributes,
                                        bke::AttrDomain::Corner,
                                        propagation_info,
                                        {},
                                        src_faces,
                                        dst_faces,
                                        face_mask,
                                        dst_attributes);

  /* Positions are not changed by the operation, so the bounds are the same. */
  dst_mesh->runtime->bounds_cache = src_mesh.runtime->bounds_cache;
  copy_loose_vert_hint(src_mesh, *dst_mesh);
  copy_overlapping_hint(src_mesh, *dst_mesh);
  return dst_mesh;
}

}  // namespace blender::geometry
