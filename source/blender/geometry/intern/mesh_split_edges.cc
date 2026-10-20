/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup geo
 */

#include "BLI_array_utils.hh"
#include "BLI_index_mask.hh"
#include "BLI_listbase_iterator.hh"
#include "BLI_ordered_edge.hh"

#include "BKE_attribute.hh"
#include "BKE_attribute_storage.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"

#include "DNA_object_types.h"

#include "GEO_mesh_selection.hh"
#include "GEO_mesh_split_edges.hh"
#include "GEO_randomize.hh"

namespace blender::geometry {

static void propagate_vert_attributes(Mesh &mesh, const Span<int> new_to_old_verts_map)
{
  /* These types aren't supported for interpolation below. */
  CustomData_free_layers(&mesh.vert_data, CD_SHAPEKEY);
  CustomData_free_layers(&mesh.vert_data, CD_CLOTH_ORCO);
  CustomData_realloc(
      &mesh.vert_data, mesh.verts_num, mesh.verts_num + new_to_old_verts_map.size());
  mesh.verts_num += new_to_old_verts_map.size();
  mesh.attribute_storage.wrap().resize(bke::AttrDomain::Point, mesh.verts_num);

  Set<StringRef> vertex_group_names;
  for (bDeformGroup &group : mesh.vertex_group_names) {
    vertex_group_names.add(group.name);
  }
  if (!vertex_group_names.is_empty() && !mesh.deform_verts().is_empty()) {
    MutableSpan<MDeformVert> dverts = mesh.deform_verts_for_write();
    bke::gather_deform_verts(
        dverts, new_to_old_verts_map, dverts.take_back(new_to_old_verts_map.size()));
  }

  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (iter.storage_type == bke::AttrStorageType::Single) {
      return;
    }
    if (iter.domain != bke::AttrDomain::Point) {
      return;
    }
    if (iter.data_type == bke::AttrType::String) {
      return;
    }
    if (vertex_group_names.contains(iter.name)) {
      return;
    }
    bke::GSpanAttributeWriter attribute = attributes.lookup_for_write_span(iter.name);
    if (!attribute) {
      return;
    }
    array_utils::gather(attribute.span,
                        new_to_old_verts_map,
                        attribute.span.take_back(new_to_old_verts_map.size()));
    attribute.finish();
  });
  if (float3 *orco = static_cast<float3 *>(
          CustomData_get_layer_for_write(&mesh.vert_data, CD_ORCO, mesh.verts_num)))
  {
    array_utils::gather(Span(orco, mesh.verts_num),
                        new_to_old_verts_map,
                        MutableSpan(orco, mesh.verts_num).take_back(new_to_old_verts_map.size()));
  }
  if (int *orig_indices = static_cast<int *>(
          CustomData_get_layer_for_write(&mesh.vert_data, CD_ORIGINDEX, mesh.verts_num)))
  {
    array_utils::gather(
        Span(orig_indices, mesh.verts_num),
        new_to_old_verts_map,
        MutableSpan(orig_indices, mesh.verts_num).take_back(new_to_old_verts_map.size()));
  }
}

static void propagate_edge_attributes(Mesh &mesh, const Span<int> new_to_old_edge_map)
{
  CustomData_realloc(&mesh.edge_data, mesh.edges_num, mesh.edges_num + new_to_old_edge_map.size());
  mesh.edges_num += new_to_old_edge_map.size();
  mesh.attribute_storage.wrap().resize(bke::AttrDomain::Edge, mesh.edges_num);

  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (iter.storage_type == bke::AttrStorageType::Single) {
      return;
    }
    if (iter.domain != bke::AttrDomain::Edge) {
      return;
    }
    if (iter.data_type == bke::AttrType::String) {
      return;
    }
    if (iter.name == ".edge_verts") {
      /* Edge vertices are updated and combined with new edges separately. */
      return;
    }
    bke::GSpanAttributeWriter attribute = attributes.lookup_for_write_span(iter.name);
    if (!attribute) {
      return;
    }
    array_utils::gather(
        attribute.span, new_to_old_edge_map, attribute.span.take_back(new_to_old_edge_map.size()));
    attribute.finish();
  });

  if (int *orig_indices = static_cast<int *>(
          CustomData_get_layer_for_write(&mesh.edge_data, CD_ORIGINDEX, mesh.edges_num)))
  {
    array_utils::gather(
        Span(orig_indices, mesh.edges_num),
        new_to_old_edge_map,
        MutableSpan(orig_indices, mesh.edges_num).take_back(new_to_old_edge_map.size()));
  }
}

/**
 * Group the corners connected to a vertex by contiguous fans of faces, bordered by boundary edges
 * or split edges. Each corner gets the index of its group, with the order of `connected_corners`.
 * We use corners instead of edges because later on in the algorithm we only relink the
 * `corner_vert` array to each group's new vertex.
 *
 * Every face using an edge has a corner at both of the edge's vertices, so the neighbors of a
 * corner across one of its edges can be found among the other corners of the same vertex.
 *
 * \return The number of groups.
 */
static int calc_corner_groups_for_vertex(const OffsetIndices<int> faces,
                                         const Span<int> corner_edges,
                                         const Span<int> corner_to_face_map,
                                         const BitSpan split_edges,
                                         const Span<int> connected_corners,
                                         MutableSpan<int> r_corner_groups)
{
  /* The two edges of each corner that use the vertex. */
  Array<int2, 16> corner_vert_edges(connected_corners.size());
  for (const int i : connected_corners.index_range()) {
    const int corner = connected_corners[i];
    const IndexRange face = faces[corner_to_face_map[corner]];
    corner_vert_edges[i] = int2(corner_edges[corner],
                                corner_edges[bke::mesh::face_corner_prev(face, corner)]);
  }

  r_corner_groups.fill(-1);
  int groups_num = 0;
  Vector<int, 8> stack;
  for (const int start : connected_corners.index_range()) {
    if (r_corner_groups[start] != -1) {
      continue;
    }
    r_corner_groups[start] = groups_num;
    stack.append(start);
    while (!stack.is_empty()) {
      const int i = stack.pop_last();
      for (const int edge : {corner_vert_edges[i][0], corner_vert_edges[i][1]}) {
        if (split_edges[edge]) {
          continue;
        }
        for (const int other : connected_corners.index_range()) {
          if (r_corner_groups[other] != -1) {
            continue;
          }
          if (ELEM(edge, corner_vert_edges[other][0], corner_vert_edges[other][1])) {
            r_corner_groups[other] = groups_num;
            stack.append(other);
          }
        }
      }
    }
    groups_num++;
  }
  return groups_num;
}

/**
 * Calculate groups of corners that are contiguously connected to each input vertex. The group
 * indices are stored in the same order as #vert_to_corner_map.
 *
 * BLI_NOINLINE because MSVC 17.7 has a codegen bug here, given there is only a single call to this
 * function, not inlining it for all platforms won't affect performance. See
 * https://developercommunity.visualstudio.com/t/10448291 for details.
 */
BLI_NOINLINE static void calc_all_corner_groups(const OffsetIndices<int> faces,
                                                const Span<int> corner_edges,
                                                const GroupedSpan<int> vert_to_corner_map,
                                                const Span<int> corner_to_face_map,
                                                const BitSpan split_edges,
                                                const IndexMask &affected_verts,
                                                MutableSpan<int> r_corner_groups,
                                                MutableSpan<int> r_groups_num)
{
  affected_verts.foreach_index(
      [&](const int vert, const int mask) {
        r_groups_num[mask] = calc_corner_groups_for_vertex(
            faces,
            corner_edges,
            corner_to_face_map,
            split_edges,
            vert_to_corner_map[vert],
            r_corner_groups.slice(vert_to_corner_map.offsets[vert]));
      },
      exec_mode::grain_size(512));
}

/** Selected and unselected loose edges attached to a vertex. */
struct VertLooseEdges {
  Vector<int> selected;
  Vector<int> unselected;
};

/** Find selected and non-selected loose edges connected to a vertex. */
static VertLooseEdges calc_vert_loose_edges(const GroupedSpan<int> vert_to_edge_map,
                                            const BitSpan loose_edges,
                                            const BitSpan split_edges,
                                            const int vert)
{
  VertLooseEdges info;
  for (const int edge : vert_to_edge_map[vert]) {
    if (loose_edges[edge]) {
      if (split_edges[edge]) {
        info.selected.append(edge);
      }
      else {
        info.unselected.append(edge);
      }
    }
  }
  return info;
}

/**
 * Every affected vertex maps to potentially multiple output vertices. Create a mapping from
 * affected vertex index to the group of output vertex indices (indices are within those groups,
 * not indices in arrays of _all_ vertices). For every original vertex, reuse the original vertex
 * for the first of:
 *  1. The last face corner group
 *  2. The last selected loose edge
 *  3. The group of non-selected loose edges
 * Using this order prioritizes the simplicity of the no-loose-edge case, which we assume is
 * more common.
 */
static OffsetIndices<int> calc_vert_ranges_per_old_vert(const IndexMask &affected_verts,
                                                        const Span<int> corner_groups_num,
                                                        const GroupedSpan<int> vert_to_edge_map,
                                                        const BitSpan loose_edges,
                                                        const BitSpan split_edges,
                                                        Array<int> &offset_data)
{
  offset_data.reinitialize(affected_verts.size() + 1);
  MutableSpan<int> new_verts_nums = offset_data;
  threading::parallel_for(affected_verts.index_range(), 2048, [&](const IndexRange range) {
    /* Start with -1 for the reused vertex. None of the final sizes should be negative. */
    new_verts_nums.slice(range).fill(-1);
    for (const int i : range) {
      new_verts_nums[i] += corner_groups_num[i];
    }
  });
  if (!loose_edges.is_empty()) {
    affected_verts.foreach_index(
        [&](const int vert, const int mask) {
          const VertLooseEdges info = calc_vert_loose_edges(
              vert_to_edge_map, loose_edges, split_edges, vert);
          new_verts_nums[mask] += info.selected.size();
          if (corner_groups_num[mask] == 0) {
            /* Loose edges share their vertex with a corner group if possible. */
            new_verts_nums[mask] += info.unselected.size() > 0;
          }
        },
        exec_mode::grain_size(512));
  }
  return offset_indices::accumulate_counts_to_offsets(offset_data);
}

/**
 * Update corner verts so that each group of corners gets its own vertex. For the last "new vertex"
 * we can reuse the original vertex, which would otherwise become unused by any faces. The loose
 * edge case will have to deal with this later.
 */
static void update_corner_verts(const int orig_verts_num,
                                const IndexMask &affected_verts,
                                const GroupedSpan<int> vert_to_corner_map,
                                const Span<int> corner_groups,
                                const Span<int> corner_groups_num,
                                const OffsetIndices<int> new_verts_by_affected_vert,
                                MutableSpan<int> new_corner_verts)
{
  affected_verts.foreach_index(
      [&](const int vert, const int mask) {
        const Span<int> corners = vert_to_corner_map[vert];
        const Span<int> groups = corner_groups.slice(vert_to_corner_map.offsets[vert]);
        const int reused_group = corner_groups_num[mask] - 1;
        const IndexRange new_verts = new_verts_by_affected_vert[mask];
        for (const int i : corners.index_range()) {
          if (groups[i] != reused_group) {
            new_corner_verts[corners[i]] = orig_verts_num + new_verts[groups[i]];
          }
        }
      },
      exec_mode::grain_size(512));
}

static OrderedEdge edge_from_corner(const OffsetIndices<int> faces,
                                    const Span<int> corner_verts,
                                    const Span<int> corner_to_face_map,
                                    const int corner)
{
  const int face = corner_to_face_map[corner];
  const int corner_next = bke::mesh::face_corner_next(faces[face], corner);
  return OrderedEdge(corner_verts[corner], corner_verts[corner_next]);
}

/**
 * Call a function for every corner using the edge, found from the corners of one of its
 * vertices. Each face using the edge has a corner at both of its vertices. The face's corner
 * using the edge is either that corner or the previous one, depending on the face's winding
 * direction.
 */
template<typename Fn>
static void foreach_edge_corner_neighbors(const OffsetIndices<int> faces,
                                          const Span<int> corner_edges,
                                          const GroupedSpan<int> vert_to_corner_map,
                                          const Span<int> corner_to_face_map,
                                          const int2 edge_verts,
                                          const int edge,
                                          const Fn &fn)
{
  const Span<int> corners_0 = vert_to_corner_map[edge_verts[0]];
  const Span<int> corners_1 = vert_to_corner_map[edge_verts[1]];
  /* The cost is linearly proportional to the number of corners of the vertex, so the vertex with
   * fewer corners is used. This helps avoid some quadratic lookups by avoiding choosing the
   * high-valence vertex a the pole of a UV-sphere, for example. */
  const Span<int> vert_corners = corners_0.size() <= corners_1.size() ? corners_0 : corners_1;
  for (const int corner : vert_corners) {
    if (corner_edges[corner] == edge) {
      fn(corner);
      continue;
    }
    const IndexRange face = faces[corner_to_face_map[corner]];
    const int corner_prev = bke::mesh::face_corner_prev(face, corner);
    if (corner_edges[corner_prev] == edge) {
      fn(corner_prev);
    }
  }
}

/**
 * Find the edges that a split edge becomes, based on the updated corner vertex indices. Each face
 * using the edge usually gives a separate edge, but in the case of isolated split edges, multiple
 * faces can still give the same edge, so they are deduplicated.
 */
static Vector<OrderedEdge> calc_unique_split_edges(const OffsetIndices<int> faces,
                                                   const Span<int> corner_verts,
                                                   const Span<int> corner_edges,
                                                   const GroupedSpan<int> vert_to_corner_map,
                                                   const Span<int> corner_to_face_map,
                                                   const int2 edge_verts,
                                                   const int edge)
{
  Vector<OrderedEdge> unique_edges;
  foreach_edge_corner_neighbors(faces,
                                corner_edges,
                                vert_to_corner_map,
                                corner_to_face_map,
                                edge_verts,
                                edge,
                                [&](const int corner) {
                                  unique_edges.append_non_duplicates(edge_from_corner(
                                      faces, corner_verts, corner_to_face_map, corner));
                                });
  return unique_edges;
}

/**
 * Count the new edges created by splitting each selected edge. The first unique edge reuses the
 * original edge, so it isn't counted.
 */
static OffsetIndices<int> calc_new_edge_offsets(const OffsetIndices<int> faces,
                                                const Span<int2> edges,
                                                const Span<int> corner_verts,
                                                const Span<int> corner_edges,
                                                const GroupedSpan<int> vert_to_corner_map,
                                                const Span<int> corner_to_face_map,
                                                const IndexMask &selected_edges,
                                                Array<int> &r_offset_data)
{
  r_offset_data.reinitialize(selected_edges.size() + 1);
  selected_edges.foreach_index(
      [&](const int edge, const int mask) {
        const Vector<OrderedEdge> unique_edges = calc_unique_split_edges(faces,
                                                                         corner_verts,
                                                                         corner_edges,
                                                                         vert_to_corner_map,
                                                                         corner_to_face_map,
                                                                         edges[edge],
                                                                         edge);
        r_offset_data[mask] = std::max<int>(unique_edges.size() - 1, 0);
      },
      exec_mode::grain_size(1024));
  return offset_indices::accumulate_counts_to_offsets(r_offset_data);
}

/**
 * Update the vertices of each selected edge and fill the new edges created by splitting it, which
 * are stored after the original edges.
 */
static void update_selected_edges(const OffsetIndices<int> faces,
                                  const Span<int> corner_verts,
                                  const Span<int> corner_edges,
                                  const GroupedSpan<int> vert_to_corner_map,
                                  const Span<int> corner_to_face_map,
                                  const IndexMask &selected_edges,
                                  const OffsetIndices<int> new_edges_by_selected_edge,
                                  const int orig_edges_num,
                                  MutableSpan<int2> edges)
{
  selected_edges.foreach_index(
      [&](const int edge, const int mask) {
        const Vector<OrderedEdge> unique_edges = calc_unique_split_edges(faces,
                                                                         corner_verts,
                                                                         corner_edges,
                                                                         vert_to_corner_map,
                                                                         corner_to_face_map,
                                                                         edges[edge],
                                                                         edge);
        if (unique_edges.is_empty()) {
          return;
        }
        edges[edge] = int2(unique_edges.first().v_low, unique_edges.first().v_high);
        edges.slice(new_edges_by_selected_edge[mask].shift(orig_edges_num))
            .copy_from(unique_edges.as_span().drop_front(1).cast<int2>());
      },
      exec_mode::grain_size(1024));
}

/**
 * Relink the corners of split edges to the new edges. Only corners of affected vertices can use
 * split edges, and processing each vertex's corners separately avoids writing to the same corner
 * from multiple threads.
 */
static void update_corner_edges(const OffsetIndices<int> faces,
                                const Span<int> corner_verts,
                                const GroupedSpan<int> vert_to_corner_map,
                                const Span<int> corner_to_face_map,
                                const BitSpan split_edges,
                                const IndexMask &affected_verts,
                                const Span<int> selected_edge_indices,
                                const OffsetIndices<int> new_edges_by_selected_edge,
                                const int orig_edges_num,
                                const Span<int2> edges,
                                MutableSpan<int> corner_edges)
{
  affected_verts.foreach_index(
      [&](const int vert) {
        for (const int corner : vert_to_corner_map[vert]) {
          const int edge = corner_edges[corner];
          if (!split_edges[edge]) {
            continue;
          }
          const OrderedEdge new_edge = edge_from_corner(
              faces, corner_verts, corner_to_face_map, corner);
          if (new_edge == OrderedEdge(edges[edge])) {
            continue;
          }
          const IndexRange new_edges =
              new_edges_by_selected_edge[selected_edge_indices[edge]].shift(orig_edges_num);
          const Span<OrderedEdge> new_edges_data = edges.slice(new_edges).cast<OrderedEdge>();
          corner_edges[corner] = new_edges[new_edges_data.first_index(new_edge)];
        }
      },
      exec_mode::grain_size(512));
}

static void update_unselected_edges(const OffsetIndices<int> faces,
                                    const Span<int> corner_verts,
                                    const Span<int> corner_edges,
                                    const GroupedSpan<int> vert_to_corner_map,
                                    const Span<int> corner_to_face_map,
                                    const IndexMask &unselected_edges,
                                    MutableSpan<int2> edges)
{
  unselected_edges.foreach_index(
      [&](const int edge) {
        std::optional<int> edge_corner;
        foreach_edge_corner_neighbors(faces,
                                      corner_edges,
                                      vert_to_corner_map,
                                      corner_to_face_map,
                                      edges[edge],
                                      edge,
                                      [&](const int corner) {
                                        if (!edge_corner) {
                                          edge_corner = corner;
                                        }
                                      });
        if (!edge_corner) {
          return;
        }
        const OrderedEdge new_edge = edge_from_corner(
            faces, corner_verts, corner_to_face_map, *edge_corner);
        edges[edge] = int2(new_edge.v_low, new_edge.v_high);
      },
      exec_mode::grain_size(1024));
}

static void swap_edge_vert(int2 &edge, const int old_vert, const int new_vert)
{
  if (edge[0] == old_vert) {
    edge[0] = new_vert;
  }
  else if (edge[1] == old_vert) {
    edge[1] = new_vert;
  }
}

/**
 * Assign the newly created vertex duplicates to the loose edges around this vertex. Every split
 * loose edge is reattached to a newly created vertex. If there are non-split loose edges attached
 * to the vertex, they all reuse the original vertex.
 */
static void reassign_loose_edge_verts(const int orig_verts_num,
                                      const IndexMask &affected_verts,
                                      const GroupedSpan<int> vert_to_edge_map,
                                      const BitSpan loose_edges,
                                      const BitSpan split_edges,
                                      const Span<int> corner_groups_num,
                                      const OffsetIndices<int> new_verts_by_affected_vert,
                                      MutableSpan<int2> edges)
{
  affected_verts.foreach_index(
      [&](const int vert, const int mask) {
        const IndexRange new_verts = new_verts_by_affected_vert[mask];
        /* Account for the reuse of the original vertex by non-loose corner groups. In practice
         * this means using the new vertices for each split loose edge until we run out of new
         * vertices. We then expect the count to match up with the number of new vertices reserved
         * by #calc_vert_ranges_per_old_vert. */
        int new_vert_i = std::max<int>(corner_groups_num[mask] - 1, 0);
        if (new_vert_i == new_verts.size()) {
          return;
        }
        const VertLooseEdges vert_info = calc_vert_loose_edges(
            vert_to_edge_map, loose_edges, split_edges, vert);
        for (const int edge : vert_info.selected) {
          const int new_vert = orig_verts_num + new_verts[new_vert_i];
          swap_edge_vert(edges[edge], vert, new_vert);
          new_vert_i++;
          if (new_vert_i == new_verts.size()) {
            return;
          }
        }
        const int new_vert = orig_verts_num + new_verts[new_vert_i];
        for (const int orig_edge : vert_info.unselected) {
          swap_edge_vert(edges[orig_edge], vert, new_vert);
        }
      },
      exec_mode::grain_size(1024));
}

/**
 * Transform the #OffsetIndices storage of new elements per source element into a more
 * standard index map which can be used with existing utilities to copy attributes.
 */
static Array<int> offsets_to_map(const IndexMask &mask, const OffsetIndices<int> offsets)
{
  Array<int> map(offsets.total_size());
  mask.foreach_index(
      [&](const int i, const int mask) { map.as_mutable_span().slice(offsets[mask]).fill(i); },
      exec_mode::grain_size(1024));
  return map;
}

void split_edges(Mesh &mesh,
                 const IndexMask &selected_edges,
                 const bke::AttributeFilter & /*attribute_filter*/)
{
  const int orig_verts_num = mesh.verts_num;
  const int orig_edges_num = mesh.edges_num;
  const Span<int2> orig_edges = mesh.edges();
  const OffsetIndices faces = mesh.faces();

  IndexMaskMemory memory;
  const IndexMask affected_verts = vert_selection_from_edge(
      orig_edges, selected_edges, orig_verts_num, memory);
  BitVector<> selection_bits(orig_edges.size());
  selected_edges.to_bits(selection_bits);
  const IndexMask &loose_edges = mesh.loose_edges();
  BitVector<> loose_edge_bits;
  if (!loose_edges.is_empty()) {
    loose_edge_bits.resize(orig_edges.size());
    loose_edges.to_bits(loose_edge_bits);
  }

  const GroupedSpan<int> vert_to_corner_map = mesh.vert_to_corner_map();

  Array<int> vert_to_edge_offsets;
  Array<int> vert_to_edge_indices;
  GroupedSpan<int> vert_to_edge_map;
  if (!loose_edges.is_empty()) {
    vert_to_edge_map = bke::mesh::build_vert_to_edge_map(
        orig_edges, orig_verts_num, vert_to_edge_offsets, vert_to_edge_indices);
  }

  const Span<int> corner_to_face_map = mesh.corner_to_face_map();

  Array<int> corner_groups(vert_to_corner_map.data.size());
  Array<int> corner_groups_num(affected_verts.size());
  calc_all_corner_groups(faces,
                         mesh.corner_edges(),
                         vert_to_corner_map,
                         corner_to_face_map,
                         selection_bits,
                         affected_verts,
                         corner_groups,
                         corner_groups_num);

  Array<int> vert_new_vert_offset_data;
  const OffsetIndices new_verts_by_affected_vert = calc_vert_ranges_per_old_vert(
      affected_verts,
      corner_groups_num,
      vert_to_edge_map,
      loose_edge_bits,
      selection_bits,
      vert_new_vert_offset_data);

  MutableSpan<int> corner_verts = mesh.corner_verts_for_write();
  update_corner_verts(orig_verts_num,
                      affected_verts,
                      vert_to_corner_map,
                      corner_groups,
                      corner_groups_num,
                      new_verts_by_affected_vert,
                      corner_verts);

  const Span<int> orig_corner_edges = mesh.corner_edges();
  Array<int> new_edge_offset_data;
  const OffsetIndices new_edges_by_selected_edge = calc_new_edge_offsets(faces,
                                                                         orig_edges,
                                                                         corner_verts,
                                                                         orig_corner_edges,
                                                                         vert_to_corner_map,
                                                                         corner_to_face_map,
                                                                         selected_edges,
                                                                         new_edge_offset_data);
  /* Resize the edge domain before writing the new edges directly to the end of the edge array. */
  const Array<int> edge_map = offsets_to_map(selected_edges, new_edges_by_selected_edge);
  propagate_edge_attributes(mesh, edge_map);

  MutableSpan<int2> edges = mesh.edges_for_write();
  update_selected_edges(faces,
                        corner_verts,
                        orig_corner_edges,
                        vert_to_corner_map,
                        corner_to_face_map,
                        selected_edges,
                        new_edges_by_selected_edge,
                        orig_edges_num,
                        edges);
  const IndexMask unselected_edges = selected_edges.complement(IndexRange(orig_edges_num), memory);
  update_unselected_edges(faces,
                          corner_verts,
                          orig_corner_edges,
                          vert_to_corner_map,
                          corner_to_face_map,
                          unselected_edges,
                          edges);
  /* Corner edges are updated last, edge updates above use them to find corners of each edge. */
  Array<int> selected_edge_indices(orig_edges_num);
  index_mask::build_reverse_map(selected_edges, selected_edge_indices.as_mutable_span());
  update_corner_edges(faces,
                      corner_verts,
                      vert_to_corner_map,
                      corner_to_face_map,
                      selection_bits,
                      affected_verts,
                      selected_edge_indices,
                      new_edges_by_selected_edge,
                      orig_edges_num,
                      edges,
                      mesh.corner_edges_for_write());

  if (!loose_edges.is_empty()) {
    reassign_loose_edge_verts(orig_verts_num,
                              affected_verts,
                              vert_to_edge_map,
                              loose_edge_bits,
                              selection_bits,
                              corner_groups_num,
                              new_verts_by_affected_vert,
                              edges);
  }

  const Array<int> vert_map = offsets_to_map(affected_verts, new_verts_by_affected_vert);
  propagate_vert_attributes(mesh, vert_map);

  mesh.tag_edges_split();

  debug_randomize_vert_order(&mesh);
  debug_randomize_edge_order(&mesh);
}

}  // namespace blender::geometry
