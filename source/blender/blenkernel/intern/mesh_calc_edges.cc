/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BLI_array_utils.hh"
#include "BLI_math_base.h"
#include "BLI_ordered_edge.hh"
#include "BLI_task.hh"
#include "BLI_threads.h"
#include "BLI_vector_set.hh"

#include "BKE_attribute.hh"
#include "BKE_attribute_filter.hh"
#include "BKE_attribute_legacy_convert.hh"
#include "BKE_attribute_math.hh"
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
                          16,
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

static OffsetIndices<int> edge_map_offsets(const Span<EdgeMap> maps, Array<int> &r_sizes)
{
  r_sizes.reinitialize(maps.size() + 1);
  for (const int map_i : maps.index_range()) {
    r_sizes[map_i] = maps[map_i].size();
  }
  return offset_indices::accumulate_counts_to_offsets(r_sizes);
}

static int edge_to_hash_map_i(const OrderedEdge edge, const uint32_t parallel_mask)
{
  return parallel_mask & edge_hash_2(edge);
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
      if (task_index == edge_to_hash_map_i(ordered_edge, parallel_mask)) {
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
        const OrderedEdge ordered_edge(vert_prev, vert);
        /* Only add the edge when it belongs into this map. */
        if (task_index == edge_to_hash_map_i(ordered_edge, parallel_mask)) {
          edge_map.add(ordered_edge);
        }
      }
    }
  });
}

static void serialize_and_initialize_deduplicated_edges(
    MutableSpan<EdgeMap> edge_maps,
    const OffsetIndices<int> edge_offsets,
    const OffsetIndices<int> prefix_skip_offsets,
    MutableSpan<int2> new_edges)
{
  threading::parallel_for_each(edge_maps, [&](EdgeMap &edge_map) {
    const int task_index = &edge_map - edge_maps.data();
    if (edge_offsets[task_index].is_empty()) {
      return;
    }

    if (prefix_skip_offsets[task_index].size() == edge_offsets[task_index].size()) {
      return;
    }

    const IndexRange all_map_edges = edge_offsets[task_index];
    const IndexRange prefix_to_skip = prefix_skip_offsets[task_index];
    const IndexRange map_edges = IndexRange::from_begin_size(
        all_map_edges.start() - prefix_to_skip.start(),
        all_map_edges.size() - prefix_to_skip.size());

    MutableSpan<int2> result_edges = new_edges.slice(map_edges);
    result_edges.copy_from(edge_map.as_span().drop_front(prefix_to_skip.size()).cast<int2>());
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
        const OrderedEdge ordered_edge(vert_prev, vert);
        const int task_index = edge_to_hash_map_i(ordered_edge, parallel_mask);
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
  threading::parallel_for_each(edge_maps, [](EdgeMap &edge_map) { edge_map.clear(); });
}

static IndexMask mask_first_distinct_edges(const Span<int2> edges,
                                           const IndexMask &edges_to_check,
                                           const Span<EdgeMap> edge_maps,
                                           const uint32_t parallel_mask,
                                           const OffsetIndices<int> edge_offsets,
                                           IndexMaskMemory &memory)
{
  if (edges_to_check.is_empty()) {
    return {};
  }

  constexpr int no_original_edge = std::numeric_limits<int>::max();
  Array<int> map_edge_to_first_original(edge_offsets.total_size());
  map_edge_to_first_original.as_mutable_span().fill(no_original_edge);

  /* TODO: Lock-free parallel version? BLI' "atomic::min<T>(T&, T);" ? */
  edges_to_check.foreach_index_optimized<int>([&](const int edge_i) {
    const OrderedEdge edge = edges[edge_i];
    const int map_i = calc_edges::edge_to_hash_map_i(edge, parallel_mask);
    const int edge_index = edge_maps[map_i].index_of(edge);

    int &original_edge = map_edge_to_first_original[edge_offsets[map_i][edge_index]];
    original_edge = math::min(original_edge, edge_i);
  });

  /* Note: #map_edge_to_first_original might still contains #no_original_edge if edges was both non
   * distinct and not full set. */

  return IndexMask::from_predicate(
      edges_to_check, GrainSize(2048), memory, [&](const int srd_edge_i) {
        const OrderedEdge edge = edges[srd_edge_i];
        const int map_i = calc_edges::edge_to_hash_map_i(edge, parallel_mask);
        const int edge_index = edge_maps[map_i].index_of(edge);
        return map_edge_to_first_original[edge_offsets[map_i][edge_index]] == srd_edge_i;
      });
}

static void map_edge_to_span_index(const Span<int2> edges,
                                   const Span<EdgeMap> edge_maps,
                                   const uint32_t parallel_mask,
                                   const OffsetIndices<int> edge_offsets,
                                   MutableSpan<int> indices)
{
  threading::parallel_for_each(edge_maps.index_range(), [&](const int map_i) {
    int edge_map_iter = 0;
    for (const int edge_i : edges.index_range()) {
      const int edge_map = calc_edges::edge_to_hash_map_i(edges[edge_i], parallel_mask);
      if (map_i != edge_map) {
        continue;
      }
      indices[edge_offsets[edge_map][edge_map_iter]] = edge_i;
      edge_map_iter++;
    }
  });
}

}  // namespace calc_edges

void mesh_calc_edges(Mesh &mesh,
                     bool keep_existing_edges,
                     const bool select_new_edges,
                     const AttributeFilter &attribute_filter)
{

  if (mesh.edges_num == 0 && mesh.corners_num == 0) {
    /* BLI_assert(BKE_mesh_is_valid(&mesh)); */
    return;
  }

  if (mesh.corners_num == 0 && !keep_existing_edges) {
    CustomData_free(&mesh.edge_data);
    mesh.edges_num = 0;
    mesh.tag_loose_edges_none();
    /* BLI_assert(BKE_mesh_is_valid(&mesh)); */
    return;
  }

  BLI_assert(std::all_of(mesh.edges().begin(), mesh.edges().end(), [&](const int2 edge) {
    return edge.x != edge.y;
  }));

  /* Parallelization is achieved by having multiple hash tables for different subsets of edges.
   * Each edge is assigned to one of the hash maps based on the lower bits of a hash value. */
  const int parallel_maps = calc_edges::get_parallel_maps_count(mesh);
  BLI_assert(is_power_of_2_i(parallel_maps));
  const uint32_t parallel_mask = uint32_t(parallel_maps) - 1;
  Array<calc_edges::EdgeMap> edge_maps(parallel_maps);
  calc_edges::reserve_hash_maps(mesh, keep_existing_edges, edge_maps);

  Array<int> original_edge_maps_prefix_size(edge_maps.size() + 1, 0);
  if (keep_existing_edges) {
    calc_edges::add_existing_edges_to_hash_maps(mesh, parallel_mask, edge_maps);
    calc_edges::edge_map_offsets(edge_maps, original_edge_maps_prefix_size);
  }
  const OffsetIndices<int> original_edge_maps_prefix(original_edge_maps_prefix_size.as_span());
  const int original_unique_edge_num = original_edge_maps_prefix.total_size();
  const bool original_edges_are_distinct = original_unique_edge_num == mesh.edges_num;

  if (mesh.corners_num == 0 && keep_existing_edges && original_edges_are_distinct) {
    /* BLI_assert(BKE_mesh_is_valid(&mesh)); */
    return;
  }

  calc_edges::add_face_edges_to_hash_maps(mesh, parallel_mask, edge_maps);
  Array<int> edge_sizes;
  const OffsetIndices<int> edge_offsets = calc_edges::edge_map_offsets(edge_maps, edge_sizes);
  const bool no_new_edges = edge_offsets.total_size() == original_unique_edge_num;

  MutableAttributeAccessor dst_attributes = mesh.attributes_for_write();
  dst_attributes.add<int>(".corner_edge", AttrDomain::Corner, AttributeInitConstruct());
  MutableSpan<int> corner_edges = mesh.corner_edges_for_write();
#ifndef NDEBUG
  corner_edges.fill(-1);
#endif

  const int result_edges_num = edge_offsets.total_size();

  const OffsetIndices<int> faces = mesh.faces();
  const Span<int2> original_edges = mesh.edges();
  const Span<int> corner_verts = mesh.corner_verts();
  if (keep_existing_edges && original_edges_are_distinct && no_new_edges) {
    /* We need a way to say from caller side if we should generate corner edge attribute even in
     * that case. TODO: make this optional. */
    calc_edges::update_edge_indices_in_face_loops(
        faces, corner_verts, edge_maps, parallel_mask, edge_offsets, corner_edges);

    Array<int> edge_map_to_result_index(result_edges_num);
#ifndef NDEBUG
    edge_map_to_result_index.as_mutable_span().fill(-1);
#endif
    calc_edges::map_edge_to_span_index(
        original_edges, edge_maps, parallel_mask, edge_offsets, edge_map_to_result_index);
    array_utils::gather(edge_map_to_result_index.as_span(), corner_edges.as_span(), corner_edges);

    BLI_assert(!corner_edges.contains(-1));
    BLI_assert(BKE_mesh_is_valid(&mesh));
    return;
  }

  IndexMaskMemory memory;
  IndexRange back_range_of_new_edges;
  IndexMask src_to_dst_mask;

  MutableSpan<int2> edge_verts(MEM_malloc_arrayN<int2>(result_edges_num, AT), result_edges_num);
#ifndef NDEBUG
  edge_verts.fill(int2(-1));
#endif

  if (keep_existing_edges) {
    back_range_of_new_edges = IndexRange(result_edges_num).drop_front(original_unique_edge_num);

    if (original_edges_are_distinct) {
      src_to_dst_mask = IndexRange(original_unique_edge_num);
    }
    else {
      src_to_dst_mask = calc_edges::mask_first_distinct_edges(original_edges,
                                                              original_edges.index_range(),
                                                              edge_maps,
                                                              parallel_mask,
                                                              edge_offsets,
                                                              memory);
    }
    BLI_assert(src_to_dst_mask.size() == original_unique_edge_num);

    array_utils::gather(
        original_edges, src_to_dst_mask, edge_verts.take_front(original_unique_edge_num));

    /* In order to reduce permutations of edge attributes we must provide result edge indices near
     * to original. */
    Array<int> edge_map_to_result_index(result_edges_num);
#ifndef NDEBUG
    edge_map_to_result_index.as_mutable_span().fill(-1);
#endif

    if (original_edges_are_distinct) {
      /* TODO: Do we can group edges by .low vertex? Or by hash, but with Span<int> of edges by
       * group?... */
      calc_edges::map_edge_to_span_index(original_edges.take_front(mesh.edges_num),
                                         edge_maps,
                                         parallel_mask,
                                         edge_offsets,
                                         edge_map_to_result_index);
    }
    else {
      src_to_dst_mask.foreach_index(
          GrainSize(1024), [&](const int src_index, const int dst_index) {
            const OrderedEdge edge = original_edges[src_index];
            const int map_i = calc_edges::edge_to_hash_map_i(edge, parallel_mask);
            const int edge_index = edge_maps[map_i].index_of(edge);
            edge_map_to_result_index[edge_offsets[map_i][edge_index]] = dst_index;
          });
    }

    if (!no_new_edges) {
      BLI_assert(edge_offsets.data().size() == original_edge_maps_prefix.data().size());

      /* TODO: Check if all new edges are range. */
      const int new_edges_start = original_unique_edge_num;
      for (const int map_i : edge_maps.index_range()) {
        const IndexRange map_edges = edge_offsets[map_i];
        const IndexRange prefix_edges = original_edge_maps_prefix[map_i];
        const IndexRange new_edges_in_map = map_edges.drop_front(prefix_edges.size());

        const int new_edges_start_pos = map_edges.start() - prefix_edges.start();
        const int map_new_edges_start = new_edges_start + new_edges_start_pos;
        array_utils::fill_index_range(
            edge_map_to_result_index.as_mutable_span().slice(new_edges_in_map),
            map_new_edges_start);
      }
    }

    BLI_assert(!edge_map_to_result_index.as_span().contains(-1));
    calc_edges::update_edge_indices_in_face_loops(
        faces, corner_verts, edge_maps, parallel_mask, edge_offsets, corner_edges);
    array_utils::gather(edge_map_to_result_index.as_span(), corner_edges.as_span(), corner_edges);

    calc_edges::serialize_and_initialize_deduplicated_edges(
        edge_maps,
        edge_offsets,
        original_edge_maps_prefix,
        edge_verts.drop_front(original_unique_edge_num));
  }
  else {
    if (mesh.edges_num != 0) {
      const IndexMask original_corner_edges = IndexMask::from_predicate(
          IndexRange(mesh.edges_num), GrainSize(2048), memory, [&](const int edge_i) {
            const OrderedEdge edge = original_edges[edge_i];
            const int map_i = calc_edges::edge_to_hash_map_i(edge, parallel_mask);
            return edge_maps[map_i].contains(edge);
          });
      src_to_dst_mask = calc_edges::mask_first_distinct_edges(
          original_edges, original_corner_edges, edge_maps, parallel_mask, edge_offsets, memory);

      const int old_corner_edges_num = src_to_dst_mask.size();
      back_range_of_new_edges = IndexRange(result_edges_num).drop_front(old_corner_edges_num);

      Array<int> edge_map_to_result_index;
      if (!src_to_dst_mask.is_empty()) {
        /* TODO: Check if mask is range. */
        edge_map_to_result_index.reinitialize(result_edges_num);
        edge_map_to_result_index.as_mutable_span().fill(1);
        src_to_dst_mask.foreach_index([&](const int original_edge_i) {
          const OrderedEdge edge = original_edges[original_edge_i];
          const int edge_map = calc_edges::edge_to_hash_map_i(edge, parallel_mask);
          const int edge_index = edge_maps[edge_map].index_of(edge);
          edge_map_to_result_index[edge_offsets[edge_map][edge_index]] = 0;
        });

        offset_indices::accumulate_counts_to_offsets(edge_map_to_result_index.as_mutable_span(),
                                                     old_corner_edges_num);

        src_to_dst_mask.foreach_index([&](const int original_edge_i, const int dst_edge_i) {
          const OrderedEdge edge = original_edges[original_edge_i];
          const int edge_map = calc_edges::edge_to_hash_map_i(edge, parallel_mask);
          const int edge_index = edge_maps[edge_map].index_of(edge);
          edge_map_to_result_index[edge_offsets[edge_map][edge_index]] = dst_edge_i;
        });

        array_utils::gather(
            original_edges, src_to_dst_mask, edge_verts.take_front(old_corner_edges_num));

        threading::parallel_for_each(edge_maps, [&](calc_edges::EdgeMap &edge_map) {
          const int task_index = &edge_map - edge_maps.data();
          if (edge_offsets[task_index].is_empty()) {
            return;
          }

          array_utils::scatter<int2>(
              edge_map.as_span().cast<int2>(),
              edge_map_to_result_index.as_span().slice(edge_offsets[task_index]),
              edge_verts);
        });

        calc_edges::update_edge_indices_in_face_loops(
            faces, corner_verts, edge_maps, parallel_mask, edge_offsets, corner_edges);

        array_utils::gather(
            edge_map_to_result_index.as_span(), corner_edges.as_span(), corner_edges);
      }
      else {
        calc_edges::update_edge_indices_in_face_loops(
            faces, corner_verts, edge_maps, parallel_mask, edge_offsets, corner_edges);
        calc_edges::serialize_and_initialize_deduplicated_edges(
            edge_maps, edge_offsets, original_edge_maps_prefix, edge_verts);
      }
    }
    else {
      back_range_of_new_edges = IndexRange(result_edges_num);
      BLI_assert(original_edge_maps_prefix.total_size() == 0);
      calc_edges::update_edge_indices_in_face_loops(
          faces, corner_verts, edge_maps, parallel_mask, edge_offsets, corner_edges);
      calc_edges::serialize_and_initialize_deduplicated_edges(
          edge_maps, edge_offsets, original_edge_maps_prefix, edge_verts);
    }
  }

  BLI_assert(std::all_of(
      edge_verts.begin(), edge_verts.end(), [&](const int2 edge) { return edge.x != edge.y; }));

  BLI_assert(!corner_edges.contains(-1));
  BLI_assert(!edge_verts.contains(int2(-1)));

  BLI_assert(src_to_dst_mask.size() + back_range_of_new_edges.size() == result_edges_num);
  BLI_assert(back_range_of_new_edges.one_after_last() == result_edges_num);

  Vector<std::string> attributes_to_drop;
  /* TODO: Need ::all_pass() on #attribute_filter to know if this loop can be skipped. */
  mesh.attributes().foreach_attribute([&](const AttributeIter &attribute) {
    if (attribute.data_type == AttrType::String) {
      return;
    }
    if (attribute.domain != AttrDomain::Edge) {
      return;
    }
    if (!attribute_filter.allow_skip(attribute.name)) {
      return;
    }
    attributes_to_drop.append(attribute.name);
  });

  for (const StringRef attribute : attributes_to_drop) {
    dst_attributes.remove(attribute);
  }

  CustomData_free_layer_named(&mesh.edge_data, ".edge_verts");
  for (CustomDataLayer &layer : MutableSpan(mesh.edge_data.layers, mesh.edge_data.totlayer)) {
    const void *src_data = layer.data;
    const size_t elem_size = CustomData_sizeof(eCustomDataType(layer.type));

    void *dst_data = MEM_malloc_arrayN(result_edges_num, elem_size, AT);
    if (src_data != nullptr) {
      if (layer.type == CD_ORIGINDEX) {
        const Span src(static_cast<const int *>(src_data), mesh.edges_num);
        MutableSpan dst(static_cast<int *>(dst_data), result_edges_num);
        array_utils::gather(src, src_to_dst_mask, dst.take_front(src_to_dst_mask.size()));
        dst.slice(back_range_of_new_edges).fill(-1);
      }
      else {
        const CPPType *type = custom_data_type_to_cpp_type(eCustomDataType(layer.type));
        BLI_assert(type != nullptr);
        const GSpan src(type, src_data, mesh.edges_num);
        GMutableSpan dst(type, dst_data, result_edges_num);
        array_utils::gather(src, src_to_dst_mask, dst.take_front(src_to_dst_mask.size()));
        type->fill_assign_n(type->default_value(),
                            dst.slice(back_range_of_new_edges).data(),
                            dst.slice(back_range_of_new_edges).size());
      }
      layer.sharing_info->remove_user_and_delete_if_last();
    }

    layer.data = dst_data;
    layer.sharing_info = implicit_sharing::info_for_mem_free(dst_data);
  }

  mesh.edges_num = result_edges_num;

  dst_attributes.add<int2>(
      ".edge_verts", AttrDomain::Edge, AttributeInitMoveArray(edge_verts.data()));

  if (select_new_edges) {
    dst_attributes.remove(".select_edge");
    if (ELEM(back_range_of_new_edges.size(), 0, mesh.edges_num)) {
      const bool fill_value = back_range_of_new_edges.size() == mesh.edges_num;
      dst_attributes.add<int2>(
          ".select_edge",
          AttrDomain::Edge,
          AttributeInitVArray(VArray<bool>::from_single(fill_value, mesh.edges_num)));
    }
    else {
      SpanAttributeWriter<bool> select_edge = dst_attributes.lookup_or_add_for_write_span<bool>(
          ".select_edge", AttrDomain::Edge);
      select_edge.span.drop_back(back_range_of_new_edges.size()).fill(false);
      select_edge.span.take_back(back_range_of_new_edges.size()).fill(true);
      select_edge.finish();
    }
  }

  if (!keep_existing_edges) {
    /* All edges are rebuilt from the faces, so there are no loose edges. */
    mesh.tag_loose_edges_none();
  }

  /* Explicitly clear edge maps, because that way it can be parallelized. */
  calc_edges::clear_hash_tables(edge_maps);

  /* BLI_assert(BKE_mesh_is_valid(&mesh)); */
}

void mesh_calc_edges(Mesh &mesh, bool keep_existing_edges, const bool select_new_edges)
{
  mesh_calc_edges(mesh, keep_existing_edges, select_new_edges, AttributeFilter::default_filter());
}

}  // namespace blender::bke
