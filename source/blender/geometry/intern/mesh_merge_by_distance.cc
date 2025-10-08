/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

// #define USE_WELD_DEBUG
// #define USE_WELD_DEBUG_TIME

#include "BKE_attribute_math.hh"
#include "BLI_array.hh"
#include "BLI_bit_vector.hh"
#include "BLI_index_mask.hh"
#include "BLI_kdtree.h"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_offset_indices.hh"
#include "BLI_vector.hh"

#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_mesh.hh"
#include "DNA_meshdata_types.h"

#include "DNA_object_types.h"
#include "GEO_mesh_merge_by_distance.hh"
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
  /* Indices relative to the original Mesh. */
  int edge_orig;
  int vert_a;
  int vert_b;
};

struct WeldLoop {
  union {
    int flag;
    struct {
      /* Indices relative to the original Mesh. */
      int edge;
      int vert;
      int loop_orig;
      int loop_next;
    };
  };
};

struct WeldPoly {
  union {
    int flag;
    struct {
      /* Indices relative to the original Mesh. */
      int poly_dst;
      int poly_orig;
      int loop_start;
      int loop_end;

      /* To find groups. */
      int loop_ctx_start;
      int loop_ctx_len;
#ifdef USE_WELD_DEBUG
      int loop_len;
#endif
    };
  };
};

struct WeldMesh {
  /* These vectors indicate the index of elements that will participate in the creation of groups.
   * These groups are used in customdata interpolation (`do_mix_data`). */
  Vector<int> double_verts;
  Vector<int> double_edges;

  /* Group of edges to be merged. */
  Array<int> edge_dest_map;
  Span<int> vert_dest_map;

  /* References all polygons and loops that will be affected. */
  Vector<WeldLoop> wloop;
  Vector<WeldPoly> wpoly;
  int wpoly_new_len;

  /* From the actual index of the element in the mesh, it indicates what is the index of the Weld
   * element above. */
  Array<int> loop_map;
  Array<int> face_map;

  int vert_kill_len;
  int edge_kill_len;
  int loop_kill_len;
  int face_kill_len; /* Including the new polygons. */

  /* Size of the affected face with more sides. */
  int max_face_len;

#ifdef USE_WELD_DEBUG
  Span<int> corner_verts;
  Span<int> corner_edges;
  OffsetIndices<int> faces;
#endif
};

struct WeldLoopOfPolyIter {
  int loop_iter;
  int loop_end;

  /* Weld group. */
  int loop_ctx_start;
  int loop_ctx_len;
  int *group;

  Span<WeldLoop> wloop;
  Span<int> corner_verts;
  Span<int> corner_edges;
  Span<int> loop_map;

  /* Return */
  int group_len;
  int v;
  int e;
};

/* -------------------------------------------------------------------- */
/** \name Debug Utils
 * \{ */

#ifdef USE_WELD_DEBUG
static bool weld_iter_loop_of_poly_begin(WeldLoopOfPolyIter &iter,
                                         const WeldPoly &wp,
                                         Span<WeldLoop> wloop,
                                         const Span<int> corner_verts,
                                         const Span<int> corner_edges,
                                         Span<int> loop_map,
                                         int *group_buffer);

static bool weld_iter_loop_of_poly_next(WeldLoopOfPolyIter &iter);

static void weld_assert_edge_kill_len(Span<int> edge_dest_map, const int expected_kill_len)
{
  int kills = 0;
  for (const int edge_orig : edge_dest_map.index_range()) {
    if (!ELEM(edge_dest_map[edge_orig], edge_orig, OUT_OF_CONTEXT)) {
      kills++;
    }
  }
  BLI_assert(kills == expected_kill_len);
}

static void weld_assert_poly_and_loop_kill_len(WeldMesh *weld_mesh,
                                               const int expected_faces_kill_len,
                                               const int expected_loop_kill_len)
{
  const Span<int> corner_verts = weld_mesh->corner_verts;
  const Span<int> corner_edges = weld_mesh->corner_edges;
  const OffsetIndices<int> faces = weld_mesh->faces;

  int poly_kills = 0;
  int loop_kills = corner_verts.size();
  for (const int i : faces.index_range()) {
    int poly_ctx = weld_mesh->face_map[i];
    if (poly_ctx != OUT_OF_CONTEXT) {
      const WeldPoly *wp = &weld_mesh->wpoly[poly_ctx];
      WeldLoopOfPolyIter iter;
      if (!weld_iter_loop_of_poly_begin(iter,
                                        *wp,
                                        weld_mesh->wloop,
                                        corner_verts,
                                        corner_edges,
                                        weld_mesh->loop_map,
                                        nullptr))
      {
        poly_kills++;
        continue;
      }
      else {
        if (wp->poly_dst != OUT_OF_CONTEXT) {
          poly_kills++;
          continue;
        }
        int remain = wp->loop_len;
        int l = wp->loop_start;
        while (remain) {
          int l_next = l + 1;
          int loop_ctx = weld_mesh->loop_map[l];
          if (loop_ctx != OUT_OF_CONTEXT) {
            const WeldLoop *wl = &weld_mesh->wloop[loop_ctx];
            if (wl->flag != ELEM_COLLAPSED) {
              loop_kills--;
              remain--;
            }
          }
          else {
            loop_kills--;
            remain--;
          }
          l = l_next;
        }
      }
    }
    else {
      loop_kills -= faces[i].size();
    }
  }

  for (const int i : weld_mesh->wpoly.index_range().take_back(weld_mesh->wpoly_new_len)) {
    const WeldPoly &wp = weld_mesh->wpoly[i];
    if (wp.poly_dst != OUT_OF_CONTEXT) {
      poly_kills++;
      continue;
    }
    int remain = wp.loop_len;
    int l = wp.loop_start;
    while (remain) {
      int l_next = l + 1;
      int loop_ctx = weld_mesh->loop_map[l];
      if (loop_ctx != OUT_OF_CONTEXT) {
        const WeldLoop *wl = &weld_mesh->wloop[loop_ctx];
        if (wl->flag != ELEM_COLLAPSED) {
          loop_kills--;
          remain--;
        }
      }
      else {
        loop_kills--;
        remain--;
      }
      l = l_next;
    }
  }

  BLI_assert(poly_kills == expected_faces_kill_len);
  BLI_assert(loop_kills == expected_loop_kill_len);
}

static void weld_assert_poly_no_vert_repetition(const WeldPoly *wp,
                                                Span<WeldLoop> wloop,
                                                const Span<int> corner_verts,
                                                const Span<int> corner_edges,
                                                Span<int> loop_map)
{
  int i = 0;
  if (wp->loop_len == 0) {
    BLI_assert(wp->flag == ELEM_COLLAPSED);
    return;
  }

  Array<int, 64> verts(wp->loop_len);
  WeldLoopOfPolyIter iter;
  if (!weld_iter_loop_of_poly_begin(
          iter, *wp, wloop, corner_verts, corner_edges, loop_map, nullptr))
  {
    return;
  }
  else {
    do {
      verts[i++] = iter.v;
    } while (weld_iter_loop_of_poly_next(iter));
  }

  BLI_assert(i == wp->loop_len);

  for (i = 0; i < wp->loop_len; i++) {
    int va = verts[i];
    for (int j = i + 1; j < wp->loop_len; j++) {
      int vb = verts[j];
      BLI_assert(va != vb);
    }
  }
}

#endif /* USE_WELD_DEBUG */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vert API
 * \{ */

/**
 * Create a Weld Verts Context.
 *
 * \return array with the context weld vertices.
 */
static Vector<int> weld_vert_ctx_alloc_and_setup(MutableSpan<int> vert_dest_map,
                                                 const int vert_kill_len)
{
  Vector<int> wvert;
  wvert.reserve(std::min<int>(2 * vert_kill_len, vert_dest_map.size()));

  for (const int i : vert_dest_map.index_range()) {
    if (vert_dest_map[i] != OUT_OF_CONTEXT) {
      const int vert_dest = vert_dest_map[i];
      wvert.append(i);

      if (vert_dest_map[vert_dest] != vert_dest) {
        /* The target vertex is also part of the context and needs to be referenced.
         * #vert_dest_map could already indicate this from the beginning, but for better
         * compatibility, it is done here as well. */
        vert_dest_map[vert_dest] = vert_dest;
        wvert.append(vert_dest);
      }
    }
  }
  return wvert;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edge API
 * \{ */

/**
 * Alloc Weld Edges.
 *
 * \return r_edge_dest_map: First step to create map of indices pointing edges that will be merged.
 */
static Vector<WeldEdge> weld_edge_ctx_alloc_and_find_collapsed(Span<int2> edges,
                                                               Span<int> vert_dest_map,
                                                               MutableSpan<int> r_edge_dest_map,
                                                               int *r_edge_collapsed_len)
{
  /* Edge Context. */
  int edge_collapsed_len = 0;

  Vector<WeldEdge> wedge;
  wedge.reserve(edges.size());

  for (const int i : edges.index_range()) {
    int v1 = edges[i][0];
    int v2 = edges[i][1];
    int v_dest_1 = vert_dest_map[v1];
    int v_dest_2 = vert_dest_map[v2];
    if (v_dest_1 == OUT_OF_CONTEXT && v_dest_2 == OUT_OF_CONTEXT) {
      r_edge_dest_map[i] = OUT_OF_CONTEXT;
      continue;
    }

    const int vert_a = (v_dest_1 == OUT_OF_CONTEXT) ? v1 : v_dest_1;
    const int vert_b = (v_dest_2 == OUT_OF_CONTEXT) ? v2 : v_dest_2;

    if (vert_a == vert_b) {
      r_edge_dest_map[i] = ELEM_COLLAPSED;
      edge_collapsed_len++;
    }
    else {
      wedge.append({i, vert_a, vert_b});
      r_edge_dest_map[i] = i;
    }
  }

  *r_edge_collapsed_len = edge_collapsed_len;
  return wedge;
}

/**
 * Fills `r_edge_dest_map` indicating the duplicated edges.
 *
 * \param weld_edges: Candidate edges for merging (edges that don't collapse and that have at least
 *                    one weld vertex).
 *
 * \param r_edge_dest_map: Resulting map of indices pointing the source edges to each target.
 * \param r_edge_double_kill_len: Resulting number of duplicate edges to be destroyed.
 */
static void weld_edge_find_doubles(Span<WeldEdge> weld_edges,
                                   int mvert_num,
                                   MutableSpan<int> r_edge_dest_map,
                                   int *r_edge_double_kill_len)
{
  /* Setup Edge Overlap. */
  int edge_double_kill_len = 0;

  if (weld_edges.is_empty()) {
    *r_edge_double_kill_len = edge_double_kill_len;
    return;
  }

  /* Add +1 to allow calculation of the length of the last group. */
  Array<int> v_links(mvert_num + 1, 0);

  for (const WeldEdge &we : weld_edges) {
    BLI_assert(r_edge_dest_map[we.edge_orig] != ELEM_COLLAPSED);
    BLI_assert(we.vert_a != we.vert_b);
    v_links[we.vert_a]++;
    v_links[we.vert_b]++;
  }

  int link_len = 0;
  for (const int i : IndexRange(mvert_num)) {
    link_len += v_links[i];
    v_links[i] = link_len;
  }
  v_links.last() = link_len;

  BLI_assert(link_len > 0);
  Array<int> link_edge_buffer(link_len);

  /* Use a reverse for loop to ensure that indexes are assigned in ascending order. */
  for (int i = weld_edges.size(); i--;) {
    const WeldEdge &we = weld_edges[i];
    BLI_assert(r_edge_dest_map[we.edge_orig] != ELEM_COLLAPSED);
    int dst_vert_a = we.vert_a;
    int dst_vert_b = we.vert_b;

    link_edge_buffer[--v_links[dst_vert_a]] = i;
    link_edge_buffer[--v_links[dst_vert_b]] = i;
  }

  for (const int i : weld_edges.index_range()) {
    const WeldEdge &we = weld_edges[i];
    BLI_assert(r_edge_dest_map[we.edge_orig] != OUT_OF_CONTEXT);
    if (r_edge_dest_map[we.edge_orig] != we.edge_orig) {
      /* Already a duplicate. */
      continue;
    }

    int dst_vert_a = we.vert_a;
    int dst_vert_b = we.vert_b;

    const int link_a = v_links[dst_vert_a];
    const int link_b = v_links[dst_vert_b];

    int edges_len_a = v_links[dst_vert_a + 1] - link_a;
    int edges_len_b = v_links[dst_vert_b + 1] - link_b;

    int edge_orig = we.edge_orig;
    if (edges_len_a <= 1 || edges_len_b <= 1) {
      /* This edge would form a group with only one element.
       * For better performance, mark these edges and avoid forming these groups. */
      r_edge_dest_map[edge_orig] = OUT_OF_CONTEXT;
      continue;
    }

    int *edges_ctx_a = &link_edge_buffer[link_a];
    int *edges_ctx_b = &link_edge_buffer[link_b];

    const int edge_double_len_prev = edge_double_kill_len;
    for (; edges_len_a--; edges_ctx_a++) {
      int e_ctx_a = *edges_ctx_a;
      if (e_ctx_a == i) {
        continue;
      }
      while (edges_len_b && *edges_ctx_b < e_ctx_a) {
        edges_ctx_b++;
        edges_len_b--;
      }
      if (edges_len_b == 0) {
        break;
      }
      int e_ctx_b = *edges_ctx_b;
      if (e_ctx_a == e_ctx_b) {
        const WeldEdge &we_b = weld_edges[e_ctx_b];
        BLI_assert(ELEM(we_b.vert_a, dst_vert_a, dst_vert_b));
        BLI_assert(ELEM(we_b.vert_b, dst_vert_a, dst_vert_b));
        BLI_assert(we_b.edge_orig != edge_orig);
        BLI_assert(r_edge_dest_map[we_b.edge_orig] == we_b.edge_orig);
        r_edge_dest_map[we_b.edge_orig] = edge_orig;
        edge_double_kill_len++;
      }
    }
    if (edge_double_len_prev == edge_double_kill_len) {
      /* This edge would form a group with only one element.
       * For better performance, mark these edges and avoid forming these groups. */
      r_edge_dest_map[edge_orig] = OUT_OF_CONTEXT;
    }
  }

  *r_edge_double_kill_len = edge_double_kill_len;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Poly and Loop API
 * \{ */

static bool weld_iter_loop_of_poly_next(WeldLoopOfPolyIter &iter)
{
  if (iter.loop_iter > iter.loop_end) {
    return false;
  }

  Span<WeldLoop> wloop = iter.wloop;
  Span<int> loop_map = iter.loop_map;
  int l = iter.loop_iter;
  int l_next = l + 1;

  int loop_ctx = loop_map[l];
  if (loop_ctx != OUT_OF_CONTEXT) {
    const WeldLoop *wl = &wloop[loop_ctx];
#ifdef USE_WELD_DEBUG
    BLI_assert(wl->flag != ELEM_COLLAPSED);
    BLI_assert(iter.v != wl->vert);
#endif
    iter.v = wl->vert;
    iter.e = wl->edge;
    if (wl->loop_next > l) {
      /* Allow the loop to break. */
      l_next = wl->loop_next;
    }

    if (iter.group) {
      iter.group_len = 0;
      int count = iter.loop_ctx_len;
      for (wl = &wloop[iter.loop_ctx_start]; count--; wl++) {
        if (wl->vert == iter.v) {
          iter.group[iter.group_len++] = wl->loop_orig;
        }
      }
    }
  }
  else {
#ifdef USE_WELD_DEBUG
    BLI_assert(iter.v != iter.corner_verts[l]);
#endif
    iter.v = iter.corner_verts[l];
    iter.e = iter.corner_edges[l];
    if (iter.group) {
      iter.group[0] = l;
      iter.group_len = 1;
    }
  }

  iter.loop_iter = l_next;
  return true;
}

static bool weld_iter_loop_of_poly_begin(WeldLoopOfPolyIter &iter,
                                         const WeldPoly &wp,
                                         Span<WeldLoop> wloop,
                                         const Span<int> corner_verts,
                                         const Span<int> corner_edges,
                                         Span<int> loop_map,
                                         int *group_buffer)
{
  if (wp.flag == ELEM_COLLAPSED) {
    return false;
  }

  iter.loop_iter = wp.loop_start;
  iter.loop_end = wp.loop_end;
  iter.loop_ctx_start = wp.loop_ctx_start;
  iter.loop_ctx_len = wp.loop_ctx_len;

  iter.wloop = wloop;
  iter.corner_verts = corner_verts;
  iter.corner_edges = corner_edges;
  iter.loop_map = loop_map;
  iter.group = group_buffer;
  iter.group_len = 0;

#ifdef USE_WELD_DEBUG
  iter.v = OUT_OF_CONTEXT;
#endif
  return weld_iter_loop_of_poly_next(iter);
}

/**
 * Alloc Weld Polygons and Weld Loops.
 *
 * \return r_weld_mesh: Loop and face members will be allocated here.
 */
static void weld_poly_loop_ctx_alloc(const OffsetIndices<int> faces,
                                     const Span<int> corner_verts,
                                     const Span<int> corner_edges,
                                     WeldMesh *r_weld_mesh)
{
  Span<int> vert_dest_map = r_weld_mesh->vert_dest_map;
  Span<int> edge_dest_map = r_weld_mesh->edge_dest_map;

  /* Loop/Poly Context. */
  Array<int> loop_map(corner_verts.size());
  Array<int> face_map(faces.size());
  int wloop_len = 0;
  int wpoly_len = 0;
  int max_ctx_poly_len = 4;

  Vector<WeldLoop> wloop;
  wloop.reserve(corner_verts.size());

  Vector<WeldPoly> wpoly;
  wpoly.reserve(faces.size());

  int maybe_new_poly = 0;

  for (const int i : faces.index_range()) {
    const int loopstart = faces[i].start();
    const int totloop = faces[i].size();
    const int loop_end = loopstart + totloop - 1;
    int v_first = corner_verts[loopstart];
    int v_dest_first = vert_dest_map[v_first];
    bool is_vert_first_ctx = v_dest_first != OUT_OF_CONTEXT;

    int v_next = v_first;
    int v_dest_next = v_dest_first;
    bool is_vert_next_ctx = is_vert_first_ctx;

    int prev_wloop_len = wloop_len;
    for (const int loop_orig : faces[i]) {
      int v = v_next;
      int v_dest = v_dest_next;
      bool is_vert_ctx = is_vert_next_ctx;

      int loop_next;
      if (loop_orig != loop_end) {
        loop_next = loop_orig + 1;
        v_next = corner_verts[loop_next];
        v_dest_next = vert_dest_map[v_next];
        is_vert_next_ctx = v_dest_next != OUT_OF_CONTEXT;
      }
      else {
        loop_next = loopstart;
        v_next = v_first;
        v_dest_next = v_dest_first;
        is_vert_next_ctx = is_vert_first_ctx;
      }

      if (is_vert_ctx || is_vert_next_ctx) {
        int e = corner_edges[loop_orig];
        int e_dest = edge_dest_map[e];
        bool is_edge_ctx = e_dest != OUT_OF_CONTEXT;

        wloop.increase_size_by_unchecked(1);
        WeldLoop &wl = wloop.last();
        wl.vert = is_vert_ctx ? v_dest : v;
        wl.edge = is_edge_ctx ? e_dest : e;
        wl.loop_orig = loop_orig;
        wl.loop_next = loop_next;

        loop_map[loop_orig] = wloop_len++;
      }
      else {
        loop_map[loop_orig] = OUT_OF_CONTEXT;
      }
    }

    if (wloop_len != prev_wloop_len) {
      int loop_ctx_len = wloop_len - prev_wloop_len;
      wpoly.increase_size_by_unchecked(1);

      WeldPoly &wp = wpoly.last();
      wp.poly_dst = OUT_OF_CONTEXT;
      wp.poly_orig = i;
      wp.loop_start = loopstart;
      wp.loop_end = loop_end;

      wp.loop_ctx_start = prev_wloop_len;
      wp.loop_ctx_len = loop_ctx_len;

#ifdef USE_WELD_DEBUG
      wp.loop_len = totloop;
#endif

      face_map[i] = wpoly_len++;
      if (totloop > 5 && loop_ctx_len > 1) {
        /* We could be smarter here and actually count how many new polygons will be created.
         * But counting this can be inefficient as it depends on the number of non-consecutive
         * self face merges. For now just estimate a maximum value. */
        int max_new = std::min((totloop / 3), loop_ctx_len) - 1;
        maybe_new_poly += max_new;
        CLAMP_MIN(max_ctx_poly_len, totloop);
      }
    }
    else {
      face_map[i] = OUT_OF_CONTEXT;
    }
  }

  wpoly.reserve(wpoly.size() + maybe_new_poly);

  r_weld_mesh->wloop = std::move(wloop);
  r_weld_mesh->wpoly = std::move(wpoly);
  r_weld_mesh->wpoly_new_len = 0;
  r_weld_mesh->loop_map = std::move(loop_map);
  r_weld_mesh->face_map = std::move(face_map);
  r_weld_mesh->max_face_len = max_ctx_poly_len;
}

static void weld_poly_split_recursive(int poly_loop_len,
                                      Span<int> vert_dest_map,
                                      WeldPoly *r_wp,
                                      WeldMesh *r_weld_mesh,
                                      int *r_poly_kill,
                                      int *r_loop_kill)
{
  if (poly_loop_len < 3) {
    return;
  }

  Span<int> loop_map = r_weld_mesh->loop_map;
  MutableSpan<WeldLoop> wloop = r_weld_mesh->wloop;

  int loop_kill = 0;

  int loop_end = r_wp->loop_end;
  int loop_ctx_a = loop_map[loop_end];
  WeldLoop *wla_prev = (loop_ctx_a != OUT_OF_CONTEXT) ? &wloop[loop_ctx_a] : nullptr;
  int la = r_wp->loop_start;
  do {
    int loop_ctx_a = loop_map[la];
    if (loop_ctx_a == OUT_OF_CONTEXT) {
      la++;
      wla_prev = nullptr;
      continue;
    }

    WeldLoop *wla = &wloop[loop_ctx_a];
    BLI_assert(wla->flag != ELEM_COLLAPSED);

    int vert_a = wla->vert;
    if (vert_dest_map[vert_a] == OUT_OF_CONTEXT) {
      /* Only test vertices that will be merged. */
      la = wla->loop_next;
      wla_prev = wla;
      continue;
    }

    int dist_a = 1;
    int lb_prev = la;
    WeldLoop *wlb_prev = wla;
    int lb = wla->loop_next;
    do {
      int loop_ctx_b = loop_map[lb];
      if (loop_ctx_b == OUT_OF_CONTEXT) {
        dist_a++;
        lb_prev = lb;
        wlb_prev = nullptr;
        lb++;
        continue;
      }

      WeldLoop *wlb = &wloop[loop_ctx_b];
      BLI_assert(wlb->flag != ELEM_COLLAPSED);
      int vert_b = wlb->vert;
      if (vert_a != vert_b) {
        dist_a++;
        lb_prev = lb;
        wlb_prev = wlb;
        lb = wlb->loop_next;
        continue;
      }

      int dist_b = poly_loop_len - dist_a;

      BLI_assert(dist_a != 0 && dist_b != 0);
      if (dist_a == 1 || dist_b == 1) {
        BLI_assert(dist_a != dist_b);
        BLI_assert((wla->flag == ELEM_COLLAPSED) || (wlb->flag == ELEM_COLLAPSED));
      }
      else if (dist_a == 2 && dist_b == 2) {
        /* All loops are "collapsed".
         * They could be flagged, but just the face is enough.
         *
         * \code{.cc}
         * WeldLoop *wla_prev = &wloop[loop_ctx_a_prev];
         * WeldLoop *wlb_prev = &wloop[loop_ctx_b_prev];
         * wla_prev->flag = ELEM_COLLAPSED;
         * wla->flag = ELEM_COLLAPSED;
         * wlb_prev->flag = ELEM_COLLAPSED;
         * wlb->flag = ELEM_COLLAPSED;
         * \endcode */
        loop_kill += 4;
        dist_b = 0;
        r_wp->flag = ELEM_COLLAPSED;
        *r_poly_kill += 1;
        *r_loop_kill += loop_kill;
        /* Since all the loops are collapsed, avoid looping through them.
         * This may result in wrong poly_kill counts. */
        return;
      }
      else {
        wla_prev->loop_next = lb;
        wlb_prev->loop_next = la;
        if (r_wp->loop_start == la) {
          r_wp->loop_start = lb;
        }

        if (dist_a == 2) {
          BLI_assert(wlb_prev->flag != ELEM_COLLAPSED);
          wla->flag = ELEM_COLLAPSED;
          wlb_prev->flag = ELEM_COLLAPSED;
          loop_kill += 2;
        }
        else if (dist_b == 2) {
          BLI_assert(wla_prev->flag != ELEM_COLLAPSED);
          wlb->flag = ELEM_COLLAPSED;
          wla_prev->flag = ELEM_COLLAPSED;
          loop_kill += 2;

          r_wp->loop_start = la;
          r_wp->loop_end = loop_end = lb_prev;

          poly_loop_len = dist_a;
          break;
        }
        else {
          r_weld_mesh->wpoly.increase_size_by_unchecked(1);
          r_weld_mesh->wpoly_new_len++;

          WeldPoly *new_test = &r_weld_mesh->wpoly.last();
          new_test->poly_dst = OUT_OF_CONTEXT;
          new_test->poly_orig = r_wp->poly_orig;
          new_test->loop_start = la;
          new_test->loop_end = lb_prev;
          new_test->loop_ctx_start = r_wp->loop_ctx_start;
          new_test->loop_ctx_len = r_wp->loop_ctx_len;

#ifdef USE_WELD_DEBUG
          new_test->loop_len = dist_a;
#endif
          weld_poly_split_recursive(
              dist_a, vert_dest_map, new_test, r_weld_mesh, r_poly_kill, r_loop_kill);
        }

        la = lb;
        wla = wlb;
        poly_loop_len = dist_b;

        dist_a = 1;
      }

      wlb_prev = wlb;
      lb_prev = lb;
      lb = wlb->loop_next;
    } while (lb_prev != loop_end);

    wla_prev = wla;
    if (la == loop_end) {
      /* No need to start again. */
      break;
    }
    la = wla->loop_next;
  } while (la != loop_end);

  *r_loop_kill += loop_kill;
#ifdef USE_WELD_DEBUG
  r_wp->loop_len = poly_loop_len;
  weld_assert_poly_no_vert_repetition(
      r_wp, wloop, r_weld_mesh->corner_verts, r_weld_mesh->corner_edges, r_weld_mesh->loop_map);
#endif
}

/**
 * Alloc Weld Polygons and Weld Loops.
 *
 * \param remain_edge_ctx_len: Context weld edges that won't be destroyed by merging.
 * \param r_vlinks: An uninitialized buffer used to compute groups of WeldPolys attached to each
 *                  weld target vertex. It doesn't need to be passed as a parameter but this is
 *                  done to reduce allocations.
 * \return r_weld_mesh: Loop and face members will be configured here.
 */
static void weld_poly_loop_ctx_setup_collapsed_and_split(const int remain_edge_ctx_len,
                                                         WeldMesh *r_weld_mesh)
{
  if (remain_edge_ctx_len == 0) {
    r_weld_mesh->face_kill_len = r_weld_mesh->wpoly.size();
    r_weld_mesh->loop_kill_len = r_weld_mesh->wloop.size();

    for (WeldPoly &wp : r_weld_mesh->wpoly) {
      wp.flag = ELEM_COLLAPSED;
    }

    return;
  }

  WeldPoly *wpoly = r_weld_mesh->wpoly.data();
  MutableSpan<WeldLoop> wloop = r_weld_mesh->wloop;
  Span<int> loop_map = r_weld_mesh->loop_map;
  Span<int> vert_dest_map = r_weld_mesh->vert_dest_map;

  int face_kill_len = 0;
  int loop_kill_len = 0;

  /* Setup Poly/Loop. */
  /* `wpoly.size()` may change during the loop,
   * so make it clear that we are only working with the original `wpoly` items. */
  IndexRange wpoly_original_range = r_weld_mesh->wpoly.index_range();
  for (const int i : wpoly_original_range) {
    WeldPoly &wp = wpoly[i];
    int poly_loop_len = (wp.loop_end - wp.loop_start) + 1;
    WeldLoop *wl_prev = nullptr;
    bool chang_loop_start = false;
    int l = wp.loop_start;
    do {
      int loop_ctx = loop_map[l];
      if (loop_ctx == OUT_OF_CONTEXT) {
        wl_prev = nullptr;
        continue;
      }

      WeldLoop *wl = &wloop[loop_ctx];
      const int edge_dest = wl->edge;
      if (edge_dest == ELEM_COLLAPSED) {
        wl->flag = ELEM_COLLAPSED;
        if (poly_loop_len == 3) {
          wp.flag = ELEM_COLLAPSED;
          face_kill_len++;
          loop_kill_len += 3;
          poly_loop_len = 0;
          break;
        }

        if (l == wp.loop_start) {
          chang_loop_start = true;
        }

        loop_kill_len++;
        poly_loop_len--;
      }
      else {
        if (chang_loop_start) {
          wp.loop_start = l;
          chang_loop_start = false;
        }
        if (wl_prev) {
          wl_prev->loop_next = l;
        }
        wl_prev = wl;
        BLI_assert(wl->loop_next == l + 1 || l == wp.loop_end);
      }
    } while (l++ != wp.loop_end);

    if (poly_loop_len) {
      if (wl_prev) {
        wl_prev->loop_next = wp.loop_start;
        wp.loop_end = wl_prev->loop_orig;
      }

#ifdef USE_WELD_DEBUG
      wp.loop_len = poly_loop_len;

      for (int loop_orig : IndexRange(wp.loop_start, poly_loop_len)) {
        int loop_ctx = loop_map[loop_orig];
        if (loop_ctx == OUT_OF_CONTEXT) {
          continue;
        }

        WeldLoop *wl = &wloop[loop_ctx];
        if (wl->flag == ELEM_COLLAPSED) {
          continue;
        }

        loop_ctx = loop_map[wl->loop_next];
        if (loop_ctx == OUT_OF_CONTEXT) {
          continue;
        }

        wl = &wloop[loop_ctx];
        BLI_assert(wl->flag != ELEM_COLLAPSED);
      }
#endif

      weld_poly_split_recursive(
          poly_loop_len, vert_dest_map, &wp, r_weld_mesh, &face_kill_len, &loop_kill_len);
    }
  }

  r_weld_mesh->face_kill_len = face_kill_len;
  r_weld_mesh->loop_kill_len = loop_kill_len;

#ifdef USE_WELD_DEBUG
  weld_assert_poly_and_loop_kill_len(
      r_weld_mesh, r_weld_mesh->face_kill_len, r_weld_mesh->loop_kill_len);
#endif
}

static int poly_find_doubles(const OffsetIndices<int> poly_corners_offsets,
                             const int poly_num,
                             const Span<int> corners,
                             const int corner_index_max,
                             Vector<int> &r_doubles_offsets,
                             Array<int> &r_doubles_buffer)
{
  /* Fills the `r_buffer` buffer with the intersection of the arrays in `buffer_a` and `buffer_b`.
   * `buffer_a` and `buffer_b` have a sequence of sorted, non-repeating indices representing
   * polygons. */
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
         * As they are already in the original array, this can cause buffer overflow. */
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

  int link_faces_buffer_len = 0;
  for (const int elem_index : IndexRange(corner_index_max)) {
    link_faces_buffer_len += linked_faces_offset[elem_index];
    linked_faces_offset[elem_index] = link_faces_buffer_len;
  }
  linked_faces_offset[corner_index_max] = link_faces_buffer_len;

  if (link_faces_buffer_len == 0) {
    return 0;
  }

  Array<int> linked_faces_buffer(link_faces_buffer_len);

  /* Use a reverse for loop to ensure that indexes are assigned in ascending order. */
  for (int face_index = poly_num; face_index--;) {
    if (poly_corners_offsets[face_index].is_empty()) {
      continue;
    }

    for (int corner_index = poly_corners_offsets[face_index].last();
         corner_index >= poly_corners_offsets[face_index].first();
         corner_index--)
    {
      const int elem_index = corners[corner_index];
      linked_faces_buffer[--linked_faces_offset[elem_index]] = face_index;
    }
  }

  Array<int> doubles_buffer(poly_num);

  Vector<int> doubles_offsets;
  doubles_offsets.reserve((poly_num / 2) + 1);
  doubles_offsets.append(0);

  BitVector<> is_double(poly_num, false);

  int doubles_buffer_num = 0;
  int doubles_num = 0;
  for (const int face_index : IndexRange(poly_num)) {
    if (is_double[face_index]) {
      continue;
    }

    int corner_num = poly_corners_offsets[face_index].size();
    if (corner_num == 0) {
      continue;
    }

    /* Set or overwrite the first slot of the possible group. */
    doubles_buffer[doubles_buffer_num] = face_index;

    int corner_first = poly_corners_offsets[face_index].first();
    int elem_index = corners[corner_first];
    int link_offs = linked_faces_offset[elem_index];
    int faces_a_num = linked_faces_offset[elem_index + 1] - link_offs;
    if (faces_a_num == 1) {
      BLI_assert(linked_faces_buffer[linked_faces_offset[elem_index]] == face_index);
      continue;
    }

    const int *faces_a = &linked_faces_buffer[link_offs];
    int poly_to_test;

    /* Skip polygons with lower index as these have already been checked. */
    do {
      poly_to_test = *faces_a;
      faces_a++;
      faces_a_num--;
    } while (poly_to_test != face_index);

    int *isect_result = doubles_buffer.data() + doubles_buffer_num + 1;

    /* `faces_a` are the polygons connected to the first corner. So skip the first corner. */
    for (int corner_index : IndexRange(corner_first + 1, corner_num - 1)) {
      elem_index = corners[corner_index];
      link_offs = linked_faces_offset[elem_index];
      int faces_b_num = linked_faces_offset[elem_index + 1] - link_offs;
      const int *faces_b = &linked_faces_buffer[link_offs];

      /* Skip polygons with lower index as these have already been checked. */
      do {
        poly_to_test = *faces_b;
        faces_b++;
        faces_b_num--;
      } while (poly_to_test != face_index);

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
      for (const int poly_double : Span<int>{isect_result, doubles_num}) {
        BLI_assert(poly_double > face_index);
        is_double[poly_double].set();
      }
      doubles_buffer_num += doubles_num;
      doubles_offsets.append(++doubles_buffer_num);

      if ((doubles_buffer_num + 1) == poly_num) {
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

static void weld_poly_find_doubles(const Span<int> corner_verts,
                                   const Span<int> corner_edges,
                                   const int medge_len,
                                   WeldMesh *r_weld_mesh)
{
  if (r_weld_mesh->face_kill_len == r_weld_mesh->wpoly.size()) {
    return;
  }

  WeldPoly *wpoly = r_weld_mesh->wpoly.data();
  MutableSpan<WeldLoop> wloop = r_weld_mesh->wloop;
  Span<int> loop_map = r_weld_mesh->loop_map;
  int face_index = 0;

  const int face_len = r_weld_mesh->wpoly.size();
  Array<int> poly_offs_(face_len + 1);
  Vector<int> new_corner_edges;
  new_corner_edges.reserve(corner_verts.size() - r_weld_mesh->loop_kill_len);

  for (const WeldPoly &wp : r_weld_mesh->wpoly) {
    poly_offs_[face_index++] = new_corner_edges.size();

    WeldLoopOfPolyIter iter;
    if (!weld_iter_loop_of_poly_begin(
            iter, wp, wloop, corner_verts, corner_edges, loop_map, nullptr))
    {
      continue;
    }

    if (wp.poly_dst != OUT_OF_CONTEXT) {
      continue;
    }

    do {
      new_corner_edges.append(iter.e);
    } while (weld_iter_loop_of_poly_next(iter));
  }

  poly_offs_[face_len] = new_corner_edges.size();
  OffsetIndices<int> poly_offs(poly_offs_);

  Vector<int> doubles_offsets;
  Array<int> doubles_buffer;
  const int doubles_num = poly_find_doubles(
      poly_offs, face_len, new_corner_edges, medge_len, doubles_offsets, doubles_buffer);

  if (doubles_num) {
    int loop_kill_num = 0;

    OffsetIndices<int> doubles_offset_indices(doubles_offsets);
    for (const int i : doubles_offset_indices.index_range()) {
      const int poly_dst = wpoly[doubles_buffer[doubles_offsets[i]]].poly_orig;

      for (const int offset : doubles_offset_indices[i].drop_front(1)) {
        const int wpoly_index = doubles_buffer[offset];
        WeldPoly &wp = wpoly[wpoly_index];

        BLI_assert(wp.poly_dst == OUT_OF_CONTEXT);
        wp.poly_dst = poly_dst;
        loop_kill_num += poly_offs[wpoly_index].size();
      }
    }

    r_weld_mesh->face_kill_len += doubles_num;
    r_weld_mesh->loop_kill_len += loop_kill_num;
  }

#ifdef USE_WELD_DEBUG
  weld_assert_poly_and_loop_kill_len(
      r_weld_mesh, r_weld_mesh->face_kill_len, r_weld_mesh->loop_kill_len);
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh API
 * \{ */

static void weld_mesh_context_create(const Mesh &mesh,
                                     MutableSpan<int> vert_dest_map,
                                     const int vert_kill_len,
                                     const bool get_doubles,
                                     WeldMesh *r_weld_mesh)
{
  const Span<int2> edges = mesh.edges();
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<int> corner_edges = mesh.corner_edges();

  Vector<int> wvert = weld_vert_ctx_alloc_and_setup(vert_dest_map, vert_kill_len);
  r_weld_mesh->vert_kill_len = vert_kill_len;

  r_weld_mesh->edge_dest_map.reinitialize(edges.size());
  r_weld_mesh->vert_dest_map = vert_dest_map;

#ifdef USE_WELD_DEBUG
  r_weld_mesh->corner_verts = corner_verts;
  r_weld_mesh->corner_edges = corner_edges;
  r_weld_mesh->faces = faces;
#endif

  int edge_collapsed_len, edge_double_kill_len;
  Vector<WeldEdge> wedge = weld_edge_ctx_alloc_and_find_collapsed(
      edges, vert_dest_map, r_weld_mesh->edge_dest_map, &edge_collapsed_len);

  weld_edge_find_doubles(wedge, mesh.verts_num, r_weld_mesh->edge_dest_map, &edge_double_kill_len);

  r_weld_mesh->edge_kill_len = edge_collapsed_len + edge_double_kill_len;

#ifdef USE_WELD_DEBUG
  weld_assert_edge_kill_len(r_weld_mesh->edge_dest_map, r_weld_mesh->edge_kill_len);
#endif

  weld_poly_loop_ctx_alloc(faces, corner_verts, corner_edges, r_weld_mesh);

  weld_poly_loop_ctx_setup_collapsed_and_split(wedge.size() - edge_double_kill_len, r_weld_mesh);

  weld_poly_find_doubles(corner_verts, corner_edges, edges.size(), r_weld_mesh);

  if (get_doubles) {
    r_weld_mesh->double_verts = std::move(wvert);
    r_weld_mesh->double_edges.reserve(wedge.size());
    for (WeldEdge &we : wedge) {
      if (r_weld_mesh->edge_dest_map[we.edge_orig] >= 0) {
        r_weld_mesh->double_edges.append(we.edge_orig);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Merging
 * \{ */

/**
 * \brief Create groups to merge.
 *
 * This function creates groups for merging elements based on the provided `dest_map`.
 *
 * \param dest_map: Map that defines the source and target elements. The source elements will be
 *                  merged into the target. Each target corresponds to a group.
 * \param double_elems: Source and target elements in `dest_map`. For quick access.
 *
 * \return r_groups_map: Map that points out the group of elements that an element belongs to.
 * \return r_groups_buffer: Buffer containing the indices of all elements that merge.
 * \return r_groups_offs: Array that indicates where each element group starts in the buffer.
 */
static void merge_groups_create(Span<int> dest_map,
                                Span<int> double_elems,
                                MutableSpan<int> r_groups_offsets,
                                Array<int> &r_groups_buffer)
{
  BLI_assert(r_groups_offsets.size() == dest_map.size() + 1);
  r_groups_offsets.fill(0);

  /* TODO: Check using #array_utils::count_indices instead. At the moment it cannot be used
   * because `dest_map` has negative values and `double_elems` (which indicates only the indexes to
   * be read) is not used. */
  for (const int elem_orig : double_elems) {
    const int elem_dest = dest_map[elem_orig];
    r_groups_offsets[elem_dest]++;
  }

  int offs = 0;
  for (const int i : dest_map.index_range()) {
    offs += r_groups_offsets[i];
    r_groups_offsets[i] = offs;
  }
  r_groups_offsets.last() = offs;

  r_groups_buffer.reinitialize(offs);
  BLI_assert(r_groups_buffer.size() == double_elems.size());

  /* Use a reverse for loop to ensure that indices are assigned in ascending order. */
  for (int i = double_elems.size(); i--;) {
    const int elem_orig = double_elems[i];
    const int elem_dest = dest_map[elem_orig];
    r_groups_buffer[--r_groups_offsets[elem_dest]] = elem_orig;
  }
}

/**
 * To indicate the new indices `r_final_map` is created.
 *
 * \param dest_map: Map that defines the source and target elements. The source elements will be
 *                  merged into the target. Each target corresponds to a group.
 * \param double_elems: Source and target elements in `dest_map`. For quick access.
 * \param do_mix_data: If true the target element will have the custom data interpolated with all
 *                     sources pointing to it.
 *
 * \return r_final_map: Array indicating the new indices of the elements.
 */
static void merge_customdata_all(Span<int> dest_map,
                                 Span<int> double_elems,
                                 const int dest_size,
                                 const bool do_mix_data,
                                 Vector<int> &r_src_index_offsets,
                                 Vector<int> &r_src_index_data,
                                 Array<int> &r_final_map)
{
  const int source_size = dest_map.size();
  r_src_index_offsets.reserve(dest_size + 1);
  r_src_index_data.reserve(source_size);

  MutableSpan<int> groups_offs_;
  Array<int> groups_buffer;
  if (do_mix_data) {
    r_final_map.reinitialize(source_size + 1);

    /* Be careful when setting values to this array as it uses the same buffer as `r_final_map`. */
    groups_offs_ = r_final_map;
    merge_groups_create(dest_map, double_elems, groups_offs_, groups_buffer);
  }
  else {
    r_final_map.reinitialize(source_size);
  }
  OffsetIndices<int> groups_offs(groups_offs_);

  bool finalize_map = false;
  int dest_index = 0;
  for (int i = 0; i < source_size; i++) {
    while (i < source_size && dest_map[i] == OUT_OF_CONTEXT) {
      r_final_map[i] = dest_index;
      r_src_index_offsets.append_unchecked(r_src_index_data.size());
      r_src_index_data.append(i);
      dest_index++;
      i++;
    }

    if (i == source_size) {
      break;
    }
    if (dest_map[i] == i) {
      if (do_mix_data) {
        r_src_index_offsets.append_unchecked(r_src_index_data.size());
        r_src_index_data.extend(groups_buffer.as_span().slice(groups_offs[i]));
      }
      else {
        r_src_index_offsets.append_unchecked(r_src_index_data.size());
        r_src_index_data.append(i);
      }
      r_final_map[i] = dest_index;
      dest_index++;
    }
    else if (dest_map[i] == ELEM_COLLAPSED) {
      /* Any value will do. This field must not be accessed anymore. */
      r_final_map[i] = 0;
    }
    else {
      const int elem_dest = dest_map[i];
      BLI_assert(elem_dest != OUT_OF_CONTEXT);
      BLI_assert(dest_map[elem_dest] == elem_dest);
      if (elem_dest < i) {
        r_final_map[i] = r_final_map[elem_dest];
        BLI_assert(r_final_map[i] < dest_size);
      }
      else {
        /* Mark as negative to set at the end. */
        r_final_map[i] = -elem_dest;
        finalize_map = true;
      }
    }
  }

  if (finalize_map) {
    for (const int i : r_final_map.index_range()) {
      if (r_final_map[i] < 0) {
        r_final_map[i] = r_final_map[-r_final_map[i]];
        BLI_assert(r_final_map[i] < dest_size);
      }
      BLI_assert(r_final_map[i] >= 0);
    }
  }

  r_src_index_offsets.append_unchecked(r_src_index_data.size());

  BLI_assert(dest_index == dest_size);
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

static void mix_src_indices(const GSpan src_attr,
                            const GroupedSpan<int> dst_to_src,
                            GMutableSpan dst_attr)
{
  bke::attribute_math::convert_to_static_type(src_attr.type(), [&](auto dummy) {
    using T = decltype(dummy);
    const Span<T> src = src_attr.typed<T>();
    MutableSpan<T> dst = dst_attr.typed<T>();
    threading::parallel_for(dst.index_range(), 2048, [&](const IndexRange range) {
      for (const int dst_index : range) {
        const Span<int> src_indices = dst_to_src[dst_index];
        if (src_indices.size() == 1) {
          dst[dst_index] = src[src_indices.first()];
          continue;
        }
        bke::attribute_math::DefaultMixer<T> mixer({&dst[dst_index], 1});
        for (const int src_index : src_indices) {
          mixer.mix_in(0, src[src_index]);
        }
        mixer.finalize();
      }
    });
  });
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
    const GVArraySpan src_attr = *iter.get();
    bke::GSpanAttributeWriter dst_attr = dst_attributes.lookup_or_add_for_write_only_span(
        iter.name, iter.domain, iter.data_type);
    mix_src_indices(src_attr, dst_to_src, dst_attr.span);
    dst_attr.finish();
  });
}

static void mix_vertex_groups(const Mesh &mesh_src,
                              const GroupedSpan<int> dst_to_src,
                              Mesh &mesh_dst)
{
  const char *func = __func__;
  const Span<MDeformVert> src_dverts = mesh_src.deform_verts();
  if (src_dverts.is_empty()) {
    return;
  }
  MutableSpan<MDeformVert> dst_dverts = mesh_dst.deform_verts_for_write();
  threading::parallel_for(dst_to_src.index_range(), 256, [&](const IndexRange range) {
    struct WeightIndexGetter {
      int operator()(const MDeformWeight &value) const
      {
        return value.def_nr;
      }
    };
    CustomIDVectorSet<MDeformWeight, WeightIndexGetter, 64> weights;

    for (const int dst_vert : range) {
      MDeformVert &dst_dvert = dst_dverts[dst_vert];

      const Span<int> src_verts = dst_to_src[dst_vert];
      if (src_verts.size() == 1) {
        const MDeformVert &src_dvert = src_dverts[src_verts.first()];
        dst_dvert.dw = MEM_malloc_arrayN<MDeformWeight>(src_dvert.totweight, func);
        std::copy_n(src_dvert.dw, src_dvert.totweight, dst_dvert.dw);
        dst_dvert.totweight = src_dvert.totweight;
        continue;
      }

      const float src_num_inv = math::rcp(float(src_verts.size()));
      for (const int src_vert : src_verts) {
        const MDeformVert &src_dvert = src_dverts[src_vert];
        for (const MDeformWeight &src_weight : Span(src_dvert.dw, src_dvert.totweight)) {
          const int i = weights.index_of_or_add(MDeformWeight{src_weight.def_nr, 0.0f});
          const_cast<MDeformWeight &>(weights[i]).weight += src_weight.weight * src_num_inv;
        }
      }

      std::sort(const_cast<MDeformWeight *>(weights.begin()),
                const_cast<MDeformWeight *>(weights.end()),
                [](const auto &a, const auto &b) { return a.def_nr < b.def_nr; });

      dst_dvert.dw = MEM_malloc_arrayN<MDeformWeight>(weights.size(), func);
      dst_dvert.totweight = weights.size();
      std::copy(weights.begin(), weights.end(), dst_dvert.dw);
      weights.clear_and_keep_capacity();
    }
  });
}

static Set<StringRef> get_vertex_group_names(const Mesh &mesh)
{
  Set<StringRef> names;
  LISTBASE_FOREACH (bDeformGroup *, group, &mesh.vertex_group_names) {
    names.add(group->name);
  }
  return names;
}

static Mesh *create_merged_mesh(const Mesh &mesh,
                                MutableSpan<int> vert_dest_map,
                                const int removed_vertex_count,
                                const bool do_mix_data)
{
#ifdef USE_WELD_DEBUG_TIME
  SCOPED_TIMER(__func__);
#endif

  const Span<int2> src_edges = mesh.edges();
  const OffsetIndices src_faces = mesh.faces();
  const Span<int> src_corner_verts = mesh.corner_verts();
  const Span<int> src_corner_edges = mesh.corner_edges();
  const bke::AttributeAccessor src_attributes = mesh.attributes();
  const int totvert = mesh.verts_num;
  const int totedge = mesh.edges_num;

  WeldMesh weld_mesh;
  weld_mesh_context_create(mesh, vert_dest_map, removed_vertex_count, do_mix_data, &weld_mesh);

  const int result_nverts = totvert - weld_mesh.vert_kill_len;
  const int result_nedges = totedge - weld_mesh.edge_kill_len;
  const int result_nloops = src_corner_verts.size() - weld_mesh.loop_kill_len;
  const int result_nfaces = src_faces.size() - weld_mesh.face_kill_len + weld_mesh.wpoly_new_len;

  Mesh *result = BKE_mesh_new_nomain(result_nverts, result_nedges, result_nfaces, result_nloops);
  BKE_mesh_copy_parameters_for_eval(result, &mesh);
  MutableSpan<int2> dst_edges = result->edges_for_write();
  MutableSpan<int> dst_face_offsets = result->face_offsets_for_write();
  MutableSpan<int> dst_corner_verts = result->corner_verts_for_write();
  MutableSpan<int> dst_corner_edges = result->corner_edges_for_write();
  bke::MutableAttributeAccessor dst_attributes = result->attributes_for_write();

  /* Vertices. */

  Array<int> vert_final_map;
  Vector<int> vert_src_index_offset_data;
  Vector<int> vert_src_index_data;
  merge_customdata_all(vert_dest_map,
                       weld_mesh.double_verts,
                       result_nverts,
                       do_mix_data,
                       vert_src_index_offset_data,
                       vert_src_index_data,
                       vert_final_map);
  const GroupedSpan<int> dst_to_src_verts(OffsetIndices<int>(vert_src_index_offset_data),
                                          vert_src_index_data);

  mix_attributes(src_attributes,
                 dst_to_src_verts,
                 bke::AttrDomain::Point,
                 get_vertex_group_names(mesh),
                 dst_attributes);
  mix_vertex_groups(mesh, dst_to_src_verts, *result);
  if (CustomData_has_layer(&mesh.vert_data, CD_ORIGINDEX)) {
    const Span src(static_cast<const int *>(CustomData_get_layer(&mesh.vert_data, CD_ORIGINDEX)),
                   mesh.verts_num);
    MutableSpan dst(static_cast<int *>(CustomData_add_layer(
                        &result->vert_data, CD_ORIGINDEX, CD_CONSTRUCT, result->verts_num)),
                    result->verts_num);
    copy_first_from_src(src, dst_to_src_verts, dst);
  }
  if (CustomData_has_layer(&mesh.vert_data, CD_MVERT_SKIN)) {
    const Span src(
        static_cast<const MVertSkin *>(CustomData_get_layer(&mesh.vert_data, CD_MVERT_SKIN)),
        mesh.verts_num);
    MutableSpan dst(static_cast<MVertSkin *>(CustomData_add_layer(
                        &result->vert_data, CD_MVERT_SKIN, CD_CONSTRUCT, result->verts_num)),
                    result->verts_num);
    threading::parallel_for(dst.index_range(), 2048, [&](const IndexRange range) {
      for (const int dst_vert : range) {
        const Span<int> src_verts = dst_to_src_verts[dst_vert];
        if (src_verts.size() == 1) {
          dst[dst_vert] = src[src_verts.first()];
          continue;
        }
        const float src_num_inv = math::rcp(float(src_verts.size()));
        for (const int src_vert : src_verts) {
          madd_v3_v3fl(dst[dst_vert].radius, src[src_vert].radius, src_num_inv);
          dst[dst_vert].flag |= src[src_vert].flag;
        }
      }
    });
  }

  /* Edges. */

  Array<int> edge_final_map;
  Vector<int> edge_src_index_offset_data;
  Vector<int> edge_src_index_data;
  merge_customdata_all(weld_mesh.edge_dest_map,
                       weld_mesh.double_edges,
                       result_nedges,
                       do_mix_data,
                       edge_src_index_offset_data,
                       edge_src_index_data,
                       edge_final_map);
  const GroupedSpan<int> dst_to_src_edges(OffsetIndices<int>(edge_src_index_offset_data),
                                          edge_src_index_data);

  mix_attributes(
      src_attributes, dst_to_src_edges, bke::AttrDomain::Edge, {".edge_verts"}, dst_attributes);
  if (CustomData_has_layer(&mesh.edge_data, CD_ORIGINDEX)) {
    const Span src(static_cast<const int *>(CustomData_get_layer(&mesh.edge_data, CD_ORIGINDEX)),
                   mesh.edges_num);
    MutableSpan dst(static_cast<int *>(CustomData_add_layer(
                        &result->edge_data, CD_ORIGINDEX, CD_CONSTRUCT, result->edges_num)),
                    result->edges_num);
    copy_first_from_src(src, dst_to_src_edges, dst);
  }

  threading::parallel_for(dst_edges.index_range(), 2048, [&](const IndexRange range) {
    for (const int dst_edge_index : range) {
      const int src_edge_index = dst_to_src_edges[dst_edge_index].first();
      const int2 src_edge = src_edges[src_edge_index];
      dst_edges[dst_edge_index] = int2(vert_final_map[src_edge[0]], vert_final_map[src_edge[1]]);
    }
  });

  /* Faces/Loops. */
  Vector<int> corner_src_index_offset_data;
  Vector<int> corner_src_index_data;

  corner_src_index_offset_data.reserve(result->corners_num + 1);
  corner_src_index_data.reserve(mesh.corners_num);

  int r_i = 0;
  int loop_cur = 0;
  Array<bool> dst_face_unaffected(result_nfaces - weld_mesh.wpoly_new_len);
  Array<int> dst_to_src_faces(result_nfaces - weld_mesh.wpoly_new_len);
  Array<int, 64> group_buffer(weld_mesh.max_face_len);
  for (const int i : src_faces.index_range()) {
    const int loop_start = loop_cur;
    const int poly_ctx = weld_mesh.face_map[i];
    if (poly_ctx == OUT_OF_CONTEXT) {
      for (const int loop_orig : src_faces[i]) {
        corner_src_index_offset_data.append_unchecked(corner_src_index_data.size());
        corner_src_index_data.append(loop_orig);
        loop_cur++;
      }
      dst_face_unaffected[r_i] = true;
    }
    else {
      const WeldPoly &wp = weld_mesh.wpoly[poly_ctx];
      WeldLoopOfPolyIter iter;
      if (!weld_iter_loop_of_poly_begin(iter,
                                        wp,
                                        weld_mesh.wloop,
                                        src_corner_verts,
                                        src_corner_edges,
                                        weld_mesh.loop_map,
                                        group_buffer.data()))
      {
        continue;
      }

      if (wp.poly_dst != OUT_OF_CONTEXT) {
        continue;
      }
      dst_face_unaffected[r_i] = false;
      do {
        corner_src_index_offset_data.append_unchecked(corner_src_index_data.size());
        corner_src_index_data.extend(Span(group_buffer.data(), iter.group_len));
        dst_corner_verts[loop_cur] = vert_final_map[iter.v];
        dst_corner_edges[loop_cur] = edge_final_map[iter.e];
        loop_cur++;
      } while (weld_iter_loop_of_poly_next(iter));
    }

    dst_to_src_faces[r_i] = i;
    dst_face_offsets[r_i] = loop_start;
    r_i++;
  }

  /* New Polygons. */
  for (const int i : weld_mesh.wpoly.index_range().take_back(weld_mesh.wpoly_new_len)) {
    const WeldPoly &wp = weld_mesh.wpoly[i];
    const int loop_start = loop_cur;
    WeldLoopOfPolyIter iter;
    if (!weld_iter_loop_of_poly_begin(iter,
                                      wp,
                                      weld_mesh.wloop,
                                      src_corner_verts,
                                      src_corner_edges,
                                      weld_mesh.loop_map,
                                      group_buffer.data()))
    {
      continue;
    }

    if (wp.poly_dst != OUT_OF_CONTEXT) {
      continue;
    }
    do {
      corner_src_index_offset_data.append_unchecked(corner_src_index_data.size());
      corner_src_index_data.extend(Span(group_buffer.data(), iter.group_len));
      dst_corner_verts[loop_cur] = vert_final_map[iter.v];
      dst_corner_edges[loop_cur] = edge_final_map[iter.e];
      loop_cur++;
    } while (weld_iter_loop_of_poly_next(iter));

    dst_face_offsets[r_i] = loop_start;
    r_i++;
  }

  corner_src_index_offset_data.append_unchecked(corner_src_index_data.size());

  const GroupedSpan<int> dst_to_src_corners(OffsetIndices<int>(corner_src_index_offset_data),
                                            corner_src_index_data);

  const OffsetIndices dst_faces = result->faces();

  src_attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (iter.domain != bke::AttrDomain::Face) {
      return;
    }
    const GVArray src_attr = *iter.get();
    const CPPType &type = src_attr.type();
    bke::GSpanAttributeWriter dst_attr = dst_attributes.lookup_or_add_for_write_only_span(
        iter.name, iter.domain, iter.data_type);
    bke::attribute_math::gather(
        src_attr, dst_to_src_faces, dst_attr.span.drop_back(weld_mesh.wpoly_new_len));
    type.fill_assign_n(type.default_value(),
                       dst_attr.span.take_back(weld_mesh.wpoly_new_len).data(),
                       weld_mesh.wpoly_new_len);
    dst_attr.finish();
  });

  if (CustomData_has_layer(&mesh.face_data, CD_ORIGINDEX)) {
    const Span src(static_cast<const int *>(CustomData_get_layer(&mesh.face_data, CD_ORIGINDEX)),
                   mesh.faces_num);
    MutableSpan dst(static_cast<int *>(CustomData_add_layer(
                        &result->face_data, CD_ORIGINDEX, CD_CONSTRUCT, result->faces_num)),
                    result->faces_num);
    bke::attribute_math::gather(src, dst_to_src_faces, dst.drop_back(weld_mesh.wpoly_new_len));
    dst.take_back(weld_mesh.wpoly_new_len).fill(ORIGINDEX_NONE);
  }

  IndexMaskMemory memory;
  const IndexMask out_of_context_faces = IndexMask::from_bools(dst_face_unaffected, memory);

  out_of_context_faces.foreach_index(GrainSize(1024), [&](const int dst_face_index) {
    const IndexRange src_face = src_faces[dst_to_src_faces[dst_face_index]];
    const IndexRange dst_face = dst_faces[dst_face_index];
    for (const int i : src_face.index_range()) {
      dst_corner_verts[dst_face[i]] = vert_final_map[src_corner_verts[src_face[i]]];
      dst_corner_edges[dst_face[i]] = edge_final_map[src_corner_edges[src_face[i]]];
    }
  });

  mix_attributes(src_attributes,
                 dst_to_src_corners,
                 bke::AttrDomain::Corner,
                 {".corner_vert", ".corner_edge"},
                 dst_attributes);

  BLI_assert(int(r_i) == result_nfaces);
  BLI_assert(loop_cur == result_nloops);

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
  Array<int> vert_dest_map(mesh.verts_num, OUT_OF_CONTEXT);

  KDTree_3d *tree = BLI_kdtree_3d_new(selection.size());

  const Span<float3> positions = mesh.vert_positions();
  selection.foreach_index([&](const int64_t i) { BLI_kdtree_3d_insert(tree, i, positions[i]); });

  BLI_kdtree_3d_balance(tree);
  const int vert_kill_len = BLI_kdtree_3d_calc_duplicates_fast(
      tree, merge_distance, true, vert_dest_map.data());
  BLI_kdtree_3d_free(tree);

  if (vert_kill_len == 0) {
    return std::nullopt;
  }

  return create_merged_mesh(mesh, vert_dest_map, vert_kill_len, true);
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

  int vert_kill_len = 0;

  /* From the original index of the vertex.
   * This indicates which vert it is or is going to be merged. */
  Array<int> vert_dest_map(mesh.verts_num, OUT_OF_CONTEXT);

  Array<WeldVertexCluster> vert_clusters(mesh.verts_num);

  for (const int i : positions.index_range()) {
    WeldVertexCluster &vc = vert_clusters[i];
    copy_v3_v3(vc.co, positions[i]);
    vc.merged_verts = 0;
  }
  const float merge_dist_sq = square_f(merge_distance);

  range_vn_i(vert_dest_map.data(), mesh.verts_num, 0);

  /* Collapse Edges that are shorter than the threshold. */
  const bke::LooseEdgeCache *loose_edges = nullptr;
  if (only_loose_edges) {
    loose_edges = &mesh.loose_edges();
    if (loose_edges->count == 0) {
      return {};
    }
  }

  for (const int i : edges.index_range()) {
    int v1 = edges[i][0];
    int v2 = edges[i][1];

    if (loose_edges && !loose_edges->is_loose_bits[i]) {
      continue;
    }
    while (v1 != vert_dest_map[v1]) {
      v1 = vert_dest_map[v1];
    }
    while (v2 != vert_dest_map[v2]) {
      v2 = vert_dest_map[v2];
    }
    if (v1 == v2) {
      continue;
    }
    if (!selection.is_empty() && (!selection[v1] || !selection[v2])) {
      continue;
    }
    if (v1 > v2) {
      std::swap(v1, v2);
    }
    WeldVertexCluster *v1_cluster = &vert_clusters[v1];
    WeldVertexCluster *v2_cluster = &vert_clusters[v2];

    float edgedir[3];
    sub_v3_v3v3(edgedir, v2_cluster->co, v1_cluster->co);
    const float dist_sq = len_squared_v3(edgedir);
    if (dist_sq <= merge_dist_sq) {
      float influence = (v2_cluster->merged_verts + 1) /
                        float(v1_cluster->merged_verts + v2_cluster->merged_verts + 2);
      madd_v3_v3fl(v1_cluster->co, edgedir, influence);

      v1_cluster->merged_verts += v2_cluster->merged_verts + 1;
      vert_dest_map[v2] = v1;
      vert_kill_len++;
    }
  }

  if (vert_kill_len == 0) {
    return std::nullopt;
  }

  for (const int i : IndexRange(mesh.verts_num)) {
    if (i == vert_dest_map[i]) {
      vert_dest_map[i] = OUT_OF_CONTEXT;
    }
    else {
      int v = i;
      while ((v != vert_dest_map[v]) && (vert_dest_map[v] != OUT_OF_CONTEXT)) {
        v = vert_dest_map[v];
      }
      vert_dest_map[v] = v;
      vert_dest_map[i] = v;
    }
  }

  return create_merged_mesh(mesh, vert_dest_map, vert_kill_len, true);
}

Mesh *mesh_merge_verts(const Mesh &mesh,
                       MutableSpan<int> vert_dest_map,
                       int vert_dest_map_len,
                       const bool do_mix_data)
{
  return create_merged_mesh(mesh, vert_dest_map, vert_dest_map_len, do_mix_data);
}

/** \} */

}  // namespace blender::geometry
