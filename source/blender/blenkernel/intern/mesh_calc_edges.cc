/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_map.hh"
#include "BLI_task.hh"
#include "BLI_threads.h"
#include "BLI_timeit.hh"

#include "BKE_attribute.hh"
#include "BKE_customdata.h"
#include "BKE_mesh.h"

namespace blender::bke::calc_edges {

/** This is used to uniquely identify edges in a hash map. */
struct OrderedEdge {
  int v_low, v_high;

  OrderedEdge(const int v1, const int v2)
  {
    if (v1 < v2) {
      v_low = v1;
      v_high = v2;
    }
    else {
      v_low = v2;
      v_high = v1;
    }
  }

  OrderedEdge(const uint v1, const uint v2) : OrderedEdge(int(v1), int(v2))
  {
  }

  uint64_t hash() const
  {
    return (this->v_low << 8) ^ this->v_high;
  }

  /** Return a hash value that is likely to be different in the low bits from the normal `hash()`
   * function. This is necessary to avoid collisions in #BKE_mesh_calc_edges. */
  uint64_t hash2() const
  {
    return this->v_low;
  }

  friend bool operator==(const OrderedEdge &e1, const OrderedEdge &e2)
  {
    BLI_assert(e1.v_low < e1.v_high);
    BLI_assert(e2.v_low < e2.v_high);
    return e1.v_low == e2.v_low && e1.v_high == e2.v_high;
  }
};

/* The map first contains an edge pointer and later an index. */
union OrigEdgeOrIndex {
  const MEdge *original_edge;
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
  const Span<MEdge> edges = mesh->edges();
  threading::parallel_for_each(edge_maps, [&](EdgeMap &edge_map) {
    const int task_index = &edge_map - edge_maps.data();
    for (const MEdge &edge : edges) {
      OrderedEdge ordered_edge{edge.v1, edge.v2};
      /* Only add the edge when it belongs into this map. */
      if (task_index == (parallel_mask & ordered_edge.hash2())) {
        edge_map.add_new(ordered_edge, {&edge});
      }
    }
  });
}

static void add_polygon_edges_to_hash_maps(Mesh *mesh,
                                           MutableSpan<EdgeMap> edge_maps,
                                           uint32_t parallel_mask)
{
  const Span<MPoly> polys = mesh->polys();
  const Span<MLoop> loops = mesh->loops();
  threading::parallel_for_each(edge_maps, [&](EdgeMap &edge_map) {
    const int task_index = &edge_map - edge_maps.data();
    for (const MPoly &poly : polys) {
      Span<MLoop> poly_loops = loops.slice(poly.loopstart, poly.totloop);
      const MLoop *prev_loop = &poly_loops.last();
      for (const MLoop &next_loop : poly_loops) {
        /* Can only be the same when the mesh data is invalid. */
        if (prev_loop->v != next_loop.v) {
          OrderedEdge ordered_edge{prev_loop->v, next_loop.v};
          /* Only add the edge when it belongs into this map. */
          if (task_index == (parallel_mask & ordered_edge.hash2())) {
            edge_map.lookup_or_add(ordered_edge, {nullptr});
          }
        }
        prev_loop = &next_loop;
      }
    }
  });
}

static void serialize_and_initialize_deduplicated_edges(MutableSpan<EdgeMap> edge_maps,
                                                        MutableSpan<MEdge> new_edges)
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
      MEdge &new_edge = new_edges[new_edge_index];
      const MEdge *orig_edge = item.value.original_edge;
      if (orig_edge != nullptr) {
        /* Copy values from original edge. */
        new_edge = *orig_edge;
      }
      else {
        /* Initialize new edge. */
        new_edge.v1 = item.key.v_low;
        new_edge.v2 = item.key.v_high;
      }
      item.value.index = new_edge_index;
      new_edge_index++;
    }
  });
}

static void update_edge_indices_in_poly_loops(Mesh *mesh,
                                              Span<EdgeMap> edge_maps,
                                              uint32_t parallel_mask)
{
  const Span<MPoly> polys = mesh->polys();
  MutableSpan<MLoop> loops = mesh->loops_for_write();
  threading::parallel_for(IndexRange(mesh->totpoly), 100, [&](IndexRange range) {
    for (const int poly_index : range) {
      const MPoly &poly = polys[poly_index];
      MutableSpan<MLoop> poly_loops = loops.slice(poly.loopstart, poly.totloop);

      MLoop *prev_loop = &poly_loops.last();
      for (MLoop &next_loop : poly_loops) {
        int edge_index;
        if (prev_loop->v != next_loop.v) {
          OrderedEdge ordered_edge{prev_loop->v, next_loop.v};
          /* Double lookup: First find the map that contains the edge, then lookup the edge. */
          const EdgeMap &edge_map = edge_maps[parallel_mask & ordered_edge.hash2()];
          edge_index = edge_map.lookup(ordered_edge).index;
        }
        else {
          /* This is an invalid edge; normally this does not happen in Blender,
           * but it can be part of an imported mesh with invalid geometry. See
           * #76514. */
          edge_index = 0;
        }
        prev_loop->e = edge_index;
        prev_loop = &next_loop;
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
  MutableSpan<MEdge> new_edges{
      static_cast<MEdge *>(MEM_calloc_arrayN(new_totedge, sizeof(MEdge), __func__)), new_totedge};
  calc_edges::serialize_and_initialize_deduplicated_edges(edge_maps, new_edges);
  calc_edges::update_edge_indices_in_poly_loops(mesh, edge_maps, parallel_mask);

  /* Free old CustomData and assign new one. */
  CustomData_free(&mesh->edata, mesh->totedge);
  CustomData_reset(&mesh->edata);
  CustomData_add_layer(&mesh->edata, CD_MEDGE, CD_ASSIGN, new_edges.data(), new_totedge);
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
    mesh->loose_edges_tag_none();
  }

  /* Explicitly clear edge maps, because that way it can be parallelized. */
  clear_hash_tables(edge_maps);
}
