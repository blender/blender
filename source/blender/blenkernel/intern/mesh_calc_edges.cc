/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BLI_array_utils.hh"
#include "BLI_ordered_edge.hh"
#include "BLI_task.hh"
#include "BLI_threads.h"
#include "BLI_vector_set.hh"

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

using EdgeMap = VectorSet<OrderedEdge,
                          DefaultProbingStrategy,
                          DefaultHash<OrderedEdge>,
                          DefaultEquality<OrderedEdge>,
                          SimpleVectorSetSlot<OrderedEdge, int>,
                          GuardedAllocator>;

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
    for (const int2 edge : edges) {
      const OrderedEdge ordered_edge(edge);
      /* Only add the edge when it belongs into this map. */
      if (task_index == (parallel_mask & edge_hash_2(ordered_edge))) {
        edge_map.add(ordered_edge);
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
        if (LIKELY(vert_prev != vert)) {
          const OrderedEdge ordered_edge(vert_prev, vert);
          /* Only add the edge when it belongs into this map. */
          if (task_index == (parallel_mask & edge_hash_2(ordered_edge))) {
            edge_map.add(ordered_edge);
          }
        }
      }
    }
  });
}

static void serialize_and_initialize_deduplicated_edges(MutableSpan<EdgeMap> edge_maps,
                                                        const OffsetIndices<int> edge_offsets,
                                                        MutableSpan<int2> new_edges)
{
  threading::parallel_for_each(edge_maps, [&](EdgeMap &edge_map) {
    const int task_index = &edge_map - edge_maps.data();
    if (edge_offsets[task_index].is_empty()) {
      return;
    }

    MutableSpan<int2> result_edges = new_edges.slice(edge_offsets[task_index]);
    result_edges.copy_from(edge_map.as_span().cast<int2>());
  });
}

static void update_edge_indices_in_face_loops(const OffsetIndices<int> faces,
                                              const Span<int> corner_verts,
                                              const Span<EdgeMap> edge_maps,
                                              const uint32_t parallel_mask,
                                              const OffsetIndices<int> edge_offsets,
                                              MutableSpan<int> corner_edges)
{
  threading::parallel_for(faces.index_range(), 100, [&](IndexRange range) {
    for (const int face_index : range) {
      const IndexRange face = faces[face_index];
      for (const int corner : face) {
        const int vert = corner_verts[corner];
        const int vert_prev = corner_verts[bke::mesh::face_corner_next(face, corner)];
        if (UNLIKELY(vert == vert_prev)) {
          /* This is an invalid edge; normally this does not happen in Blender,
           * but it can be part of an imported mesh with invalid geometry. See
           * #76514. */
          corner_edges[corner] = 0;
          continue;
        }

        const OrderedEdge ordered_edge(vert_prev, vert);
        const int task_index = parallel_mask & edge_hash_2(ordered_edge);
        const EdgeMap &edge_map = edge_maps[task_index];
        const int edge_i = edge_map.index_of(ordered_edge);
        const int edge_index = edge_offsets[task_index][edge_i];
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

static void deselect_known_edges(const OffsetIndices<int> edge_offsets,
                                 const Span<EdgeMap> edge_maps,
                                 const uint32_t parallel_mask,
                                 const Span<int2> known_edges,
                                 MutableSpan<bool> selection)
{
  threading::parallel_for(known_edges.index_range(), 2048, [&](const IndexRange range) {
    for (const int2 original_edge : known_edges.slice(range)) {
      const OrderedEdge ordered_edge(original_edge);
      const int task_index = parallel_mask & edge_hash_2(ordered_edge);
      const EdgeMap &edge_map = edge_maps[task_index];
      const int edge_i = edge_map.index_of(ordered_edge);
      const int edge_index = edge_offsets[task_index][edge_i];
      selection[edge_index] = false;
    }
  });
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

  Array<int> edge_sizes(edge_maps.size() + 1);
  for (const int i : edge_maps.index_range()) {
    edge_sizes[i] = edge_maps[i].size();
  }
  const OffsetIndices<int> edge_offsets = offset_indices::accumulate_counts_to_offsets(edge_sizes);

  /* Create new edges. */
  MutableAttributeAccessor attributes = mesh.attributes_for_write();
  attributes.add<int>(".corner_edge", AttrDomain::Corner, AttributeInitConstruct());
  MutableSpan<int2> new_edges(MEM_cnew_array<int2>(edge_offsets.total_size(), __func__),
                              edge_offsets.total_size());
  calc_edges::serialize_and_initialize_deduplicated_edges(edge_maps, edge_offsets, new_edges);
  calc_edges::update_edge_indices_in_face_loops(mesh.faces(),
                                                mesh.corner_verts(),
                                                edge_maps,
                                                parallel_mask,
                                                edge_offsets,
                                                mesh.corner_edges_for_write());

  Array<int2> original_edges;
  if (keep_existing_edges && select_new_edges) {
    original_edges.reinitialize(mesh.edges_num);
    array_utils::copy(mesh.edges(), original_edges.as_mutable_span());
  }

  /* Free old CustomData and assign new one. */
  CustomData_free(&mesh.edge_data, mesh.edges_num);
  CustomData_reset(&mesh.edge_data);
  mesh.edges_num = edge_offsets.total_size();
  attributes.add<int2>(".edge_verts", AttrDomain::Edge, AttributeInitMoveArray(new_edges.data()));

  if (select_new_edges) {
    MutableAttributeAccessor attributes = mesh.attributes_for_write();
    SpanAttributeWriter<bool> select_edge = attributes.lookup_or_add_for_write_span<bool>(
        ".select_edge", AttrDomain::Edge);
    if (select_edge) {
      select_edge.span.fill(true);
      if (!original_edges.is_empty()) {
        calc_edges::deselect_known_edges(
            edge_offsets, edge_maps, parallel_mask, original_edges, select_edge.span);
      }
      select_edge.finish();
    }
  }

  if (!keep_existing_edges) {
    /* All edges are rebuilt from the faces, so there are no loose edges. */
    mesh.tag_loose_edges_none();
  }

  /* Explicitly clear edge maps, because that way it can be parallelized. */
  calc_edges::clear_hash_tables(edge_maps);
}

}  // namespace blender::bke
