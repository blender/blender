/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"
#include "BLI_index_mask.hh"
#include "BLI_index_mask_ops.hh"
#include "BLI_ordered_edge.hh"

#include "BKE_attribute.hh"
#include "BKE_attribute_math.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.h"

#include "GEO_mesh_split_edges.hh"

namespace blender::geometry {

static void propagate_vert_attributes(Mesh &mesh, const Span<int> new_to_old_verts_map)
{
  /* These types aren't supported for interpolation below. */
  CustomData_free_layers(&mesh.vdata, CD_BWEIGHT, mesh.totvert);
  CustomData_free_layers(&mesh.vdata, CD_SHAPEKEY, mesh.totvert);
  CustomData_free_layers(&mesh.vdata, CD_CLOTH_ORCO, mesh.totvert);
  CustomData_free_layers(&mesh.vdata, CD_MVERT_SKIN, mesh.totvert);
  CustomData_realloc(&mesh.vdata, mesh.totvert, mesh.totvert + new_to_old_verts_map.size());
  mesh.totvert += new_to_old_verts_map.size();

  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  for (const bke::AttributeIDRef &id : attributes.all_ids()) {
    if (attributes.lookup_meta_data(id)->domain != ATTR_DOMAIN_POINT) {
      continue;
    }
    bke::GSpanAttributeWriter attribute = attributes.lookup_for_write_span(id);
    if (!attribute) {
      continue;
    }

    bke::attribute_math::gather(attribute.span,
                                new_to_old_verts_map,
                                attribute.span.take_back(new_to_old_verts_map.size()));

    attribute.finish();
  }
  if (float3 *orco = static_cast<float3 *>(
          CustomData_get_layer_for_write(&mesh.vdata, CD_ORCO, mesh.totvert)))
  {
    array_utils::gather(Span(orco, mesh.totvert),
                        new_to_old_verts_map,
                        MutableSpan(orco, mesh.totvert).take_back(new_to_old_verts_map.size()));
  }
  if (int *orig_indices = static_cast<int *>(
          CustomData_get_layer_for_write(&mesh.vdata, CD_ORIGINDEX, mesh.totvert)))
  {
    array_utils::gather(
        Span(orig_indices, mesh.totvert),
        new_to_old_verts_map,
        MutableSpan(orig_indices, mesh.totvert).take_back(new_to_old_verts_map.size()));
  }
}

static void propagate_edge_attributes(Mesh &mesh, const Span<int> new_to_old_edge_map)
{
  CustomData_free_layers(&mesh.edata, CD_FREESTYLE_EDGE, mesh.totedge);
  CustomData_realloc(&mesh.edata, mesh.totedge, mesh.totedge + new_to_old_edge_map.size());
  mesh.totedge += new_to_old_edge_map.size();

  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  for (const bke::AttributeIDRef &id : attributes.all_ids()) {
    if (attributes.lookup_meta_data(id)->domain != ATTR_DOMAIN_EDGE) {
      continue;
    }
    if (id.name() == ".edge_verts") {
      /* Edge vertices are updated and combined with new edges separately. */
      continue;
    }
    bke::GSpanAttributeWriter attribute = attributes.lookup_for_write_span(id);
    if (!attribute) {
      continue;
    }
    bke::attribute_math::gather(
        attribute.span, new_to_old_edge_map, attribute.span.take_back(new_to_old_edge_map.size()));
    attribute.finish();
  }

  if (int *orig_indices = static_cast<int *>(
          CustomData_get_layer_for_write(&mesh.edata, CD_ORIGINDEX, mesh.totedge)))
  {
    array_utils::gather(
        Span(orig_indices, mesh.totedge),
        new_to_old_edge_map,
        MutableSpan(orig_indices, mesh.totedge).take_back(new_to_old_edge_map.size()));
  }
}

/** A vertex is selected if it's used by a selected edge. */
static IndexMask vert_selection_from_edge(const Span<int2> edges,
                                          const IndexMask &selected_edges,
                                          const int verts_num,
                                          Vector<int64_t> &memory)
{
  Array<bool> array(verts_num, false);
  threading::parallel_for(selected_edges.index_range(), 2048, [&](const IndexRange range) {
    for (const int i : selected_edges.slice(range)) {
      array[edges[i][0]] = true;
      array[edges[i][1]] = true;
    }
  });
  for (const int i : array.index_range()) {
    if (array[i]) {
      memory.append(i);
    }
  }
  return IndexMask(memory);
}

static BitVector<> selection_to_bit_vector(const IndexMask &selection, const int total_size)
{
  BitVector<> bits(total_size);
  for (const int i : selection) {
    bits[i].set();
  }
  return bits;
}

/**
 * Used for fanning around the corners connected to a vertex.
 *
 * Depending on the winding direction of neighboring faces, travelling from a corner across an edge
 * to a different face can give a corner that uses a different vertex than the original. To find
 * the face's corner that uses the original vertex, we may have to use the next corner instead.
 */
static int corner_on_edge_connected_to_vert(const Span<int> corner_verts,
                                            const int corner,
                                            const IndexRange face,
                                            const int vert)
{
  if (corner_verts[corner] == vert) {
    return corner;
  }
  const int other = bke::mesh::poly_corner_next(face, corner);
  BLI_assert(corner_verts[other] == vert);
  return other;
}

using CornerGroup = Vector<int>;

/**
 * Collect groups of corners connected by edges bordered by boundary edges or split edges. We store
 * corner indices instead of edge indices because later on in the algorithm we only relink the
 * `corner_vert` array to each group's new vertex.
 *
 * The corners are not ordered in winding order, since we only need to group connected faces into
 * each group.
 */
static Vector<CornerGroup> calc_corner_groups_for_vertex(
    const OffsetIndices<int> faces,
    const Span<int> corner_verts,
    const Span<int> corner_edges,
    const Span<Vector<int>> edge_to_corner_map,
    const Span<int> corner_to_face_map,
    const BitSpan split_edges,
    const Span<int> connected_corners,
    const int vert)
{
  Vector<CornerGroup> groups;
  /* Each corner should only be added to a single group. */
  BitVector<> used_corners(connected_corners.size());
  for (const int start_corner : connected_corners) {
    CornerGroup group;
    Vector<int> corner_stack({start_corner});
    while (!corner_stack.is_empty()) {
      const int corner = corner_stack.pop_last();
      const int i = connected_corners.first_index(corner);
      if (used_corners[i]) {
        continue;
      }
      used_corners[i].set();
      group.append(corner);
      const int face = corner_to_face_map[corner];
      const int prev_corner = bke::mesh::poly_corner_prev(faces[face], corner);
      /* Travel across the two edges neighboring this vertex, if they aren't split. */
      for (const int edge : {corner_edges[corner], corner_edges[prev_corner]}) {
        if (split_edges[edge]) {
          continue;
        }
        for (const int other_corner : edge_to_corner_map[edge]) {
          const int other_face = corner_to_face_map[other_corner];
          if (other_face == face) {
            /* Avoid continuing back to the same face. */
            continue;
          }
          const int neighbor_corner = corner_on_edge_connected_to_vert(
              corner_verts, other_corner, faces[other_face], vert);
          corner_stack.append(neighbor_corner);
        }
      }
    }
    if (!group.is_empty()) {
      groups.append(std::move(group));
    }
  }

  return groups;
}

/* Calculate groups of corners that are contiguously connected to each input vertex. */
static Array<Vector<CornerGroup>> calc_all_corner_groups(
    const OffsetIndices<int> faces,
    const Span<int> corner_verts,
    const Span<int> corner_edges,
    const Span<Vector<int>> vert_to_corner_map,
    const Span<Vector<int>> edge_to_corner_map,
    const Span<int> corner_to_face_map,
    const BitSpan split_edges,
    const IndexMask &affected_verts)
{
  Array<Vector<CornerGroup>> corner_groups(affected_verts.size(), NoInitialization());
  threading::parallel_for(affected_verts.index_range(), 512, [&](const IndexRange range) {
    for (const int mask : range) {
      const int vert = affected_verts[mask];
      new (&corner_groups[mask])
          Vector<CornerGroup>(calc_corner_groups_for_vertex(faces,
                                                            corner_verts,
                                                            corner_edges,
                                                            edge_to_corner_map,
                                                            corner_to_face_map,
                                                            split_edges,
                                                            vert_to_corner_map[vert],
                                                            vert));
    }
  });
  return corner_groups;
}

/** Selected and unselected loose edges attached to a vertex. */
struct VertLooseEdges {
  Vector<int> selected;
  Vector<int> unselected;
};

/** Find selected and non-selected loose edges connected to a vertex. */
static VertLooseEdges calc_vert_loose_edges(const Span<Vector<int>> vert_to_edge_map,
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
static OffsetIndices<int> calc_vert_ranges_per_old_vert(
    const IndexMask &affected_verts,
    const Span<Vector<CornerGroup>> corner_groups,
    const Span<Vector<int>> vert_to_edge_map,
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
      new_verts_nums[i] += corner_groups[i].size();
    }
  });
  if (!loose_edges.is_empty()) {
    threading::parallel_for(affected_verts.index_range(), 512, [&](const IndexRange range) {
      for (const int mask : range) {
        const int vert = affected_verts[mask];
        const VertLooseEdges info = calc_vert_loose_edges(
            vert_to_edge_map, loose_edges, split_edges, vert);
        new_verts_nums[mask] += info.selected.size();
        if (corner_groups[mask].is_empty()) {
          /* Loose edges share their vertex with a corner group if possible. */
          new_verts_nums[mask] += info.unselected.size() > 0;
        }
      }
    });
  }
  offset_indices::accumulate_counts_to_offsets(offset_data);
  return OffsetIndices<int>(offset_data);
}

/**
 * Update corner verts so that each group of corners gets its own vertex. For the last "new vertex"
 * we can reuse the original vertex, which would otherwise become unused by any faces. The loose
 * edge case will have to deal with this later.
 */
static void update_corner_verts(const int orig_verts_num,
                                const Span<Vector<CornerGroup>> corner_groups,
                                const OffsetIndices<int> new_verts_by_affected_vert,
                                MutableSpan<int> new_corner_verts)
{
  threading::parallel_for(corner_groups.index_range(), 512, [&](const IndexRange range) {
    for (const int new_vert : range) {
      const Span<CornerGroup> groups = corner_groups[new_vert];
      const IndexRange new_verts = new_verts_by_affected_vert[new_vert];
      for (const int group : groups.index_range().drop_back(1)) {
        const int new_vert = orig_verts_num + new_verts[group];
        new_corner_verts.fill_indices(groups[group].as_span(), new_vert);
      }
    }
  });
}

static OrderedEdge edge_from_corner(const OffsetIndices<int> faces,
                                    const Span<int> corner_verts,
                                    const Span<int> corner_to_face_map,
                                    const int corner)
{
  const int face = corner_to_face_map[corner];
  const int corner_next = bke::mesh::poly_corner_next(faces[face], corner);
  return OrderedEdge(corner_verts[corner], corner_verts[corner_next]);
}

/**
 * Based on updated corner vertex indices, update the edges in each face. This includes updating
 * corner edge indices, adding new edges, and reusing original edges for the first "split" edge.
 * The main complexity comes from the fact that in the case of single isolated split edges, no new
 * edges are created because they all end up identical. We need to handle this case, but since it's
 * rare, we optimize for the case that it doesn't happen first.
 */
static Array<int2> calc_new_edges(const OffsetIndices<int> faces,
                                  const Span<int> corner_verts,
                                  const Span<Vector<int>> edge_to_corner_map,
                                  const Span<int> corner_to_face_map,
                                  const IndexMask &selected_edges,
                                  MutableSpan<int2> edges,
                                  MutableSpan<int> corner_edges,
                                  MutableSpan<int> r_new_edge_offsets)
{
  /* Calculate the offset of new edges assuming no new edges are identical and are merged. */
  threading::parallel_for(selected_edges.index_range(), 4096, [&](const IndexRange range) {
    for (const int mask : range) {
      const int edge = selected_edges[mask];
      r_new_edge_offsets[mask] = std::max<int>(edge_to_corner_map[edge].size() - 1, 0);
    }
  });
  offset_indices::accumulate_counts_to_offsets(r_new_edge_offsets);
  const OffsetIndices<int> offsets(r_new_edge_offsets);

  Array<int2> new_edges(offsets.total_size());

  /* Count the number of final new edges per edge, to use as offsets if there are duplicates. */
  Array<int> num_edges_per_edge_merged(r_new_edge_offsets.size());
  std::atomic<bool> found_duplicate = false;

  /* The first new edge for each selected edge is reused-- we modify the existing edge in
   * place. Simply reusing the first new edge isn't enough because deduplication might make
   * multiple new edges reuse the original. */
  Array<bool> is_reused(corner_verts.size(), false);

  /* Calculate per-original split edge deduplication of new edges, which are stored by the
   * corner vertices of connected faces. Update corner verts to store the updated indices. */
  threading::parallel_for(selected_edges.index_range(), 1024, [&](const IndexRange range) {
    for (const int mask : range) {
      const int edge = selected_edges[mask];
      if (edge_to_corner_map[edge].is_empty()) {
        /* Handle loose edges. */
        num_edges_per_edge_merged[mask] = 0;
        continue;
      }

      const int new_edges_start = offsets[mask].start();
      Vector<OrderedEdge> deduplication;
      for (const int corner : edge_to_corner_map[edge]) {
        const OrderedEdge edge = edge_from_corner(faces, corner_verts, corner_to_face_map, corner);
        int index = deduplication.first_index_of_try(edge);
        if (UNLIKELY(index != -1)) {
          found_duplicate.store(true, std::memory_order_relaxed);
        }
        else {
          index = deduplication.append_and_get_index(edge);
        }

        if (index == 0) {
          is_reused[corner] = true;
        }
        else {
          corner_edges[corner] = edges.size() + new_edges_start + index - 1;
        }
      }

      const int new_edges_num = deduplication.size() - 1;

      edges[edge] = int2(deduplication.first().v_low, deduplication.first().v_high);
      new_edges.as_mutable_span()
          .slice(new_edges_start, new_edges_num)
          .copy_from(deduplication.as_span().drop_front(1).cast<int2>());

      num_edges_per_edge_merged[mask] = new_edges_num;
    }
  });

  if (!found_duplicate) {
    /* No edges were merged, we can use the existing output array and offsets. */
    return new_edges;
  }

  /* Update corner edges to remove the "holes" left by merged new edges. */
  offset_indices::accumulate_counts_to_offsets(num_edges_per_edge_merged);
  const OffsetIndices<int> offsets_merged(num_edges_per_edge_merged);
  threading::parallel_for(selected_edges.index_range(), 2048, [&](const IndexRange range) {
    for (const int mask : range) {
      const int edge = selected_edges[mask];
      const int difference = offsets[mask].start() - offsets_merged[mask].start();
      for (const int corner : edge_to_corner_map[edge]) {
        if (!is_reused[corner]) {
          corner_edges[corner] -= difference;
        }
      }
    }
  });

  /* Create new edges without the empty slots for the duplicates */
  Array<int2> new_edges_merged(offsets_merged.total_size());
  threading::parallel_for(offsets_merged.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      new_edges_merged.as_mutable_span()
          .slice(offsets_merged[i])
          .copy_from(new_edges.as_span().slice(offsets[i].start(), offsets_merged[i].size()));
    }
  });

  r_new_edge_offsets.copy_from(num_edges_per_edge_merged);
  return new_edges_merged;
}

static void update_unselected_edges(const OffsetIndices<int> faces,
                                    const Span<int> corner_verts,
                                    const Span<Vector<int>> edge_to_corner_map,
                                    const Span<int> corner_to_face_map,
                                    const IndexMask unselected_edges,
                                    MutableSpan<int2> edges)
{
  threading::parallel_for(unselected_edges.index_range(), 1024, [&](const IndexRange range) {
    for (const int edge : unselected_edges.slice(range)) {
      const Span<int> edge_corners = edge_to_corner_map[edge];
      if (edge_corners.is_empty()) {
        continue;
      }
      const int corner = edge_corners.first();
      const OrderedEdge new_edge = edge_from_corner(
          faces, corner_verts, corner_to_face_map, corner);
      edges[edge] = int2(new_edge.v_low, new_edge.v_high);
    }
  });
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
                                      const Span<Vector<int>> vert_to_edge_map,
                                      const BitSpan loose_edges,
                                      const BitSpan split_edges,
                                      const Span<Vector<CornerGroup>> corner_groups,
                                      const OffsetIndices<int> new_verts_by_affected_vert,
                                      MutableSpan<int2> edges)
{
  threading::parallel_for(affected_verts.index_range(), 512, [&](const IndexRange range) {
    for (const int mask : range) {
      const int vert = affected_verts[mask];
      const IndexRange new_verts = new_verts_by_affected_vert[mask];
      /* Account for the reuse of the original vertex by non-loose corner groups. In practice this
       * means using the new vertices for each split loose edge until we run out of new vertices.
       * We then expect the count to match up with the number of new vertices reserved by
       * #calc_vert_ranges_per_old_vert. */
      int new_vert_i = std::max<int>(corner_groups[mask].size() - 1, 0);
      if (new_vert_i == new_verts.size()) {
        continue;
      }
      const VertLooseEdges vert_info = calc_vert_loose_edges(
          vert_to_edge_map, loose_edges, split_edges, vert);

      bool finished = false;
      for (const int edge : vert_info.selected) {
        const int new_vert = orig_verts_num + new_verts[new_vert_i];
        swap_edge_vert(edges[edge], vert, new_vert);
        new_vert_i++;
        if (new_vert_i == new_verts.size()) {
          finished = true;
          break;
        }
      }
      if (finished) {
        continue;
      }

      const int new_vert = orig_verts_num + new_verts[new_vert_i];
      for (const int orig_edge : vert_info.unselected) {
        swap_edge_vert(edges[orig_edge], vert, new_vert);
      }
    }
  });
}

/**
 * Transform the #OffsetIndices storage of new elements per source element into a more
 * standard index map which can be used with existing utilities to copy attributes.
 */
static Array<int> offsets_to_map(const IndexMask &mask, const OffsetIndices<int> offsets)
{
  Array<int> map(offsets.total_size());
  threading::parallel_for(mask.index_range(), 512, [&](const IndexRange range) {
    for (const int mask_i : range) {
      const int i = mask[mask_i];
      map.as_mutable_span().slice(offsets[mask_i]).fill(i);
    }
  });
  return map;
}

void split_edges(Mesh &mesh,
                 const IndexMask selected_edges,
                 const bke::AnonymousAttributePropagationInfo & /*propagation_info*/)
{
  const int orig_verts_num = mesh.totvert;
  const Span<int2> orig_edges = mesh.edges();
  const OffsetIndices faces = mesh.polys();

  Vector<int64_t> affected_verts_indices;
  const IndexMask affected_verts = vert_selection_from_edge(
      orig_edges, selected_edges, orig_verts_num, affected_verts_indices);
  const BitVector<> selection_bits = selection_to_bit_vector(selected_edges, orig_edges.size());
  const bke::LooseEdgeCache &loose_edges = mesh.loose_edges();

  const Array<Vector<int>> vert_to_corner_map = bke::mesh_topology::build_vert_to_loop_map(
      mesh.corner_verts(), orig_verts_num);

  const Array<Vector<int>> edge_to_corner_map = bke::mesh_topology::build_edge_to_loop_map(
      mesh.corner_edges(), orig_edges.size());

  Array<Vector<int>> vert_to_edge_map;
  if (loose_edges.count > 0) {
    vert_to_edge_map = bke::mesh_topology::build_vert_to_edge_map(orig_edges, orig_verts_num);
  }

  const Array<int> corner_to_face_map = bke::mesh_topology::build_loop_to_poly_map(mesh.polys());

  const Array<Vector<CornerGroup>> corner_groups = calc_all_corner_groups(faces,
                                                                          mesh.corner_verts(),
                                                                          mesh.corner_edges(),
                                                                          vert_to_corner_map,
                                                                          edge_to_corner_map,
                                                                          corner_to_face_map,
                                                                          selection_bits,
                                                                          affected_verts);

  Array<int> vert_new_vert_offset_data;
  const OffsetIndices new_verts_by_affected_vert = calc_vert_ranges_per_old_vert(
      affected_verts,
      corner_groups,
      vert_to_edge_map,
      loose_edges.is_loose_bits,
      selection_bits,
      vert_new_vert_offset_data);

  MutableSpan<int> corner_verts = mesh.corner_verts_for_write();
  update_corner_verts(orig_verts_num, corner_groups, new_verts_by_affected_vert, corner_verts);

  Array<int> new_edge_offsets(selected_edges.size() + 1);
  Array<int2> new_edges = calc_new_edges(faces,
                                         corner_verts,
                                         edge_to_corner_map,
                                         corner_to_face_map,
                                         selected_edges,
                                         mesh.edges_for_write(),
                                         mesh.corner_edges_for_write(),
                                         new_edge_offsets);
  Vector<int64_t> unselected_indices;
  const IndexMask unselected_edges = selected_edges.invert(orig_edges.index_range(),
                                                           unselected_indices);
  update_unselected_edges(faces,
                          corner_verts,
                          edge_to_corner_map,
                          corner_to_face_map,
                          unselected_edges,
                          mesh.edges_for_write());

  if (loose_edges.count > 0) {
    reassign_loose_edge_verts(orig_verts_num,
                              affected_verts,
                              vert_to_edge_map,
                              loose_edges.is_loose_bits,
                              selection_bits,
                              corner_groups,
                              new_verts_by_affected_vert,
                              mesh.edges_for_write());
  }

  const Array<int> edge_map = offsets_to_map(selected_edges, new_edge_offsets.as_span());
  propagate_edge_attributes(mesh, edge_map);
  mesh.edges_for_write().take_back(new_edges.size()).copy_from(new_edges);

  const Array<int> vert_map = offsets_to_map(affected_verts, new_verts_by_affected_vert);
  propagate_vert_attributes(mesh, vert_map);

  BKE_mesh_tag_edges_split(&mesh);
}

}  // namespace blender::geometry
