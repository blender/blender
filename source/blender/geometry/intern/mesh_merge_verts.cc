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
#include "BLI_bit_vector.hh"
#include "BLI_index_mask.hh"
#include "BLI_kdtree_new.hh"
#include "BLI_listbase.hh"
#include "BLI_math_vector_c.hh"
#include "BLI_offset_indices.hh"
#include "BLI_ordered_edge.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "BKE_attribute.hh"
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

struct WeldFace {
  /**
   * #OUT_OF_CONTEXT if the face remains in the result, #ELEM_COLLAPSED if all of its corners
   * collapse, or the index of the face it's a duplicate of.
   */
  int face_dst;
  /* Indices in to the source Mesh. */
  int face_src;
  /** The first and last corner of the face, see #WeldMesh::corner_next. */
  int corner_start;
  int corner_end;
#ifdef USE_WELD_DEBUG
  int corners_num;
#endif
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
#ifdef USE_WELD_DEBUG
        weld_face.corners_num = face.size();
#endif
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

static void weld_face_split_recursive(int face_size,
                                      const Span<int> corner_verts,
                                      WeldFace *r_wp,
                                      WeldMesh *r_weld_mesh,
                                      int *r_removed_faces_num,
                                      int *r_removed_corners_num)
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
        *r_removed_faces_num += 1;
        *r_removed_corners_num += removed_corners_num;
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

#ifdef USE_WELD_DEBUG
          new_test->corners_num = dist_a;
#endif
          weld_face_split_recursive(dist_a,
                                    corner_verts,
                                    new_test,
                                    r_weld_mesh,
                                    r_removed_faces_num,
                                    r_removed_corners_num);
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

  *r_removed_corners_num += removed_corners_num;
#ifdef USE_WELD_DEBUG
  r_wp->corners_num = face_size;
  weld_assert_face_no_vert_repetition(*r_wp, *r_weld_mesh);
#endif
}

/**
 * Remove the corners of collapsed edges from the weld faces, and split faces that contain the same
 * vertex multiple times.
 *
 * \param remaining_edge_ctx_num: Context weld edges that won't be destroyed by merging.
 * \return r_weld_mesh: Corner and face members will be configured here.
 */
static void weld_face_corner_ctx_setup_collapsed_and_split(const Span<int> corner_verts,
                                                           const Span<int> corner_edges,
                                                           const int remaining_edge_ctx_num,
                                                           WeldMesh *r_weld_mesh)
{
  if (remaining_edge_ctx_num == 0) {
    int removed_corners_num = 0;
    for (WeldFace &weld_face : r_weld_mesh->weld_faces) {
      removed_corners_num += weld_face.corner_end - weld_face.corner_start + 1;
      weld_face.face_dst = ELEM_COLLAPSED;
    }
    r_weld_mesh->removed_faces_num = r_weld_mesh->weld_faces.size();
    r_weld_mesh->removed_corners_num = removed_corners_num;
    return;
  }

  WeldFace *weld_faces = r_weld_mesh->weld_faces.data();
  const Span<int> edge_src_to_target = r_weld_mesh->edge_src_to_target;
  MutableSpan<int> corner_next = r_weld_mesh->corner_next;

  int removed_faces_num = 0;
  int removed_corners_num = 0;

  /* Setup Face/Corner. */
  /* `weld_faces.size()` may change while iterating, so make it clear that only the items that
   * already exist are visited. */
  IndexRange weld_faces_src_range = r_weld_mesh->weld_faces.index_range();
  for (const int i : weld_faces_src_range) {
    WeldFace &weld_face = weld_faces[i];
    int face_size = (weld_face.corner_end - weld_face.corner_start) + 1;
    int corner_prev = -1;
    bool changed_corner_start = false;
    int corner = weld_face.corner_start;
    do {
      if (edge_src_to_target[corner_edges[corner]] == ELEM_COLLAPSED) {
        if (face_size == 3) {
          weld_face.face_dst = ELEM_COLLAPSED;
          removed_faces_num++;
          removed_corners_num += 3;
          face_size = 0;
          break;
        }

        if (corner == weld_face.corner_start) {
          changed_corner_start = true;
        }

        removed_corners_num++;
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

    if (face_size) {
      corner_next[corner_prev] = weld_face.corner_start;
      weld_face.corner_end = corner_prev;

#ifdef USE_WELD_DEBUG
      weld_face.corners_num = face_size;
#endif

      weld_face_split_recursive(face_size,
                                corner_verts,
                                &weld_face,
                                r_weld_mesh,
                                &removed_faces_num,
                                &removed_corners_num);
    }
  }

  r_weld_mesh->removed_faces_num = removed_faces_num;
  r_weld_mesh->removed_corners_num = removed_corners_num;

#ifdef USE_WELD_DEBUG
  weld_assert_removed_faces_and_corners_num(
      *r_weld_mesh, r_weld_mesh->removed_faces_num, r_weld_mesh->removed_corners_num);
#endif
}

static int face_find_doubles(const OffsetIndices<int> face_corner_offsets,
                             const int faces_num,
                             const Span<int> corners,
                             const int corner_index_max,
                             Vector<int> &r_doubles_offsets,
                             Array<int> &r_doubles_buffer)
{
  /* Fills the `r_buffer` buffer with the intersection of the arrays in `buffer_a` and `buffer_b`.
   * `buffer_a` and `buffer_b` have a sequence of sorted, non-repeating indices representing
   * faces. */
  const auto intersect = [](const Span<int> buffer_a,
                            const Span<int> buffer_b,
                            const BitVector<> &is_double,
                            int *r_buffer) {
    int result_num = 0;
    int index_a = 0, index_b = 0;
    while (index_a < buffer_a.size() && index_b < buffer_b.size()) {
      const int value_a = buffer_a[index_a];
      const int value_b = buffer_b[index_b];
      if (value_a < value_b) {
        index_a++;
      }
      else if (value_b < value_a) {
        index_b++;
      }
      else {
        /* Equality. */

        /* Do not add duplicates.
         * As they are already in the source array, this can cause buffer overflow. */
        if (!is_double[value_a]) {
          r_buffer[result_num++] = value_a;
        }
        index_a++;
        index_b++;
      }
    }

    return result_num;
  };

  /* Add +1 to allow calculation of the length of the last group. */
  Array<int> linked_faces_offset(corner_index_max + 1, 0);

  for (const int elem_index : corners) {
    linked_faces_offset[elem_index]++;
  }

  int link_faces_buffer_num = 0;
  for (const int elem_index : IndexRange(corner_index_max)) {
    link_faces_buffer_num += linked_faces_offset[elem_index];
    linked_faces_offset[elem_index] = link_faces_buffer_num;
  }
  linked_faces_offset[corner_index_max] = link_faces_buffer_num;

  if (link_faces_buffer_num == 0) {
    return 0;
  }

  Array<int> linked_faces_buffer(link_faces_buffer_num);

  /* Use a reverse for loop to ensure that indexes are assigned in ascending order. */
  for (int face_index = faces_num; face_index--;) {
    if (face_corner_offsets[face_index].is_empty()) {
      continue;
    }

    for (int corner_index = face_corner_offsets[face_index].last();
         corner_index >= face_corner_offsets[face_index].first();
         corner_index--)
    {
      const int elem_index = corners[corner_index];
      linked_faces_buffer[--linked_faces_offset[elem_index]] = face_index;
    }
  }

  Array<int> doubles_buffer(faces_num);

  Vector<int> doubles_offsets;
  doubles_offsets.reserve((faces_num / 2) + 1);
  doubles_offsets.append(0);

  BitVector<> is_double(faces_num, false);

  int doubles_buffer_num = 0;
  int doubles_num = 0;
  for (const int face_index : IndexRange(faces_num)) {
    if (is_double[face_index]) {
      continue;
    }

    int corner_num = face_corner_offsets[face_index].size();
    if (corner_num == 0) {
      continue;
    }

    /* Set or overwrite the first slot of the possible group. */
    doubles_buffer[doubles_buffer_num] = face_index;

    int corner_first = face_corner_offsets[face_index].first();
    int elem_index = corners[corner_first];
    int link_offs = linked_faces_offset[elem_index];
    int faces_a_num = linked_faces_offset[elem_index + 1] - link_offs;
    if (faces_a_num == 1) {
      BLI_assert(linked_faces_buffer[linked_faces_offset[elem_index]] == face_index);
      continue;
    }

    const int *faces_a = &linked_faces_buffer[link_offs];
    int face_to_test;

    /* Skip faces with lower index as these have already been checked. */
    do {
      face_to_test = *faces_a;
      faces_a++;
      faces_a_num--;
    } while (face_to_test != face_index);

    int *isect_result = doubles_buffer.data() + doubles_buffer_num + 1;

    /* `faces_a` are the faces connected to the first corner. So skip the first corner. */
    for (int corner_index : IndexRange(corner_first + 1, corner_num - 1)) {
      elem_index = corners[corner_index];
      link_offs = linked_faces_offset[elem_index];
      int faces_b_num = linked_faces_offset[elem_index + 1] - link_offs;
      const int *faces_b = &linked_faces_buffer[link_offs];

      /* Skip faces with lower index as these have already been checked. */
      do {
        face_to_test = *faces_b;
        faces_b++;
        faces_b_num--;
      } while (face_to_test != face_index);

      doubles_num = intersect(Span<int>{faces_a, faces_a_num},
                              Span<int>{faces_b, faces_b_num},
                              is_double,
                              isect_result);

      if (doubles_num == 0) {
        break;
      }

      /* Intersect the last result. */
      faces_a = isect_result;
      faces_a_num = doubles_num;
    }

    if (doubles_num) {
      for (const int face_double : Span<int>{isect_result, doubles_num}) {
        BLI_assert(face_double > face_index);
        is_double[face_double].set();
      }
      doubles_buffer_num += doubles_num;
      doubles_offsets.append(++doubles_buffer_num);

      if ((doubles_buffer_num + 1) == faces_num) {
        /* The last slot is the remaining unduplicated face.
         * Avoid checking intersection as there are no more slots left. */
        break;
      }
    }
  }

  r_doubles_buffer = std::move(doubles_buffer);
  r_doubles_offsets = std::move(doubles_offsets);
  return doubles_buffer_num - (r_doubles_offsets.size() - 1);
}

static void weld_face_find_doubles(const Span<int> corner_edges,
                                   const int src_edges_num,
                                   WeldMesh *r_weld_mesh)
{
  if (r_weld_mesh->removed_faces_num == r_weld_mesh->weld_faces.size()) {
    return;
  }

  WeldFace *weld_faces = r_weld_mesh->weld_faces.data();
  const Span<int> edge_src_to_target = r_weld_mesh->edge_src_to_target;
  int face_index = 0;

  const int face_size = r_weld_mesh->weld_faces.size();
  Array<int> face_offsets_(face_size + 1);
  Vector<int> new_corner_edges;
  new_corner_edges.reserve(corner_edges.size() - r_weld_mesh->removed_corners_num);

  for (const WeldFace &weld_face : r_weld_mesh->weld_faces) {
    face_offsets_[face_index++] = new_corner_edges.size();
    if (weld_face.face_dst != OUT_OF_CONTEXT) {
      continue;
    }
    foreach_weld_face_corner(weld_face, r_weld_mesh->corner_next, [&](const int corner) {
      new_corner_edges.append(edge_src_to_target[corner_edges[corner]]);
    });
  }

  face_offsets_[face_size] = new_corner_edges.size();
  OffsetIndices<int> face_offsets(face_offsets_);

  Vector<int> doubles_offsets;
  Array<int> doubles_buffer;
  const int doubles_num = face_find_doubles(
      face_offsets, face_size, new_corner_edges, src_edges_num, doubles_offsets, doubles_buffer);

  if (doubles_num) {
    int removed_corners_num = 0;

    OffsetIndices<int> doubles_offset_indices(doubles_offsets);
    for (const int i : doubles_offset_indices.index_range()) {
      const int face_dst = weld_faces[doubles_buffer[doubles_offsets[i]]].face_src;

      for (const int offset : doubles_offset_indices[i].drop_front(1)) {
        const int weld_face_index = doubles_buffer[offset];
        WeldFace &weld_face = weld_faces[weld_face_index];

        BLI_assert(weld_face.face_dst == OUT_OF_CONTEXT);
        weld_face.face_dst = face_dst;
        removed_corners_num += face_offsets[weld_face_index].size();
      }
    }

    r_weld_mesh->removed_faces_num += doubles_num;
    r_weld_mesh->removed_corners_num += removed_corners_num;
  }

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

  weld_face_find_doubles(corner_edges, edges.size(), r_weld_mesh);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Merging
 * \{ */

/**
 * Group the source elements by the element they merge into.
 *
 * \param src_to_target: For each element, the element it merges into. Elements that don't merge
 * into anything else point at themselves.
 * \param kept: The elements that survive as part of some group. Everything outside of this mask is
 * removed entirely rather than merged (collapsed edges).
 */
static GroupedSpan<int> merge_groups_create(const Span<int> src_to_target,
                                            const IndexMask &kept,
                                            Array<int> &r_group_offsets,
                                            Array<int> &r_group_indices)
{
  if (kept.size() == src_to_target.size()) {
    return offset_indices::build_groups_from_indices(
        src_to_target, src_to_target.size(), r_group_offsets, r_group_indices);
  }
  Array<int> kept_src_to_target(kept.size());
  array_utils::gather(src_to_target, kept, kept_src_to_target.as_mutable_span());
  return offset_indices::build_groups_from_indices(
      kept_src_to_target, src_to_target.size(), r_group_offsets, r_group_indices, kept);
}

/**
 * The elements that are kept in the result. In other words, the merge targets and also the "out of
 * context" elements.
 */
static IndexMask merge_survivors(const Span<int> src_to_target, IndexMaskMemory &memory)
{
  return IndexMask::from_predicate(
      src_to_target.index_range(), memory, [&](const int i) { return src_to_target[i] == i; });
}

/**
 * Build a map from each element in the result to the source elements it's created from.
 *
 * \param src_to_target: The target element each element merges into.
 * \param kept: The elements that aren't removed entirely (see #merge_groups_create).
 * \param survivors: The elements kept in the result (see #merge_survivors).
 * \param do_mix_data: Build maps for mixing attribute values from all of an elements source
 * elements. Otherwise just the value for the target element is used.
 */
static GroupedSpan<int> merge_dst_to_src_map(const Span<int> src_to_target,
                                             const IndexMask &kept,
                                             const IndexMask &survivors,
                                             const bool do_mix_data,
                                             Array<int> &r_offsets,
                                             Array<int> &r_indices)
{
  PRF_scope(ProfileCategory::Default);
  if (!do_mix_data) {
    r_indices.reinitialize(survivors.size());
    survivors.to_indices(r_indices.as_mutable_span());
    r_offsets.reinitialize(survivors.size() + 1);
    array_utils::fill_index_range(r_offsets.as_mutable_span());
    return {OffsetIndices<int>(r_offsets), r_indices};
  }

  Array<int> group_offsets_by_src;
  merge_groups_create(src_to_target, kept, group_offsets_by_src, r_indices);

  /* The groups are laid out in ascending order of the element they merge into, and elements that
   * aren't kept have empty groups. So dropping the empty groups compresses the offsets into
   * exactly the result order, and the group indices are already the result's source indices. */
  r_offsets.reinitialize(survivors.size() + 1);
  /* #gather_selected_offsets leaves the array untouched when there is nothing to gather. */
  r_offsets.last() = 0;
  offset_indices::gather_selected_offsets(
      OffsetIndices<int>(group_offsets_by_src), survivors, r_offsets);
  return {OffsetIndices<int>(r_offsets), r_indices};
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

template<typename T>
static void copy_first_from_src(const Span<T> src,
                                const GroupedSpan<int> dst_to_src,
                                MutableSpan<T> dst)
{
  for (const int dst_index : dst.index_range()) {
    const int src_index = dst_to_src[dst_index].first();
    dst[dst_index] = src[src_index];
  }
}

static void mix_attributes(const bke::AttributeAccessor src_attributes,
                           const GroupedSpan<int> dst_to_src,
                           const bke::AttrDomain domain,
                           const bke::AttributeFilter &attribute_filter,
                           const Set<StringRef> &skip_names,
                           bke::MutableAttributeAccessor dst_attributes)
{
  src_attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (iter.domain != domain) {
      return;
    }
    if (skip_names.contains(iter.name)) {
      return;
    }
    if (attribute_filter.allow_skip(iter.name)) {
      return;
    }
    if (iter.data_type == bke::AttrType::String) {
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
    bke::GSpanAttributeWriter dst_attr = dst_attributes.lookup_or_add_for_write_only_span(
        iter.name, iter.domain, iter.data_type);
    bke::attribute_math::mix_groups(GVArraySpan(src_attr), dst_to_src, dst_attr.span);
    dst_attr.finish();
  });
}

static void mix_vertex_groups(const Mesh &mesh_src,
                              const GroupedSpan<int> dst_to_src,
                              Mesh &mesh_dst)
{
  const Span<MDeformVert> src_dverts = mesh_src.deform_verts();
  if (src_dverts.is_empty()) {
    return;
  }
  MutableSpan<MDeformVert> dst_dverts = mesh_dst.deform_verts_for_write();
  threading::parallel_for(dst_to_src.index_range(), 256, [&](const IndexRange range) {
    bke::MDeformWeightSet weights;
    for (const int dst_vert : range) {
      dst_dverts[dst_vert] = mix_deform_verts(src_dverts, dst_to_src[dst_vert], {}, weights);
    }
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

  Array<int> vert_dst_to_src_offsets;
  Array<int> vert_dst_to_src_indices;
  const GroupedSpan<int> vert_dst_to_src = merge_dst_to_src_map(vert_src_to_target,
                                                                IndexMask(src_verts_num),
                                                                vert_survivors,
                                                                do_mix_data,
                                                                vert_dst_to_src_offsets,
                                                                vert_dst_to_src_indices);

  mix_attributes(src_attributes,
                 vert_dst_to_src,
                 bke::AttrDomain::Point,
                 attribute_filter,
                 get_vertex_group_names(mesh),
                 dst_attributes);
  mix_vertex_groups(mesh, vert_dst_to_src, *result);
  if (CustomData_has_layer(&mesh.vert_data, CD_ORIGINDEX)) {
    const Span src(static_cast<const int *>(CustomData_get_layer(&mesh.vert_data, CD_ORIGINDEX)),
                   mesh.verts_num);
    MutableSpan dst(static_cast<int *>(CustomData_add_layer(
                        &result->vert_data, CD_ORIGINDEX, CD_CONSTRUCT, result->verts_num)),
                    result->verts_num);
    copy_first_from_src(src, vert_dst_to_src, dst);
  }

  /* Edges. */

  /* Collapsed edges have no target, so they can't be part of any group. */
  const IndexMask kept_edges = IndexMask::from_predicate(
      IndexMask(src_edges_num), mask_memory, [&](const int edge) {
        return weld_mesh.edge_src_to_target[edge] != ELEM_COLLAPSED;
      });

  const IndexMask edge_survivors = merge_survivors(weld_mesh.edge_src_to_target, mask_memory);
  BLI_assert(edge_survivors.size() == dst_edges_num);

  const Array<int> edge_src_to_dst = merge_src_to_dst_map(weld_mesh.edge_src_to_target,
                                                          edge_survivors);

  Array<int> edge_dst_to_src_offsets;
  Array<int> edge_dst_to_src_indices;
  const GroupedSpan<int> edge_dst_to_src = merge_dst_to_src_map(weld_mesh.edge_src_to_target,
                                                                kept_edges,
                                                                edge_survivors,
                                                                do_mix_data,
                                                                edge_dst_to_src_offsets,
                                                                edge_dst_to_src_indices);

  mix_attributes(src_attributes,
                 edge_dst_to_src,
                 bke::AttrDomain::Edge,
                 attribute_filter,
                 {".edge_verts"},
                 dst_attributes);
  if (CustomData_has_layer(&mesh.edge_data, CD_ORIGINDEX)) {
    const Span src(static_cast<const int *>(CustomData_get_layer(&mesh.edge_data, CD_ORIGINDEX)),
                   mesh.edges_num);
    MutableSpan dst(static_cast<int *>(CustomData_add_layer(
                        &result->edge_data, CD_ORIGINDEX, CD_CONSTRUCT, result->edges_num)),
                    result->edges_num);
    copy_first_from_src(src, edge_dst_to_src, dst);
  }

  threading::parallel_for(dst_edges.index_range(), 2048, [&](const IndexRange range) {
    for (const int dst_edge_index : range) {
      const int src_edge_index = edge_dst_to_src[dst_edge_index].first();
      const int2 src_edge = src_edges[src_edge_index];
      dst_edges[dst_edge_index] = int2(vert_src_to_dst[src_edge[0]], vert_src_to_dst[src_edge[1]]);
    }
  });

  /* Faces/Loops. */
  Vector<int> corner_src_index_offset_data;
  Vector<int> corner_src_index_data;

  corner_src_index_offset_data.reserve(result->corners_num + 1);
  corner_src_index_data.reserve(mesh.corners_num);

  /* Add the remaining corners of a weld face, with the source corners that are merged into each of
   * them: the corners of the source face that have the same vertex after merging. */
  const auto add_weld_face_corners = [&](const WeldFace &weld_face, int &dst_corner) {
    const IndexRange src_face = src_faces[weld_face.face_src];
    foreach_weld_face_corner(weld_face, weld_mesh.corner_next, [&](const int corner) {
      const int vert = vert_src_to_target[src_corner_verts[corner]];
      corner_src_index_offset_data.append_unchecked(corner_src_index_data.size());
      if (vert_affected[vert]) {
        for (const int group_corner : src_face) {
          if (vert_src_to_target[src_corner_verts[group_corner]] == vert) {
            corner_src_index_data.append(group_corner);
          }
        }
      }
      else {
        corner_src_index_data.append(corner);
      }
      dst_corner_verts[dst_corner] = vert_src_to_dst[vert];
      dst_corner_edges[dst_corner] =
          edge_src_to_dst[weld_mesh.edge_src_to_target[src_corner_edges[corner]]];
      dst_corner++;
    });
  };

  int r_i = 0;
  int dst_corner = 0;
  Vector<bool> dst_face_unaffected;
  dst_face_unaffected.reserve(dst_faces_num);
  Vector<int> dst_to_src_faces;
  dst_to_src_faces.reserve(dst_faces_num);
  for (const int i : src_faces.index_range()) {
    const int corner_start = dst_corner;
    const int face_ctx = weld_mesh.face_to_weld_face[i];
    if (face_ctx == OUT_OF_CONTEXT) {
      for (const int corner_src : src_faces[i]) {
        corner_src_index_offset_data.append_unchecked(corner_src_index_data.size());
        corner_src_index_data.append(corner_src);
        dst_corner++;
      }
      dst_face_unaffected.append_unchecked(true);
    }
    else {
      const WeldFace &weld_face = weld_mesh.weld_faces[face_ctx];
      if (weld_face.face_dst != OUT_OF_CONTEXT) {
        continue;
      }
      dst_face_unaffected.append_unchecked(false);
      add_weld_face_corners(weld_face, dst_corner);
    }

    dst_to_src_faces.append_unchecked(i);
    dst_face_offsets[r_i] = corner_start;
    r_i++;
  }

  /* New Polygons.
   * NOTE: The number of "src" and "new" faces might not match `new_faces_num`. */
  for (const int i : weld_mesh.weld_faces.index_range().take_back(weld_mesh.new_faces_num)) {
    const WeldFace &weld_face = weld_mesh.weld_faces[i];
    if (weld_face.face_dst != OUT_OF_CONTEXT) {
      continue;
    }
    dst_face_offsets[r_i] = dst_corner;
    add_weld_face_corners(weld_face, dst_corner);
    r_i++;
  }

  BLI_assert(int(r_i) == dst_faces_num);
  BLI_assert(dst_corner == dst_corners_num);

  corner_src_index_offset_data.append_unchecked(corner_src_index_data.size());

  const GroupedSpan<int> dst_to_src_corners(OffsetIndices<int>(corner_src_index_offset_data),
                                            corner_src_index_data);

  const OffsetIndices dst_faces = result->faces();

  src_attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (iter.domain != bke::AttrDomain::Face) {
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
    const CPPType &type = src_attr.type();
    bke::GSpanAttributeWriter dst_attr = dst_attributes.lookup_or_add_for_write_only_span(
        iter.name, iter.domain, iter.data_type);
    bke::attribute_math::gather(
        src_attr, dst_to_src_faces, dst_attr.span.take_front(dst_to_src_faces.size()));
    GMutableSpan default_data = dst_attr.span.drop_front(dst_to_src_faces.size());
    type.fill_assign_n(type.default_value(), default_data.data(), default_data.size());
    dst_attr.finish();
  });

  if (CustomData_has_layer(&mesh.face_data, CD_ORIGINDEX)) {
    const Span src(static_cast<const int *>(CustomData_get_layer(&mesh.face_data, CD_ORIGINDEX)),
                   mesh.faces_num);
    MutableSpan dst(static_cast<int *>(CustomData_add_layer(
                        &result->face_data, CD_ORIGINDEX, CD_CONSTRUCT, result->faces_num)),
                    result->faces_num);
    bke::attribute_math::gather(src, dst_to_src_faces, dst.take_front(dst_to_src_faces.size()));
    dst.drop_front(dst_to_src_faces.size()).fill(ORIGINDEX_NONE);
  }

  IndexMaskMemory memory;
  const IndexMask out_of_context_faces = IndexMask::from_bools(dst_face_unaffected, memory);

  out_of_context_faces.foreach_index(
      [&](const int dst_face_index) {
        const IndexRange src_face = src_faces[dst_to_src_faces[dst_face_index]];
        const IndexRange dst_face = dst_faces[dst_face_index];
        for (const int i : src_face.index_range()) {
          dst_corner_verts[dst_face[i]] = vert_src_to_dst[src_corner_verts[src_face[i]]];
          dst_corner_edges[dst_face[i]] = edge_src_to_dst[src_corner_edges[src_face[i]]];
        }
      },
      exec_mode::grain_size(1024));

  mix_attributes(src_attributes,
                 dst_to_src_corners,
                 bke::AttrDomain::Corner,
                 attribute_filter,
                 {".corner_vert", ".corner_edge"},
                 dst_attributes);
  if (const auto *src = static_cast<const float2 *>(
          CustomData_get_layer(&mesh.corner_data, CD_ORIGSPACE_MLOOP)))
  {
    float2 *dst = static_cast<float2 *>(CustomData_add_layer(
        &result->corner_data, CD_ORIGSPACE_MLOOP, CD_CONSTRUCT, result->corners_num));
    bke::attribute_math::mix_groups(
        Span(src, mesh.corners_num), dst_to_src_corners, MutableSpan(dst, result->corners_num));
  }

  for (const eCustomDataType type : {CD_MDISPS, CD_GRID_PAINT_MASK}) {
    if (!CustomData_has_layer(&mesh.corner_data, type)) {
      continue;
    }
    CustomData_add_layer(&result->corner_data, type, CD_CONSTRUCT, result->corners_num);
    for (const int dst_corner : IndexRange(result->corners_num)) {
      const int src_corner = dst_to_src_corners[dst_corner].first();
      CustomData_copy_layer_type_data(
          &mesh.corner_data, &result->corner_data, type, src_corner, dst_corner, 1);
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
  VectorSet<int> group_indices;
  selection.foreach_index_optimized<int>([&](const int i) { group_indices.add(merge_ids[i]); });
  const int removed_verts_num = selection.size() - group_indices.size();
  if (removed_verts_num == 0) {
    return std::nullopt;
  }

  Array<int> dst_vert_by_group(group_indices.size(), -1);
  selection.foreach_index_optimized<int>([&](const int i) {
    const int group_i = group_indices.index_of(merge_ids[i]);
    if (dst_vert_by_group[group_i] == -1) {
      dst_vert_by_group[group_i] = i;
    }
  });

  Array<int> vert_src_to_target(mesh.verts_num);
  array_utils::fill_index_range(vert_src_to_target.as_mutable_span());
  selection.foreach_index_optimized<int>(
      [&](const int i) {
        const int group_i = group_indices.index_of(merge_ids[i]);
        vert_src_to_target[i] = dst_vert_by_group[group_i];
      },
      exec_mode::grain_size(8192));

  return create_merged_mesh(mesh, vert_src_to_target, removed_verts_num, true, attribute_filter);
}

}  // namespace blender::geometry
