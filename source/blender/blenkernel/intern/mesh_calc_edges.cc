/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BLI_map.hh"
#include "BLI_ordered_edge.hh"
#include "BLI_task.hh"
#include "BLI_threads.h"

#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_mesh.hh"

namespace blender::bke {

namespace calc_edges {

/**
 * Return a hash value that is likely to be different in the low bits from the normal `hash()`
 * function. This is necessary to avoid collisions in #mesh_calc_edges.
 */
static uint64_t edge_hash_2(const OrderedEdge &edge)
{
  return edge.v_low;
}

/* The map first contains an edge pointer and later an index. */
union OrigEdgeOrIndex {
  const int2 *original_edge;
  int index;
};
using EdgeMap = Map<OrderedEdge, OrigEdgeOrIndex>;

static void reserve_hash_maps(const Mesh &mesh,
                              const bool keep_existing_edges,
                              MutableSpan<EdgeMap> edge_maps)
{
  const int totedge_guess = std::max(keep_existing_edges ? mesh.edges_num : 0, mesh.faces_num * 2);
  threading::parallel_for_each(
      edge_maps, [&](EdgeMap &edge_map) { edge_map.reserve(totedge_guess / edge_maps.size()); });
}

static void add_existing_edges_to_hash_maps(const Mesh &mesh,
                                            const uint32_t parallel_mask,
                                            MutableSpan<EdgeMap> edge_maps)
{
  /* Assume existing edges are valid. */
  const Span<int2> edges = mesh.edges();
  threading::parallel_for_each(edge_maps, [&](EdgeMap &edge_map) {
    const int task_index = &edge_map - edge_maps.data();
    for (const int2 &edge : edges) {
      const OrderedEdge ordered_edge(edge[0], edge[1]);
      /* Only add the edge when it belongs into this map. */
      if (task_index == (parallel_mask & edge_hash_2(ordered_edge))) {
        edge_map.add_new(ordered_edge, {&edge});
      }
    }
  });
}

static void add_face_edges_to_hash_maps(const Mesh &mesh,
                                        const uint32_t parallel_mask,
                                        MutableSpan<EdgeMap> edge_maps)
{
  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  threading::parallel_for_each(edge_maps, [&](EdgeMap &edge_map) {
    const int task_index = &edge_map - edge_maps.data();
    for (const int face_i : faces.index_range()) {
      const IndexRange face = faces[face_i];
      for (const int corner : face) {
        const int vert = corner_verts[corner];
        const int vert_prev = corner_verts[bke::mesh::face_corner_prev(face, corner)];
        /* Can only be the same when the mesh data is invalid. */
        if (vert_prev != vert) {
          const OrderedEdge ordered_edge(vert_prev, vert);
          /* Only add the edge when it belongs into this map. */
          if (task_index == (parallel_mask & edge_hash_2(ordered_edge))) {
            edge_map.lookup_or_add(ordered_edge, {nullptr});
          }
        }
      }
    }
  });
}

static void serialize_and_initialize_deduplicated_edges(MutableSpan<EdgeMap> edge_maps,
                                                        MutableSpan<int2> new_edges)
{
  /* All edges are distributed in the hash tables now. They have to be serialized into a single
   * array below. To be able to parallelize this, we have to compute edge index offsets for each
   * map. */
  Array<int> edge_sizes(edge_maps.size() + 1);
  for (const int i : edge_maps.index_range()) {
    edge_sizes[i] = edge_maps[i].size();
  }
  const OffsetIndices<int> edge_offsets = offset_indices::accumulate_counts_to_offsets(edge_sizes);

  threading::parallel_for_each(edge_maps, [&](EdgeMap &edge_map) {
    const int task_index = &edge_map - edge_maps.data();

    int new_edge_index = edge_offsets[task_index].first();
    for (EdgeMap::MutableItem item : edge_map.items()) {
      int2 &new_edge = new_edges[new_edge_index];
      const int2 *orig_edge = item.value.original_edge;
      if (orig_edge != nullptr) {
        /* Copy values from original edge. */
        new_edge = *orig_edge;
      }
      else {
        /* Initialize new edge. */
        new_edge = int2(item.key.v_low, item.key.v_high);
      }
      item.value.index = new_edge_index;
      new_edge_index++;
    }
  });
}

static void update_edge_indices_in_face_loops(const OffsetIndices<int> faces,
                                              const Span<int> corner_verts,
                                              const Span<EdgeMap> edge_maps,
                                              const uint32_t parallel_mask,
                                              MutableSpan<int> corner_edges)
{
  threading::parallel_for(faces.index_range(), 100, [&](IndexRange range) {
    for (const int face_index : range) {
      const IndexRange face = faces[face_index];
      for (const int corner : face) {
        const int vert = corner_verts[corner];
        const int vert_prev = corner_verts[bke::mesh::face_corner_next(face, corner)];

        int edge_index;
        if (vert_prev != vert) {
          const OrderedEdge ordered_edge(vert_prev, vert);
          /* Double lookup: First find the map that contains the edge, then lookup the edge. */
          const EdgeMap &edge_map = edge_maps[parallel_mask & edge_hash_2(ordered_edge)];
          edge_index = edge_map.lookup(ordered_edge).index;
        }
        else {
          /* This is an invalid edge; normally this does not happen in Blender,
           * but it can be part of an imported mesh with invalid geometry. See
           * #76514. */
          edge_index = 0;
        }
        corner_edges[corner] = edge_index;
      }
    }
  });
}

static int get_parallel_maps_count(const Mesh &mesh)
{
  /* Don't use parallelization when the mesh is small. */
  if (mesh.faces_num < 1000) {
    return 1;
  }
  /* Use at most 8 separate hash tables. Using more threads has diminishing returns. These threads
   * are better off doing something more useful instead. */
  const int system_thread_count = BLI_system_thread_count();
  return power_of_2_min_i(std::min(8, system_thread_count));
}

static void clear_hash_tables(MutableSpan<EdgeMap> edge_maps)
{
  threading::parallel_for_each(edge_maps, [](EdgeMap &edge_map) { edge_map.clear_and_shrink(); });
}

}  // namespace calc_edges

void mesh_calc_edges(Mesh &mesh, bool keep_existing_edges, const bool select_new_edges)
{
  /* Parallelization is achieved by having multiple hash tables for different subsets of edges.
   * Each edge is assigned to one of the hash maps based on the lower bits of a hash value. */
  const int parallel_maps = calc_edges::get_parallel_maps_count(mesh);
  BLI_assert(is_power_of_2_i(parallel_maps));
  const uint32_t parallel_mask = uint32_t(parallel_maps) - 1;
  Array<calc_edges::EdgeMap> edge_maps(parallel_maps);
  calc_edges::reserve_hash_maps(mesh, keep_existing_edges, edge_maps);

  /* Add all edges. */
  if (keep_existing_edges) {
    calc_edges::add_existing_edges_to_hash_maps(mesh, parallel_mask, edge_maps);
  }
  calc_edges::add_face_edges_to_hash_maps(mesh, parallel_mask, edge_maps);

  /* Compute total number of edges. */
  int new_totedge = 0;
  for (const calc_edges::EdgeMap &edge_map : edge_maps) {
    new_totedge += edge_map.size();
  }

  /* Create new edges. */
  MutableAttributeAccessor attributes = mesh.attributes_for_write();
  attributes.add<int>(".corner_edge", AttrDomain::Corner, AttributeInitConstruct());
  MutableSpan<int2> new_edges(MEM_cnew_array<int2>(new_totedge, __func__), new_totedge);
  calc_edges::serialize_and_initialize_deduplicated_edges(edge_maps, new_edges);
  calc_edges::update_edge_indices_in_face_loops(
      mesh.faces(), mesh.corner_verts(), edge_maps, parallel_mask, mesh.corner_edges_for_write());

  /* Free old CustomData and assign new one. */
  CustomData_free(&mesh.edge_data, mesh.edges_num);
  CustomData_reset(&mesh.edge_data);
  mesh.edges_num = new_totedge;
  attributes.add<int2>(".edge_verts", AttrDomain::Edge, AttributeInitMoveArray(new_edges.data()));

  if (select_new_edges) {
    MutableAttributeAccessor attributes = mesh.attributes_for_write();
    SpanAttributeWriter<bool> select_edge = attributes.lookup_or_add_for_write_span<bool>(
        ".select_edge", AttrDomain::Edge);
    if (select_edge) {
      int new_edge_index = 0;
      for (const calc_edges::EdgeMap &edge_map : edge_maps) {
        for (const calc_edges::EdgeMap::Item item : edge_map.items()) {
          if (item.value.original_edge == nullptr) {
            select_edge.span[new_edge_index] = true;
          }
          new_edge_index++;
        }
      }
      select_edge.finish();
    }
  }

  if (!keep_existing_edges) {
    /* All edges are rebuilt from the faces, so there are no loose edges. */
    mesh.tag_loose_edges_none();
  }

  /* Explicitly clear edge maps, because that way it can be parallelized. */
  clear_hash_tables(edge_maps);
}

}  // namespace blender::bke
