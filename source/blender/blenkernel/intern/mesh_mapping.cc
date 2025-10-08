/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Functions for accessing mesh connectivity data.
 * eg: faces connected to verts, UVs connected to verts.
 */

#include <algorithm>

#include "MEM_guardedalloc.h"

#include "atomic_ops.h"

#include "BLI_array.hh"
#include "BLI_bitmap.h"
#include "BLI_function_ref.hh"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "BKE_customdata.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BLI_memarena.h"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

/* -------------------------------------------------------------------- */
/** \name Mesh Connectivity Mapping
 * \{ */

UvVertMap *BKE_mesh_uv_vert_map_create(blender::OffsetIndices<int> faces,
                                       blender::Span<int> corner_verts,
                                       blender::Span<blender::float2> uv_map,
                                       int verts_num,
                                       const blender::float2 &limit,
                                       bool use_winding)
{
  using namespace blender;
  /* NOTE: N-gon version WIP, based on #BM_uv_vert_map_create. */
  if (faces.is_empty()) {
    return nullptr;
  }
  const int corners_num = faces.total_size();

  UvVertMap *vmap = MEM_callocN<UvVertMap>("UvVertMap");
  UvMapVert *buf = vmap->buf = MEM_calloc_arrayN<UvMapVert>(size_t(corners_num), "UvMapVert");
  vmap->vert = MEM_calloc_arrayN<UvMapVert *>(size_t(verts_num), "UvMapVert*");

  if (!vmap->vert || !vmap->buf) {
    BKE_mesh_uv_vert_map_free(vmap);
    return nullptr;
  }

  Array<bool> winding;
  if (use_winding) {
    winding = Array<bool>(faces.size(), false);
    threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
      for (const int64_t face : range) {
        const Span<float2> face_uvs = uv_map.slice(faces[face]);
        winding[face] = cross_poly_v2(reinterpret_cast<const float (*)[2]>(face_uvs.data()),
                                      uint(faces[face].size())) < 0.0f;
      }
    });
  }

  for (const int64_t a : faces.index_range()) {
    const IndexRange face = faces[a];
    for (const int64_t i : face.index_range()) {
      buf->loop_of_face_index = ushort(i);
      buf->face_index = uint(a);
      buf->separate = false;
      buf->next = vmap->vert[corner_verts[face[i]]];
      vmap->vert[corner_verts[face[i]]] = buf;
      buf++;
    }
  }

  /* sort individual uvs for each vert */
  for (const int64_t vert : IndexRange(verts_num)) {
    UvMapVert *newvlist = nullptr, *vlist = vmap->vert[vert];
    UvMapVert *iterv, *v, *lastv, *next;
    const float *uv, *uv2;
    float uvdiff[2];

    while (vlist) {
      v = vlist;
      vlist = vlist->next;
      v->next = newvlist;
      newvlist = v;

      uv = uv_map[faces[v->face_index].start() + v->loop_of_face_index];
      lastv = nullptr;
      iterv = vlist;

      while (iterv) {
        next = iterv->next;

        uv2 = uv_map[faces[iterv->face_index].start() + iterv->loop_of_face_index];
        sub_v2_v2v2(uvdiff, uv2, uv);

        if (fabsf(uv[0] - uv2[0]) < limit[0] && fabsf(uv[1] - uv2[1]) < limit[1] &&
            (!use_winding || winding[iterv->face_index] == winding[v->face_index]))
        {
          if (lastv) {
            lastv->next = next;
          }
          else {
            vlist = next;
          }
          iterv->next = newvlist;
          newvlist = iterv;
        }
        else {
          lastv = iterv;
        }

        iterv = next;
      }

      newvlist->separate = true;
    }

    vmap->vert[vert] = newvlist;
  }

  return vmap;
}

UvMapVert *BKE_mesh_uv_vert_map_get_vert(UvVertMap *vmap, uint v)
{
  return vmap->vert[v];
}

void BKE_mesh_uv_vert_map_free(UvVertMap *vmap)
{
  if (vmap) {
    if (vmap->vert) {
      MEM_freeN(vmap->vert);
    }
    if (vmap->buf) {
      MEM_freeN(vmap->buf);
    }
    MEM_freeN(vmap);
  }
}

void BKE_mesh_vert_corner_tri_map_create(MeshElemMap **r_map,
                                         int **r_mem,
                                         const int totvert,
                                         const blender::int3 *corner_tris,
                                         const int tris_num,
                                         const int *corner_verts,
                                         const int /*corners_num*/)
{
  MeshElemMap *map = MEM_calloc_arrayN<MeshElemMap>(size_t(totvert), __func__);
  int *indices = MEM_malloc_arrayN<int>(size_t(tris_num) * 3, __func__);
  int *index_step;
  int i;

  /* count face users */
  for (i = 0; i < tris_num; i++) {
    for (int j = 3; j--;) {
      map[corner_verts[corner_tris[i][j]]].count++;
    }
  }

  /* create offsets */
  index_step = indices;
  for (i = 0; i < totvert; i++) {
    map[i].indices = index_step;
    index_step += map[i].count;

    /* re-count, using this as an index below */
    map[i].count = 0;
  }

  /* assign corner_tri-edge users */
  for (i = 0; i < tris_num; i++) {
    for (int j = 3; j--;) {
      MeshElemMap *map_ele = &map[corner_verts[corner_tris[i][j]]];
      map_ele->indices[map_ele->count++] = i;
    }
  }

  *r_map = map;
  *r_mem = indices;
}

void BKE_mesh_origindex_map_create(MeshElemMap **r_map,
                                   int **r_mem,
                                   const int totsource,
                                   const int *final_origindex,
                                   const int totfinal)
{
  MeshElemMap *map = MEM_calloc_arrayN<MeshElemMap>(size_t(totsource), __func__);
  int *indices = MEM_malloc_arrayN<int>(size_t(totfinal), __func__);
  int *index_step;
  int i;

  /* count face users */
  for (i = 0; i < totfinal; i++) {
    if (final_origindex[i] != ORIGINDEX_NONE) {
      BLI_assert(final_origindex[i] < totsource);
      map[final_origindex[i]].count++;
    }
  }

  /* create offsets */
  index_step = indices;
  for (i = 0; i < totsource; i++) {
    map[i].indices = index_step;
    index_step += map[i].count;

    /* re-count, using this as an index below */
    map[i].count = 0;
  }

  /* Assign face-tessellation users. */
  for (i = 0; i < totfinal; i++) {
    if (final_origindex[i] != ORIGINDEX_NONE) {
      MeshElemMap *map_ele = &map[final_origindex[i]];
      map_ele->indices[map_ele->count++] = i;
    }
  }

  *r_map = map;
  *r_mem = indices;
}

void BKE_mesh_origindex_map_create_corner_tri(MeshElemMap **r_map,
                                              int **r_mem,
                                              const blender::OffsetIndices<int> faces,
                                              const int *corner_tri_faces,
                                              const int corner_tris_num)
{
  MeshElemMap *map = MEM_calloc_arrayN<MeshElemMap>(size_t(faces.size()), __func__);
  int *indices = MEM_malloc_arrayN<int>(size_t(corner_tris_num), __func__);
  int *index_step;

  /* create offsets */
  index_step = indices;
  for (const int64_t i : faces.index_range()) {
    map[i].indices = index_step;
    index_step += blender::bke::mesh::face_triangles_num(int(faces[i].size()));
  }

  /* Assign face-tessellation users. */
  for (int i = 0; i < corner_tris_num; i++) {
    MeshElemMap *map_ele = &map[corner_tri_faces[i]];
    map_ele->indices[map_ele->count++] = i;
  }

  *r_map = map;
  *r_mem = indices;
}

namespace blender::bke::mesh {

static Array<int> create_reverse_offsets(const Span<int> indices, const int items_num)
{
  Array<int> offsets(items_num + 1, 0);
  offset_indices::build_reverse_offsets(indices, offsets);
  return offsets;
}

static void sort_small_groups(const OffsetIndices<int> groups,
                              const int grain_size,
                              MutableSpan<int> indices)
{
  threading::parallel_for(groups.index_range(), grain_size, [&](const IndexRange range) {
    for (const int64_t index : range) {
      MutableSpan<int> group = indices.slice(groups[index]);
      std::sort(group.begin(), group.end());
    }
  });
}

static Array<int> reverse_indices_in_groups(const Span<int> group_indices,
                                            const OffsetIndices<int> offsets)
{
  if (group_indices.is_empty()) {
    return {};
  }
  BLI_assert(*std::max_element(group_indices.begin(), group_indices.end()) < offsets.size());
  BLI_assert(*std::min_element(group_indices.begin(), group_indices.end()) >= 0);

  /* `counts` keeps track of how many elements have been added to each group, and is incremented
   * atomically by many threads in parallel. `calloc` can be measurably faster than a parallel fill
   * of zero. Alternatively the offsets could be copied and incremented directly, but the cost of
   * the copy is slightly higher than the cost of `calloc`. */
  int *counts = MEM_calloc_arrayN<int>(size_t(offsets.size()), __func__);
  BLI_SCOPED_DEFER([&]() { MEM_freeN(counts); })
  Array<int> results(group_indices.size());
  threading::parallel_for(group_indices.index_range(), 1024, [&](const IndexRange range) {
    for (const int64_t i : range) {
      const int group_index = group_indices[i];
      const int index_in_group = atomic_fetch_and_add_int32(&counts[group_index], 1);
      results[offsets[group_index][index_in_group]] = int(i);
    }
  });
  sort_small_groups(offsets, 1024, results);
  return results;
}

/* A version of #reverse_indices_in_groups that stores face indices instead of corner indices. */
static void reverse_group_indices_in_groups(const OffsetIndices<int> groups,
                                            const Span<int> group_to_elem,
                                            const OffsetIndices<int> offsets,
                                            MutableSpan<int> results)
{
  int *counts = MEM_calloc_arrayN<int>(size_t(offsets.size()), __func__);
  BLI_SCOPED_DEFER([&]() { MEM_freeN(counts); })
  threading::parallel_for(groups.index_range(), 1024, [&](const IndexRange range) {
    for (const int64_t face : range) {
      for (const int elem : group_to_elem.slice(groups[face])) {
        const int index_in_group = atomic_fetch_and_add_int32(&counts[elem], 1);
        results[offsets[elem][index_in_group]] = int(face);
      }
    }
  });
  sort_small_groups(offsets, 1024, results);
}

static GroupedSpan<int> gather_groups(const Span<int> group_indices,
                                      const int groups_num,
                                      Array<int> &r_offsets,
                                      Array<int> &r_indices)
{
  r_offsets = create_reverse_offsets(group_indices, groups_num);
  r_indices = reverse_indices_in_groups(group_indices, r_offsets.as_span());
  return {OffsetIndices<int>(r_offsets), r_indices};
}

Array<int> build_corner_to_face_map(const OffsetIndices<int> faces)
{
  Array<int> map(faces.total_size());
  offset_indices::build_reverse_map(faces, map);
  return map;
}

GroupedSpan<int> build_vert_to_edge_map(const Span<int2> edges,
                                        const int verts_num,
                                        Array<int> &r_offsets,
                                        Array<int> &r_indices)
{
  r_offsets = create_reverse_offsets(edges.cast<int>(), verts_num);
  const OffsetIndices<int> offsets(r_offsets);
  r_indices.reinitialize(offsets.total_size());

  /* Version of #reverse_indices_in_groups that accounts for storing two indices for each edge. */
  int *counts = MEM_calloc_arrayN<int>(size_t(offsets.size()), __func__);
  BLI_SCOPED_DEFER([&]() { MEM_freeN(counts); })
  threading::parallel_for(edges.index_range(), 1024, [&](const IndexRange range) {
    for (const int64_t edge : range) {
      for (const int vert : {edges[edge][0], edges[edge][1]}) {
        const int index_in_group = atomic_fetch_and_add_int32(&counts[vert], 1);
        r_indices[offsets[vert][index_in_group]] = int(edge);
      }
    }
  });
  sort_small_groups(offsets, 1024, r_indices);
  return {offsets, r_indices};
}

void build_vert_to_face_indices(const OffsetIndices<int> faces,
                                const Span<int> corner_verts,
                                const OffsetIndices<int> offsets,
                                MutableSpan<int> face_indices)
{
  reverse_group_indices_in_groups(faces, corner_verts, offsets, face_indices);
}

GroupedSpan<int> build_vert_to_face_map(const OffsetIndices<int> faces,
                                        const Span<int> corner_verts,
                                        const int verts_num,
                                        Array<int> &r_offsets,
                                        Array<int> &r_indices)
{
  r_offsets = create_reverse_offsets(corner_verts, verts_num);
  r_indices.reinitialize(r_offsets.last());
  build_vert_to_face_indices(faces, corner_verts, OffsetIndices<int>(r_offsets), r_indices);
  return {OffsetIndices<int>(r_offsets), r_indices};
}

Array<int> build_vert_to_corner_indices(const Span<int> corner_verts,
                                        const OffsetIndices<int> offsets)
{
  return reverse_indices_in_groups(corner_verts, offsets);
}

GroupedSpan<int> build_vert_to_corner_map(const Span<int> corner_verts,
                                          const int verts_num,
                                          Array<int> &r_offsets,
                                          Array<int> &r_indices)
{
  return gather_groups(corner_verts, verts_num, r_offsets, r_indices);
}

GroupedSpan<int> build_edge_to_corner_map(const Span<int> corner_edges,
                                          const int edges_num,
                                          Array<int> &r_offsets,
                                          Array<int> &r_indices)
{
  return gather_groups(corner_edges, edges_num, r_offsets, r_indices);
}

GroupedSpan<int> build_edge_to_face_map(const OffsetIndices<int> faces,
                                        const Span<int> corner_edges,
                                        const int edges_num,
                                        Array<int> &r_offsets,
                                        Array<int> &r_indices)
{
  r_offsets = create_reverse_offsets(corner_edges, edges_num);
  r_indices.reinitialize(r_offsets.last());
  reverse_group_indices_in_groups(faces, corner_edges, OffsetIndices<int>(r_offsets), r_indices);
  return {OffsetIndices<int>(r_offsets), r_indices};
}

}  // namespace blender::bke::mesh

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh loops/face islands.
 * Used currently for UVs and 'smooth groups'.
 * \{ */

/**
 * Callback deciding whether the given face/loop/edge define an island boundary or not.
 */
using MeshRemap_CheckIslandBoundary =
    blender::FunctionRef<bool(int face_index,
                              int corner,
                              int edge_index,
                              int edge_user_count,
                              const blender::Span<int> edge_face_map_elem)>;

static void face_edge_loop_islands_calc_bitflags_exclude_at_boundary(
    const int *face_groups,
    const blender::Span<int> faces_from_item,
    const int face_group_id,
    const int face_group_id_overflowed,
    int &r_bit_face_group_mask)
{
  /* Find neighbor faces (either from a boundary edge, or a boundary vertex) that already have a
   * group assigned, and exclude these groups' bits from the available set of groups bits that can
   * be assigned to the currently processed group. */
  for (const int face_idx : faces_from_item) {
    int bit = face_groups[face_idx];
    if (!ELEM(bit, 0, face_group_id, face_group_id_overflowed) && !(r_bit_face_group_mask & bit)) {
      r_bit_face_group_mask |= bit;
    }
  }
}

/**
 * ABOUT #use_boundary_vertices_for_bitflags:
 *
 * Also exclude bits used in other groups sharing the same boundary vertex, i.e. if one edge
 * around the vertex of the current corner is a boundary edge.
 *
 * NOTE: The reason for this requirement is not very clear. Bit-flags groups are only handled here
 * for I/O purposes, Blender itself does not have this feature. Main external apps heavily
 * relying on these bit-flags groups for their smooth shading computation seem to generate invalid
 * results when two different groups share the same bits, and are connected by a vertex only
 * (i.e. have no edge in common). See #104434.
 *
 * The downside of also considering boundary vertex-only neighbor faces is that it becomes much
 * more likely to run out of bits, e.g. in a case of a fan with many faces/edges around a same
 * vertex, each in their own face group...
 */
static void face_edge_loop_islands_calc(const int totedge,
                                        const int totvert,
                                        const blender::OffsetIndices<int> faces,
                                        const blender::Span<int> corner_edges,
                                        const blender::Span<int> corner_verts,
                                        blender::GroupedSpan<int> edge_face_map,
                                        blender::GroupedSpan<int> vert_face_map,
                                        const bool use_bitflags,
                                        const bool use_boundary_vertices_for_bitflags,
                                        MeshRemap_CheckIslandBoundary edge_boundary_check,
                                        int **r_face_groups,
                                        int *r_totgroup,
                                        BLI_bitmap **r_edge_boundaries,
                                        int *r_totedgeboundaries)
{
  int *face_groups;
  int *face_stack;

  BLI_bitmap *edge_boundaries = nullptr;
  int num_edgeboundaries = 0;

  int face_prev = 0;
  constexpr int temp_face_group_id = 3; /* Placeholder value. */

  /* For bitflags groups, group we could not find any available bit for, will be reset to 0 at the
   * end. */
  constexpr int face_group_id_overflowed = 5;

  int tot_group = 0;
  bool group_id_overflow = false;

  if (faces.is_empty()) {
    *r_totgroup = 0;
    *r_face_groups = nullptr;
    if (r_edge_boundaries) {
      *r_edge_boundaries = nullptr;
      *r_totedgeboundaries = 0;
    }
    return;
  }

  if (r_edge_boundaries) {
    edge_boundaries = BLI_BITMAP_NEW(totedge, __func__);
    *r_totedgeboundaries = 0;
  }

  blender::Array<int> edge_to_face_src_offsets;
  blender::Array<int> edge_to_face_src_indices;
  if (edge_face_map.is_empty()) {
    edge_face_map = blender::bke::mesh::build_edge_to_face_map(
        faces, corner_edges, totedge, edge_to_face_src_offsets, edge_to_face_src_indices);
  }
  blender::Array<int> vert_to_face_src_offsets;
  blender::Array<int> vert_to_face_src_indices;
  if (use_bitflags && vert_face_map.is_empty()) {
    vert_face_map = blender::bke::mesh::build_vert_to_face_map(
        faces, corner_verts, totvert, vert_to_face_src_offsets, vert_to_face_src_indices);
  }

  face_groups = MEM_calloc_arrayN<int>(size_t(faces.size()), __func__);
  face_stack = MEM_malloc_arrayN<int>(size_t(faces.size()), __func__);

  while (true) {
    int face;
    int bit_face_group_mask = 0;
    int face_group_id;
    int ps_curr_idx = 0, ps_end_idx = 0; /* stack indices */

    for (face = face_prev; face < int(faces.size()); face++) {
      if (face_groups[face] == 0) {
        break;
      }
    }

    if (face == int(faces.size())) {
      /* all done */
      break;
    }

    face_group_id = use_bitflags ? temp_face_group_id : ++tot_group;

    /* start searching from here next time */
    face_prev = face + 1;

    face_groups[face] = face_group_id;
    face_stack[ps_end_idx++] = face;

    while (ps_curr_idx != ps_end_idx) {
      face = face_stack[ps_curr_idx++];
      BLI_assert(face_groups[face] == face_group_id);

      for (const int64_t loop : faces[face]) {
        const int edge = corner_edges[loop];
        /* loop over face users */
        const blender::Span<int> map_ele = edge_face_map[edge];
        const int *p = map_ele.data();
        int i = int(map_ele.size());
        if (!edge_boundary_check(face, int(loop), edge, i, map_ele)) {
          for (; i--; p++) {
            /* if we meet other non initialized its a bug */
            BLI_assert(ELEM(face_groups[*p], 0, face_group_id));

            if (face_groups[*p] == 0) {
              face_groups[*p] = face_group_id;
              face_stack[ps_end_idx++] = *p;
            }
          }
        }
        else {
          if (edge_boundaries && !BLI_BITMAP_TEST(edge_boundaries, edge)) {
            BLI_BITMAP_ENABLE(edge_boundaries, edge);
            num_edgeboundaries++;
          }
          if (use_bitflags) {
            /* Exclude bits used in other groups sharing the same boundary edge. */
            face_edge_loop_islands_calc_bitflags_exclude_at_boundary(face_groups,
                                                                     map_ele,
                                                                     face_group_id,
                                                                     face_group_id_overflowed,
                                                                     bit_face_group_mask);
            if (use_boundary_vertices_for_bitflags) {
              /* Exclude bits used in other groups sharing the same boundary vertex. */
              /* NOTE: Checking one vertex for each edge (the corner vertex) should be enough:
               *   - Thanks to winding, a fully boundary vertex (i.e. a vertex for which at least
               *     two of the adjacent edges in the same group are boundary ones) will be
               *     processed by at least one of the edges/corners. If not when processing the
               *     first face's corner, then when processing the other face's corner in the same
               *     group.
               *   - Isolated boundary edges (i.e. boundary edges only connected to faces of the
               *     same group) cannot be represented by bit-flags groups, at least not with
               *     current algorithm (they cannot define more than one group).
               *   - Inversions of winding (aka flipped faces) always generate boundary edges in
               *     current use-case (smooth groups), i.e. two faces with opposed winding cannot
               *     belong to the same group. */
              const int vert = corner_verts[loop];
              face_edge_loop_islands_calc_bitflags_exclude_at_boundary(face_groups,
                                                                       vert_face_map[vert],
                                                                       face_group_id,
                                                                       face_group_id_overflowed,
                                                                       bit_face_group_mask);
            }
          }
        }
      }
    }
    /* And now, we have all our face from current group in face_stack
     * (from 0 to (ps_end_idx - 1)),
     * as well as all smoothgroups bits we can't use in bit_face_group_mask.
     */
    if (use_bitflags) {
      int i, *p, gid_bit = 0;
      face_group_id = 1;

      /* Find first bit available! */
      for (; (face_group_id & bit_face_group_mask) && (gid_bit < 32); gid_bit++) {
        face_group_id <<= 1; /* will 'overflow' on last possible iteration. */
      }
      if (UNLIKELY(gid_bit > 31)) {
        /* All bits used in contiguous smooth groups, not much to do.
         *
         * NOTE: If only considering boundary edges, this is *very* unlikely to happen.
         * Theoretically, four groups are enough, this is probably not achievable with such a
         * simple algorithm, but 32 groups should always be more than enough.
         *
         * When also considering boundary vertices (which is the case currently, see comment
         * above), a fairly simple fan case with over 30 faces all belonging to different groups
         * will be enough to cause an overflow.
         */
        printf(
            "Warning, could not find an available id for current smooth group, faces will me "
            "marked "
            "as out of any smooth group...\n");

        /* Can't use 0, will have to set them to this value later. */
        face_group_id = face_group_id_overflowed;

        group_id_overflow = true;
      }
      tot_group = std::max(gid_bit, tot_group);
      /* And assign the final smooth group id to that face group! */
      for (i = ps_end_idx, p = face_stack; i--; p++) {
        face_groups[*p] = face_group_id;
      }
    }
  }

  if (use_bitflags) {
    /* used bits are zero-based. */
    tot_group++;
  }

  if (UNLIKELY(group_id_overflow)) {
    int i = int(faces.size()), *gid = face_groups;
    for (; i--; gid++) {
      if (*gid == face_group_id_overflowed) {
        *gid = 0;
      }
    }
    /* Using 0 as group id adds one more group! */
    tot_group++;
  }

  MEM_freeN(face_stack);

  *r_totgroup = tot_group;
  *r_face_groups = face_groups;
  if (r_edge_boundaries) {
    *r_edge_boundaries = edge_boundaries;
    *r_totedgeboundaries = num_edgeboundaries;
  }
}

static int *mesh_calc_smoothgroups(const int edges_num,
                                   const int verts_num,
                                   const blender::OffsetIndices<int> faces,
                                   const blender::Span<int> corner_edges,
                                   const blender::Span<int> corner_verts,
                                   const blender::Span<bool> sharp_edges,
                                   const blender::Span<bool> sharp_faces,
                                   int *r_totgroup,
                                   const bool use_bitflags,
                                   const bool use_boundary_vertices_for_bitflags)
{
  int *face_groups = nullptr;

  auto face_is_smooth = [&](const int i) { return sharp_faces.is_empty() || !sharp_faces[i]; };

  auto face_is_island_boundary_smooth = [&](const int face_index,
                                            const int /*corner*/,
                                            const int edge_index,
                                            const int edge_user_count,
                                            const blender::Span<int> edge_face_map_elem) {
    /* Edge is sharp if one of its faces is flat, or edge itself is sharp,
     * or edge is not used by exactly two faces. */
    if (face_is_smooth(face_index) && !(!sharp_edges.is_empty() && sharp_edges[edge_index]) &&
        (edge_user_count == 2))
    {
      /* In that case, edge appears to be smooth, but we need to check its other face too. */
      const int other_face_index = (face_index == edge_face_map_elem[0]) ? edge_face_map_elem[1] :
                                                                           edge_face_map_elem[0];
      return !face_is_smooth(other_face_index);
    }
    return true;
  };

  face_edge_loop_islands_calc(edges_num,
                              verts_num,
                              faces,
                              corner_edges,
                              corner_verts,
                              {},
                              {},
                              use_bitflags,
                              use_boundary_vertices_for_bitflags,
                              face_is_island_boundary_smooth,
                              &face_groups,
                              r_totgroup,
                              nullptr,
                              nullptr);

  return face_groups;
}

int *BKE_mesh_calc_smoothgroups(int edges_num,
                                const blender::OffsetIndices<int> faces,
                                const blender::Span<int> corner_edges,
                                const blender::Span<bool> sharp_edges,
                                const blender::Span<bool> sharp_faces,
                                int *r_totgroup)
{
  return mesh_calc_smoothgroups(
      edges_num, 0, faces, corner_edges, {}, sharp_edges, sharp_faces, r_totgroup, false, false);
}

int *BKE_mesh_calc_smoothgroups_bitflags(int edges_num,
                                         int verts_num,
                                         const blender::OffsetIndices<int> faces,
                                         const blender::Span<int> corner_edges,
                                         const blender::Span<int> corner_verts,
                                         const blender::Span<bool> sharp_edges,
                                         const blender::Span<bool> sharp_faces,
                                         const bool use_boundary_vertices_for_bitflags,
                                         int *r_totgroup)
{
  return mesh_calc_smoothgroups(edges_num,
                                verts_num,
                                faces,
                                corner_edges,
                                corner_verts,
                                sharp_edges,
                                sharp_faces,
                                r_totgroup,
                                true,
                                use_boundary_vertices_for_bitflags);
}

#define MISLAND_DEFAULT_BUFSIZE 64

void BKE_mesh_loop_islands_init(MeshIslandStore *island_store,
                                const short item_type,
                                const int items_num,
                                const short island_type,
                                const short innercut_type)
{
  MemArena *mem = island_store->mem;

  if (mem == nullptr) {
    mem = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
    island_store->mem = mem;
  }
  /* else memarena should be cleared */

  BLI_assert(
      ELEM(item_type, MISLAND_TYPE_VERT, MISLAND_TYPE_EDGE, MISLAND_TYPE_POLY, MISLAND_TYPE_LOOP));
  BLI_assert(ELEM(
      island_type, MISLAND_TYPE_VERT, MISLAND_TYPE_EDGE, MISLAND_TYPE_POLY, MISLAND_TYPE_LOOP));

  island_store->item_type = item_type;
  island_store->items_to_islands_num = items_num;
  island_store->items_to_islands = static_cast<int *>(
      BLI_memarena_alloc(mem, sizeof(*island_store->items_to_islands) * size_t(items_num)));

  island_store->island_type = island_type;
  island_store->islands_num_alloc = MISLAND_DEFAULT_BUFSIZE;
  island_store->islands = static_cast<MeshElemMap **>(
      BLI_memarena_alloc(mem, sizeof(*island_store->islands) * island_store->islands_num_alloc));

  island_store->innercut_type = innercut_type;
  island_store->innercuts = static_cast<MeshElemMap **>(
      BLI_memarena_alloc(mem, sizeof(*island_store->innercuts) * island_store->islands_num_alloc));
}

void BKE_mesh_loop_islands_clear(MeshIslandStore *island_store)
{
  island_store->item_type = MISLAND_TYPE_NONE;
  island_store->items_to_islands_num = 0;
  island_store->items_to_islands = nullptr;

  island_store->island_type = MISLAND_TYPE_NONE;
  island_store->islands_num = 0;
  island_store->islands = nullptr;

  island_store->innercut_type = MISLAND_TYPE_NONE;
  island_store->innercuts = nullptr;

  if (island_store->mem) {
    BLI_memarena_clear(island_store->mem);
  }

  island_store->islands_num_alloc = 0;
}

void BKE_mesh_loop_islands_free(MeshIslandStore *island_store)
{
  if (island_store->mem) {
    BLI_memarena_free(island_store->mem);
    island_store->mem = nullptr;
  }
}

void BKE_mesh_loop_islands_add(MeshIslandStore *island_store,
                               const int item_num,
                               const int *items_indices,
                               const int num_island_items,
                               int *island_item_indices,
                               const int num_innercut_items,
                               int *innercut_item_indices)
{
  MemArena *mem = island_store->mem;

  MeshElemMap *isld, *innrcut;
  const int curr_island_idx = island_store->islands_num++;
  const size_t curr_num_islands = size_t(island_store->islands_num);
  int i = item_num;

  while (i--) {
    island_store->items_to_islands[items_indices[i]] = curr_island_idx;
  }

  if (UNLIKELY(curr_num_islands > island_store->islands_num_alloc)) {
    MeshElemMap **islds, **innrcuts;

    island_store->islands_num_alloc *= 2;
    islds = static_cast<MeshElemMap **>(
        BLI_memarena_alloc(mem, sizeof(*islds) * island_store->islands_num_alloc));
    memcpy(islds, island_store->islands, sizeof(*islds) * (curr_num_islands - 1));
    island_store->islands = islds;

    innrcuts = static_cast<MeshElemMap **>(
        BLI_memarena_alloc(mem, sizeof(*innrcuts) * island_store->islands_num_alloc));
    memcpy(innrcuts, island_store->innercuts, sizeof(*innrcuts) * (curr_num_islands - 1));
    island_store->innercuts = innrcuts;
  }

  island_store->islands[curr_island_idx] = isld = static_cast<MeshElemMap *>(
      BLI_memarena_alloc(mem, sizeof(*isld)));
  isld->count = num_island_items;
  isld->indices = static_cast<int *>(
      BLI_memarena_alloc(mem, sizeof(*isld->indices) * size_t(num_island_items)));
  memcpy(isld->indices, island_item_indices, sizeof(*isld->indices) * size_t(num_island_items));

  island_store->innercuts[curr_island_idx] = innrcut = static_cast<MeshElemMap *>(
      BLI_memarena_alloc(mem, sizeof(*innrcut)));
  innrcut->count = num_innercut_items;
  innrcut->indices = static_cast<int *>(
      BLI_memarena_alloc(mem, sizeof(*innrcut->indices) * size_t(num_innercut_items)));
  memcpy(innrcut->indices,
         innercut_item_indices,
         sizeof(*innrcut->indices) * size_t(num_innercut_items));
}

static bool mesh_calc_islands_loop_face_uv(const int totedge,
                                           const blender::Span<bool> uv_seams,
                                           const blender::OffsetIndices<int> faces,
                                           const blender::Span<int> corner_edges,
                                           MeshIslandStore *r_island_store)
{
  using namespace blender;
  int *face_groups = nullptr;
  int num_face_groups;

  int *face_indices;
  int *loop_indices;
  int num_pidx, num_lidx;

  /* Those are used to detect 'inner cuts', i.e. edges that are boundaries,
   * and yet have two or more faces of a same group using them
   * (typical case: seam used to unwrap properly a cylinder). */
  BLI_bitmap *edge_boundaries = nullptr;
  int num_edge_boundaries = 0;
  char *edge_boundary_count = nullptr;
  int *edge_innercut_indices = nullptr;
  int num_einnercuts = 0;

  int grp_idx;

  BKE_mesh_loop_islands_clear(r_island_store);
  BKE_mesh_loop_islands_init(r_island_store,
                             MISLAND_TYPE_LOOP,
                             int(corner_edges.size()),
                             MISLAND_TYPE_POLY,
                             MISLAND_TYPE_EDGE);

  Array<int> edge_to_face_offsets;
  Array<int> edge_to_face_indices;
  const GroupedSpan<int> edge_to_face_map = bke::mesh::build_edge_to_face_map(
      faces, corner_edges, totedge, edge_to_face_offsets, edge_to_face_indices);

  /* TODO: I'm not sure edge seam flag is enough to define UV islands?
   *       Maybe we should also consider UV-maps values
   *       themselves (i.e. different UV-edges for a same mesh-edge => boundary edge too?).
   *       Would make things much more complex though,
   *       and each UVMap would then need its own mesh mapping, not sure we want that at all!
   */
  auto mesh_check_island_boundary_uv = [&](const int /*face_index*/,
                                           const int /*corner*/,
                                           const int edge_index,
                                           const int /*edge_user_count*/,
                                           const Span<int> /*edge_face_map_elem*/) -> bool {
    /* Edge is UV boundary if tagged as seam. */
    return !uv_seams.is_empty() && uv_seams[edge_index];
  };

  face_edge_loop_islands_calc(totedge,
                              0,
                              faces,
                              corner_edges,
                              {},
                              edge_to_face_map,
                              {},
                              false,
                              false,
                              mesh_check_island_boundary_uv,
                              &face_groups,
                              &num_face_groups,
                              &edge_boundaries,
                              &num_edge_boundaries);

  if (!num_face_groups) {
    if (num_edge_boundaries) {
      MEM_freeN(edge_boundaries);
    }
    return false;
  }

  if (num_edge_boundaries) {
    edge_boundary_count = MEM_malloc_arrayN<char>(size_t(totedge), __func__);
    edge_innercut_indices = MEM_malloc_arrayN<int>(size_t(num_edge_boundaries), __func__);
  }

  face_indices = MEM_malloc_arrayN<int>(size_t(faces.size()), __func__);
  loop_indices = MEM_malloc_arrayN<int>(size_t(corner_edges.size()), __func__);

  /* NOTE: here we ignore '0' invalid group - this should *never* happen in this case anyway? */
  for (grp_idx = 1; grp_idx <= num_face_groups; grp_idx++) {
    num_pidx = num_lidx = 0;
    if (num_edge_boundaries) {
      num_einnercuts = 0;
      memset(edge_boundary_count, 0, sizeof(*edge_boundary_count) * size_t(totedge));
    }

    for (const int64_t p_idx : faces.index_range()) {
      if (face_groups[p_idx] != grp_idx) {
        continue;
      }
      face_indices[num_pidx++] = int(p_idx);
      for (const int64_t corner : faces[p_idx]) {
        const int edge_i = corner_edges[corner];
        loop_indices[num_lidx++] = int(corner);
        if (num_edge_boundaries && BLI_BITMAP_TEST(edge_boundaries, edge_i) &&
            (edge_boundary_count[edge_i] < 2))
        {
          edge_boundary_count[edge_i]++;
          if (edge_boundary_count[edge_i] == 2) {
            edge_innercut_indices[num_einnercuts++] = edge_i;
          }
        }
      }
    }

    BKE_mesh_loop_islands_add(r_island_store,
                              num_lidx,
                              loop_indices,
                              num_pidx,
                              face_indices,
                              num_einnercuts,
                              edge_innercut_indices);
  }

  MEM_freeN(face_indices);
  MEM_freeN(loop_indices);
  MEM_freeN(face_groups);

  if (num_edge_boundaries) {
    MEM_freeN(edge_boundaries);
  }

  if (num_edge_boundaries) {
    MEM_freeN(edge_boundary_count);
    MEM_freeN(edge_innercut_indices);
  }
  return true;
}

bool BKE_mesh_calc_islands_loop_face_edgeseam(const blender::Span<blender::float3> vert_positions,
                                              const blender::Span<blender::int2> edges,
                                              const blender::Span<bool> uv_seams,
                                              const blender::OffsetIndices<int> faces,
                                              const blender::Span<int> /*corner_verts*/,
                                              const blender::Span<int> corner_edges,
                                              MeshIslandStore *r_island_store)
{
  UNUSED_VARS(vert_positions);
  return mesh_calc_islands_loop_face_uv(
      int(edges.size()), uv_seams, faces, corner_edges, r_island_store);
}

/** \} */
