/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup geo
 */

// #define USE_WELD_DEBUG
// #define USE_WELD_DEBUG_TIME

#include "BKE_attribute_math.hh"
#include "BLI_array.hh"
#include "BLI_array_utils.hh"
#include "BLI_index_mask.hh"
#include "BLI_kdtree_new.hh"
#include "BLI_listbase.hh"
#include "BLI_math_vector_c.hh"
#include "BLI_offset_indices.hh"
#include "BLI_ordered_edge.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "BKE_attribute.hh"
#include "BKE_attribute_filters.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_mesh.hh"
#include "DNA_meshdata_types.h"

#include "DNA_object_types.h"
#include "GEO_mesh_merge_verts.hh"
#include "GEO_randomize.hh"

#ifdef USE_WELD_DEBUG_TIME
#  include "BLI_timeit.hh"

#  if WIN32 and NDEBUG
#    pragma optimize("t", on)
#  endif
#endif

namespace blender::geometry {

/* Indicates when the element was not computed. */
#define OUT_OF_CONTEXT int(-1)
/* Indicates if the edge or face will be collapsed. */
#define ELEM_COLLAPSED int(-2)
/* indicates whether an edge or vertex in groups_map will be merged. */
#define ELEM_MERGED int(-2)
/* Indicates that a face is a duplicate of another face after merging. */
#define ELEM_DUPLICATE int(-3)

struct WeldFace {
  /**
   * #OUT_OF_CONTEXT if the face remains in the result, #ELEM_COLLAPSED if all of its corners
   * collapse, or #ELEM_DUPLICATE if it's a duplicate of another face.
   */
  int face_dst;
  /* Indices in to the source Mesh. */
  int face_src;
  /** The first and last corner of the face, see #WeldMesh::corner_next. */
  int corner_start;
  int corner_end;
  /** The number of remaining corners. */
  int corners_num;
};

/** The number of faces and corners removed by merging. */
struct RemovedFacesAndCorners {
  int faces = 0;
  int corners = 0;

  friend RemovedFacesAndCorners operator+(const RemovedFacesAndCorners &a,
                                          const RemovedFacesAndCorners &b)
  {
    return {a.faces + b.faces, a.corners + b.corners};
  }
};

struct WeldMesh {
  /* Group of edges to be merged. */
  Array<int> edge_src_to_target;
  Span<int> vert_src_to_target;
  /* Vertices that merge into another vertex, and the vertices they merge into. */
  Span<bool> vert_affected;

  /* References all faces that will be affected. */
  Vector<WeldFace> weld_faces;
  int new_faces_num;

  /**
   * The next corner of each corner of affected faces, initially the next corner in the
   * source face. Corners are collapsed by relinking the previous corner, and faces that
   * contain the same vertex multiple times after merging are split into separate cycles.
   * Uninitialized for faces that aren't affected by the merge.
   */
  Array<int> corner_next;

  Array<int> face_to_weld_face;

  int removed_verts_num;
  int removed_edges_num;
  int removed_corners_num;
  int removed_faces_num; /* Including the new faces. */

#ifdef USE_WELD_DEBUG
  Span<int> corner_verts;
  Span<int> corner_edges;
  OffsetIndices<int> faces;
#endif
};

template<typename Fn>
static void foreach_weld_face_corner(const WeldFace &weld_face,
                                     const Span<int> corner_next,
                                     const Fn &fn)
{
  BLI_assert(weld_face.face_dst == OUT_OF_CONTEXT);
  int corner = weld_face.corner_start;
  do {
    fn(corner);
    corner = corner_next[corner];
  } while (corner != weld_face.corner_start);
}

/* -------------------------------------------------------------------- */
/** \name Debug Utils
 * \{ */

#ifdef USE_WELD_DEBUG
static void weld_assert_removed_edges_num(Span<int> edge_src_to_target,
                                          const int expected_removed_num)
{
  int kills = 0;
  for (const int edge_src : edge_src_to_target.index_range()) {
    if (edge_src_to_target[edge_src] != edge_src) {
      kills++;
    }
  }
  BLI_assert(kills == expected_removed_num);
}

static void weld_assert_removed_faces_and_corners_num(const WeldMesh &weld_mesh,
                                                      const int expected_removed_faces_num,
                                                      const int expected_removed_corners_num)
{
  const OffsetIndices<int> faces = weld_mesh.faces;
  int removed_faces = 0;
  int remaining_corners = 0;
  for (const int i : faces.index_range()) {
    if (weld_mesh.face_to_weld_face[i] == OUT_OF_CONTEXT) {
      remaining_corners += faces[i].size();
    }
  }
  for (const WeldFace &weld_face : weld_mesh.weld_faces) {
    if (weld_face.face_dst != OUT_OF_CONTEXT) {
      removed_faces++;
      continue;
    }
    foreach_weld_face_corner(
        weld_face, weld_mesh.corner_next, [&](const int /*corner*/) { remaining_corners++; });
  }
  BLI_assert(removed_faces == expected_removed_faces_num);
  BLI_assert(weld_mesh.corner_verts.size() - remaining_corners == expected_removed_corners_num);
}

static void weld_assert_face_no_vert_repetition(const WeldFace &weld_face,
                                                const WeldMesh &weld_mesh)
{
  if (weld_face.face_dst != OUT_OF_CONTEXT) {
    return;
  }
  Vector<int> verts;
  foreach_weld_face_corner(weld_face, weld_mesh.corner_next, [&](const int corner) {
    verts.append(weld_mesh.vert_src_to_target[weld_mesh.corner_verts[corner]]);
  });
  BLI_assert(verts.size() == weld_face.corners_num);
  for (const int i : verts.index_range()) {
    for (const int j : verts.index_range().drop_front(i + 1)) {
      BLI_assert(verts[i] != verts[j]);
    }
  }
}

#endif /* USE_WELD_DEBUG */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vert API
 * \{ */

/**
 * Find the vertices affected by the merge: the vertices that merge into another vertex, and the
 * vertices they merge into. The topology collapsing code can skip work for everything else.
 */
static Array<bool> find_affected_verts(const Span<int> vert_src_to_target)
{
  Array<bool> affected(vert_src_to_target.size(), false);
  threading::parallel_for(vert_src_to_target.index_range(), 4096, [&](const IndexRange range) {
    for (const int vert : range) {
      const int vert_target = vert_src_to_target[vert];
      if (vert_target != vert) {
        BLI_assert(vert_src_to_target[vert_target] == vert_target);
        affected[vert] = true;
        /* All threads will store the same "true" value. */
        affected[vert_target] = true;
      }
    }
  });
  return affected;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edge API
 * \{ */

/**
 * Find edges that collapse because both of their vertices merge into the same vertex, and the
 * other edges affected by the merge (the "weld edges").
 *
 * \param r_edge_src_to_target: Filled with #ELEM_COLLAPSED for collapsed edges, and the edge index
 * itself for all other edges.
 * \return The weld edges.
 */
static IndexMask find_collapsed_and_weld_edges(const Span<int2> edges,
                                               const Span<int> vert_src_to_target,
                                               const Span<bool> vert_affected,
                                               IndexMaskMemory &memory,
                                               MutableSpan<int> r_edge_src_to_target,
                                               int *r_collapsed_edges_num)
{
  const IndexMask affected_edges = IndexMask::from_predicate(
      edges.index_range(),
      memory,
      [&](const int edge) {
        return vert_affected[edges[edge][0]] || vert_affected[edges[edge][1]];
      },
      exec_mode::grain_size(4096));
  const IndexMask weld_edges = IndexMask::from_predicate(
      affected_edges,
      memory,
      [&](const int edge) {
        return vert_src_to_target[edges[edge][0]] != vert_src_to_target[edges[edge][1]];
      },
      exec_mode::grain_size(4096));
  const IndexMask collapsed_edges = IndexMask::from_difference(affected_edges, weld_edges, memory);

  array_utils::fill_index_range<int>(edges.index_range(), r_edge_src_to_target);
  index_mask::masked_fill(r_edge_src_to_target, ELEM_COLLAPSED, collapsed_edges);
  *r_collapsed_edges_num = collapsed_edges.size();
  return weld_edges;
}

/**
 * Find weld edges that connect the same two vertices after merging. The edge with the lowest index
 * in each group of duplicates is kept, and the others are mapped to it in \a r_edge_src_to_target.
 *
 * \return The number of duplicate edges.
 */
static int find_duplicate_edges(const Span<int2> edges,
                                const Span<int> vert_src_to_target,
                                const IndexMask &weld_edges,
                                const int verts_num,
                                MutableSpan<int> r_edge_src_to_target)
{
  if (weld_edges.is_empty()) {
    return 0;
  }

  Array<int> edge_indices(weld_edges.size());
  Array<int> low_verts(weld_edges.size());
  Array<int> high_verts(weld_edges.size());
  weld_edges.foreach_index(
      [&](const int edge, const int pos) {
        const OrderedEdge verts(vert_src_to_target[edges[edge][0]],
                                vert_src_to_target[edges[edge][1]]);
        edge_indices[pos] = edge;
        low_verts[pos] = verts.v_low;
        high_verts[pos] = verts.v_high;
      },
      exec_mode::grain_size(4096));

  /* Duplicate edges share their lower vertex, so only edges in the same group can be duplicates.
   */
  Array<int> offset_data;
  Array<int> index_data;
  const OffsetIndices edges_by_low_vert = offset_indices::build_groups_from_indices(
                                              low_verts, verts_num, offset_data, index_data)
                                              .offsets;

  return threading::parallel_reduce(
      edges_by_low_vert.index_range(),
      1024,
      0,
      [&](const IndexRange range, int duplicates_num) {
        for (const int vert : range) {
          MutableSpan<int> group = index_data.as_mutable_span().slice(edges_by_low_vert[vert]);
          if (group.size() < 2) {
            continue;
          }
          /* After sorting by the higher vertex, duplicates are next to each other, and the first
           * of each group of duplicates has the lowest index. */
          std::ranges::sort(group, [&](const int a, const int b) {
            return std::pair(high_verts[a], a) < std::pair(high_verts[b], b);
          });
          int first = group[0];
          for (const int weld_edge : group.drop_front(1)) {
            if (high_verts[weld_edge] != high_verts[first]) {
              first = weld_edge;
              continue;
            }
            r_edge_src_to_target[edge_indices[weld_edge]] = edge_indices[first];
            duplicates_num++;
          }
        }
        return duplicates_num;
      },
      std::plus<>());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Poly and Loop API
 * \{ */

/**
 * Build the context weld faces and the corner links for them.
 *
 * \return r_weld_mesh: Corner and face members will be allocated here.
 */
static void weld_face_corner_ctx_alloc(const OffsetIndices<int> faces,
                                       const Span<int> corner_verts,
                                       WeldMesh *r_weld_mesh)
{
  const Span<bool> vert_affected = r_weld_mesh->vert_affected;

  IndexMaskMemory memory;
  const IndexMask affected_faces = IndexMask::from_predicate(
      faces.index_range(),
      memory,
      [&](const int face) {
        return std::ranges::any_of(corner_verts.slice(faces[face]),
                                   [&](const int vert) { return vert_affected[vert]; });
      },
      exec_mode::grain_size(1024));

  /* Estimate the maximum number of new faces created by splitting faces that contain the same
   * vertex multiple times after merging. We could be smarter here and actually count how many new
   * faces will be created. But counting this can be inefficient as it depends on the number of
   * non-consecutive self face merges. */
  const int maybe_new_faces_num = threading::parallel_reduce(
      affected_faces.index_range(),
      1024,
      0,
      [&](const IndexRange range, int maybe_new_faces_num) {
        affected_faces.slice(range).foreach_index([&](const int face_index) {
          const IndexRange face = faces[face_index];
          if (face.size() <= 5) {
            return;
          }
          /* Corners are affected by the merge when their vertex or the next vertex merges. */
          int corner_ctx_num = 0;
          for (const int corner : face) {
            const int corner_next = bke::mesh::face_corner_next(face, corner);
            if (vert_affected[corner_verts[corner]] || vert_affected[corner_verts[corner_next]]) {
              corner_ctx_num++;
            }
          }
          if (corner_ctx_num > 1) {
            maybe_new_faces_num += std::min(int(face.size() / 3), corner_ctx_num) - 1;
          }
        });
        return maybe_new_faces_num;
      },
      std::plus<>());

  Array<int> corner_next(corner_verts.size());
  Array<int> face_to_weld_face(faces.size());
  Vector<WeldFace> weld_faces;
  weld_faces.reserve(affected_faces.size() + maybe_new_faces_num);
  weld_faces.resize(affected_faces.size());

  affected_faces.foreach_index(
      [&](const int face_index, const int weld_face_index) {
        const IndexRange face = faces[face_index];
        for (const int corner : face.drop_back(1)) {
          corner_next[corner] = corner + 1;
        }
        corner_next[face.last()] = face.first();

        WeldFace &weld_face = weld_faces[weld_face_index];
        weld_face.face_dst = OUT_OF_CONTEXT;
        weld_face.face_src = face_index;
        weld_face.corner_start = face.first();
        weld_face.corner_end = face.last();
        weld_face.corners_num = face.size();
        face_to_weld_face[face_index] = weld_face_index;
      },
      exec_mode::grain_size(1024));
  index_mask::masked_fill(face_to_weld_face.as_mutable_span(),
                          OUT_OF_CONTEXT,
                          affected_faces.complement(faces.index_range(), memory));

  r_weld_mesh->weld_faces = std::move(weld_faces);
  r_weld_mesh->new_faces_num = 0;
  r_weld_mesh->corner_next = std::move(corner_next);
  r_weld_mesh->face_to_weld_face = std::move(face_to_weld_face);
}

/** Split faces that contain the same vertex multiple times after merging into separate faces. */
static void weld_face_split_recursive(int face_size,
                                      const Span<int> corner_verts,
                                      WeldFace *r_wp,
                                      WeldMesh *r_weld_mesh,
                                      RemovedFacesAndCorners &r_removed)
{
  if (face_size < 3) {
    return;
  }

  const Span<int> vert_src_to_target = r_weld_mesh->vert_src_to_target;
  const Span<bool> vert_affected = r_weld_mesh->vert_affected;
  MutableSpan<int> corner_next = r_weld_mesh->corner_next;

  int removed_corners_num = 0;

  int corner_end = r_wp->corner_end;
  int corner_a_prev = corner_end;
  int corner_a = r_wp->corner_start;
  do {
    const int vert_a = vert_src_to_target[corner_verts[corner_a]];
    if (!vert_affected[vert_a]) {
      /* Only test vertices that will be merged. */
      corner_a_prev = corner_a;
      corner_a = corner_next[corner_a];
      continue;
    }

    int dist_a = 1;
    int lb_prev = corner_a;
    int corner_b = corner_next[corner_a];
    do {
      const int vert_b = vert_src_to_target[corner_verts[corner_b]];
      if (vert_a != vert_b) {
        dist_a++;
        lb_prev = corner_b;
        corner_b = corner_next[corner_b];
        continue;
      }

      int dist_b = face_size - dist_a;

      BLI_assert(dist_a != 0 && dist_b != 0);
      if (dist_a == 1 || dist_b == 1) {
        BLI_assert(dist_a != dist_b);
      }
      else if (dist_a == 2 && dist_b == 2) {
        /* All corners are "collapsed". */
        removed_corners_num += 4;
        dist_b = 0;
        r_wp->face_dst = ELEM_COLLAPSED;
        r_removed.faces += 1;
        r_removed.corners += removed_corners_num;
        /* Since all the corners are collapsed, avoid iterating through them.
         * This may result in wrong removed_faces_num counts. */
        return;
      }
      else {
        corner_next[corner_a_prev] = corner_b;
        corner_next[lb_prev] = corner_a;
        if (r_wp->corner_start == corner_a) {
          r_wp->corner_start = corner_b;
        }

        if (dist_a == 2) {
          removed_corners_num += 2;
        }
        else if (dist_b == 2) {
          removed_corners_num += 2;

          r_wp->corner_start = corner_a;
          r_wp->corner_end = corner_end = lb_prev;

          face_size = dist_a;
          break;
        }
        else {
          r_weld_mesh->weld_faces.increase_size_by_unchecked(1);
          r_weld_mesh->new_faces_num++;

          WeldFace *new_test = &r_weld_mesh->weld_faces.last();
          new_test->face_dst = OUT_OF_CONTEXT;
          new_test->face_src = r_wp->face_src;
          new_test->corner_start = corner_a;
          new_test->corner_end = lb_prev;
          new_test->corners_num = dist_a;
          weld_face_split_recursive(dist_a, corner_verts, new_test, r_weld_mesh, r_removed);
        }

        corner_a = corner_b;
        face_size = dist_b;

        dist_a = 1;
      }

      lb_prev = corner_b;
      corner_b = corner_next[corner_b];
    } while (lb_prev != corner_end);

    corner_a_prev = corner_a;
    if (corner_a == corner_end) {
      /* No need to start again. */
      break;
    }
    corner_a = corner_next[corner_a];
  } while (corner_a != corner_end);

  r_removed.corners += removed_corners_num;
  r_wp->corners_num = face_size;
#ifdef USE_WELD_DEBUG
  weld_assert_face_no_vert_repetition(*r_wp, *r_weld_mesh);
#endif
}

/**
 * Remove the corners of collapsed edges from a weld face by relinking the remaining corners.
 * \return The number of remaining corners, or zero if the whole face collapses.
 */
static int collapse_weld_face(const Span<int> corner_edges,
                              const Span<int> edge_src_to_target,
                              WeldFace &weld_face,
                              MutableSpan<int> corner_next,
                              RemovedFacesAndCorners &r_removed)
{
  int face_size = (weld_face.corner_end - weld_face.corner_start) + 1;
  int corner_prev = -1;
  bool changed_corner_start = false;
  int corner = weld_face.corner_start;
  do {
    if (edge_src_to_target[corner_edges[corner]] == ELEM_COLLAPSED) {
      if (face_size == 3) {
        weld_face.face_dst = ELEM_COLLAPSED;
        r_removed.faces++;
        r_removed.corners += 3;
        return 0;
      }

      if (corner == weld_face.corner_start) {
        changed_corner_start = true;
      }

      r_removed.corners++;
      face_size--;
    }
    else {
      if (changed_corner_start) {
        weld_face.corner_start = corner;
        changed_corner_start = false;
      }
      if (corner_prev != -1) {
        corner_next[corner_prev] = corner;
      }
      corner_prev = corner;
    }
  } while (corner++ != weld_face.corner_end);

  corner_next[corner_prev] = weld_face.corner_start;
  weld_face.corner_end = corner_prev;
  weld_face.corners_num = face_size;
  return face_size;
}

/**
 * Remove corners for collapsed edges from the weld faces, and split faces that contain the same
 * vertex multiple times.
 *
 * \param remaining_edge_ctx_num: Context weld edges that won't be destroyed by merging.
 */
static void weld_face_corner_ctx_setup_collapsed_and_split(const Span<int> corner_verts,
                                                           const Span<int> corner_edges,
                                                           const int remaining_edge_ctx_num,
                                                           WeldMesh *r_weld_mesh)
{
  if (remaining_edge_ctx_num == 0) {
    int removed_corners_num = 0;
    for (WeldFace &weld_face : r_weld_mesh->weld_faces) {
      removed_corners_num += weld_face.corners_num;
      weld_face.face_dst = ELEM_COLLAPSED;
    }
    r_weld_mesh->removed_faces_num = r_weld_mesh->weld_faces.size();
    r_weld_mesh->removed_corners_num = removed_corners_num;
    return;
  }

  /* Splitting adds new faces at the end of the vector, which has enough space reserved already, so
   * the existing faces aren't reallocated. Only the faces that already exist are visited here. */
  WeldFace *weld_faces = r_weld_mesh->weld_faces.data();
  const IndexRange weld_faces_src_range = r_weld_mesh->weld_faces.index_range();
  const Span<int> edge_src_to_target = r_weld_mesh->edge_src_to_target;
  MutableSpan<int> corner_next = r_weld_mesh->corner_next;

  /* Only faces with at least 6 corners can be split into two new faces with at least 3 corners.
   * All other faces are processed in parallel. Faces that may be split are split afterwards on a
   * single thread, so the new faces are added in a deterministic order. */
  constexpr int min_split_face_size = 6;
  RemovedFacesAndCorners removed = threading::parallel_reduce(
      weld_faces_src_range,
      1024,
      RemovedFacesAndCorners(),
      [&](const IndexRange range, RemovedFacesAndCorners removed) {
        for (const int i : range) {
          const int face_size = collapse_weld_face(
              corner_edges, edge_src_to_target, weld_faces[i], corner_next, removed);
          if (face_size > 0 && face_size < min_split_face_size) {
            weld_face_split_recursive(
                face_size, corner_verts, &weld_faces[i], r_weld_mesh, removed);
          }
        }
        return removed;
      },
      std::plus<>());

  for (const int i : weld_faces_src_range) {
    WeldFace &weld_face = weld_faces[i];
    if (weld_face.face_dst == OUT_OF_CONTEXT && weld_face.corners_num >= min_split_face_size) {
      weld_face_split_recursive(
          weld_face.corners_num, corner_verts, &weld_face, r_weld_mesh, removed);
    }
  }

  r_weld_mesh->removed_faces_num = removed.faces;
  r_weld_mesh->removed_corners_num = removed.corners;

#ifdef USE_WELD_DEBUG
  weld_assert_removed_faces_and_corners_num(
      *r_weld_mesh, r_weld_mesh->removed_faces_num, r_weld_mesh->removed_corners_num);
#endif
}

static void weld_face_find_doubles(const Span<int> corner_verts, WeldMesh *r_weld_mesh)
{
  if (r_weld_mesh->removed_faces_num == r_weld_mesh->weld_faces.size()) {
    return;
  }

  MutableSpan<WeldFace> weld_faces = r_weld_mesh->weld_faces;
  const Span<int> vert_src_to_target = r_weld_mesh->vert_src_to_target;
  const Span<int> corner_next = r_weld_mesh->corner_next;

  IndexMaskMemory memory;
  const IndexMask remaining_faces = IndexMask::from_predicate(
      weld_faces.index_range(),
      memory,
      [&](const int i) { return weld_faces[i].face_dst == OUT_OF_CONTEXT; },
      exec_mode::grain_size(4096));

  /* Gather the vertices of the remaining faces after merging. */
  Array<int> face_offset_data(weld_faces.size() + 1, 0);
  remaining_faces.foreach_index(
      [&](const int i) { face_offset_data[i] = weld_faces[i].corners_num; },
      exec_mode::grain_size(4096));
  const OffsetIndices face_offsets = offset_indices::accumulate_counts_to_offsets(
      face_offset_data);
  Array<int> face_verts(face_offsets.total_size());
  remaining_faces.foreach_index(
      [&](const int i) {
        int *vert = face_verts.as_mutable_span().slice(face_offsets[i]).data();
        foreach_weld_face_corner(weld_faces[i], corner_next, [&](const int corner) {
          *vert++ = vert_src_to_target[corner_verts[corner]];
        });
      },
      exec_mode::grain_size(1024));

  const IndexMask duplicate_faces = bke::find_duplicate_faces(
      face_offsets, face_verts, remaining_faces, memory);
  duplicate_faces.foreach_index([&](const int i) { weld_faces[i].face_dst = ELEM_DUPLICATE; },
                                exec_mode::grain_size(4096));

  r_weld_mesh->removed_faces_num += duplicate_faces.size();
  r_weld_mesh->removed_corners_num += offset_indices::sum_group_sizes(face_offsets,
                                                                      duplicate_faces);

#ifdef USE_WELD_DEBUG
  weld_assert_removed_faces_and_corners_num(
      *r_weld_mesh, r_weld_mesh->removed_faces_num, r_weld_mesh->removed_corners_num);
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh API
 * \{ */

/**
 * \param vert_affected: The vertices that are part of the merge (see #find_affected_verts).
 */
static void weld_mesh_context_create(const Mesh &mesh,
                                     const Span<int> vert_src_to_target,
                                     const Span<bool> vert_affected,
                                     const int removed_verts_num,
                                     WeldMesh *r_weld_mesh)
{
  PRF_scope(ProfileCategory::Default);
  const Span<int2> edges = mesh.edges();
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<int> corner_edges = mesh.corner_edges();

  r_weld_mesh->removed_verts_num = removed_verts_num;

  r_weld_mesh->edge_src_to_target.reinitialize(edges.size());
  r_weld_mesh->vert_src_to_target = vert_src_to_target;
  r_weld_mesh->vert_affected = vert_affected;

#ifdef USE_WELD_DEBUG
  r_weld_mesh->corner_verts = corner_verts;
  r_weld_mesh->corner_edges = corner_edges;
  r_weld_mesh->faces = faces;
#endif

  IndexMaskMemory memory;
  int collapsed_edges_num;
  const IndexMask weld_edges = find_collapsed_and_weld_edges(edges,
                                                             vert_src_to_target,
                                                             vert_affected,
                                                             memory,
                                                             r_weld_mesh->edge_src_to_target,
                                                             &collapsed_edges_num);
  const int removed_double_edges_num = find_duplicate_edges(
      edges, vert_src_to_target, weld_edges, mesh.verts_num, r_weld_mesh->edge_src_to_target);

  r_weld_mesh->removed_edges_num = collapsed_edges_num + removed_double_edges_num;

#ifdef USE_WELD_DEBUG
  weld_assert_removed_edges_num(r_weld_mesh->edge_src_to_target, r_weld_mesh->removed_edges_num);
#endif

  weld_face_corner_ctx_alloc(faces, corner_verts, r_weld_mesh);

  weld_face_corner_ctx_setup_collapsed_and_split(
      corner_verts, corner_edges, weld_edges.size() - removed_double_edges_num, r_weld_mesh);

  weld_face_find_doubles(corner_verts, r_weld_mesh);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Merging
 * \{ */

/**
 * The elements that are kept in the result. In other words, the merge targets and also the "out of
 * context" elements.
 */
static IndexMask merge_survivors(const Span<int> src_to_target, IndexMaskMemory &memory)
{
  return IndexMask::from_predicate(
      src_to_target.index_range(), memory, [&](const int i) { return src_to_target[i] == i; });
}

struct MergePropagationMap {
  /** A source element for every result element, used to copy values. */
  Array<int> dst_to_src;
  /** Result elements created from more than one source element. */
  IndexMask mixed;
  /** The source elements for each element in #mixed, starting with the element in #dst_to_src. */
  Array<int> mixed_offsets;
  Array<int> mixed_indices;

  GroupedSpan<int> mixed_groups() const
  {
    return {OffsetIndices<int>(mixed_offsets), mixed_indices};
  }
};

/**
 * \param src_to_target: The target element each element merges into. Elements that are removed
 * entirely (collapsed edges) have negative values.
 * \param survivors: The elements kept in the result (see #merge_survivors).
 * \param do_mix_data: Build groups for mixing attribute values from all of an element's source
 * elements. Otherwise just the value of the target element is used.
 */
static MergePropagationMap merge_propagation_map(const Span<int> src_to_target,
                                                 const IndexMask &survivors,
                                                 const Span<int> src_to_dst,
                                                 const bool do_mix_data,
                                                 IndexMaskMemory &memory)
{
  PRF_scope(ProfileCategory::Default);
  MergePropagationMap map;
  map.dst_to_src.reinitialize(survivors.size());
  survivors.to_indices(map.dst_to_src.as_mutable_span());
  if (!do_mix_data) {
    return map;
  }

  /* Elements that merge into another element. */
  const IndexMask merged = IndexMask::from_predicate(
      src_to_target.index_range(), memory, [&](const int i) {
        return src_to_target[i] != i && src_to_target[i] >= 0;
      });
  if (merged.is_empty()) {
    return map;
  }

  /* Only the merged elements are grouped by their result element here, since grouping the
   * targets as well is significantly slower than inserting them afterwards. */
  Array<int> merged_dst(merged.size());
  array_utils::gather(GSpan(src_to_dst), merged, GMutableSpan(merged_dst.as_mutable_span()));
  Array<int> merged_offset_data;
  Array<int> merged_index_data;
  const GroupedSpan<int> merged_by_dst = offset_indices::build_groups_from_indices(
      merged_dst, survivors.size(), merged_offset_data, merged_index_data, merged);

  map.mixed = IndexMask::from_predicate(IndexRange(survivors.size()), memory, [&](const int dst) {
    return !merged_by_dst[dst].is_empty();
  });
  map.mixed_offsets.reinitialize(map.mixed.size() + 1);
  map.mixed.foreach_index_optimized<int>(
      [&](const int dst, const int pos) {
        /* The target is part of the group too. */
        map.mixed_offsets[pos] = merged_by_dst[dst].size() + 1;
      },
      exec_mode::grain_size(4096));
  const OffsetIndices mixed_offsets = offset_indices::accumulate_counts_to_offsets(
      map.mixed_offsets);

  /* Each group starts with the target, followed by the elements that merge into it. */
  map.mixed_indices.reinitialize(mixed_offsets.total_size());
  map.mixed.foreach_index(
      [&](const int dst, const int pos) {
        MutableSpan<int> group = map.mixed_indices.as_mutable_span().slice(mixed_offsets[pos]);
        group.first() = map.dst_to_src[dst];
        group.drop_front(1).copy_from(merged_by_dst[dst]);
      },
      exec_mode::grain_size(1024));
  return map;
}

/**
 * Build a map from each source element to the element it becomes in the result. Values for
 * elements removed entirely (collapsed edges) are left uninitialized.
 */
static Array<int> merge_src_to_dst_map(const Span<int> src_to_target, const IndexMask &survivors)
{
  PRF_scope(ProfileCategory::Default);
  Array<int> src_to_dst(src_to_target.size());
  index_mask::build_reverse_map<int>(survivors, src_to_dst);

  threading::parallel_for(src_to_target.index_range(), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      const int elem_target = src_to_target[i];
      if (elem_target != i) {
        /* Collapsed elements have no target. Any value will do, it's never read. */
        src_to_dst[i] = elem_target >= 0 ? src_to_dst[elem_target] : 0;
      }
    }
  });
  return src_to_dst;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Vertex Merging
 * \{ */

/**
 * Copy attributes from a source element for every result element, and mix the values for
 * result elements created from multiple source elements. When nothing is mixed and the result
 * domain is unchanged, attribute arrays are shared with the source.
 */
static void copy_and_mix_attributes(const bke::AttributeAccessor src_attributes,
                                    const bke::AttrDomain domain,
                                    const bke::AttributeFilter &attribute_filter,
                                    const MergePropagationMap &map,
                                    bke::MutableAttributeAccessor dst_attributes)
{
  if (map.mixed.is_empty()) {
    bke::gather_attributes(
        src_attributes, domain, domain, attribute_filter, map.dst_to_src, dst_attributes);
    return;
  }
  src_attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (iter.domain != domain) {
      return;
    }
    if (iter.data_type == bke::AttrType::String) {
      return;
    }
    if (attribute_filter.allow_skip(iter.name)) {
      return;
    }
    const GVArray src_attr = *iter.get();
    const CommonVArrayInfo info = src_attr.common_info();
    if (info.type == CommonVArrayInfo::Type::Single) {
      const bke::AttributeInitValue init(GPointer(src_attr.type(), info.data));
      if (dst_attributes.add(iter.name, iter.domain, iter.data_type, init)) {
        return;
      }
    }
    const GVArraySpan src_span(src_attr);
    bke::GSpanAttributeWriter dst_attr = dst_attributes.lookup_or_add_for_write_only_span(
        iter.name, iter.domain, iter.data_type);
    bke::attribute_math::gather(src_span, map.dst_to_src.as_span(), dst_attr.span);
    const GroupedSpan<int> mixed_groups = map.mixed_groups();
    bke::attribute_math::mix_groups(
        src_span, mixed_groups.offsets, mixed_groups.data, std::nullopt, map.mixed, dst_attr.span);
    dst_attr.finish();
  });
}

static void mix_vertex_groups(const Mesh &mesh_src, const MergePropagationMap &map, Mesh &mesh_dst)
{
  const Span<MDeformVert> src_dverts = mesh_src.deform_verts();
  if (src_dverts.is_empty()) {
    return;
  }
  MutableSpan<MDeformVert> dst_dverts = mesh_dst.deform_verts_for_write();
  const GroupedSpan<int> mixed_groups = map.mixed_groups();
  IndexMaskMemory memory;
  const IndexMask copied = map.mixed.complement(dst_dverts.index_range(), memory);
  bke::gather_deform_verts(src_dverts, map.dst_to_src, copied, dst_dverts);
  threading::parallel_for(map.mixed.index_range(), 256, [&](const IndexRange range) {
    bke::MDeformWeightSet weights;
    map.mixed.slice(range).foreach_index([&](const int dst_vert, const int pos) {
      dst_dverts[dst_vert] = mix_deform_verts(src_dverts, mixed_groups[range[pos]], {}, weights);
    });
  });
}

static Set<StringRef> get_vertex_group_names(const Mesh &mesh)
{
  Set<StringRef> names;
  for (bDeformGroup &group : mesh.vertex_group_names) {
    names.add(group.name);
  }
  return names;
}

static Mesh *create_merged_mesh(const Mesh &mesh,
                                const Span<int> vert_src_to_target,
                                const int removed_vertex_count,
                                const bool do_mix_data,
                                const bke::AttributeFilter &attribute_filter)
{
  PRF_scope(ProfileCategory::Default);
#ifdef USE_WELD_DEBUG_TIME
  SCOPED_TIMER(__func__);
#endif

  const Span<int2> src_edges = mesh.edges();
  const OffsetIndices src_faces = mesh.faces();
  const Span<int> src_corner_verts = mesh.corner_verts();
  const Span<int> src_corner_edges = mesh.corner_edges();
  const bke::AttributeAccessor src_attributes = mesh.attributes();
  const int src_verts_num = mesh.verts_num;
  const int src_edges_num = mesh.edges_num;

  const Array<bool> vert_affected = find_affected_verts(vert_src_to_target);

  WeldMesh weld_mesh;
  weld_mesh_context_create(
      mesh, vert_src_to_target, vert_affected, removed_vertex_count, &weld_mesh);

  const int dst_verts_num = src_verts_num - weld_mesh.removed_verts_num;
  const int dst_edges_num = src_edges_num - weld_mesh.removed_edges_num;
  const int dst_corners_num = src_corner_verts.size() - weld_mesh.removed_corners_num;
  const int dst_faces_num = src_faces.size() - weld_mesh.removed_faces_num +
                            weld_mesh.new_faces_num;

  Mesh *result = BKE_mesh_new_nomain(dst_verts_num, dst_edges_num, dst_faces_num, dst_corners_num);
  BKE_mesh_copy_parameters_for_eval(result, &mesh);
  MutableSpan<int2> dst_edges = result->edges_for_write();
  MutableSpan<int> dst_face_offsets = result->face_offsets_for_write();
  MutableSpan<int> dst_corner_verts = result->corner_verts_for_write();
  MutableSpan<int> dst_corner_edges = result->corner_edges_for_write();
  bke::MutableAttributeAccessor dst_attributes = result->attributes_for_write();

  /* Vertices. */

  IndexMaskMemory mask_memory;
  const IndexMask vert_survivors = merge_survivors(vert_src_to_target, mask_memory);
  BLI_assert(vert_survivors.size() == dst_verts_num);

  const Array<int> vert_src_to_dst = merge_src_to_dst_map(vert_src_to_target, vert_survivors);
  const MergePropagationMap vert_map = merge_propagation_map(
      vert_src_to_target, vert_survivors, vert_src_to_dst, do_mix_data, mask_memory);

  const Set<StringRef> vertex_group_names = get_vertex_group_names(mesh);
  copy_and_mix_attributes(
      src_attributes,
      bke::AttrDomain::Point,
      bke::attribute_filter_with_skip_ref(attribute_filter, vertex_group_names),
      vert_map,
      dst_attributes);
  mix_vertex_groups(mesh, vert_map, *result);
  if (CustomData_has_layer(&mesh.vert_data, CD_ORIGINDEX)) {
    const Span src(static_cast<const int *>(CustomData_get_layer(&mesh.vert_data, CD_ORIGINDEX)),
                   mesh.verts_num);
    MutableSpan dst(static_cast<int *>(CustomData_add_layer(
                        &result->vert_data, CD_ORIGINDEX, CD_CONSTRUCT, result->verts_num)),
                    result->verts_num);
    array_utils::gather(src, vert_map.dst_to_src.as_span(), dst);
  }

  /* Edges. */

  const IndexMask edge_survivors = merge_survivors(weld_mesh.edge_src_to_target, mask_memory);
  BLI_assert(edge_survivors.size() == dst_edges_num);

  const Array<int> edge_src_to_dst = merge_src_to_dst_map(weld_mesh.edge_src_to_target,
                                                          edge_survivors);
  const MergePropagationMap edge_map = merge_propagation_map(
      weld_mesh.edge_src_to_target, edge_survivors, edge_src_to_dst, do_mix_data, mask_memory);

  copy_and_mix_attributes(src_attributes,
                          bke::AttrDomain::Edge,
                          bke::attribute_filter_with_skip_ref(attribute_filter, {".edge_verts"}),
                          edge_map,
                          dst_attributes);
  if (CustomData_has_layer(&mesh.edge_data, CD_ORIGINDEX)) {
    const Span src(static_cast<const int *>(CustomData_get_layer(&mesh.edge_data, CD_ORIGINDEX)),
                   mesh.edges_num);
    MutableSpan dst(static_cast<int *>(CustomData_add_layer(
                        &result->edge_data, CD_ORIGINDEX, CD_CONSTRUCT, result->edges_num)),
                    result->edges_num);
    array_utils::gather(src, edge_map.dst_to_src.as_span(), dst);
  }

  threading::parallel_for(dst_edges.index_range(), 2048, [&](const IndexRange range) {
    for (const int dst_edge_index : range) {
      const int2 src_edge = src_edges[edge_map.dst_to_src[dst_edge_index]];
      dst_edges[dst_edge_index] = int2(vert_src_to_dst[src_edge[0]], vert_src_to_dst[src_edge[1]]);
    }
  });

  /* Faces and corners. */

  const Span<WeldFace> weld_faces = weld_mesh.weld_faces;
  const Span<int> corner_next = weld_mesh.corner_next;

  /* The source faces that remain keep their order, followed by the new faces created by splitting
   * source faces. */
  const IndexMask kept_src_faces = IndexMask::from_predicate(
      src_faces.index_range(),
      mask_memory,
      [&](const int face) {
        const int face_ctx = weld_mesh.face_to_weld_face[face];
        return face_ctx == OUT_OF_CONTEXT || weld_faces[face_ctx].face_dst == OUT_OF_CONTEXT;
      },
      exec_mode::grain_size(4096));
  const IndexMask kept_new_faces = IndexMask::from_predicate(
      weld_faces.index_range().take_back(weld_mesh.new_faces_num),
      mask_memory,
      [&](const int weld_face) { return weld_faces[weld_face].face_dst == OUT_OF_CONTEXT; },
      exec_mode::grain_size(4096));
  BLI_assert(kept_src_faces.size() + kept_new_faces.size() == dst_faces_num);
  const IndexRange dst_new_faces(kept_src_faces.size(), kept_new_faces.size());

  kept_src_faces.foreach_index(
      [&](const int src_face, const int dst_face) {
        const int face_ctx = weld_mesh.face_to_weld_face[src_face];
        dst_face_offsets[dst_face] = face_ctx == OUT_OF_CONTEXT ? src_faces[src_face].size() :
                                                                  weld_faces[face_ctx].corners_num;
      },
      exec_mode::grain_size(4096));
  kept_new_faces.foreach_index(
      [&](const int weld_face, const int pos) {
        dst_face_offsets[dst_new_faces[pos]] = weld_faces[weld_face].corners_num;
      },
      exec_mode::grain_size(4096));
  if (!dst_face_offsets.is_empty()) {
    offset_indices::accumulate_counts_to_offsets(dst_face_offsets);
  }
  const OffsetIndices dst_faces = result->faces();
  BLI_assert(dst_faces.total_size() == dst_corners_num);

  /* The source corners merged into a result corner are the corners of the source face with the
   * same vertex after merging. Only corners with affected vertices can have more than one. */
  const auto foreach_corner_in_group =
      [&](const IndexRange src_face, const int corner, const auto &fn) {
        const int vert = vert_src_to_target[src_corner_verts[corner]];
        if (!vert_affected[vert]) {
          fn(corner);
          return;
        }
        for (const int group_corner : src_face) {
          if (vert_src_to_target[src_corner_verts[group_corner]] == vert) {
            fn(group_corner);
          }
        }
      };

  /* Count the result corners of every weld face that are created from multiple source corners,
   * and their source corners, so that only those need groups for mixing attribute values. */
  Array<int> mixed_corner_offset_data(weld_faces.size() + 1, 0);
  Array<int> mixed_source_offset_data(weld_faces.size() + 1, 0);
  threading::parallel_for(weld_faces.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      const WeldFace &weld_face = weld_faces[i];
      if (weld_face.face_dst != OUT_OF_CONTEXT) {
        continue;
      }
      const IndexRange src_face = src_faces[weld_face.face_src];
      foreach_weld_face_corner(weld_face, corner_next, [&](const int corner) {
        int group_size = 0;
        foreach_corner_in_group(
            src_face, corner, [&](const int /*group_corner*/) { group_size++; });
        if (group_size > 1) {
          mixed_corner_offset_data[i]++;
          mixed_source_offset_data[i] += group_size;
        }
      });
    }
  });
  const OffsetIndices mixed_corners_by_weld_face = offset_indices::accumulate_counts_to_offsets(
      mixed_corner_offset_data);
  const OffsetIndices mixed_sources_by_weld_face = offset_indices::accumulate_counts_to_offsets(
      mixed_source_offset_data);

  MergePropagationMap corner_map;
  corner_map.dst_to_src.reinitialize(dst_corners_num);
  Array<int> mixed_dst_corners(mixed_corners_by_weld_face.total_size());
  corner_map.mixed_offsets.reinitialize(mixed_corners_by_weld_face.total_size() + 1);
  corner_map.mixed_indices.reinitialize(mixed_sources_by_weld_face.total_size());
  corner_map.mixed_offsets.last() = corner_map.mixed_indices.size();

  const auto fill_weld_face = [&](const int weld_face_index, const IndexRange dst_face) {
    const WeldFace &weld_face = weld_faces[weld_face_index];
    const IndexRange src_face = src_faces[weld_face.face_src];
    int dst_corner = dst_face.start();
    int mixed_corner = mixed_corners_by_weld_face[weld_face_index].start();
    int mixed_source = mixed_sources_by_weld_face[weld_face_index].start();
    foreach_weld_face_corner(weld_face, corner_next, [&](const int corner) {
      const int vert = vert_src_to_target[src_corner_verts[corner]];
      const int edge = weld_mesh.edge_src_to_target[src_corner_edges[corner]];
      dst_corner_verts[dst_corner] = vert_src_to_dst[vert];
      dst_corner_edges[dst_corner] = edge_src_to_dst[edge];

      int group_size = 0;
      foreach_corner_in_group(src_face, corner, [&](const int group_corner) {
        if (group_size++ == 0) {
          corner_map.dst_to_src[dst_corner] = group_corner;
        }
      });
      if (group_size > 1) {
        mixed_dst_corners[mixed_corner] = dst_corner;
        corner_map.mixed_offsets[mixed_corner] = mixed_source;
        foreach_corner_in_group(src_face, corner, [&](const int group_corner) {
          corner_map.mixed_indices[mixed_source++] = group_corner;
        });
        mixed_corner++;
      }
      dst_corner++;
    });
    BLI_assert(dst_corner == dst_face.one_after_last());
  };

  kept_src_faces.foreach_index(
      [&](const int src_face_index, const int dst_face_index) {
        const IndexRange dst_face = dst_faces[dst_face_index];
        const int face_ctx = weld_mesh.face_to_weld_face[src_face_index];
        if (face_ctx != OUT_OF_CONTEXT) {
          fill_weld_face(face_ctx, dst_face);
          return;
        }
        const IndexRange src_face = src_faces[src_face_index];
        for (const int i : src_face.index_range()) {
          dst_corner_verts[dst_face[i]] = vert_src_to_dst[src_corner_verts[src_face[i]]];
          dst_corner_edges[dst_face[i]] = edge_src_to_dst[src_corner_edges[src_face[i]]];
          corner_map.dst_to_src[dst_face[i]] = src_face[i];
        }
      },
      exec_mode::grain_size(512));
  kept_new_faces.foreach_index(
      [&](const int weld_face, const int pos) {
        fill_weld_face(weld_face, dst_faces[dst_new_faces[pos]]);
      },
      exec_mode::grain_size(512));
  corner_map.mixed = IndexMask::from_indices(mixed_dst_corners.as_span(), mask_memory);

  /* Face attributes. New faces get default values. */
  src_attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (iter.domain != bke::AttrDomain::Face) {
      return;
    }
    if (attribute_filter.allow_skip(iter.name)) {
      return;
    }
    const bke::GAttributeReader src_attr = iter.get();
    const CommonVArrayInfo info = src_attr.varray.common_info();
    if (info.type == CommonVArrayInfo::Type::Single) {
      const bke::AttributeInitValue init(GPointer(src_attr.varray.type(), info.data));
      if (dst_attributes.add(iter.name, iter.domain, iter.data_type, init)) {
        return;
      }
    }
    if (kept_src_faces.size() == src_faces.size() && dst_new_faces.is_empty() &&
        src_attr.sharing_info && src_attr.varray.is_span())
    {
      const bke::AttributeInitShared init(src_attr.varray.get_internal_span().data(),
                                          *src_attr.sharing_info);
      if (dst_attributes.add(iter.name, iter.domain, iter.data_type, init)) {
        return;
      }
    }
    const CPPType &type = src_attr.varray.type();
    bke::GSpanAttributeWriter dst_attr = dst_attributes.lookup_or_add_for_write_only_span(
        iter.name, iter.domain, iter.data_type);
    array_utils::gather(
        src_attr.varray, kept_src_faces, dst_attr.span.take_front(kept_src_faces.size()));
    GMutableSpan default_data = dst_attr.span.slice(dst_new_faces);
    type.fill_assign_n(type.default_value(), default_data.data(), default_data.size());
    dst_attr.finish();
  });

  if (CustomData_has_layer(&mesh.face_data, CD_ORIGINDEX)) {
    const Span src(static_cast<const int *>(CustomData_get_layer(&mesh.face_data, CD_ORIGINDEX)),
                   mesh.faces_num);
    MutableSpan dst(static_cast<int *>(CustomData_add_layer(
                        &result->face_data, CD_ORIGINDEX, CD_CONSTRUCT, result->faces_num)),
                    result->faces_num);
    array_utils::gather(
        GSpan(src), kept_src_faces, GMutableSpan(dst.take_front(kept_src_faces.size())));
    dst.slice(dst_new_faces).fill(ORIGINDEX_NONE);
  }

  copy_and_mix_attributes(
      src_attributes,
      bke::AttrDomain::Corner,
      bke::attribute_filter_with_skip_ref(attribute_filter, {".corner_vert", ".corner_edge"}),
      corner_map,
      dst_attributes);
  if (const auto *src = static_cast<const float2 *>(
          CustomData_get_layer(&mesh.corner_data, CD_ORIGSPACE_MLOOP)))
  {
    float2 *dst = static_cast<float2 *>(CustomData_add_layer(
        &result->corner_data, CD_ORIGSPACE_MLOOP, CD_CONSTRUCT, result->corners_num));
    const Span src_span(src, mesh.corners_num);
    const MutableSpan dst_span(dst, result->corners_num);
    array_utils::gather(src_span, corner_map.dst_to_src.as_span(), dst_span);
    const GroupedSpan<int> mixed_groups = corner_map.mixed_groups();
    bke::attribute_math::mix_groups(GSpan(src_span),
                                    mixed_groups.offsets,
                                    mixed_groups.data,
                                    std::nullopt,
                                    corner_map.mixed,
                                    GMutableSpan(dst_span));
  }

  for (const eCustomDataType type : {CD_MDISPS, CD_GRID_PAINT_MASK}) {
    if (!CustomData_has_layer(&mesh.corner_data, type)) {
      continue;
    }
    CustomData_add_layer(&result->corner_data, type, CD_CONSTRUCT, result->corners_num);
    for (const int dst_corner : IndexRange(result->corners_num)) {
      CustomData_copy_layer_type_data(&mesh.corner_data,
                                      &result->corner_data,
                                      type,
                                      corner_map.dst_to_src[dst_corner],
                                      dst_corner,
                                      1);
    }
  }

  debug_randomize_mesh_order(result);

  return result;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Merge Map Creation
 * \{ */

std::optional<Mesh *> mesh_merge_by_distance_all(const Mesh &mesh,
                                                 const IndexMask &selection,
                                                 const float merge_distance,
                                                 const bke::AttributeFilter &attribute_filter)
{
  Array<int> vert_src_to_target(mesh.verts_num, OUT_OF_CONTEXT);
  KDTreeNew<float3> tree(mesh.vert_positions(), selection);
  const int removed_verts_num = kdtree::calc_duplicates(
      tree, merge_distance, selection, vert_src_to_target);
  if (removed_verts_num == 0) {
    return std::nullopt;
  }

  /* The KD-tree leaves vertices that aren't merged at -1. */
  threading::parallel_for(vert_src_to_target.index_range(), 4096, [&](const IndexRange range) {
    for (const int vert : range) {
      if (vert_src_to_target[vert] == OUT_OF_CONTEXT) {
        vert_src_to_target[vert] = vert;
      }
    }
  });

  return create_merged_mesh(mesh, vert_src_to_target, removed_verts_num, true, attribute_filter);
}

struct WeldVertexCluster {
  float co[3];
  int merged_verts;
};

std::optional<Mesh *> mesh_merge_by_distance_connected(
    const Mesh &mesh,
    Span<bool> selection,
    const float merge_distance,
    const bool only_loose_edges,
    const bke::AttributeFilter &attribute_filter)
{
  const Span<float3> positions = mesh.vert_positions();
  const Span<int2> edges = mesh.edges();

  int removed_verts_num = 0;

  /* From the source index of the vertex.
   * This indicates which vert it is or is going to be merged. */
  Array<int> vert_src_to_target(mesh.verts_num);

  Array<WeldVertexCluster> vert_clusters(mesh.verts_num);

  for (const int i : positions.index_range()) {
    WeldVertexCluster &cluster = vert_clusters[i];
    copy_v3_v3(cluster.co, positions[i]);
    cluster.merged_verts = 0;
  }
  const float merge_dist_sq = square_f(merge_distance);

  array_utils::fill_index_range(vert_src_to_target.as_mutable_span());

  /* Collapse Edges that are shorter than the threshold. */

  const IndexMask mask = only_loose_edges ? mesh.loose_edges() : IndexMask(mesh.edges().size());

  mask.foreach_index([&](const int i) {
    int vert_1 = edges[i][0];
    int vert_2 = edges[i][1];

    while (vert_1 != vert_src_to_target[vert_1]) {
      vert_1 = vert_src_to_target[vert_1];
    }
    while (vert_2 != vert_src_to_target[vert_2]) {
      vert_2 = vert_src_to_target[vert_2];
    }
    if (vert_1 == vert_2) {
      return;
    }
    if (!selection.is_empty() && (!selection[vert_1] || !selection[vert_2])) {
      return;
    }
    if (vert_1 > vert_2) {
      std::swap(vert_1, vert_2);
    }
    WeldVertexCluster *cluster_1 = &vert_clusters[vert_1];
    WeldVertexCluster *cluster_2 = &vert_clusters[vert_2];

    float edge_dir[3];
    sub_v3_v3v3(edge_dir, cluster_2->co, cluster_1->co);
    const float dist_sq = len_squared_v3(edge_dir);
    if (dist_sq <= merge_dist_sq) {
      float influence = (cluster_2->merged_verts + 1) /
                        float(cluster_1->merged_verts + cluster_2->merged_verts + 2);
      madd_v3_v3fl(cluster_1->co, edge_dir, influence);

      cluster_1->merged_verts += cluster_2->merged_verts + 1;
      vert_src_to_target[vert_2] = vert_1;
      removed_verts_num++;
    }
  });

  if (removed_verts_num == 0) {
    return std::nullopt;
  }

  /* Collapse chains built above, since chained mappings aren't supported. */
  for (const int i : IndexRange(mesh.verts_num)) {
    int vert = i;
    while (vert != vert_src_to_target[vert]) {
      vert = vert_src_to_target[vert];
    }
    vert_src_to_target[i] = vert;
  }

  return create_merged_mesh(mesh, vert_src_to_target, removed_verts_num, true, attribute_filter);
}

Mesh *mesh_merge_verts(const Mesh &mesh,
                       const Span<int> vert_src_to_target,
                       const int removed_verts_num,
                       const bool do_mix_data)
{
  BLI_assert(vert_src_to_target.size() == mesh.verts_num);
  return create_merged_mesh(mesh,
                            vert_src_to_target,
                            removed_verts_num,
                            do_mix_data,
                            bke::AttributeFilter::default_filter());
}

/** \} */

std::optional<Mesh *> mesh_merge_verts(const Mesh &mesh,
                                       const IndexMask &selection,
                                       const Span<int> merge_ids,
                                       const bke::AttributeFilter &attribute_filter)
{
  Array<int> group_indices(selection.size());
  Vector<int> first_verts;
  const int groups_num = array_utils::group_ids_to_indices(
      merge_ids, selection, group_indices, &first_verts);
  const int removed_verts_num = selection.size() - groups_num;
  if (removed_verts_num == 0) {
    return std::nullopt;
  }

  /* Every vertex merges into the first vertex with the same ID. */
  Array<int> vert_src_to_target(mesh.verts_num);
  array_utils::fill_index_range(vert_src_to_target.as_mutable_span());
  selection.foreach_index_optimized<int>(
      [&](const int i, const int pos) { vert_src_to_target[i] = first_verts[group_indices[pos]]; },
      exec_mode::grain_size(8192));

  return create_merged_mesh(mesh, vert_src_to_target, removed_verts_num, true, attribute_filter);
}

}  // namespace blender::geometry
