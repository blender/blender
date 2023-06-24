/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_map.hh"
#include "BLI_ordered_edge.hh"
#include "BLI_task.hh"
#include "BLI_threads.h"
#include "BLI_timeit.hh"

#include "BKE_attribute.hh"
#include "BKE_customdata.h"
#include "BKE_mesh.hh"

namespace blender::bke::calc_edges {

/**
 * Return a hash value that is likely to be different in the low bits from the normal `hash()`
 * function. This is necessary to avoid collisions in #BKE_mesh_calc_edges.
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

static void reserve_hash_maps(const Mesh *mesh,
                              const bool keep_existing_edges,
                              MutableSpan<EdgeMap> edge_maps)
{
  const int totedge_guess = std::max(keep_existing_edges ? mesh->totedge : 0, mesh->totpoly * 2);
  threading::parallel_for_each(
      edge_maps, [&](EdgeMap &edge_map) { edge_map.reserve(totedge_guess / edge_maps.size()); });
}

static void add_existing_edges_to_hash_maps(Mesh *mesh,
                                            MutableSpan<EdgeMap> edge_maps,
                                            uint32_t parallel_mask)
{
  /* Assume existing edges are valid. */
  const Span<int2> edges = mesh->edges();
  threading::parallel_for_each(edge_maps, [&](EdgeMap &edge_map) {
    const int task_index = &edge_map - edge_maps.data();
    for (const int2 &edge : edges) {
      OrderedEdge ordered_edge{edge[0], edge[1]};
      /* Only add the edge when it belongs into this map. */
      if (task_index == (parallel_mask & edge_hash_2(ordered_edge))) {
        edge_map.add_new(ordered_edge, {&edge});
      }
    }
  });
}

static void add_polygon_edges_to_hash_maps(Mesh *mesh,
                                           MutableSpan<EdgeMap> edge_maps,
                                           uint32_t parallel_mask)
{
  const OffsetIndices polys = mesh->polys();
  const Span<int> corner_verts = mesh->corner_verts();
  threading::parallel_for_each(edge_maps, [&](EdgeMap &edge_map) {
    const int task_index = &edge_map - edge_maps.data();
    for (const int i : polys.index_range()) {
      const Span<int> poly_verts = corner_verts.slice(polys[i]);
      int vert_prev = poly_verts.last();
      for (const int vert : poly_verts) {
        /* Can only be the same when the mesh data is invalid. */
        if (vert_prev != vert) {
          OrderedEdge ordered_edge{vert_prev, vert};
          /* Only add the edge when it belongs into this map. */
          if (task_index == (parallel_mask & edge_hash_2(ordered_edge))) {
            edge_map.lookup_or_add(ordered_edge, {nullptr});
          }
        }
        vert_prev = vert;
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
  Array<int> edge_index_offsets(edge_maps.size());
  edge_index_offsets[0] = 0;
  for (const int i : IndexRange(edge_maps.size() - 1)) {
    edge_index_offsets[i + 1] = edge_index_offsets[i] + edge_maps[i].size();
  }

  threading::parallel_for_each(edge_maps, [&](EdgeMap &edge_map) {
    const int task_index = &edge_map - edge_maps.data();

    int new_edge_index = edge_index_offsets[task_index];
    for (EdgeMap::MutableItem item : edge_map.items()) {
      int2 &new_edge = new_edges[new_edge_index];
      const int2 *orig_edge = item.value.original_edge;
      if (orig_edge != nullptr) {
        /* Copy values from original edge. */
        new_edge = *orig_edge;
      }
      else {
        /* Initialize new edge. */
        new_edge[0] = item.key.v_low;
        new_edge[1] = item.key.v_high;
      }
      item.value.index = new_edge_index;
      new_edge_index++;
    }
  });
}

static void update_edge_indices_in_poly_loops(const OffsetIndices<int> polys,
                                              const Span<int> corner_verts,
                                              const Span<EdgeMap> edge_maps,
                                              const uint32_t parallel_mask,
                                              MutableSpan<int> corner_edges)
{
  threading::parallel_for(polys.index_range(), 100, [&](IndexRange range) {
    for (const int poly_index : range) {
      const IndexRange poly = polys[poly_index];
      int prev_corner = poly.last();
      for (const int next_corner : poly) {
        const int vert = corner_verts[next_corner];
        const int vert_prev = corner_verts[prev_corner];

        int edge_index;
        if (vert_prev != vert) {
          OrderedEdge ordered_edge{vert_prev, vert};
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
        corner_edges[prev_corner] = edge_index;
        prev_corner = next_corner;
      }
    }
  });
}

static int get_parallel_maps_count(const Mesh *mesh)
{
  /* Don't use parallelization when the mesh is small. */
  if (mesh->totpoly < 1000) {
    return 1;
  }
  /* Use at most 8 separate hash tables. Using more threads has diminishing returns. These threads
   * can better do something more useful instead. */
  const int system_thread_count = BLI_system_thread_count();
  return power_of_2_min_i(std::min(8, system_thread_count));
}

static void clear_hash_tables(MutableSpan<EdgeMap> edge_maps)
{
  threading::parallel_for_each(edge_maps, [](EdgeMap &edge_map) { edge_map.clear(); });
}

}  // namespace blender::bke::calc_edges

void BKE_mesh_calc_edges(Mesh *mesh, bool keep_existing_edges, const bool select_new_edges)
{
  using namespace blender;
  using namespace blender::bke;
  using namespace blender::bke::calc_edges;

  /* Parallelization is achieved by having multiple hash tables for different subsets of edges.
   * Each edge is assigned to one of the hash maps based on the lower bits of a hash value. */
  const int parallel_maps = get_parallel_maps_count(mesh);
  BLI_assert(is_power_of_2_i(parallel_maps));
  const uint32_t parallel_mask = uint32_t(parallel_maps) - 1;
  Array<EdgeMap> edge_maps(parallel_maps);
  reserve_hash_maps(mesh, keep_existing_edges, edge_maps);

  /* Add all edges. */
  if (keep_existing_edges) {
    calc_edges::add_existing_edges_to_hash_maps(mesh, edge_maps, parallel_mask);
  }
  calc_edges::add_polygon_edges_to_hash_maps(mesh, edge_maps, parallel_mask);

  /* Compute total number of edges. */
  int new_totedge = 0;
  for (EdgeMap &edge_map : edge_maps) {
    new_totedge += edge_map.size();
  }

  /* Create new edges. */
  if (!CustomData_has_layer_named(&mesh->ldata, CD_PROP_INT32, ".corner_edge")) {
    CustomData_add_layer_named(
        &mesh->ldata, CD_PROP_INT32, CD_CONSTRUCT, mesh->totloop, ".corner_edge");
  }
  MutableSpan<int2> new_edges{
      static_cast<int2 *>(MEM_calloc_arrayN(new_totedge, sizeof(int2), __func__)), new_totedge};
  calc_edges::serialize_and_initialize_deduplicated_edges(edge_maps, new_edges);
  calc_edges::update_edge_indices_in_poly_loops(mesh->polys(),
                                                mesh->corner_verts(),
                                                edge_maps,
                                                parallel_mask,
                                                mesh->corner_edges_for_write());

  /* Free old CustomData and assign new one. */
  CustomData_free(&mesh->edata, mesh->totedge);
  CustomData_reset(&mesh->edata);
  CustomData_add_layer_named_with_data(
      &mesh->edata, CD_PROP_INT32_2D, new_edges.data(), new_totedge, ".edge_verts", nullptr);
  mesh->totedge = new_totedge;

  if (select_new_edges) {
    MutableAttributeAccessor attributes = mesh->attributes_for_write();
    SpanAttributeWriter<bool> select_edge = attributes.lookup_or_add_for_write_span<bool>(
        ".select_edge", ATTR_DOMAIN_EDGE);
    if (select_edge) {
      int new_edge_index = 0;
      for (const EdgeMap &edge_map : edge_maps) {
        for (EdgeMap::Item item : edge_map.items()) {
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
    mesh->tag_loose_edges_none();
  }

  /* Explicitly clear edge maps, because that way it can be parallelized. */
  clear_hash_tables(edge_maps);
}
