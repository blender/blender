/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "atomic_ops.h"

#include "BLI_array_utils.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_index_mask.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_ordered_edge.hh"
#include "BLI_polyfill_2d.h"
#include "BLI_polyfill_2d_beautify.h"
#include "BLI_vector_set.hh"

#include "BLI_heap.h"
#include "BLI_index_ranges_builder.hh"
#include "BLI_memarena.h"

#include "BKE_attribute.hh"
#include "BKE_attribute_math.hh"
#include "BKE_customdata.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"

#include "GEO_mesh_triangulate.hh"

namespace blender::geometry {

static void gather(const Span<int> src, const Span<int16_t> indices, MutableSpan<int> dst)
{
  for (const int i : indices.index_range()) {
    dst[i] = src[indices[i]];
  }
}

static Span<int> gather_or_reference(const Span<int> src,
                                     const Span<int16_t> indices,
                                     Vector<int> &dst)
{
  if (unique_sorted_indices::non_empty_is_range(indices)) {
    return src.slice(indices[0], indices.size());
  }
  dst.reinitialize(indices.size());
  gather(src, indices, dst);
  return dst.as_span();
}

static Span<int> gather_or_reference(const Span<int> src,
                                     const IndexMaskSegment mask,
                                     Vector<int> &dst)
{
  return gather_or_reference(src.drop_front(mask.offset()), mask.base_span(), dst);
}

/**
 * If a significant number of Ngons are selected (> 25% of the faces), then use the
 * face normals cache, in case the cache is persistent (or already calculated).
 */
static Span<float3> face_normals_if_worthwhile(const Mesh &src_mesh, const int selection_size)
{
  if (src_mesh.runtime->face_normals_cache.is_cached()) {
    return src_mesh.face_normals();
  }
  if (selection_size > src_mesh.faces_num / 4) {
    return src_mesh.face_normals();
  }
  return {};
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

static OffsetIndices<int> calc_face_offsets(const OffsetIndices<int> src_faces,
                                            const IndexMask &unselected,
                                            MutableSpan<int> offsets)
{
  MutableSpan<int> new_tri_offsets = offsets.drop_back(unselected.size());
  offset_indices::fill_constant_group_size(3, new_tri_offsets.first(), new_tri_offsets);
  offset_indices::gather_selected_offsets(
      src_faces, unselected, new_tri_offsets.last(), offsets.take_back(unselected.size() + 1));
  return OffsetIndices<int>(offsets);
}

namespace quad {

/**
 *  #Edge_0_2       #Edge_1_3
 * 3 ------- 2     3 ------- 2
 * | 1     / |     | \     1 |
 * |     /   |     |   \     |
 * |   /     |     |     \   |
 * | /     0 |     | 0     \ |
 * 0 ------- 1     0 ------- 1
 */
enum class QuadDirection : int8_t {
  Edge_0_2 = 0,
  Edge_1_3 = 1,
};

/**
 * \note This behavior is meant to be the same as #BM_verts_calc_rotate_beauty.
 * The order of vertices requires special attention.
 */
static QuadDirection calc_quad_direction_beauty(const float3 &v0,
                                                const float3 &v1,
                                                const float3 &v2,
                                                const float3 &v3)
{
  const int flip_flag = is_quad_flip_v3(v1, v2, v3, v0);
  if (UNLIKELY(flip_flag & (1 << 0))) {
    return QuadDirection::Edge_0_2;
  }
  if (UNLIKELY(flip_flag & (1 << 1))) {
    return QuadDirection::Edge_1_3;
  }
  return BLI_polyfill_edge_calc_rotate_beauty__area(v1, v2, v3, v0, false) > 0.0f ?
             QuadDirection::Edge_0_2 :
             QuadDirection::Edge_1_3;
}

static void calc_quad_directions(const Span<float3> positions,
                                 const Span<int> face_offsets,
                                 const Span<int> corner_verts,
                                 const TriangulateQuadMode quad_mode,
                                 MutableSpan<QuadDirection> directions)
{
  switch (quad_mode) {
    case TriangulateQuadMode::Fixed: {
      directions.fill(QuadDirection::Edge_0_2);
      break;
    }
    case TriangulateQuadMode::Alternate: {
      directions.fill(QuadDirection::Edge_1_3);
      break;
    }
    case TriangulateQuadMode::ShortEdge: {
      for (const int i : face_offsets.index_range()) {
        const Span<int> verts = corner_verts.slice(face_offsets[i], 4);
        const float dist_0_2 = math::distance_squared(positions[verts[0]], positions[verts[2]]);
        const float dist_1_3 = math::distance_squared(positions[verts[1]], positions[verts[3]]);
        directions[i] = dist_0_2 < dist_1_3 ? QuadDirection::Edge_0_2 : QuadDirection::Edge_1_3;
      }
      break;
    }
    case TriangulateQuadMode::LongEdge: {
      for (const int i : face_offsets.index_range()) {
        const Span<int> verts = corner_verts.slice(face_offsets[i], 4);
        const float dist_0_2 = math::distance_squared(positions[verts[0]], positions[verts[2]]);
        const float dist_1_3 = math::distance_squared(positions[verts[1]], positions[verts[3]]);
        directions[i] = dist_0_2 > dist_1_3 ? QuadDirection::Edge_0_2 : QuadDirection::Edge_1_3;
      }
      break;
    }
    case TriangulateQuadMode::Beauty: {
      for (const int i : face_offsets.index_range()) {
        const Span<int> verts = corner_verts.slice(face_offsets[i], 4);
        directions[i] = calc_quad_direction_beauty(
            positions[verts[0]], positions[verts[1]], positions[verts[2]], positions[verts[3]]);
      }

      break;
    }
  }
}

static void calc_corner_tris(const Span<int> face_offsets,
                             const Span<QuadDirection> directions,
                             MutableSpan<int3> corner_tris)
{
  for (const int i : face_offsets.index_range()) {
    MutableSpan<int> quad_map = corner_tris.slice(2 * i, 2).cast<int>();
    /* These corner orders give new edges based on the first vertex of each triangle. */
    switch (directions[i]) {
      case QuadDirection::Edge_0_2:
        quad_map.copy_from({2, 0, 1, 0, 2, 3});
        break;
      case QuadDirection::Edge_1_3:
        quad_map.copy_from({1, 3, 0, 3, 1, 2});
        break;
    }
    const int src_face_start = face_offsets[i];
    for (int &i : quad_map) {
      i += src_face_start;
    }
  }
}

static void calc_corner_tris(const Span<float3> positions,
                             const OffsetIndices<int> src_faces,
                             const Span<int> src_corner_verts,
                             const IndexMask &quads,
                             const TriangulateQuadMode quad_mode,
                             MutableSpan<int3> corner_tris)
{
  struct TLS {
    Vector<int> offsets;
    Vector<QuadDirection> directions;
  };
  threading::EnumerableThreadSpecific<TLS> tls;

  quads.foreach_segment(GrainSize(1024), [&](const IndexMaskSegment quads, const int64_t pos) {
    TLS &data = tls.local();
    data.directions.reinitialize(quads.size());

    /* Find the offsets of each face in the local selection. We can gather them together even if
     * they aren't contiguous because we only need to know the start of each face; the size is
     * just 4. */
    const Span<int> offsets = gather_or_reference(src_faces.data(), quads, data.offsets);
    calc_quad_directions(positions, offsets, src_corner_verts, quad_mode, data.directions);
    const IndexRange tris_range(pos * 2, offsets.size() * 2);
    quad::calc_corner_tris(offsets, data.directions, corner_tris.slice(tris_range));
  });
}

/**
 * Each triangulated quad creates one additional edge in the result mesh, between the two
 * triangles. The corner_verts are just the corners of the quads, and the edges are just the new
 * edges for these quads.
 */
static void calc_edges(const Span<int> quad_corner_verts, MutableSpan<int2> new_quad_edges)
{
  const int quads_num = quad_corner_verts.size() / 6;
  for (const int i : IndexRange(quads_num)) {
    const Span<int> verts = quad_corner_verts.slice(6 * i, 6);
    /* Use the first vertex of each triangle. */
    new_quad_edges[i] = int2(verts[0], verts[1]);
  }
}

static void calc_quad_corner_edges(const Span<int> src_corner_edges,
                                   const Span<int3> corner_tris,
                                   const int edges_start,
                                   MutableSpan<int> corner_edges)
{
  /* Each triangle starts at the new edge and winds in the same order as corner vertices
   * described by the corner map. */
  for (const int tri : corner_tris.index_range()) {
    corner_edges[3 * tri + 0] = edges_start + tri / 2;
    corner_edges[3 * tri + 1] = src_corner_edges[corner_tris[tri][1]];
    corner_edges[3 * tri + 2] = src_corner_edges[corner_tris[tri][2]];
  }
}

static void calc_edges(const Span<int> src_corner_edges,
                       const Span<int3> corner_tris,
                       const Span<int> corner_verts,
                       const int edges_start,
                       MutableSpan<int2> edges,
                       MutableSpan<int> quad_corner_edges)
{
  const int quads_num = corner_tris.size() / 2;
  threading::parallel_for(IndexRange(quads_num), 1024, [&](const IndexRange quads) {
    const IndexRange tris_range(quads.start() * 2, quads.size() * 2);
    const IndexRange corners(quads.start() * 6, quads.size() * 6);
    calc_edges(corner_verts.slice(corners), edges.slice(quads));
    calc_quad_corner_edges(src_corner_edges,
                           corner_tris.slice(tris_range),
                           edges_start + quads.start(),
                           quad_corner_edges.slice(corners));
  });
}

template<typename T>
static void copy_quad_data_to_tris(const Span<T> src, const IndexMask &quads, MutableSpan<T> dst)
{
  quads.foreach_index_optimized<int>([&](const int src_i, const int dst_i) {
    dst[2 * dst_i + 0] = src[src_i];
    dst[2 * dst_i + 1] = src[src_i];
  });
}

static void copy_quad_data_to_tris(const GSpan src, const IndexMask &quads, GMutableSpan dst)
{
  bke::attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    copy_quad_data_to_tris(src.typed<T>(), quads, dst.typed<T>());
  });
}

}  // namespace quad

static OffsetIndices<int> gather_selected_offsets(const OffsetIndices<int> src_offsets,
                                                  const IndexMaskSegment selection,
                                                  MutableSpan<int> dst_offsets)
{
  int offset = 0;
  for (const int64_t i : selection.index_range()) {
    dst_offsets[i] = offset;
    offset += src_offsets[selection[i]].size();
  }
  dst_offsets.last() = offset;
  return OffsetIndices<int>(dst_offsets);
}

namespace ngon {

static OffsetIndices<int> calc_tris_by_ngon(const OffsetIndices<int> src_faces,
                                            const IndexMask &ngons,
                                            MutableSpan<int> face_offset_data)
{
  ngons.foreach_index(GrainSize(2048), [&](const int face, const int mask) {
    face_offset_data[mask] = bke::mesh::face_triangles_num(src_faces[face].size());
  });
  return offset_indices::accumulate_counts_to_offsets(face_offset_data);
}

static OffsetIndices<int> calc_edges_by_ngon(const OffsetIndices<int> src_faces,
                                             const IndexMask &selection,
                                             MutableSpan<int> edge_offset_data)
{
  selection.foreach_index(GrainSize(2048), [&](const int face, const int mask) {
    /* The number of new inner edges for each face is the number of corners - 3. */
    edge_offset_data[mask] = src_faces[face].size() - 3;
  });
  return offset_indices::accumulate_counts_to_offsets(edge_offset_data);
}

static void calc_corner_tris(const Span<float3> positions,
                             const OffsetIndices<int> src_faces,
                             const Span<int> src_corner_verts,
                             const Span<float3> face_normals,
                             const IndexMask &ngons,
                             const OffsetIndices<int> tris_by_ngon,
                             const TriangulateNGonMode ngon_mode,
                             MutableSpan<int3> corner_tris)
{
  struct LocalData {
    Vector<float3x3> projections;
    Array<int> offset_data;
    Vector<float2> projected_positions;

    /* Only used for the "Beauty" method. */
    MemArena *arena = nullptr;
    Heap *heap = nullptr;

    ~LocalData()
    {
      if (arena) {
        BLI_memarena_free(arena);
      }
      if (heap) {
        BLI_heap_free(heap, nullptr);
      }
    }
  };
  threading::EnumerableThreadSpecific<LocalData> tls;

  ngons.foreach_segment(GrainSize(128), [&](const IndexMaskSegment ngons, const int pos) {
    LocalData &data = tls.local();

    /* In order to simplify and "parallelize" the next loops, gather offsets used to group an array
     * large enough for all the local face corners. */
    data.offset_data.reinitialize(ngons.size() + 1);
    const OffsetIndices local_corner_offsets = gather_selected_offsets(
        src_faces, ngons, data.offset_data);

    /* Use face normals to build projection matrices to make the face positions 2D. */
    data.projections.reinitialize(ngons.size());
    MutableSpan<float3x3> projections = data.projections;
    if (face_normals.is_empty()) {
      for (const int i : ngons.index_range()) {
        const IndexRange src_face = src_faces[ngons[i]];
        const Span<int> face_verts = src_corner_verts.slice(src_face);
        const float3 normal = bke::mesh::face_normal_calc(positions, face_verts);
        axis_dominant_v3_to_m3_negate(projections[i].ptr(), normal);
      }
    }
    else {
      for (const int i : ngons.index_range()) {
        axis_dominant_v3_to_m3_negate(projections[i].ptr(), face_normals[ngons[i]]);
      }
    }

    /* Project the face positions into 2D using the matrices calculated above. */
    data.projected_positions.reinitialize(local_corner_offsets.total_size());
    MutableSpan<float2> projected_positions = data.projected_positions;
    for (const int i : ngons.index_range()) {
      const IndexRange src_face = src_faces[ngons[i]];
      const Span<int> face_verts = src_corner_verts.slice(src_face);
      const float3x3 &matrix = projections[i];

      MutableSpan<float2> positions_2d = projected_positions.slice(local_corner_offsets[i]);
      for (const int i : face_verts.index_range()) {
        mul_v2_m3v3(positions_2d[i], matrix.ptr(), positions[face_verts[i]]);
      }
    }

    if (ngon_mode == TriangulateNGonMode::Beauty) {
      if (!data.arena) {
        data.arena = BLI_memarena_new(BLI_POLYFILL_ARENA_SIZE, __func__);
      }
      if (!data.heap) {
        data.heap = BLI_heap_new_ex(BLI_POLYFILL_ALLOC_NGON_RESERVE);
      }
    }

    /* Calculate the triangulation of corners indices local to each face. */
    for (const int i : ngons.index_range()) {
      const Span<float2> positions_2d = projected_positions.slice(local_corner_offsets[i]);
      const IndexRange tris_range = tris_by_ngon[pos + i];
      MutableSpan<int> map = corner_tris.slice(tris_range).cast<int>();
      BLI_polyfill_calc(reinterpret_cast<const float(*)[2]>(positions_2d.data()),
                        positions_2d.size(),
                        1,
                        reinterpret_cast<uint(*)[3]>(map.data()));
      if (ngon_mode == TriangulateNGonMode::Beauty) {
        BLI_polyfill_beautify(reinterpret_cast<const float(*)[2]>(positions_2d.data()),
                              positions_2d.size(),
                              reinterpret_cast<uint(*)[3]>(map.data()),
                              data.arena,
                              data.heap);
        BLI_memarena_clear(data.arena);
      }
    }

    /* "Globalize" the triangulation created above so the map source indices reference _all_ of the
     * source vertices, not just within the source face. */
    for (const int i : ngons.index_range()) {
      const IndexRange tris_range = tris_by_ngon[pos + i];
      const int src_face_start = src_faces[ngons[i]].start();
      MutableSpan<int> map = corner_tris.slice(tris_range).cast<int>();
      for (int &vert : map) {
        vert += src_face_start;
      }
    }
  });
}

static void calc_inner_tri_edges(const IndexRange src_face,
                                 const Span<int> src_corner_verts,
                                 const Span<int> src_corner_edges,
                                 const Span<int3> corner_tris,
                                 const int edges_start,
                                 MutableSpan<int> corner_edges,
                                 VectorSet<OrderedEdge> &deduplication)
{
  const OrderedEdge last_edge(int(src_face.first()), int(src_face.last()));
  auto add_edge = [&](const OrderedEdge corner_edge) -> int {
    if (corner_edge == last_edge) {
      return src_corner_edges[src_face.last()];
    }
    if (corner_edge.v_high == corner_edge.v_low + 1) {
      return src_corner_edges[corner_edge.v_low];
    }
    const OrderedEdge vert_edge(src_corner_verts[corner_edge.v_low],
                                src_corner_verts[corner_edge.v_high]);
    return edges_start + deduplication.index_of_or_add(vert_edge);
  };

  for (const int i : corner_tris.index_range()) {
    const int3 tri = corner_tris[i];
    corner_edges[3 * i + 0] = add_edge({tri[0], tri[1]});
    corner_edges[3 * i + 1] = add_edge({tri[1], tri[2]});
    corner_edges[3 * i + 2] = add_edge({tri[2], tri[0]});
  }
}

static void calc_edges(const OffsetIndices<int> src_faces,
                       const Span<int> src_corner_verts,
                       const Span<int> src_corner_edges,
                       const IndexMask &ngons,
                       const OffsetIndices<int> tris_by_ngon,
                       const OffsetIndices<int> edges_by_ngon,
                       const IndexRange ngon_edges_range,
                       const Span<int3> corner_tris,
                       MutableSpan<int2> edges,
                       MutableSpan<int> corner_edges)
{
  MutableSpan<int2> inner_edges = edges.slice(ngon_edges_range);
  threading::EnumerableThreadSpecific<VectorSet<OrderedEdge>> tls;
  ngons.foreach_segment(GrainSize(128), [&](const IndexMaskSegment ngons, const int pos) {
    VectorSet<OrderedEdge> &deduplication = tls.local();
    for (const int16_t i : ngons.index_range()) {
      const IndexRange edges = edges_by_ngon[pos + i];
      const IndexRange tris_range = tris_by_ngon[pos + i];
      const IndexRange corners(tris_range.start() * 3, tris_range.size() * 3);
      deduplication.clear();
      calc_inner_tri_edges(src_faces[ngons[i]],
                           src_corner_verts,
                           src_corner_edges,
                           corner_tris.slice(tris_range),
                           ngon_edges_range[edges.start()],
                           corner_edges.slice(corners),
                           deduplication);
      inner_edges.slice(edges).copy_from(deduplication.as_span().cast<int2>());
    }
  });
}

}  // namespace ngon

namespace deduplication {

static GroupedSpan<int> build_vert_to_tri_map(const int verts_num,
                                              const Span<int3> vert_tris,
                                              Array<int> &r_offsets,
                                              Array<int> &r_indices)
{
  r_offsets = Array<int>(verts_num + 1, 0);
  offset_indices::build_reverse_offsets(vert_tris.cast<int>(), r_offsets);
  const OffsetIndices offsets(r_offsets.as_span());

  r_indices.reinitialize(offsets.total_size());
  int *counts = MEM_calloc_arrayN<int>(offsets.size(), __func__);
  BLI_SCOPED_DEFER([&]() { MEM_freeN(counts); })
  threading::parallel_for(vert_tris.index_range(), 1024, [&](const IndexRange range) {
    for (const int tri : range) {
      for (const int vert : {vert_tris[tri][0], vert_tris[tri][1], vert_tris[tri][2]}) {
        const int index_in_group = atomic_fetch_and_add_int32(&counts[vert], 1);
        r_indices[offsets[vert][index_in_group]] = tri;
      }
    }
  });

  return {r_offsets.as_span(), r_indices.as_span()};
}

/**
 * To avoid adding duplicate faces to the mesh without complicating the triangulation code to
 * support that unlikely case, check if triangles (which are all unselected) have an equivalent
 * newly created triangle, and don't copy them to the result mesh if so.
 */
static IndexMask calc_unselected_faces(const Mesh &mesh,
                                       const OffsetIndices<int> src_faces,
                                       const Span<int> src_corner_verts,
                                       const IndexMask &selection,
                                       const Span<int3> corner_tris,
                                       IndexMaskMemory &memory)
{
  const IndexMask unselected = selection.complement(src_faces.index_range(), memory);
  if (mesh.no_overlapping_topology()) {
    return unselected;
  }
  const IndexMask unselected_tris = IndexMask::from_batch_predicate(
      unselected,
      GrainSize(4096),
      memory,
      [&](const IndexMaskSegment universe_segment, IndexRangesBuilder<int16_t> &builder) {
        if (unique_sorted_indices::non_empty_is_range(universe_segment.base_span())) {
          const IndexRange universe_as_range = unique_sorted_indices::non_empty_as_range(
              universe_segment.base_span());
          const IndexRange segment_range = universe_as_range.shift(universe_segment.offset());
          const OffsetIndices segment_faces = src_faces.slice(segment_range);
          if (segment_faces.total_size() == segment_faces.size() * 3) {
            /* All faces in segment are triangles. */
            builder.add_range(universe_as_range.start(), universe_as_range.one_after_last());
            return universe_segment.offset();
          }
        }

        for (const int16_t i : universe_segment.base_span()) {
          const int face = int(universe_segment.offset() + i);
          if (src_faces[face].size() == 3) {
            builder.add(i);
          }
        }
        return universe_segment.offset();
      });

  if (unselected_tris.is_empty()) {
    return unselected;
  }

  Array<int3> vert_tris(corner_tris.size());
  bke::attribute_math::gather(
      src_corner_verts, corner_tris.cast<int>(), vert_tris.as_mutable_span().cast<int>());

  Array<int> vert_to_tri_offsets;
  Array<int> vert_to_tri_indices;
  const GroupedSpan<int> vert_to_tri = build_vert_to_tri_map(
      mesh.verts_num, vert_tris, vert_to_tri_offsets, vert_to_tri_indices);

  auto tri_exists = [&](const std::array<int, 3> &tri_verts) {
    /* TODO: Sorting the three values with a few comparisons would be faster than a #Set. */
    const Set<int, 3> vert_set(tri_verts);
    return std::any_of(tri_verts.begin(), tri_verts.end(), [&](const int vert) {
      return std::any_of(vert_to_tri[vert].begin(), vert_to_tri[vert].end(), [&](const int tri) {
        const Set<int, 3> other_tri_verts(Span(&vert_tris[tri].x, 3));
        return other_tri_verts == vert_set;
      });
    });
  };

  const IndexMask duplicate_triangles = IndexMask::from_predicate(
      unselected_tris, GrainSize(1024), memory, [&](const int i) {
        const Span<int> face_verts = src_corner_verts.slice(src_faces[i]);
        return tri_exists({face_verts[0], face_verts[1], face_verts[2]});
      });

  return IndexMask::from_difference(unselected, duplicate_triangles, memory);
}

static std::optional<int> find_edge_duplicate(const GroupedSpan<int> vert_to_edge_map,
                                              const Span<int2> edges,
                                              const OrderedEdge edge)
{
  for (const int vert : {edge.v_low, edge.v_high}) {
    for (const int src_edge : vert_to_edge_map[vert]) {
      if (OrderedEdge(edges[src_edge]) == edge) {
        return src_edge;
      }
    }
  }
  return std::nullopt;
}

/**
 * Given all the edges on the new mesh, find new edges that are duplicates of existing edges.
 * If there are any, remove them and references to them in the corner edge array.
 *
 * \return The final number of edges in the mesh.
 */
static int calc_new_edges(const Mesh &src_mesh,
                          const Span<int2> src_edges,
                          const IndexRange new_edges_range,
                          MutableSpan<int2> edges,
                          MutableSpan<int> corner_edges)
{
  if (src_mesh.no_overlapping_topology()) {
    return edges.size();
  }

  Array<int> vert_to_edge_offsets;
  Array<int> vert_to_edge_indices;
  const GroupedSpan<int> vert_to_edge = bke::mesh::build_vert_to_edge_map(
      src_edges, src_mesh.verts_num, vert_to_edge_offsets, vert_to_edge_indices);

  const Span<int2> new_edges = edges.slice(new_edges_range);
  Array<int> duplicate_remap(new_edges.size());
  threading::parallel_for(new_edges.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      duplicate_remap[i] = find_edge_duplicate(vert_to_edge, src_edges, new_edges[i]).value_or(-1);
    }
  });
  IndexMaskMemory memory;
  const IndexMask non_duplicate_new_edges = IndexMask::from_predicate(
      new_edges.index_range(), GrainSize(4096), memory, [&](const int i) {
        return duplicate_remap[i] == -1;
      });
  if (non_duplicate_new_edges.size() == new_edges.size()) {
    return edges.size();
  }

  non_duplicate_new_edges.foreach_index_optimized<int>(
      GrainSize(4096), [&](const int index, const int pos) {
        duplicate_remap[index] = pos + new_edges_range.start();
      });
  threading::parallel_for(corner_edges.index_range(), 4096, [&](const IndexRange range) {
    for (const int corner : range) {
      const int edge = corner_edges[corner];
      if (edge < new_edges_range.start()) {
        continue;
      }
      const int remap_index = edge - new_edges_range.start();
      corner_edges[corner] = duplicate_remap[remap_index];
    }
  });

  Array<int2> edges_with_duplicates = new_edges;
  array_utils::gather(edges_with_duplicates.as_span(),
                      non_duplicate_new_edges,
                      edges.slice(new_edges_range.start(), non_duplicate_new_edges.size()));
  return src_edges.size() + non_duplicate_new_edges.size();
}

}  // namespace deduplication

std::optional<Mesh *> mesh_triangulate(const Mesh &src_mesh,
                                       const IndexMask &selection_with_tris,
                                       const TriangulateNGonMode ngon_mode,
                                       const TriangulateQuadMode quad_mode,
                                       const bke::AttributeFilter &attribute_filter)
{
  const Span<float3> positions = src_mesh.vert_positions();
  const Span<int2> src_edges = src_mesh.edges();
  const OffsetIndices src_faces = src_mesh.faces();
  const Span<int> src_corner_verts = src_mesh.corner_verts();
  const Span<int> src_corner_edges = src_mesh.corner_edges();
  const bke::AttributeAccessor src_attributes = src_mesh.attributes();

  /* Divide the input selection into separate selections for each face type. This isn't necessary
   * for correctness, but considering groups of each face type separately simplifies optimizing
   * for each type. For example, quad triangulation is much simpler than Ngon triangulation. */
  IndexMaskMemory memory;
  const IndexMask quads = IndexMask::from_predicate(
      selection_with_tris, GrainSize(4096), memory, [&](const int i) {
        return src_faces[i].size() == 4;
      });
  const IndexMask ngons = IndexMask::from_predicate(
      selection_with_tris, GrainSize(4096), memory, [&](const int i) {
        return src_faces[i].size() > 4;
      });
  if (quads.is_empty() && ngons.is_empty()) {
    /* All selected faces are already triangles. */
    return std::nullopt;
  }

  const IndexMask selection = IndexMask::from_union(quads, ngons, memory);

  /* Calculate group of triangle indices for each selected Ngon to facilitate calculating them in
   * parallel later. */
  Array<int> tris_by_ngon_data(ngons.size() + 1);
  const OffsetIndices tris_by_ngon = ngon::calc_tris_by_ngon(src_faces, ngons, tris_by_ngon_data);
  const int ngon_tris_num = tris_by_ngon.total_size();
  const int quad_tris_num = quads.size() * 2;
  const IndexRange tris_range(ngon_tris_num + quad_tris_num);
  const IndexRange ngon_tris_range = tris_range.take_front(ngon_tris_num);
  const IndexRange quad_tris_range = tris_range.take_back(quad_tris_num);

  const int ngon_corners_num = tris_by_ngon.total_size() * 3;
  const int quad_corners_num = quads.size() * 6;
  const IndexRange tri_corners_range(quad_corners_num + ngon_corners_num);
  const IndexRange ngon_corners_range = tri_corners_range.take_front(ngon_corners_num);
  const IndexRange quad_corners_range = tri_corners_range.take_back(quad_corners_num);

  /* Calculate groups of new inner edges for each selected Ngon so they can be filled in parallel
   * later. */
  Array<int> edge_offset_data(ngons.size() + 1);
  const OffsetIndices edges_by_ngon = ngon::calc_edges_by_ngon(src_faces, ngons, edge_offset_data);
  const int ngon_edges_num = edges_by_ngon.total_size();
  const int quad_edges_num = quads.size();
  const IndexRange src_edges_range(0, src_edges.size());
  const IndexRange tri_edges_range(src_edges_range.one_after_last(),
                                   ngon_edges_num + quad_edges_num);
  const IndexRange ngon_edges_range = tri_edges_range.take_front(ngon_edges_num);
  const IndexRange quad_edges_range = tri_edges_range.take_back(quad_edges_num);

  /* An index map that maps from newly created corners in `tri_corners_range` to original corner
   * indices. This is used to interpolate `corner_vert` indices and face corner attributes. If
   * there are no face corner attributes, theoretically the map could be skipped and corner
   * vertex indices could be interpolated immediately, but that isn't done for simplicity. */
  Array<int3> corner_tris(tris_range.size());

  if (!ngons.is_empty()) {
    ngon::calc_corner_tris(positions,
                           src_faces,
                           src_corner_verts,
                           face_normals_if_worthwhile(src_mesh, ngons.size()),
                           ngons,
                           tris_by_ngon,
                           ngon_mode,
                           corner_tris.as_mutable_span().slice(ngon_tris_range));
  }
  if (!quads.is_empty()) {
    quad::calc_corner_tris(positions,
                           src_faces,
                           src_corner_verts,
                           quads,
                           quad_mode,
                           corner_tris.as_mutable_span().slice(quad_tris_range));
  }

  const IndexMask unselected = deduplication::calc_unselected_faces(
      src_mesh, src_faces, src_corner_verts, selection, corner_tris, memory);
  const IndexRange unselected_range(tris_range.one_after_last(), unselected.size());

  /* Create a mesh with no face corners.
   * - We haven't yet counted the number of corners from unselected faces. Creating the final face
   *   offsets will give us that number anyway, so wait to create the edges.
   * - The number of edges is a guess that doesn't include deduplication of new edges with
   *   existing edges. If those are found, the mesh will be resized later.
   * - Don't create attributes to facilitate implicit sharing of the positions array. */
  Mesh *mesh = bke::mesh_new_no_attributes(src_mesh.verts_num,
                                           src_edges.size() + tri_edges_range.size(),
                                           tris_range.size() + unselected.size(),
                                           0);
  BKE_mesh_copy_parameters_for_eval(mesh, &src_mesh);

  /* Find the face corner ranges using the offsets array from the new mesh. That gives us the
   * final number of face corners. */
  const OffsetIndices faces = calc_face_offsets(
      src_faces, unselected, mesh->face_offsets_for_write());
  mesh->corners_num = faces.total_size();
  const OffsetIndices faces_unselected = faces.slice(unselected_range);

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  attributes.add<int2>(".edge_verts", bke::AttrDomain::Edge, bke::AttributeInitConstruct());
  attributes.add<int>(".corner_vert", bke::AttrDomain::Corner, bke::AttributeInitConstruct());
  attributes.add<int>(".corner_edge", bke::AttrDomain::Corner, bke::AttributeInitConstruct());

  MutableSpan<int2> edges_with_duplicates = mesh->edges_for_write();
  MutableSpan<int> corner_verts = mesh->corner_verts_for_write();
  MutableSpan<int> corner_edges = mesh->corner_edges_for_write();

  array_utils::gather(
      src_corner_verts, corner_tris.as_span().cast<int>(), corner_verts.slice(tri_corners_range));

  if (!ngons.is_empty()) {
    ngon::calc_edges(src_faces,
                     src_corner_verts,
                     src_corner_edges,
                     ngons,
                     tris_by_ngon,
                     edges_by_ngon,
                     ngon_edges_range,
                     corner_tris.as_mutable_span().slice(ngon_tris_range),
                     edges_with_duplicates,
                     corner_edges.slice(ngon_corners_range));
  }

  if (!quads.is_empty()) {
    quad::calc_edges(src_corner_edges,
                     corner_tris.as_mutable_span().slice(quad_tris_range),
                     corner_verts.slice(quad_corners_range),
                     quad_edges_range.start(),
                     edges_with_duplicates.slice(quad_edges_range),
                     corner_edges.slice(quad_corners_range));
  }

  mesh->edges_num = deduplication::calc_new_edges(
      src_mesh, src_edges, tri_edges_range, edges_with_duplicates, corner_edges);

  edges_with_duplicates.take_front(src_edges.size()).copy_from(src_edges);

  /* Vertex attributes are totally unaffected and can be shared with implicit sharing.
   * Use the #CustomData API for simpler support for vertex groups. */
  CustomData_merge(&src_mesh.vert_data, &mesh->vert_data, CD_MASK_MESH.vmask, mesh->verts_num);

  for (auto &attribute : bke::retrieve_attributes_for_transfer(
           src_attributes,
           attributes,
           ATTR_DOMAIN_MASK_EDGE,
           bke::attribute_filter_with_skip_ref(attribute_filter, {".edge_verts"})))
  {
    attribute.dst.span.slice(src_edges_range).copy_from(attribute.src);
    GMutableSpan new_data = attribute.dst.span.drop_front(src_edges.size());
    /* It would be reasonable interpolate data from connected edges within each face.
     * Currently the data from new edges is just set to the type's default value. */
    const void *default_value = new_data.type().default_value();
    new_data.type().fill_construct_n(default_value, new_data.data(), new_data.size());
    attribute.dst.finish();
  }
  if (CustomData_has_layer(&src_mesh.edge_data, CD_ORIGINDEX)) {
    const Span src(
        static_cast<const int *>(CustomData_get_layer(&src_mesh.edge_data, CD_ORIGINDEX)),
        src_mesh.edges_num);
    MutableSpan dst(static_cast<int *>(CustomData_add_layer(
                        &mesh->edge_data, CD_ORIGINDEX, CD_CONSTRUCT, mesh->edges_num)),
                    mesh->edges_num);
    dst.drop_front(src_edges.size()).fill(ORIGINDEX_NONE);
    array_utils::copy(src, dst.slice(src_edges_range));
  }

  for (auto &attribute : bke::retrieve_attributes_for_transfer(
           src_attributes, attributes, ATTR_DOMAIN_MASK_FACE, attribute_filter))
  {
    bke::attribute_math::gather_to_groups(
        tris_by_ngon, ngons, attribute.src, attribute.dst.span.slice(ngon_tris_range));
    quad::copy_quad_data_to_tris(attribute.src, quads, attribute.dst.span.slice(quad_tris_range));
    array_utils::gather(attribute.src, unselected, attribute.dst.span.slice(unselected_range));
    attribute.dst.finish();
  }
  if (CustomData_has_layer(&src_mesh.face_data, CD_ORIGINDEX)) {
    const Span src(
        static_cast<const int *>(CustomData_get_layer(&src_mesh.face_data, CD_ORIGINDEX)),
        src_mesh.faces_num);
    MutableSpan dst(static_cast<int *>(CustomData_add_layer(
                        &mesh->face_data, CD_ORIGINDEX, CD_CONSTRUCT, mesh->faces_num)),
                    mesh->faces_num);
    bke::attribute_math::gather_to_groups(tris_by_ngon, ngons, src, dst.slice(ngon_tris_range));
    quad::copy_quad_data_to_tris(src, quads, dst.slice(quad_tris_range));
    array_utils::gather(src, unselected, dst.slice(unselected_range));
  }

  array_utils::gather_group_to_group(
      src_faces, faces_unselected, unselected, src_corner_verts, corner_verts);
  array_utils::gather_group_to_group(
      src_faces, faces_unselected, unselected, src_corner_edges, corner_edges);
  for (auto &attribute : bke::retrieve_attributes_for_transfer(
           src_attributes,
           attributes,
           ATTR_DOMAIN_MASK_CORNER,
           bke::attribute_filter_with_skip_ref(attribute_filter,
                                               {".corner_vert", ".corner_edge"})))
  {
    bke::attribute_math::gather_group_to_group(
        src_faces, faces_unselected, unselected, attribute.src, attribute.dst.span);
    bke::attribute_math::gather(attribute.src,
                                corner_tris.as_span().cast<int>(),
                                attribute.dst.span.slice(tri_corners_range));
    attribute.dst.finish();
  }

  mesh->runtime->bounds_cache = src_mesh.runtime->bounds_cache;
  copy_loose_vert_hint(src_mesh, *mesh);
  copy_loose_edge_hint(src_mesh, *mesh);
  if (src_mesh.no_overlapping_topology()) {
    mesh->tag_overlapping_none();
  }
  BLI_assert(BKE_mesh_is_valid(mesh));
  return mesh;
}

}  // namespace blender::geometry
