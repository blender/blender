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
#include "BLI_kdtree.hh"
#include "BLI_listbase.hh"
#include "BLI_math_vector_c.hh"
#include "BLI_offset_indices.hh"
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

struct WeldEdge {
  /* Indices relative to the source Mesh. */
  int edge_src;
  int vert_a;
  int vert_b;
};

struct WeldCorner {
  union {
    int flag;
    struct {
      /* Indices relative to the source Mesh. */
      int edge;
      int vert;
      int corner_src;
      int corner_next;
    };
  };
};

struct WeldFace {
  union {
    int flag;
    struct {
      /* Indices relative to the source Mesh. */
      int face_dst;
      int face_src;
      int corner_start;
      int corner_end;

      /* To find groups. */
      int corner_ctx_start;
      int corner_ctx_num;
#ifdef USE_WELD_DEBUG
      int corners_num;
#endif
    };
  };
};

struct WeldMesh {
  /* Group of edges to be merged. */
  Array<int> edge_src_to_target;
  Span<int> vert_src_to_target;

  /* References all faces and corners that will be affected. */
  Vector<WeldCorner> weld_corners;
  Vector<WeldFace> weld_faces;
  int new_faces_num;

  /* From the actual index of the element in the mesh, it indicates what is the index of the Weld
   * element above. */
  Array<int> corner_to_weld_corner;
  Array<int> face_to_weld_face;

  int removed_verts_num;
  int removed_edges_num;
  int removed_corners_num;
  int removed_faces_num; /* Including the new faces. */

  /* Number of corners of the largest affected face. */
  int max_face_size;

#ifdef USE_WELD_DEBUG
  Span<int> corner_verts;
  Span<int> corner_edges;
  OffsetIndices<int> faces;
#endif
};

struct WeldCornerOfFaceIter {
  int corner_iter;
  int corner_end;

  /* Weld group. */
  int corner_ctx_start;
  int corner_ctx_num;
  int *group;

  Span<WeldCorner> weld_corners;
  Span<int> corner_verts;
  Span<int> corner_edges;
  Span<int> corner_to_weld_corner;

  /* Return */
  int group_size;
  int vert;
  int edge;
};

/* -------------------------------------------------------------------- */
/** \name Debug Utils
 * \{ */

#ifdef USE_WELD_DEBUG
static bool weld_iter_corner_of_face_begin(WeldCornerOfFaceIter &iter,
                                           const WeldFace &weld_face,
                                           Span<WeldCorner> weld_corners,
                                           const Span<int> corner_verts,
                                           const Span<int> corner_edges,
                                           Span<int> corner_to_weld_corner,
                                           int *group_buffer);

static bool weld_iter_corner_of_face_next(WeldCornerOfFaceIter &iter);

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

static void weld_assert_removed_faces_and_corners_num(WeldMesh *weld_mesh,
                                                      const int expected_removed_faces_num,
                                                      const int expected_removed_corners_num)
{
  const Span<int> corner_verts = weld_mesh->corner_verts;
  const Span<int> corner_edges = weld_mesh->corner_edges;
  const OffsetIndices<int> faces = weld_mesh->faces;

  int removed_faces = 0;
  int removed_corners = corner_verts.size();
  for (const int i : faces.index_range()) {
    int face_ctx = weld_mesh->face_to_weld_face[i];
    if (face_ctx != OUT_OF_CONTEXT) {
      const WeldFace *weld_face = &weld_mesh->weld_faces[face_ctx];
      WeldCornerOfFaceIter iter;
      if (!weld_iter_corner_of_face_begin(iter,
                                          *weld_face,
                                          weld_mesh->weld_corners,
                                          corner_verts,
                                          corner_edges,
                                          weld_mesh->corner_to_weld_corner,
                                          nullptr))
      {
        removed_faces++;
        continue;
      }
      else {
        if (weld_face->face_dst != OUT_OF_CONTEXT) {
          removed_faces++;
          continue;
        }
        int remain = weld_face->corners_num;
        int corner = weld_face->corner_start;
        while (remain) {
          int corner_next = corner + 1;
          int corner_ctx = weld_mesh->corner_to_weld_corner[corner];
          if (corner_ctx != OUT_OF_CONTEXT) {
            const WeldCorner *weld_corner = &weld_mesh->weld_corners[corner_ctx];
            if (weld_corner->flag != ELEM_COLLAPSED) {
              removed_corners--;
              remain--;
            }
          }
          else {
            removed_corners--;
            remain--;
          }
          corner = corner_next;
        }
      }
    }
    else {
      removed_corners -= faces[i].size();
    }
  }

  for (const int i : weld_mesh->weld_faces.index_range().take_back(weld_mesh->new_faces_num)) {
    const WeldFace &weld_face = weld_mesh->weld_faces[i];
    if (weld_face.face_dst != OUT_OF_CONTEXT) {
      removed_faces++;
      continue;
    }
    int remain = weld_face.corners_num;
    int corner = weld_face.corner_start;
    while (remain) {
      int corner_next = corner + 1;
      int corner_ctx = weld_mesh->corner_to_weld_corner[corner];
      if (corner_ctx != OUT_OF_CONTEXT) {
        const WeldCorner *weld_corner = &weld_mesh->weld_corners[corner_ctx];
        if (weld_corner->flag != ELEM_COLLAPSED) {
          removed_corners--;
          remain--;
        }
      }
      else {
        removed_corners--;
        remain--;
      }
      corner = corner_next;
    }
  }

  BLI_assert(removed_faces == expected_removed_faces_num);
  BLI_assert(removed_corners == expected_removed_corners_num);
}

static void weld_assert_face_no_vert_repetition(const WeldFace *weld_face,
                                                Span<WeldCorner> weld_corners,
                                                const Span<int> corner_verts,
                                                const Span<int> corner_edges,
                                                Span<int> corner_to_weld_corner)
{
  int i = 0;
  if (weld_face->corners_num == 0) {
    BLI_assert(weld_face->flag == ELEM_COLLAPSED);
    return;
  }

  Array<int, 64> verts(weld_face->corners_num);
  WeldCornerOfFaceIter iter;
  if (!weld_iter_corner_of_face_begin(iter,
                                      *weld_face,
                                      weld_corners,
                                      corner_verts,
                                      corner_edges,
                                      corner_to_weld_corner,
                                      nullptr))
  {
    return;
  }
  else {
    do {
      verts[i++] = iter.vert;
    } while (weld_iter_corner_of_face_next(iter));
  }

  BLI_assert(i == weld_face->corners_num);

  for (i = 0; i < weld_face->corners_num; i++) {
    int vert_a = verts[i];
    for (int j = i + 1; j < weld_face->corners_num; j++) {
      int vert_b = verts[j];
      BLI_assert(vert_a != vert_b);
    }
  }
}

#endif /* USE_WELD_DEBUG */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vert API
 * \{ */

/**
 * The maps from source elements to their merge targets store the element each element merges into,
 * with un-merged elements pointing at themselves. This replaces those self-referencing
 * single-groups with the #OUT_OF_CONTEXT value so the topology collapsing code can skip work for
 * them more easily.
 */
static Array<int> vert_src_to_target_with_context(const Span<int> vert_src_to_target)
{
  Array<int> result(vert_src_to_target.size(), OUT_OF_CONTEXT);
  for (const int vert : vert_src_to_target.index_range()) {
    const int vert_target = vert_src_to_target[vert];
    if (vert_target != vert) {
      BLI_assert(vert_src_to_target[vert_target] == vert_target);
      result[vert] = vert_target;
      /* The target is affected by the merge too. */
      result[vert_target] = vert_target;
    }
  }
  return result;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edge API
 * \{ */

/**
 * Build the context weld edges.
 *
 * \return r_edge_src_to_target: First step to create map of indices pointing edges that will be
 * merged.
 */
static Vector<WeldEdge> weld_edges_build_and_find_collapsed(Span<int2> edges,
                                                            Span<int> vert_src_to_target,
                                                            MutableSpan<int> r_edge_src_to_target,
                                                            int *r_collapsed_edges_num)
{
  /* Edge Context. */
  int collapsed_edges_num = 0;

  Vector<WeldEdge> weld_edges;
  weld_edges.reserve(edges.size());

  for (const int i : edges.index_range()) {
    int vert_1 = edges[i][0];
    int vert_2 = edges[i][1];
    int vert_target_1 = vert_src_to_target[vert_1];
    int vert_target_2 = vert_src_to_target[vert_2];
    if (vert_target_1 == OUT_OF_CONTEXT && vert_target_2 == OUT_OF_CONTEXT) {
      r_edge_src_to_target[i] = i;
      continue;
    }

    const int vert_a = (vert_target_1 == OUT_OF_CONTEXT) ? vert_1 : vert_target_1;
    const int vert_b = (vert_target_2 == OUT_OF_CONTEXT) ? vert_2 : vert_target_2;

    if (vert_a == vert_b) {
      r_edge_src_to_target[i] = ELEM_COLLAPSED;
      collapsed_edges_num++;
    }
    else {
      weld_edges.append({i, vert_a, vert_b});
      r_edge_src_to_target[i] = i;
    }
  }

  *r_collapsed_edges_num = collapsed_edges_num;
  return weld_edges;
}

/**
 * Fills `r_edge_src_to_target` indicating the duplicated edges.
 *
 * \param weld_edges: Candidate edges for merging (edges that don't collapse and that have at least
 *                    one weld vertex).
 *
 * \param r_edge_src_to_target: Resulting map of indices pointing the source edges to each target.
 * \param r_removed_double_edges_num: Resulting number of duplicate edges to be destroyed.
 */
static void weld_edge_find_doubles(Span<WeldEdge> weld_edges,
                                   int src_verts_num,
                                   MutableSpan<int> r_edge_src_to_target,
                                   int *r_removed_double_edges_num)
{
  /* Setup Edge Overlap. */
  int removed_double_edges_num = 0;

  if (weld_edges.is_empty()) {
    *r_removed_double_edges_num = removed_double_edges_num;
    return;
  }

  /* Add +1 to allow calculation of the length of the last group. */
  Array<int> vert_to_edges_offsets(src_verts_num + 1, 0);

  for (const WeldEdge &weld_edge : weld_edges) {
    BLI_assert(r_edge_src_to_target[weld_edge.edge_src] != ELEM_COLLAPSED);
    BLI_assert(weld_edge.vert_a != weld_edge.vert_b);
    vert_to_edges_offsets[weld_edge.vert_a]++;
    vert_to_edges_offsets[weld_edge.vert_b]++;
  }

  int links_num = 0;
  for (const int i : IndexRange(src_verts_num)) {
    links_num += vert_to_edges_offsets[i];
    vert_to_edges_offsets[i] = links_num;
  }
  vert_to_edges_offsets.last() = links_num;

  BLI_assert(links_num > 0);
  Array<int> vert_to_edges_indices(links_num);

  /* Use a reverse for loop to ensure that indexes are assigned in ascending order. */
  for (int i = weld_edges.size(); i--;) {
    const WeldEdge &weld_edge = weld_edges[i];
    BLI_assert(r_edge_src_to_target[weld_edge.edge_src] != ELEM_COLLAPSED);
    int vert_target_a = weld_edge.vert_a;
    int vert_target_b = weld_edge.vert_b;

    vert_to_edges_indices[--vert_to_edges_offsets[vert_target_a]] = i;
    vert_to_edges_indices[--vert_to_edges_offsets[vert_target_b]] = i;
  }

  for (const int i : weld_edges.index_range()) {
    const WeldEdge &weld_edge = weld_edges[i];
    if (r_edge_src_to_target[weld_edge.edge_src] != weld_edge.edge_src) {
      /* Already a duplicate. */
      continue;
    }

    int vert_target_a = weld_edge.vert_a;
    int vert_target_b = weld_edge.vert_b;

    const int link_a = vert_to_edges_offsets[vert_target_a];
    const int link_b = vert_to_edges_offsets[vert_target_b];

    int edges_num_a = vert_to_edges_offsets[vert_target_a + 1] - link_a;
    int edges_num_b = vert_to_edges_offsets[vert_target_b + 1] - link_b;

    int edge_src = weld_edge.edge_src;
    if (edges_num_a <= 1 || edges_num_b <= 1) {
      /* No other edge can share both of this edge's vertices, so it survives on its own. */
      continue;
    }

    int *edges_ctx_a = &vert_to_edges_indices[link_a];
    int *edges_ctx_b = &vert_to_edges_indices[link_b];

    for (; edges_num_a--; edges_ctx_a++) {
      int edge_ctx_a = *edges_ctx_a;
      if (edge_ctx_a == i) {
        continue;
      }
      while (edges_num_b && *edges_ctx_b < edge_ctx_a) {
        edges_ctx_b++;
        edges_num_b--;
      }
      if (edges_num_b == 0) {
        break;
      }
      int edge_ctx_b = *edges_ctx_b;
      if (edge_ctx_a == edge_ctx_b) {
        const WeldEdge &we_b = weld_edges[edge_ctx_b];
        BLI_assert(ELEM(we_b.vert_a, vert_target_a, vert_target_b));
        BLI_assert(ELEM(we_b.vert_b, vert_target_a, vert_target_b));
        BLI_assert(we_b.edge_src != edge_src);
        BLI_assert(r_edge_src_to_target[we_b.edge_src] == we_b.edge_src);
        r_edge_src_to_target[we_b.edge_src] = edge_src;
        removed_double_edges_num++;
      }
    }
  }

  *r_removed_double_edges_num = removed_double_edges_num;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Poly and Loop API
 * \{ */

static bool weld_iter_corner_of_face_next(WeldCornerOfFaceIter &iter)
{
  if (iter.corner_iter > iter.corner_end) {
    return false;
  }

  Span<WeldCorner> weld_corners = iter.weld_corners;
  Span<int> corner_to_weld_corner = iter.corner_to_weld_corner;
  int corner = iter.corner_iter;
  int corner_next = corner + 1;

  int corner_ctx = corner_to_weld_corner[corner];
  if (corner_ctx != OUT_OF_CONTEXT) {
    const WeldCorner *weld_corner = &weld_corners[corner_ctx];
#ifdef USE_WELD_DEBUG
    BLI_assert(weld_corner->flag != ELEM_COLLAPSED);
    BLI_assert(iter.vert != weld_corner->vert);
#endif
    iter.vert = weld_corner->vert;
    iter.edge = weld_corner->edge;
    if (weld_corner->corner_next > corner) {
      /* Allow the loop to break. */
      corner_next = weld_corner->corner_next;
    }

    if (iter.group) {
      iter.group_size = 0;
      int count = iter.corner_ctx_num;
      for (weld_corner = &weld_corners[iter.corner_ctx_start]; count--; weld_corner++) {
        if (weld_corner->vert == iter.vert) {
          iter.group[iter.group_size++] = weld_corner->corner_src;
        }
      }
    }
  }
  else {
#ifdef USE_WELD_DEBUG
    BLI_assert(iter.vert != iter.corner_verts[corner]);
#endif
    iter.vert = iter.corner_verts[corner];
    iter.edge = iter.corner_edges[corner];
    if (iter.group) {
      iter.group[0] = corner;
      iter.group_size = 1;
    }
  }

  iter.corner_iter = corner_next;
  return true;
}

static bool weld_iter_corner_of_face_begin(WeldCornerOfFaceIter &iter,
                                           const WeldFace &weld_face,
                                           Span<WeldCorner> weld_corners,
                                           const Span<int> corner_verts,
                                           const Span<int> corner_edges,
                                           Span<int> corner_to_weld_corner,
                                           int *group_buffer)
{
  if (weld_face.flag == ELEM_COLLAPSED) {
    return false;
  }

  iter.corner_iter = weld_face.corner_start;
  iter.corner_end = weld_face.corner_end;
  iter.corner_ctx_start = weld_face.corner_ctx_start;
  iter.corner_ctx_num = weld_face.corner_ctx_num;

  iter.weld_corners = weld_corners;
  iter.corner_verts = corner_verts;
  iter.corner_edges = corner_edges;
  iter.corner_to_weld_corner = corner_to_weld_corner;
  iter.group = group_buffer;
  iter.group_size = 0;

#ifdef USE_WELD_DEBUG
  iter.vert = OUT_OF_CONTEXT;
#endif
  return weld_iter_corner_of_face_next(iter);
}

/**
 * Build the context weld faces and weld corners.
 *
 * \return r_weld_mesh: Corner and face members will be allocated here.
 */
static void weld_face_corner_ctx_alloc(const OffsetIndices<int> faces,
                                       const Span<int> corner_verts,
                                       const Span<int> corner_edges,
                                       WeldMesh *r_weld_mesh)
{
  Span<int> vert_src_to_target = r_weld_mesh->vert_src_to_target;
  Span<int> edge_src_to_target = r_weld_mesh->edge_src_to_target;

  /* Corner/Face Context. */
  Array<int> corner_to_weld_corner(corner_verts.size());
  Array<int> face_to_weld_face(faces.size());
  int weld_corners_num = 0;
  int weld_faces_num = 0;
  int max_ctx_face_size = 4;

  Vector<WeldCorner> weld_corners;
  weld_corners.reserve(corner_verts.size());

  Vector<WeldFace> weld_faces;
  weld_faces.reserve(faces.size());

  int maybe_new_faces_num = 0;

  for (const int i : faces.index_range()) {
    const int corner_start = faces[i].start();
    const int face_size = faces[i].size();
    const int corner_end = corner_start + face_size - 1;
    int vert_first = corner_verts[corner_start];
    int vert_target_first = vert_src_to_target[vert_first];
    bool is_vert_first_ctx = vert_target_first != OUT_OF_CONTEXT;

    int vert_next = vert_first;
    int vert_target_next = vert_target_first;
    bool is_vert_next_ctx = is_vert_first_ctx;

    int prev_weld_corners_num = weld_corners_num;
    for (const int corner_src : faces[i]) {
      int vert = vert_next;
      int vert_target = vert_target_next;
      bool is_vert_ctx = is_vert_next_ctx;

      int corner_next;
      if (corner_src != corner_end) {
        corner_next = corner_src + 1;
        vert_next = corner_verts[corner_next];
        vert_target_next = vert_src_to_target[vert_next];
        is_vert_next_ctx = vert_target_next != OUT_OF_CONTEXT;
      }
      else {
        corner_next = corner_start;
        vert_next = vert_first;
        vert_target_next = vert_target_first;
        is_vert_next_ctx = is_vert_first_ctx;
      }

      if (is_vert_ctx || is_vert_next_ctx) {
        weld_corners.increase_size_by_unchecked(1);
        WeldCorner &weld_corner = weld_corners.last();
        weld_corner.vert = is_vert_ctx ? vert_target : vert;
        weld_corner.edge = edge_src_to_target[corner_edges[corner_src]];
        weld_corner.corner_src = corner_src;
        weld_corner.corner_next = corner_next;

        corner_to_weld_corner[corner_src] = weld_corners_num++;
      }
      else {
        corner_to_weld_corner[corner_src] = OUT_OF_CONTEXT;
      }
    }

    if (weld_corners_num != prev_weld_corners_num) {
      int corner_ctx_num = weld_corners_num - prev_weld_corners_num;
      weld_faces.increase_size_by_unchecked(1);

      WeldFace &weld_face = weld_faces.last();
      weld_face.face_dst = OUT_OF_CONTEXT;
      weld_face.face_src = i;
      weld_face.corner_start = corner_start;
      weld_face.corner_end = corner_end;

      weld_face.corner_ctx_start = prev_weld_corners_num;
      weld_face.corner_ctx_num = corner_ctx_num;

#ifdef USE_WELD_DEBUG
      weld_face.corners_num = face_size;
#endif

      face_to_weld_face[i] = weld_faces_num++;
      if (face_size > 5 && corner_ctx_num > 1) {
        /* We could be smarter here and actually count how many new faces will be created.
         * But counting this can be inefficient as it depends on the number of non-consecutive
         * self face merges. For now just estimate a maximum value. */
        int max_new = std::min((face_size / 3), corner_ctx_num) - 1;
        maybe_new_faces_num += max_new;
        CLAMP_MIN(max_ctx_face_size, face_size);
      }
    }
    else {
      face_to_weld_face[i] = OUT_OF_CONTEXT;
    }
  }

  weld_faces.reserve(weld_faces.size() + maybe_new_faces_num);

  r_weld_mesh->weld_corners = std::move(weld_corners);
  r_weld_mesh->weld_faces = std::move(weld_faces);
  r_weld_mesh->new_faces_num = 0;
  r_weld_mesh->corner_to_weld_corner = std::move(corner_to_weld_corner);
  r_weld_mesh->face_to_weld_face = std::move(face_to_weld_face);
  r_weld_mesh->max_face_size = max_ctx_face_size;
}

static void weld_face_split_recursive(int face_size,
                                      Span<int> vert_src_to_target,
                                      WeldFace *r_wp,
                                      WeldMesh *r_weld_mesh,
                                      int *r_removed_faces_num,
                                      int *r_removed_corners_num)
{
  if (face_size < 3) {
    return;
  }

  Span<int> corner_to_weld_corner = r_weld_mesh->corner_to_weld_corner;
  MutableSpan<WeldCorner> weld_corners = r_weld_mesh->weld_corners;

  int removed_corners_num = 0;

  int corner_end = r_wp->corner_end;
  int corner_ctx_a = corner_to_weld_corner[corner_end];
  WeldCorner *weld_corner_a_prev = (corner_ctx_a != OUT_OF_CONTEXT) ? &weld_corners[corner_ctx_a] :
                                                                      nullptr;
  int corner_a = r_wp->corner_start;
  do {
    int corner_ctx_a = corner_to_weld_corner[corner_a];
    if (corner_ctx_a == OUT_OF_CONTEXT) {
      corner_a++;
      weld_corner_a_prev = nullptr;
      continue;
    }

    WeldCorner *weld_corner_a = &weld_corners[corner_ctx_a];
    BLI_assert(weld_corner_a->flag != ELEM_COLLAPSED);

    int vert_a = weld_corner_a->vert;
    if (vert_src_to_target[vert_a] == OUT_OF_CONTEXT) {
      /* Only test vertices that will be merged. */
      corner_a = weld_corner_a->corner_next;
      weld_corner_a_prev = weld_corner_a;
      continue;
    }

    int dist_a = 1;
    int lb_prev = corner_a;
    WeldCorner *weld_corner_b_prev = weld_corner_a;
    int corner_b = weld_corner_a->corner_next;
    do {
      int corner_ctx_b = corner_to_weld_corner[corner_b];
      if (corner_ctx_b == OUT_OF_CONTEXT) {
        dist_a++;
        lb_prev = corner_b;
        weld_corner_b_prev = nullptr;
        corner_b++;
        continue;
      }

      WeldCorner *weld_corner_b = &weld_corners[corner_ctx_b];
      BLI_assert(weld_corner_b->flag != ELEM_COLLAPSED);
      int vert_b = weld_corner_b->vert;
      if (vert_a != vert_b) {
        dist_a++;
        lb_prev = corner_b;
        weld_corner_b_prev = weld_corner_b;
        corner_b = weld_corner_b->corner_next;
        continue;
      }

      int dist_b = face_size - dist_a;

      BLI_assert(dist_a != 0 && dist_b != 0);
      if (dist_a == 1 || dist_b == 1) {
        BLI_assert(dist_a != dist_b);
        BLI_assert((weld_corner_a->flag == ELEM_COLLAPSED) ||
                   (weld_corner_b->flag == ELEM_COLLAPSED));
      }
      else if (dist_a == 2 && dist_b == 2) {
        /* All corners are "collapsed".
         * They could be flagged, but just the face is enough.
         *
         * \code{.cc}
         * WeldCorner *weld_corner_a_prev = &weld_corners[corner_ctx_a_prev];
         * WeldCorner *weld_corner_b_prev = &weld_corners[corner_ctx_b_prev];
         * weld_corner_a_prev->flag = ELEM_COLLAPSED;
         * weld_corner_a->flag = ELEM_COLLAPSED;
         * weld_corner_b_prev->flag = ELEM_COLLAPSED;
         * weld_corner_b->flag = ELEM_COLLAPSED;
         * \endcode */
        removed_corners_num += 4;
        dist_b = 0;
        r_wp->flag = ELEM_COLLAPSED;
        *r_removed_faces_num += 1;
        *r_removed_corners_num += removed_corners_num;
        /* Since all the corners are collapsed, avoid iterating through them.
         * This may result in wrong removed_faces_num counts. */
        return;
      }
      else {
        weld_corner_a_prev->corner_next = corner_b;
        weld_corner_b_prev->corner_next = corner_a;
        if (r_wp->corner_start == corner_a) {
          r_wp->corner_start = corner_b;
        }

        if (dist_a == 2) {
          BLI_assert(weld_corner_b_prev->flag != ELEM_COLLAPSED);
          weld_corner_a->flag = ELEM_COLLAPSED;
          weld_corner_b_prev->flag = ELEM_COLLAPSED;
          removed_corners_num += 2;
        }
        else if (dist_b == 2) {
          BLI_assert(weld_corner_a_prev->flag != ELEM_COLLAPSED);
          weld_corner_b->flag = ELEM_COLLAPSED;
          weld_corner_a_prev->flag = ELEM_COLLAPSED;
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
          new_test->corner_ctx_start = r_wp->corner_ctx_start;
          new_test->corner_ctx_num = r_wp->corner_ctx_num;

#ifdef USE_WELD_DEBUG
          new_test->corners_num = dist_a;
#endif
          weld_face_split_recursive(dist_a,
                                    vert_src_to_target,
                                    new_test,
                                    r_weld_mesh,
                                    r_removed_faces_num,
                                    r_removed_corners_num);
        }

        corner_a = corner_b;
        weld_corner_a = weld_corner_b;
        face_size = dist_b;

        dist_a = 1;
      }

      weld_corner_b_prev = weld_corner_b;
      lb_prev = corner_b;
      corner_b = weld_corner_b->corner_next;
    } while (lb_prev != corner_end);

    weld_corner_a_prev = weld_corner_a;
    if (corner_a == corner_end) {
      /* No need to start again. */
      break;
    }
    corner_a = weld_corner_a->corner_next;
  } while (corner_a != corner_end);

  *r_removed_corners_num += removed_corners_num;
#ifdef USE_WELD_DEBUG
  r_wp->corners_num = face_size;
  weld_assert_face_no_vert_repetition(r_wp,
                                      weld_corners,
                                      r_weld_mesh->corner_verts,
                                      r_weld_mesh->corner_edges,
                                      r_weld_mesh->corner_to_weld_corner);
#endif
}

/**
 * Build the context weld faces and weld corners.
 *
 * \param remaining_edge_ctx_num: Context weld edges that won't be destroyed by merging.
 * \return r_weld_mesh: Loop and face members will be configured here.
 */
static void weld_face_corner_ctx_setup_collapsed_and_split(const int remaining_edge_ctx_num,
                                                           WeldMesh *r_weld_mesh)
{
  if (remaining_edge_ctx_num == 0) {
    r_weld_mesh->removed_faces_num = r_weld_mesh->weld_faces.size();
    r_weld_mesh->removed_corners_num = r_weld_mesh->weld_corners.size();

    for (WeldFace &weld_face : r_weld_mesh->weld_faces) {
      weld_face.flag = ELEM_COLLAPSED;
    }

    return;
  }

  WeldFace *weld_faces = r_weld_mesh->weld_faces.data();
  MutableSpan<WeldCorner> weld_corners = r_weld_mesh->weld_corners;
  Span<int> corner_to_weld_corner = r_weld_mesh->corner_to_weld_corner;
  Span<int> vert_src_to_target = r_weld_mesh->vert_src_to_target;

  int removed_faces_num = 0;
  int removed_corners_num = 0;

  /* Setup Face/Corner. */
  /* `weld_faces.size()` may change while iterating, so make it clear that only the items that
   * already exist are visited. */
  IndexRange weld_faces_src_range = r_weld_mesh->weld_faces.index_range();
  for (const int i : weld_faces_src_range) {
    WeldFace &weld_face = weld_faces[i];
    int face_size = (weld_face.corner_end - weld_face.corner_start) + 1;
    WeldCorner *weld_corner_prev = nullptr;
    bool changed_corner_start = false;
    int corner = weld_face.corner_start;
    do {
      int corner_ctx = corner_to_weld_corner[corner];
      if (corner_ctx == OUT_OF_CONTEXT) {
        weld_corner_prev = nullptr;
        continue;
      }

      WeldCorner *weld_corner = &weld_corners[corner_ctx];
      const int edge_target = weld_corner->edge;
      if (edge_target == ELEM_COLLAPSED) {
        weld_corner->flag = ELEM_COLLAPSED;
        if (face_size == 3) {
          weld_face.flag = ELEM_COLLAPSED;
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
        if (weld_corner_prev) {
          weld_corner_prev->corner_next = corner;
        }
        weld_corner_prev = weld_corner;
        BLI_assert(weld_corner->corner_next == corner + 1 || corner == weld_face.corner_end);
      }
    } while (corner++ != weld_face.corner_end);

    if (face_size) {
      if (weld_corner_prev) {
        weld_corner_prev->corner_next = weld_face.corner_start;
        weld_face.corner_end = weld_corner_prev->corner_src;
      }

#ifdef USE_WELD_DEBUG
      weld_face.corners_num = face_size;

      for (int corner_src : IndexRange(weld_face.corner_start, face_size)) {
        int corner_ctx = corner_to_weld_corner[corner_src];
        if (corner_ctx == OUT_OF_CONTEXT) {
          continue;
        }

        WeldCorner *weld_corner = &weld_corners[corner_ctx];
        if (weld_corner->flag == ELEM_COLLAPSED) {
          continue;
        }

        corner_ctx = corner_to_weld_corner[weld_corner->corner_next];
        if (corner_ctx == OUT_OF_CONTEXT) {
          continue;
        }

        weld_corner = &weld_corners[corner_ctx];
        BLI_assert(weld_corner->flag != ELEM_COLLAPSED);
      }
#endif

      weld_face_split_recursive(face_size,
                                vert_src_to_target,
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
      r_weld_mesh, r_weld_mesh->removed_faces_num, r_weld_mesh->removed_corners_num);
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

static void weld_face_find_doubles(const Span<int> corner_verts,
                                   const Span<int> corner_edges,
                                   const int src_edges_num,
                                   WeldMesh *r_weld_mesh)
{
  if (r_weld_mesh->removed_faces_num == r_weld_mesh->weld_faces.size()) {
    return;
  }

  WeldFace *weld_faces = r_weld_mesh->weld_faces.data();
  MutableSpan<WeldCorner> weld_corners = r_weld_mesh->weld_corners;
  Span<int> corner_to_weld_corner = r_weld_mesh->corner_to_weld_corner;
  int face_index = 0;

  const int face_size = r_weld_mesh->weld_faces.size();
  Array<int> face_offsets_(face_size + 1);
  Vector<int> new_corner_edges;
  new_corner_edges.reserve(corner_verts.size() - r_weld_mesh->removed_corners_num);

  for (const WeldFace &weld_face : r_weld_mesh->weld_faces) {
    face_offsets_[face_index++] = new_corner_edges.size();

    WeldCornerOfFaceIter iter;
    if (!weld_iter_corner_of_face_begin(iter,
                                        weld_face,
                                        weld_corners,
                                        corner_verts,
                                        corner_edges,
                                        corner_to_weld_corner,
                                        nullptr))
    {
      continue;
    }

    if (weld_face.face_dst != OUT_OF_CONTEXT) {
      continue;
    }

    do {
      new_corner_edges.append(iter.edge);
    } while (weld_iter_corner_of_face_next(iter));
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
      r_weld_mesh, r_weld_mesh->removed_faces_num, r_weld_mesh->removed_corners_num);
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh API
 * \{ */

/**
 * \param vert_src_to_target: The vertex map result map, but with vertices that aren't involved in
 * the merge set to #OUT_OF_CONTEXT (see #vert_src_to_target_with_context).
 */
static void weld_mesh_context_create(const Mesh &mesh,
                                     const Span<int> vert_src_to_target,
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

#ifdef USE_WELD_DEBUG
  r_weld_mesh->corner_verts = corner_verts;
  r_weld_mesh->corner_edges = corner_edges;
  r_weld_mesh->faces = faces;
#endif

  int collapsed_edges_num, removed_double_edges_num;
  Vector<WeldEdge> weld_edges = weld_edges_build_and_find_collapsed(
      edges, vert_src_to_target, r_weld_mesh->edge_src_to_target, &collapsed_edges_num);

  weld_edge_find_doubles(
      weld_edges, mesh.verts_num, r_weld_mesh->edge_src_to_target, &removed_double_edges_num);

  r_weld_mesh->removed_edges_num = collapsed_edges_num + removed_double_edges_num;

#ifdef USE_WELD_DEBUG
  weld_assert_removed_edges_num(r_weld_mesh->edge_src_to_target, r_weld_mesh->removed_edges_num);
#endif

  weld_face_corner_ctx_alloc(faces, corner_verts, corner_edges, r_weld_mesh);

  weld_face_corner_ctx_setup_collapsed_and_split(weld_edges.size() - removed_double_edges_num,
                                                 r_weld_mesh);

  weld_face_find_doubles(corner_verts, corner_edges, edges.size(), r_weld_mesh);
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
  survivors.foreach_index_optimized<int>(
      [&](const int src, const int dst) { src_to_dst[src] = dst; }, exec_mode::grain_size(4096));

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
                                const bool do_mix_data)
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

  const Array<int> vert_src_to_target_ctx = vert_src_to_target_with_context(vert_src_to_target);

  WeldMesh weld_mesh;
  weld_mesh_context_create(mesh, vert_src_to_target_ctx, removed_vertex_count, &weld_mesh);

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

  mix_attributes(
      src_attributes, edge_dst_to_src, bke::AttrDomain::Edge, {".edge_verts"}, dst_attributes);
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

  int r_i = 0;
  int dst_corner = 0;
  Vector<bool> dst_face_unaffected;
  dst_face_unaffected.reserve(dst_faces_num);
  Vector<int> dst_to_src_faces;
  dst_to_src_faces.reserve(dst_faces_num);
  Array<int, 64> group_buffer(weld_mesh.max_face_size);
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
      WeldCornerOfFaceIter iter;
      if (!weld_iter_corner_of_face_begin(iter,
                                          weld_face,
                                          weld_mesh.weld_corners,
                                          src_corner_verts,
                                          src_corner_edges,
                                          weld_mesh.corner_to_weld_corner,
                                          group_buffer.data()))
      {
        continue;
      }

      if (weld_face.face_dst != OUT_OF_CONTEXT) {
        continue;
      }
      dst_face_unaffected.append_unchecked(false);
      do {
        corner_src_index_offset_data.append_unchecked(corner_src_index_data.size());
        corner_src_index_data.extend(Span(group_buffer.data(), iter.group_size));
        dst_corner_verts[dst_corner] = vert_src_to_dst[iter.vert];
        dst_corner_edges[dst_corner] = edge_src_to_dst[iter.edge];
        dst_corner++;
      } while (weld_iter_corner_of_face_next(iter));
    }

    dst_to_src_faces.append_unchecked(i);
    dst_face_offsets[r_i] = corner_start;
    r_i++;
  }

  /* New Polygons.
   * NOTE: The number of "src" and "new" faces might not match `new_faces_num`. */
  for (const int i : weld_mesh.weld_faces.index_range().take_back(weld_mesh.new_faces_num)) {
    const WeldFace &weld_face = weld_mesh.weld_faces[i];
    const int corner_start = dst_corner;
    WeldCornerOfFaceIter iter;
    if (!weld_iter_corner_of_face_begin(iter,
                                        weld_face,
                                        weld_mesh.weld_corners,
                                        src_corner_verts,
                                        src_corner_edges,
                                        weld_mesh.corner_to_weld_corner,
                                        group_buffer.data()))
    {
      continue;
    }

    if (weld_face.face_dst != OUT_OF_CONTEXT) {
      continue;
    }
    do {
      corner_src_index_offset_data.append_unchecked(corner_src_index_data.size());
      corner_src_index_data.extend(Span(group_buffer.data(), iter.group_size));
      dst_corner_verts[dst_corner] = vert_src_to_dst[iter.vert];
      dst_corner_edges[dst_corner] = edge_src_to_dst[iter.edge];
      dst_corner++;
    } while (weld_iter_corner_of_face_next(iter));

    dst_face_offsets[r_i] = corner_start;
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
                                                 const float merge_distance)
{
  Array<int> vert_src_to_target(mesh.verts_num, OUT_OF_CONTEXT);

  KDTree<float3> *tree = kdtree_new<float3>(selection.size());

  const Span<float3> positions = mesh.vert_positions();
  selection.foreach_index([&](const int64_t i) { kdtree_insert<float3>(tree, i, positions[i]); });

  kdtree_balance<float3>(tree);
  const int removed_verts_num = kdtree_calc_duplicates_fast<float3>(
      tree, merge_distance, true, vert_src_to_target.data());
  kdtree_free<float3>(tree);

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

  return create_merged_mesh(mesh, vert_src_to_target, removed_verts_num, true);
}

struct WeldVertexCluster {
  float co[3];
  int merged_verts;
};

std::optional<Mesh *> mesh_merge_by_distance_connected(const Mesh &mesh,
                                                       Span<bool> selection,
                                                       const float merge_distance,
                                                       const bool only_loose_edges)
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

  return create_merged_mesh(mesh, vert_src_to_target, removed_verts_num, true);
}

Mesh *mesh_merge_verts(const Mesh &mesh,
                       const Span<int> vert_src_to_target,
                       const int removed_verts_num,
                       const bool do_mix_data)
{
  BLI_assert(vert_src_to_target.size() == mesh.verts_num);
  return create_merged_mesh(mesh, vert_src_to_target, removed_verts_num, do_mix_data);
}

/** \} */

Mesh *mesh_merge_verts(const Mesh &mesh,
                       const IndexMask &selection,
                       const Span<int> merge_ids,
                       const bke::AttributeFilter & /*attribute_filter*/)
{
  VectorSet<int> group_indices;
  selection.foreach_index_optimized<int>([&](const int i) { group_indices.add(merge_ids[i]); });

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

  return create_merged_mesh(
      mesh, vert_src_to_target, selection.size() - group_indices.size(), true);
}

}  // namespace blender::geometry
