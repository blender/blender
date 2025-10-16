/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_index_mask.hh"
#include "BLI_index_mask_expression.hh"
#include "BLI_index_ranges_builder.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_polyfill_2d.h"
#include "BLI_polyfill_2d_beautify.h"
#include "BLI_vector_set.hh"

#include "BLI_heap.h"
#include "BLI_memarena.h"

#include "BKE_attribute.hh"
#include "BKE_attribute_math.hh"
#include "BKE_customdata.hh"
#include "BKE_mesh.hh"

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
      BLI_polyfill_calc(reinterpret_cast<const float (*)[2]>(positions_2d.data()),
                        positions_2d.size(),
                        1,
                        reinterpret_cast<uint(*)[3]>(map.data()));
      if (ngon_mode == TriangulateNGonMode::Beauty) {
        BLI_polyfill_beautify(reinterpret_cast<const float (*)[2]>(positions_2d.data()),
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

}  // namespace ngon

struct TriKey {
  int tri_index;
  /* The lowest vertex index in the face is used as a hash value and a way to compare face keys to
   * avoid memory lookup in all false cases. */
  int tri_lower_vert;

  TriKey(const int tri_index, Span<int3> tris)
      : tri_index(tri_index), tri_lower_vert(tris[tri_index][0])
  {
    [[maybe_unused]] const int3 &tri_verts = tris[tri_index];
    BLI_assert(std::is_sorted(&tri_verts[0], &tri_verts[0] + 3));
  }
};

struct FaceHash {
  uint64_t operator()(const TriKey value) const
  {
    return uint64_t(value.tri_lower_vert);
  }

  uint64_t operator()(const int3 value) const
  {
    BLI_assert(std::is_sorted(&value[0], &value[0] + 3));
    return uint64_t(value[0]);
  }
};

struct FacesEquality {
  Span<int3> tris;
  bool operator()(const TriKey a, const TriKey b) const
  {
    return a.tri_lower_vert == b.tri_lower_vert && tris[a.tri_index] == tris[b.tri_index];
  }

  bool operator()(const int3 a, const TriKey b) const
  {
    BLI_assert(std::is_sorted(&a[0], &a[0] + 3));
    return b.tri_lower_vert == a[0] && tris[b.tri_index] == a;
  }
};

static int3 tri_to_ordered(const int3 tri)
{
  int3 res;
  res[0] = std::min({tri[0], tri[1], tri[2]});
  res[2] = std::max({tri[0], tri[1], tri[2]});
  res[1] = (tri[0] - res[0]) + (tri[2] - res[2]) + tri[1];
  return res;
}

static Span<int3> tri_to_ordered_tri(MutableSpan<int3> tris)
{
  threading::parallel_for(tris.index_range(), 4096, [&](const IndexRange range) {
    for (int3 &tri : tris.slice(range)) {
      tri = tri_to_ordered(tri);
    }
  });
  return tris;
}

static IndexMask face_tris_mask(const OffsetIndices<int> src_faces,
                                const IndexMask &mask,
                                IndexMaskMemory &memory)
{
  return IndexMask::from_batch_predicate(
      mask,
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
}

static IndexMask tris_in_set(const IndexMask &tri_mask,
                             const OffsetIndices<int> faces,
                             const Span<int> corner_verts,
                             const VectorSet<TriKey,
                                             4,
                                             DefaultProbingStrategy,
                                             FaceHash,
                                             FacesEquality,
                                             SimpleVectorSetSlot<TriKey, int>> &unique_tris,
                             IndexMaskMemory &memory)
{
  return IndexMask::from_predicate(tri_mask, GrainSize(4096), memory, [&](const int face_i) {
    BLI_assert(faces[face_i].size() == 3);
    const int3 corner_tri(&corner_verts[faces[face_i].start()]);
    return unique_tris.contains_as(tri_to_ordered(corner_tri));
  });
}

static void face_keys_to_face_indices(const Span<TriKey> faces, MutableSpan<int> indices)
{
  BLI_assert(faces.size() == indices.size());
  threading::parallel_for(faces.index_range(), 4096, [&](const IndexRange range) {
    for (const int face_i : range) {
      indices[face_i] = faces[face_i].tri_index;
    }
  });
}

static void quad_indices_of_tris(const IndexMask &quads, MutableSpan<int> indices)
{
  BLI_assert(quads.size() * 2 == indices.size());
  quads.foreach_index_optimized<int>(GrainSize(4096), [&](const int index, const int pos) {
    indices[2 * pos + 0] = index;
    indices[2 * pos + 1] = index;
  });
}

static void ngon_indices_of_tris(const IndexMask &ngons,
                                 const OffsetIndices<int> tris_by_ngon,
                                 MutableSpan<int> indices)
{
  BLI_assert(tris_by_ngon.size() == ngons.size());
  BLI_assert(tris_by_ngon.total_size() == indices.size());
  ngons.foreach_index_optimized<int>(GrainSize(4096), [&](const int index, const int pos) {
    indices.slice(tris_by_ngon[pos]).fill(index);
  });
}

std::optional<Mesh *> mesh_triangulate(const Mesh &src_mesh,
                                       const IndexMask &selection_with_tris,
                                       const TriangulateNGonMode ngon_mode,
                                       const TriangulateQuadMode quad_mode,
                                       const bke::AttributeFilter &attribute_filter)
{
  const Span<float3> positions = src_mesh.vert_positions();
  const OffsetIndices src_faces = src_mesh.faces();
  const Span<int> src_corner_verts = src_mesh.corner_verts();
  const bke::AttributeAccessor src_attributes = src_mesh.attributes();

  IndexMaskMemory memory;

  /* If there are a lot of triangles, they can be skipped quickly for filtering. */
  const IndexMask src_tris = face_tris_mask(src_faces, src_faces.index_range(), memory);
  const IndexMask selection = IndexMask::from_difference(selection_with_tris, src_tris, memory);

  /* Divide the input selection into separate selections for each face type. This isn't necessary
   * for correctness, but considering groups of each face type separately simplifies optimizing
   * for each type. For example, quad triangulation is much simpler than Ngon triangulation. */
  const IndexMask quads = IndexMask::from_predicate(
      selection, GrainSize(4096), memory, [&](const int i) { return src_faces[i].size() == 4; });
  const IndexMask ngons = IndexMask::from_predicate(
      selection, GrainSize(4096), memory, [&](const int i) { return src_faces[i].size() > 4; });
  if (quads.is_empty() && ngons.is_empty()) {
    /* All selected faces are already triangles. */
    return std::nullopt;
  }

  /* Calculate group of triangle indices for each selected Ngon to facilitate calculating them in
   * parallel later. */
  Array<int> tris_by_ngon_data(ngons.size() + 1);
  const OffsetIndices tris_by_ngon = ngon::calc_tris_by_ngon(src_faces, ngons, tris_by_ngon_data);
  const int ngon_tris_num = tris_by_ngon.total_size();
  const int quad_tris_num = quads.size() * 2;
  const IndexRange tris_range(ngon_tris_num + quad_tris_num);
  const IndexRange ngon_tris_range = tris_range.take_front(ngon_tris_num);
  const IndexRange quad_tris_range = tris_range.take_back(quad_tris_num);

  Array<int3> corner_tris(ngon_tris_num + quad_tris_num);

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

  /* There are 3 separate sets of triangles: original mesh triangles, new triangles from quads,
   * and triangles from n-gons. Deduplication can result in a mix of parts of multiple quads,
   * multiple quads, original triangle, and even concatenation of parts of multiple n-gons.
   * So we have to deduplicate all triangles together. */
  Array<int3> vert_tris(ngon_tris_num + quad_tris_num);
  array_utils::gather(src_corner_verts,
                      corner_tris.as_span().cast<int>(),
                      vert_tris.as_mutable_span().cast<int>());
  const Span<int3> ordered_vert_tris = tri_to_ordered_tri(vert_tris.as_mutable_span());

  /* Use ordered vertex triplets (a < b < c) to represent all new triangles.
   * #TriKey knows indices of the face and points into #ordered_vert_tris, but probe can be done
   * without #TriKey but dirrectly with a triplet so probe not necessary to be a part of
   * #ordered_vert_tris. */
  VectorSet<TriKey,
            4,
            DefaultProbingStrategy,
            FaceHash,
            FacesEquality,
            SimpleVectorSetSlot<TriKey, int>>
      unique_tris(FaceHash{}, FacesEquality{ordered_vert_tris});

  /* Could be done parallel using grouping of faces by their lowest vertex and the next linear
   * deduplication, but right now this is just a sequential hash-set. */
  for (const int face_i : ordered_vert_tris.index_range()) {
    const TriKey face_key(face_i, ordered_vert_tris);
    unique_tris.add(face_key);
  }
  const int unique_tri_num = unique_tris.size();

  /* Since currently deduplication is greedy, there is no mix of data of deduplicated triangles,
   * instead some of them are removed. Priority: Original triangles removed if any of new triangles
   * are the same. For all new triangles here is direct order dependency. */
  const IndexMask src_tris_duplicated = tris_in_set(
      src_tris, src_faces, src_corner_verts, unique_tris, memory);

  index_mask::ExprBuilder mask_builder;
  const IndexMask unique_src_faces = index_mask::evaluate_expression(
      mask_builder.subtract(src_faces.index_range(), {&quads, &ngons, &src_tris_duplicated}),
      memory);

  const IndexRange unique_faces_range(unique_tri_num + unique_src_faces.size());
  const IndexRange unique_tri_range = unique_faces_range.take_front(unique_tri_num);
  const IndexRange unique_src_faces_range = unique_faces_range.take_back(unique_src_faces.size());

  /* Create a mesh with no face corners.
   * - We haven't yet counted the number of corners from unselected faces. Creating the final face
   *   offsets will give us that number anyway, so wait to create the edges.
   * - Don't create attributes to facilitate implicit sharing of the positions array. */
  Mesh *mesh = bke::mesh_new_no_attributes(
      src_mesh.verts_num, src_mesh.edges_num, unique_faces_range.size(), 0);
  BKE_mesh_copy_parameters_for_eval(mesh, &src_mesh);

  MutableSpan<int> dst_offsets = mesh->face_offsets_for_write();
  offset_indices::fill_constant_group_size(
      3, 0, dst_offsets.take_front(unique_tri_range.size() + 1));
  const int total_new_tri_corners = unique_tri_range.size() * 3;
  offset_indices::gather_selected_offsets(
      src_faces,
      unique_src_faces,
      total_new_tri_corners,
      dst_offsets.take_back(unique_src_faces_range.size() + 1));

  const OffsetIndices<int> faces(dst_offsets);
  mesh->corners_num = faces.total_size();

  /* Vertex attributes are totally unaffected and can be shared with implicit sharing.
   * Use the #CustomData API for simpler support for vertex groups. */
  CustomData_merge(&src_mesh.vert_data, &mesh->vert_data, CD_MASK_MESH.vmask, mesh->verts_num);
  /* Edge attributes are the same for original edges. New edges will be generated by
   * #bke::mesh_calc_edges later. */
  CustomData_merge(&src_mesh.edge_data, &mesh->edge_data, CD_MASK_MESH.emask, mesh->edges_num);

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();

  const bool has_duplicate_faces = unique_tri_num != (ngon_tris_num + quad_tris_num);

  Array<int> dst_tri_to_src_face(unique_tri_num);
  face_keys_to_face_indices(unique_tris.as_span(), dst_tri_to_src_face.as_mutable_span());

  Array<int3> unique_corner_tris_data;
  if (has_duplicate_faces) {
    unique_corner_tris_data.reinitialize(unique_tri_num);
    array_utils::gather(corner_tris.as_span(),
                        dst_tri_to_src_face.as_span(),
                        unique_corner_tris_data.as_mutable_span());
  }

  {
    Array<int> src_to_unique_map(ngon_tris_num + quad_tris_num);
    quad_indices_of_tris(quads, src_to_unique_map.as_mutable_span().slice(quad_tris_range));
    ngon_indices_of_tris(
        ngons, tris_by_ngon, src_to_unique_map.as_mutable_span().slice(ngon_tris_range));

    array_utils::gather(src_to_unique_map.as_span(),
                        dst_tri_to_src_face.as_span(),
                        dst_tri_to_src_face.as_mutable_span());
  }

  const Span<int3> unique_corner_tris = has_duplicate_faces ? unique_corner_tris_data.as_span() :
                                                              corner_tris.as_span();

  for (auto &attribute : bke::retrieve_attributes_for_transfer(
           src_attributes, attributes, {bke::AttrDomain::Face}, attribute_filter))
  {
    bke::attribute_math::gather(
        attribute.src, dst_tri_to_src_face.as_span(), attribute.dst.span.slice(unique_tri_range));
    array_utils::gather(
        attribute.src, unique_src_faces, attribute.dst.span.slice(unique_src_faces_range));
    attribute.dst.finish();
  }
  if (CustomData_has_layer(&src_mesh.face_data, CD_ORIGINDEX)) {
    const Span src(
        static_cast<const int *>(CustomData_get_layer(&src_mesh.face_data, CD_ORIGINDEX)),
        src_mesh.faces_num);
    MutableSpan dst(static_cast<int *>(CustomData_add_layer(
                        &mesh->face_data, CD_ORIGINDEX, CD_CONSTRUCT, mesh->faces_num)),
                    mesh->faces_num);

    array_utils::gather(src, dst_tri_to_src_face.as_span(), dst.slice(unique_tri_range));
    array_utils::gather(src, unique_src_faces, dst.slice(unique_src_faces_range));
  }

  attributes.add<int>(".corner_vert", bke::AttrDomain::Corner, bke::AttributeInitConstruct());

  MutableSpan<int> corner_verts = mesh->corner_verts_for_write();
  array_utils::gather_group_to_group(src_faces,
                                     faces.slice(unique_src_faces_range),
                                     unique_src_faces,
                                     src_corner_verts,
                                     corner_verts);
  array_utils::gather(src_corner_verts,
                      unique_corner_tris.cast<int>(),
                      corner_verts.take_front(total_new_tri_corners));

  for (auto &attribute : bke::retrieve_attributes_for_transfer(
           src_attributes,
           attributes,
           {bke::AttrDomain::Corner},
           bke::attribute_filter_with_skip_ref(attribute_filter,
                                               {".corner_vert", ".corner_edge"})))
  {
    bke::attribute_math::gather_group_to_group(
        src_faces,
        faces.slice(IndexRange(unique_tri_num, unique_src_faces.size())),
        unique_src_faces,
        attribute.src,
        attribute.dst.span);
    bke::attribute_math::gather(attribute.src,
                                unique_corner_tris.cast<int>(),
                                attribute.dst.span.slice(0, unique_tri_num * 3));
    attribute.dst.finish();
  }

  /* Automatically generate new edges between new triangles, with necessary deduplication. */
  bke::mesh_calc_edges(*mesh, true, false, attribute_filter);

  mesh->runtime->bounds_cache = src_mesh.runtime->bounds_cache;
  copy_loose_vert_hint(src_mesh, *mesh);
  if (src_mesh.no_overlapping_topology()) {
    mesh->tag_overlapping_none();
  }
  BLI_assert(BKE_mesh_is_valid(mesh));
  return mesh;
}

}  // namespace blender::geometry
