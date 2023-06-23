/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"
#include "BLI_index_mask.hh"
#include "BLI_ordered_edge.hh"
#include "BLI_vector_set.hh"

#include "BKE_attribute.hh"
#include "BKE_attribute_math.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.h"

#include "GEO_mesh_split_edges.hh"

namespace blender::geometry {

static void add_new_vertices(Mesh &mesh, const Span<int> new_to_old_verts_map)
{
  /* These types aren't supported for interpolation below. */
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

static void add_new_edges(Mesh &mesh,
                          const Span<int2> new_edges,
                          const Span<int> new_to_old_edges_map,
                          const bke::AnonymousAttributePropagationInfo &propagation_info)
{
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();

  /* Store a copy of the IDs locally since we will remove the existing attributes which
   * can also free the names, since the API does not provide pointer stability. */
  Vector<std::string> named_ids;
  Vector<bke::AnonymousAttributeIDPtr> anonymous_ids;
  for (const bke::AttributeIDRef &id : attributes.all_ids()) {
    if (attributes.lookup_meta_data(id)->domain != ATTR_DOMAIN_EDGE) {
      continue;
    }
    if (id.is_anonymous() && !propagation_info.propagate(id.anonymous_id())) {
      continue;
    }
    if (!id.is_anonymous()) {
      if (id.name() != ".edge_verts") {
        named_ids.append(id.name());
      }
    }
    else {
      anonymous_ids.append(&id.anonymous_id());
      id.anonymous_id().add_user();
    }
  }
  Vector<bke::AttributeIDRef> local_edge_ids;
  for (const StringRef name : named_ids) {
    local_edge_ids.append(name);
  }
  for (const bke::AnonymousAttributeIDPtr &id : anonymous_ids) {
    local_edge_ids.append(*id);
  }

  /* Build new arrays for the copied edge attributes. Unlike vertices, new edges aren't all at the
   * end of the array, so just copying to the new edges would overwrite old values when they were
   * still needed. */
  struct NewAttributeData {
    const bke::AttributeIDRef &local_id;
    const CPPType &type;
    void *array;
  };
  Vector<NewAttributeData> dst_attributes;
  for (const bke::AttributeIDRef &local_id : local_edge_ids) {
    bke::GAttributeReader attribute = attributes.lookup(local_id);
    if (!attribute) {
      continue;
    }

    const CPPType &type = attribute.varray.type();
    void *new_data = MEM_malloc_arrayN(new_edges.size(), type.size(), __func__);

    bke::attribute_math::gather(
        attribute.varray, new_to_old_edges_map, GMutableSpan(type, new_data, new_edges.size()));

    /* Free the original attribute as soon as possible to lower peak memory usage. */
    attributes.remove(local_id);
    dst_attributes.append({local_id, type, new_data});
  }

  int *new_orig_indices = nullptr;
  if (const int *orig_indices = static_cast<const int *>(
          CustomData_get_layer(&mesh.edata, CD_ORIGINDEX)))
  {
    new_orig_indices = static_cast<int *>(
        MEM_malloc_arrayN(new_edges.size(), sizeof(int), __func__));
    array_utils::gather(Span(orig_indices, mesh.totedge),
                        new_to_old_edges_map,
                        {new_orig_indices, new_edges.size()});
  }

  CustomData_free(&mesh.edata, mesh.totedge);
  mesh.totedge = new_edges.size();
  CustomData_add_layer_named(
      &mesh.edata, CD_PROP_INT32_2D, CD_CONSTRUCT, mesh.totedge, ".edge_verts");
  mesh.edges_for_write().copy_from(new_edges);

  if (new_orig_indices != nullptr) {
    CustomData_add_layer_with_data(
        &mesh.edata, CD_ORIGINDEX, new_orig_indices, mesh.totedge, nullptr);
  }

  for (NewAttributeData &new_data : dst_attributes) {
    attributes.add(new_data.local_id,
                   ATTR_DOMAIN_EDGE,
                   bke::cpp_type_to_custom_data_type(new_data.type),
                   bke::AttributeInitMoveArray(new_data.array));
  }
}

/** Split the vertex into duplicates so that each fan has a different vertex. */
static void split_vertex_per_fan(const int vertex,
                                 const int start_offset,
                                 const int orig_verts_num,
                                 const Span<int> fans,
                                 const Span<int> fan_sizes,
                                 const Span<Vector<int>> edge_to_loop_map,
                                 MutableSpan<int> corner_verts,
                                 MutableSpan<int> new_to_old_verts_map)
{
  int fan_start = 0;
  /* We don't need to create a new vertex for the last fan. That fan can just be connected to the
   * original vertex. */
  for (const int i : fan_sizes.index_range().drop_back(1)) {
    const int new_vert_i = start_offset + i;
    new_to_old_verts_map[new_vert_i - orig_verts_num] = vertex;

    for (const int edge_i : fans.slice(fan_start, fan_sizes[i])) {
      for (const int loop_i : edge_to_loop_map[edge_i]) {
        if (corner_verts[loop_i] == vertex) {
          corner_verts[loop_i] = new_vert_i;
        }
        /* The old vertex is on the loop containing the adjacent edge. Since this function is also
         * called on the adjacent edge, we don't replace it here. */
      }
    }
    fan_start += fan_sizes[i];
  }
}

/** Assign the newly created vertex duplicates to the loose edges around this vertex. */
static void reassign_loose_edge_verts(const int vertex,
                                      const int start_offset,
                                      const Span<int> fans,
                                      const Span<int> fan_sizes,
                                      const BoundedBitSpan loose_edges,
                                      MutableSpan<int2> edges)
{
  int fan_start = 0;
  /* We don't need to create a new vertex for the last fan. That fan can just be connected to the
   * original vertex. */
  for (const int i : fan_sizes.index_range().drop_back(1)) {
    const int new_vert = start_offset + i;
    for (const int edge_i : fans.slice(fan_start, fan_sizes[i])) {
      if (loose_edges[edge_i]) {
        if (edges[edge_i][0] == vertex) {
          edges[edge_i][0] = new_vert;
        }
        else if (edges[edge_i][1] == vertex) {
          edges[edge_i][1] = new_vert;
        }
      }
    }
    fan_start += fan_sizes[i];
  }
}

/**
 * Get the index of the adjacent edge to a loop connected to a vertex. In other words, for the
 * given polygon return the unique edge connected to the given vertex and not on the given loop.
 */
static int adjacent_edge(const Span<int> corner_verts,
                         const Span<int> corner_edges,
                         const int loop_i,
                         const IndexRange poly,
                         const int vertex)
{
  const int adjacent_loop_i = (corner_verts[loop_i] == vertex) ?
                                  bke::mesh::poly_corner_prev(poly, loop_i) :
                                  bke::mesh::poly_corner_next(poly, loop_i);
  return corner_edges[adjacent_loop_i];
}

/**
 * Calculate the disjoint fans connected to the vertex, where a fan is a group of edges connected
 * through polygons. The connected_edges vector is rearranged in such a way that edges in the same
 * fan are grouped together. The r_fans_sizes Vector gives the sizes of the different fans, and can
 * be used to retrieve the fans from connected_edges.
 */
static void calc_vertex_fans(const int vertex,
                             const Span<int> corner_verts,
                             const Span<int> new_corner_edges,
                             const OffsetIndices<int> polys,
                             const Span<Vector<int>> edge_to_loop_map,
                             const Span<int> loop_to_poly_map,
                             MutableSpan<int> connected_edges,
                             Vector<int> &r_fan_sizes)
{
  if (connected_edges.size() <= 1) {
    r_fan_sizes.append(connected_edges.size());
    return;
  }

  Vector<int> search_edges;
  int total_found_edges_num = 0;
  int fan_size = 0;
  const int total_edge_num = connected_edges.size();
  /* Iteratively go through the connected edges. The front contains already handled edges, while
   * the back contains unhandled edges. */
  while (true) {
    /* This edge has not been visited yet. */
    int curr_i = total_found_edges_num;
    int curr_edge_i = connected_edges[curr_i];

    /* Gather all the edges in this fan. */
    while (true) {
      fan_size++;

      /* Add adjacent edges to search stack. */
      for (const int loop_i : edge_to_loop_map[curr_edge_i]) {
        const int adjacent_edge_i = adjacent_edge(
            corner_verts, new_corner_edges, loop_i, polys[loop_to_poly_map[loop_i]], vertex);

        /* Find out if this edge was visited already. */
        int i = curr_i + 1;
        for (; i < total_edge_num; i++) {
          if (connected_edges[i] == adjacent_edge_i) {
            break;
          }
        }
        if (i == total_edge_num) {
          /* Already visited this edge. */
          continue;
        }
        search_edges.append(adjacent_edge_i);
        curr_i++;
        std::swap(connected_edges[curr_i], connected_edges[i]);
      }

      if (search_edges.is_empty()) {
        break;
      }

      curr_edge_i = search_edges.pop_last();
    }
    /* We have now collected all the edges in this fan. */
    total_found_edges_num += fan_size;
    BLI_assert(total_found_edges_num <= total_edge_num);
    r_fan_sizes.append(fan_size);
    if (total_found_edges_num == total_edge_num) {
      /* We have found all the edges, so this final batch must be the last connected fan. */
      break;
    }
    fan_size = 0;
  }
}

/**
 * Splits the edge into duplicates, so that each edge is connected to one poly.
 */
static void split_edge_per_poly(const int edge_i,
                                const int new_edge_start,
                                MutableSpan<Vector<int>> edge_to_loop_map,
                                MutableSpan<int> corner_edges)
{
  if (edge_to_loop_map[edge_i].size() <= 1) {
    return;
  }
  int new_edge_index = new_edge_start;
  for (const int loop_i : edge_to_loop_map[edge_i].as_span().drop_front(1)) {
    edge_to_loop_map[new_edge_index].append({loop_i});
    corner_edges[loop_i] = new_edge_index;
    new_edge_index++;
  }
  /* Only the first loop is now connected to this edge. */
  edge_to_loop_map[edge_i].resize(1);
}

void split_edges(Mesh &mesh,
                 const IndexMask &mask,
                 const bke::AnonymousAttributePropagationInfo &propagation_info)
{
  /* Flag vertices that need to be split. */
  Array<bool> should_split_vert(mesh.totvert, false);
  const Span<int2> edges = mesh.edges();
  mask.foreach_index([&](const int edge_i) {
    const int2 &edge = edges[edge_i];
    should_split_vert[edge[0]] = true;
    should_split_vert[edge[1]] = true;
  });

  /* Precalculate topology info. */
  Array<Vector<int>> vert_to_edge_map(mesh.totvert);
  for (const int i : edges.index_range()) {
    vert_to_edge_map[edges[i][0]].append(i);
    vert_to_edge_map[edges[i][1]].append(i);
  }

  Array<int> orig_edge_to_loop_offsets;
  Array<int> orig_edge_to_loop_indices;
  const GroupedSpan<int> orig_edge_to_loop_map = bke::mesh::build_edge_to_loop_map(
      mesh.corner_edges(), mesh.totedge, orig_edge_to_loop_offsets, orig_edge_to_loop_indices);

  Array<int> loop_to_poly_map = bke::mesh::build_loop_to_poly_map(mesh.polys());

  /* Store offsets, so we can split edges in parallel. */
  Array<int> edge_offsets(edges.size());
  Array<int> num_edge_duplicates(edges.size());
  int new_edges_size = edges.size();
  mask.foreach_index([&](const int edge) {
    edge_offsets[edge] = new_edges_size;
    /* We add duplicates of the edge for each poly (except the first). */
    const int num_connected_loops = orig_edge_to_loop_map[edge].size();
    const int num_duplicates = std::max(0, num_connected_loops - 1);
    new_edges_size += num_duplicates;
    num_edge_duplicates[edge] = num_duplicates;
  });

  const OffsetIndices polys = mesh.polys();
  const Array<int> orig_corner_edges = mesh.corner_edges();
  IndexMaskMemory memory;
  const bke::LooseEdgeCache &loose_edges_cache = mesh.loose_edges();
  const IndexMask loose_edges = IndexMask::from_bits(loose_edges_cache.is_loose_bits, memory);

  MutableSpan<int> corner_edges = mesh.corner_edges_for_write();

  Vector<Vector<int>> edge_to_loop_map(new_edges_size);
  threading::parallel_for(edges.index_range(), 512, [&](const IndexRange range) {
    for (const int i : range) {
      edge_to_loop_map[i].extend(orig_edge_to_loop_map[i]);
    }
  });

  /* Split corner edge indices and update the edge to corner map. This step does not take into
   * account future deduplication of the new edges, but is necessary in order to calculate the
   * new fans around each vertex. */
  mask.foreach_index([&](const int edge_i) {
    split_edge_per_poly(edge_i, edge_offsets[edge_i], edge_to_loop_map, corner_edges);
  });

  /* Update vertex to edge map with new vertices from duplicated edges. */
  mask.foreach_index([&](const int edge_i) {
    const int2 &edge = edges[edge_i];
    for (const int duplicate_i : IndexRange(edge_offsets[edge_i], num_edge_duplicates[edge_i])) {
      vert_to_edge_map[edge[0]].append(duplicate_i);
      vert_to_edge_map[edge[1]].append(duplicate_i);
    }
  });

  MutableSpan<int> corner_verts = mesh.corner_verts_for_write();

  /* Calculate vertex fans by reordering the vertex to edge maps. Fans are the the ordered
   * groups of consecutive edges between consecutive faces looping around a vertex.  */
  Array<Vector<int>> vertex_fan_sizes(mesh.totvert);
  threading::parallel_for(IndexRange(mesh.totvert), 512, [&](IndexRange range) {
    for (const int vert : range) {
      if (!should_split_vert[vert]) {
        continue;
      }
      calc_vertex_fans(vert,
                       corner_verts,
                       corner_edges,
                       polys,
                       edge_to_loop_map,
                       loop_to_poly_map,
                       vert_to_edge_map[vert],
                       vertex_fan_sizes[vert]);
    }
  });

  /* Calculate result indices per source vertex as offsets for parallelizing the next step. */
  Array<int> vert_offsets(mesh.totvert);
  int total_verts_num = mesh.totvert;
  for (const int vert : IndexRange(mesh.totvert)) {
    if (!should_split_vert[vert]) {
      continue;
    }
    vert_offsets[vert] = total_verts_num;
    /* We only create a new vertex for each fan different from the first. */
    total_verts_num += vertex_fan_sizes[vert].size() - 1;
  }

  /* Split the vertices into their duplicates so that each fan has its own result vertex.
   * Build a map from each new vertex to an old vertex to use for transferring attributes later. */
  const int new_verts_num = total_verts_num - mesh.totvert;
  Array<int> new_to_old_verts_map(new_verts_num);
  threading::parallel_for(IndexRange(mesh.totvert), 512, [&](IndexRange range) {
    for (const int vert : range) {
      if (!should_split_vert[vert]) {
        continue;
      }
      split_vertex_per_fan(vert,
                           vert_offsets[vert],
                           mesh.totvert,
                           vert_to_edge_map[vert],
                           vertex_fan_sizes[vert],
                           edge_to_loop_map,
                           corner_verts,
                           new_to_old_verts_map);
    }
  });

  /* Create deduplicated new edges based on the corner vertices at each polygon. */
  VectorSet<OrderedEdge> new_edges;
  new_edges.reserve(new_edges_size + loose_edges.size());
  for (const int i : polys.index_range()) {
    const IndexRange poly = polys[i];
    for (const int corner : poly) {
      const int vert_1 = corner_verts[corner];
      const int vert_2 = corner_verts[bke::mesh::poly_corner_next(poly, corner)];
      corner_edges[corner] = new_edges.index_of_or_add_as(OrderedEdge(vert_1, vert_2));
    }
  }
  loose_edges.foreach_index([&](const int64_t i) { new_edges.add(OrderedEdge(edges[i])); });

  /* Build a map of old to new edges for transferring attributes. */
  Array<int> new_to_old_edges_map(new_edges.size());
  auto index_mask_to_indices = [&](const IndexMask &mask, MutableSpan<int> indices) {
    for (const int i : mask.index_range()) {
      indices[i] = mask[i];
    }
  };
  index_mask_to_indices(loose_edges,
                        new_to_old_edges_map.as_mutable_span().take_back(loose_edges.size()));
  for (const int i : polys.index_range()) {
    const IndexRange poly = polys[i];
    for (const int corner : poly) {
      const int new_edge_i = corner_edges[corner];
      const int old_edge_i = orig_corner_edges[corner];
      new_to_old_edges_map[new_edge_i] = old_edge_i;
    }
  }

  /* Resize the mesh to add the new vertices and rebuild the edges. */
  add_new_vertices(mesh, new_to_old_verts_map);
  add_new_edges(mesh, new_edges.as_span().cast<int2>(), new_to_old_edges_map, propagation_info);

  /* Connect loose edges to duplicated vertices. */
  if (loose_edges_cache.count > 0) {
    MutableSpan<int2> new_edges_span = mesh.edges_for_write();
    threading::parallel_for(should_split_vert.index_range(), 512, [&](IndexRange range) {
      for (const int vert : range) {
        if (!should_split_vert[vert]) {
          continue;
        }
        reassign_loose_edge_verts(vert,
                                  vert_offsets[vert],
                                  vert_to_edge_map[vert],
                                  vertex_fan_sizes[vert],
                                  loose_edges_cache.is_loose_bits,
                                  new_edges_span);
      }
    });
  }

  BKE_mesh_tag_edges_split(&mesh);
}

}  // namespace blender::geometry
