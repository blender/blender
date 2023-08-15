/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

// #define USE_WELD_DEBUG
// #define USE_WELD_DEBUG_TIME

#include "BLI_array.hh"
#include "BLI_bit_vector.hh"
#include "BLI_index_mask.hh"
#include "BLI_kdtree.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_offset_indices.hh"
#include "BLI_vector.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_customdata.h"
#include "BKE_mesh.hh"

#include "GEO_mesh_merge_by_distance.hh"

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
      int vert;
      int edge;
      int loop_orig;
      /* Switches to this loop when iterating. Except when it is the first loop of the polygon. */
      int switch_to;
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
#ifdef USE_WELD_DEBUG
      /* Final Polygon Size. */
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
  int loop_orig_start;
  int loop_end;

  Span<WeldLoop> wloop;
  Span<int> corner_verts;
  Span<int> corner_edges;
  Span<int> loop_map;
  /* Weld group. */
  int *group;

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
          iter, *wp, wloop, corner_verts, corner_edges, loop_map, nullptr)) {
    return;
  }
  else {
    while (weld_iter_loop_of_poly_next(iter)) {
      verts[i++] = iter.v;
    }
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

static void weld_assert_poly_len(const WeldPoly *wp,
                                 const Span<WeldLoop> wloop,
                                 Span<int> loop_map)
{
  if (wp->flag == ELEM_COLLAPSED) {
    return;
  }

  int len = 0;
  for (int l = wp->loop_start; l <= wp->loop_end; l++) {
    int loop_ctx = loop_map[l];
    if (loop_ctx == OUT_OF_CONTEXT) {
      len++;
      continue;
    }
    const WeldLoop *wl = &wloop[loop_ctx];
    BLI_assert(wp->loop_start <= wl->loop_orig);
    if (wl->flag != ELEM_COLLAPSED) {
      len++;
    }
  }
  BLI_assert(wp->loop_len >= len);
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

  if (weld_edges.size() == 0) {
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
  iter.loop_orig_start = wp.loop_start;
  iter.loop_end = wp.loop_end;

  iter.wloop = wloop;
  iter.corner_verts = corner_verts;
  iter.corner_edges = corner_edges;
  iter.loop_map = loop_map;
  iter.group = group_buffer;

  int group_len = 0;
  if (group_buffer) {
    /* Include any final loops that are collapsed.
     * Would it be better to move this to `weld_iter_loop_of_poly_next`? */
    int loop_end_ctx;
    int loop_end = wp.loop_end;
    while (((loop_end_ctx = loop_map[loop_end]) != OUT_OF_CONTEXT) &&
           (wloop[loop_end_ctx].flag == ELEM_COLLAPSED))
    {
      loop_end--;
      group_len++;
    }
    int i = 0;
    while (loop_end < wp.loop_end) {
      iter.group[i++] = ++loop_end;
    }
  }
  iter.group_len = group_len;

#ifdef USE_WELD_DEBUG
  iter.v = OUT_OF_CONTEXT;
#endif
  return true;
}

static bool weld_iter_loop_of_poly_next(WeldLoopOfPolyIter &iter)
{
  Span<WeldLoop> wloop = iter.wloop;
  Span<int> loop_map = iter.loop_map;
  int l = iter.loop_iter;
  if (l == iter.loop_orig_start) {
    /* `grupo_len` is already calculated in the first loop */
  }
  else {
    iter.group_len = 0;
  }

  while (l <= iter.loop_end) {
    if (iter.group) {
      iter.group[iter.group_len++] = l;
    }

    int loop_ctx = loop_map[l];
    if (loop_ctx != OUT_OF_CONTEXT) {
      const WeldLoop *wl = &wloop[loop_ctx];
      if (wl->flag == ELEM_COLLAPSED) {
        l++;
        continue;
      }
      else if (wl->switch_to != OUT_OF_CONTEXT) {
        if (iter.group) {
          iter.group[iter.group_len++] = wl->switch_to;
        }
        if (l != iter.loop_orig_start) {
          l = wl->switch_to;
          loop_ctx = loop_map[l];
          wl = &wloop[loop_ctx];
        }
      }
#ifdef USE_WELD_DEBUG
      BLI_assert(iter.v != wl->vert);
#endif
      iter.v = wl->vert;
      iter.e = wl->edge;
    }
    else {
#ifdef USE_WELD_DEBUG
      BLI_assert(iter.v != iter.corner_verts[l]);
#endif
      iter.v = iter.corner_verts[l];
      iter.e = iter.corner_edges[l];
    }
    iter.loop_iter = l + 1;
    return true;
  }

  return false;
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

    int prev_wloop_len = wloop_len;
    for (const int loop_orig : faces[i]) {
      int v = corner_verts[loop_orig];
      int e = corner_edges[loop_orig];
      int v_dest = vert_dest_map[v];
      int e_dest = edge_dest_map[e];
      bool is_vert_ctx = v_dest != OUT_OF_CONTEXT;
      bool is_edge_ctx = e_dest != OUT_OF_CONTEXT;
      if (is_vert_ctx || is_edge_ctx) {
        wloop.increase_size_by_unchecked(1);

        WeldLoop &wl = wloop.last();
        wl.vert = is_vert_ctx ? v_dest : v;
        wl.edge = is_edge_ctx ? e_dest : e;
        wl.loop_orig = loop_orig;
        wl.switch_to = OUT_OF_CONTEXT;

        loop_map[loop_orig] = wloop_len++;
      }
      else {
        loop_map[loop_orig] = OUT_OF_CONTEXT;
      }
    }
    if (wloop_len != prev_wloop_len) {
      wpoly.increase_size_by_unchecked(1);

      WeldPoly &wp = wpoly.last();
      int loops_len = wloop_len - prev_wloop_len;
      wp.poly_dst = OUT_OF_CONTEXT;
      wp.poly_orig = i;
      wp.loop_start = loopstart;
      wp.loop_end = loop_end;

#ifdef USE_WELD_DEBUG
      wp.loop_len = totloop;
#endif

      face_map[i] = wpoly_len++;
      if (totloop > 5 && loops_len > 1) {
        /* We could be smarter here and actually count how many new polygons will be created.
         * But counting this can be inefficient as it depends on the number of non-consecutive
         * self face merges. For now just estimate a maximum value. */
        int max_new = std::min((totloop / 3), loops_len) - 1;
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
                                      const int start_loop_from,
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

  int loop_end_ctx;
  int loop_end = r_wp->loop_end;
  while (((loop_end_ctx = loop_map[loop_end]) == OUT_OF_CONTEXT) ||
         (wloop[loop_end_ctx].flag == ELEM_COLLAPSED))
  {
    loop_end--;
  }

  int loop_kill = 0;
  // WeldLoop *wla_prev = &wloop[loop_end_ctx]; /* UNUSED. */

  for (int la = start_loop_from; la <= loop_end; la++) {
    int loop_ctx_a = loop_map[la];
    if (loop_ctx_a == OUT_OF_CONTEXT) {
      continue;
    }

    WeldLoop *wla = &wloop[loop_ctx_a];
    if (wla->flag == ELEM_COLLAPSED) {
      continue;
    }
    int vert_a = wla->vert;
    if (vert_dest_map[vert_a] == OUT_OF_CONTEXT) {
      /* Only test vertices that will be merged. */
      // wla_prev = wla; /* UNUSED. */
      continue;
    }
  wa_continue:
    // WeldLoop *wlb_prev = wla; /* UNUSED. */
    int killed_ab = 0;
    for (int lb = la + 1; lb <= loop_end; lb++) {
      int loop_ctx_b = loop_map[lb];
      if (loop_ctx_b == OUT_OF_CONTEXT) {
        continue;
      }

      WeldLoop *wlb = &wloop[loop_ctx_b];
      if (wlb->flag == ELEM_COLLAPSED) {
        killed_ab++;
        continue;
      }
      int vert_b = wlb->vert;
      if (vert_a != vert_b) {
        // wlb_prev = wlb; /* UNUSED. */
        continue;
      }

      int dist_a = wlb->loop_orig - wla->loop_orig - killed_ab;
      int dist_b = poly_loop_len - dist_a;

      BLI_assert(dist_a != 0 && dist_b != 0);
      if (dist_a == 1 || dist_b == 1) {
        BLI_assert(dist_a != dist_b);
        BLI_assert((wla->flag == ELEM_COLLAPSED) || (wlb->flag == ELEM_COLLAPSED));
      }
      else if (dist_a == 2 && dist_b == 2) {
        r_wp->flag = ELEM_COLLAPSED;
        *r_poly_kill += 1;
        loop_kill += 4;
        dist_b = 0;
      }
      else {
        if (dist_a == 2) {
          WeldLoop *wl_next = wla + 1;
          BLI_assert(wl_next->flag != ELEM_COLLAPSED);
          BLI_assert(wl_next->loop_orig == (wla->loop_orig + 1));
          wla->flag = ELEM_COLLAPSED;
          wl_next->flag = ELEM_COLLAPSED;
          loop_kill += 2;
          dist_a = dist_b;
        }
        else if (dist_b == 2) {
          WeldLoop *wl_next = wla - 1;
          BLI_assert(wl_next->flag != ELEM_COLLAPSED);
          wlb->flag = ELEM_COLLAPSED;
          wl_next->flag = ELEM_COLLAPSED;
          loop_kill += 2;
          dist_b = dist_a;
          lb = la;
        }
        else {
          if (r_wp->loop_start == la) {
            r_wp->loop_start = lb;
            /* Specify a value for the `switch_to` in order to form groups and a more pretty merge.
             * But it's not really necessary for Weld to work. */
            wlb->switch_to = wla->switch_to != OUT_OF_CONTEXT ? wla->switch_to : la;
          }
          wla->switch_to = lb;

          r_weld_mesh->wpoly.increase_size_by_unchecked(1);
          r_weld_mesh->wpoly_new_len++;

          WeldPoly *new_test = &r_weld_mesh->wpoly.last();
          new_test->poly_dst = OUT_OF_CONTEXT;
          new_test->poly_orig = r_wp->poly_orig;
          new_test->loop_start = la;
          new_test->loop_end = lb - 1;

#ifdef USE_WELD_DEBUG
          new_test->loop_len = dist_a;
#endif
          weld_poly_split_recursive(
              dist_a, la, vert_dest_map, new_test, r_weld_mesh, r_poly_kill, r_loop_kill);
        }

        la = lb;
        wla = wlb;
        poly_loop_len = dist_b;
        goto wa_continue;
      }
    }
  }

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
    for (int l = wp.loop_start; l <= wp.loop_end; l++) {
      int loop_ctx = loop_map[l];
      if (loop_ctx == OUT_OF_CONTEXT) {
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
        loop_kill_len++;
        poly_loop_len--;
      }
    }

    if (poly_loop_len) {
#ifdef USE_WELD_DEBUG
      wp.loop_len = poly_loop_len;
      weld_assert_poly_len(&wp, wloop, loop_map);
#endif

      weld_poly_split_recursive(poly_loop_len,
                                wp.loop_start,
                                vert_dest_map,
                                &wp,
                                r_weld_mesh,
                                &face_kill_len,
                                &loop_kill_len);
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
    if (poly_corners_offsets[face_index].size() == 0) {
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
            iter, wp, wloop, corner_verts, corner_edges, loop_map, nullptr)) {
      continue;
    }

    if (wp.poly_dst != OUT_OF_CONTEXT) {
      continue;
    }

    while (weld_iter_loop_of_poly_next(iter)) {
      new_corner_edges.append(iter.e);
    }
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

  weld_edge_find_doubles(wedge, mesh.totvert, r_weld_mesh->edge_dest_map, &edge_double_kill_len);

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
/** \name CustomData
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

static void customdata_weld(
    const CustomData *source, CustomData *dest, const int *src_indices, int count, int dest_index)
{
  if (count == 1) {
    CustomData_copy_data(source, dest, src_indices[0], dest_index, 1);
    return;
  }

  CustomData_interp(source, dest, (const int *)src_indices, nullptr, nullptr, count, dest_index);

  int src_i, dest_i;
  int j;

  int vs_flag = 0;

  /* interpolates a layer at a time */
  dest_i = 0;
  for (src_i = 0; src_i < source->totlayer; src_i++) {
    const eCustomDataType type = eCustomDataType(source->layers[src_i].type);

    /* find the first dest layer with type >= the source type
     * (this should work because layers are ordered by type)
     */
    while (dest_i < dest->totlayer && dest->layers[dest_i].type < type) {
      dest_i++;
    }

    /* if there are no more dest layers, we're done */
    if (dest_i == dest->totlayer) {
      break;
    }

    /* if we found a matching layer, add the data */
    if (dest->layers[dest_i].type == type) {
      void *src_data = source->layers[src_i].data;
      if (type == CD_MVERT_SKIN) {
        /* The `typeInfo->interp` of #CD_MVERT_SKIN does not include the flags, so #MVERT_SKIN_ROOT
         * and #MVERT_SKIN_LOOSE are lost after the interpolation.
         *
         * This behavior is not incorrect. Ideally, islands should be checked to avoid repeated
         * roots.
         *
         * However, for now, to prevent the loss of flags, they are simply re-added if any of the
         * merged vertices have them. */
        for (j = 0; j < count; j++) {
          MVertSkin *vs = &((MVertSkin *)src_data)[src_indices[j]];
          vs_flag |= vs->flag;
        }
      }
      else if (CustomData_layer_has_interp(dest, dest_i)) {
        /* Already calculated.
         * TODO: Optimize by exposing `typeInfo->interp`. */
      }
      else if (CustomData_layer_has_math(dest, dest_i)) {
        const int size = CustomData_sizeof(type);
        void *dst_data = dest->layers[dest_i].data;
        void *v_dst = POINTER_OFFSET(dst_data, size_t(dest_index) * size);
        for (j = 0; j < count; j++) {
          CustomData_data_add(
              type, v_dst, POINTER_OFFSET(src_data, size_t(src_indices[j]) * size));
        }
      }
      else {
        CustomData_copy_layer_type_data(source, dest, type, src_indices[0], dest_index, 1);
      }

      /* if there are multiple source & dest layers of the same type,
       * we don't want to copy all source layers to the same dest, so
       * increment dest_i
       */
      dest_i++;
    }
  }

  float fac = 1.0f / count;

  for (dest_i = 0; dest_i < dest->totlayer; dest_i++) {
    CustomDataLayer *layer_dst = &dest->layers[dest_i];
    const eCustomDataType type = eCustomDataType(layer_dst->type);
    if (type == CD_MVERT_SKIN) {
      MVertSkin *vs = &((MVertSkin *)layer_dst->data)[dest_index];
      vs->flag = vs_flag;
    }
    else if (CustomData_layer_has_interp(dest, dest_i)) {
      /* Already calculated. */
    }
    else if (CustomData_layer_has_math(dest, dest_i)) {
      const int size = CustomData_sizeof(type);
      void *dst_data = layer_dst->data;
      void *v_dst = POINTER_OFFSET(dst_data, size_t(dest_index) * size);
      CustomData_data_multiply(type, v_dst, fac);
    }
  }
}

/**
 * \brief Applies to `CustomData *dest` the values in `CustomData *source`.
 *
 * This function creates the CustomData of the resulting mesh according to the merge map in
 * `dest_map`. The resulting customdata will not have the source elements, so the indexes will be
 * modified. To indicate the new indices `r_final_map` is also created.
 *
 * \param dest_map: Map that defines the source and target elements. The source elements will be
 *                  merged into the target. Each target corresponds to a group.
 * \param double_elems: Source and target elements in `dest_map`. For quick access.
 * \param do_mix_data: If true the target element will have the custom data interpolated with all
 *                     sources pointing to it.
 *
 * \return r_final_map: Array indicating the new indices of the elements.
 */
static void merge_customdata_all(const CustomData *source,
                                 CustomData *dest,
                                 Span<int> dest_map,
                                 Span<int> double_elems,
                                 const int dest_size,
                                 const bool do_mix_data,
                                 Array<int> &r_final_map)
{
  UNUSED_VARS_NDEBUG(dest_size);

  const int source_size = dest_map.size();

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
    const int source_index = i;
    int count = 0;
    while (i < source_size && dest_map[i] == OUT_OF_CONTEXT) {
      r_final_map[i] = dest_index + count;
      count++;
      i++;
    }
    if (count) {
      CustomData_copy_data(source, dest, source_index, dest_index, count);
      dest_index += count;
    }
    if (i == source_size) {
      break;
    }
    if (dest_map[i] == i) {
      if (do_mix_data) {
        const IndexRange grp_buffer_range = groups_offs[i];
        customdata_weld(source,
                        dest,
                        &groups_buffer[grp_buffer_range.start()],
                        grp_buffer_range.size(),
                        dest_index);
      }
      else {
        CustomData_copy_data(source, dest, i, dest_index, 1);
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

  BLI_assert(dest_index == dest_size);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Vertex Merging
 * \{ */

static Mesh *create_merged_mesh(const Mesh &mesh,
                                MutableSpan<int> vert_dest_map,
                                const int removed_vertex_count,
                                const bool do_mix_data)
{
#ifdef USE_WELD_DEBUG_TIME
  SCOPED_TIMER(__func__);
#endif

  const OffsetIndices src_faces = mesh.faces();
  const Span<int> src_corner_verts = mesh.corner_verts();
  const Span<int> src_corner_edges = mesh.corner_edges();
  const int totvert = mesh.totvert;
  const int totedge = mesh.totedge;

  WeldMesh weld_mesh;
  weld_mesh_context_create(mesh, vert_dest_map, removed_vertex_count, do_mix_data, &weld_mesh);

  const int result_nverts = totvert - weld_mesh.vert_kill_len;
  const int result_nedges = totedge - weld_mesh.edge_kill_len;
  const int result_nloops = src_corner_verts.size() - weld_mesh.loop_kill_len;
  const int result_nfaces = src_faces.size() - weld_mesh.face_kill_len + weld_mesh.wpoly_new_len;

  Mesh *result = BKE_mesh_new_nomain_from_template(
      &mesh, result_nverts, result_nedges, result_nfaces, result_nloops);
  MutableSpan<int2> dst_edges = result->edges_for_write();
  MutableSpan<int> dst_face_offsets = result->face_offsets_for_write();
  MutableSpan<int> dst_corner_verts = result->corner_verts_for_write();
  MutableSpan<int> dst_corner_edges = result->corner_edges_for_write();

  /* Vertices. */

  Array<int> vert_final_map;

  merge_customdata_all(&mesh.vert_data,
                       &result->vert_data,
                       vert_dest_map,
                       weld_mesh.double_verts,
                       result_nverts,
                       do_mix_data,
                       vert_final_map);

  /* Edges. */

  Array<int> edge_final_map;

  merge_customdata_all(&mesh.edge_data,
                       &result->edge_data,
                       weld_mesh.edge_dest_map,
                       weld_mesh.double_edges,
                       result_nedges,
                       do_mix_data,
                       edge_final_map);

  for (int2 &edge : dst_edges) {
    edge[0] = vert_final_map[edge[0]];
    edge[1] = vert_final_map[edge[1]];
    BLI_assert(edge[0] != edge[1]);
    BLI_assert(IN_RANGE_INCL(edge[0], 0, result_nverts - 1));
    BLI_assert(IN_RANGE_INCL(edge[1], 0, result_nverts - 1));
  }

  /* Faces/Loops. */

  int r_i = 0;
  int loop_cur = 0;
  Array<int, 64> group_buffer(weld_mesh.max_face_len);
  for (const int i : src_faces.index_range()) {
    const int loop_start = loop_cur;
    const int poly_ctx = weld_mesh.face_map[i];
    if (poly_ctx == OUT_OF_CONTEXT) {
      int mp_loop_len = src_faces[i].size();
      CustomData_copy_data(&mesh.loop_data,
                           &result->loop_data,
                           src_faces[i].start(),
                           loop_cur,
                           src_faces[i].size());
      for (; mp_loop_len--; loop_cur++) {
        dst_corner_verts[loop_cur] = vert_final_map[dst_corner_verts[loop_cur]];
        dst_corner_edges[loop_cur] = edge_final_map[dst_corner_edges[loop_cur]];
      }
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
      while (weld_iter_loop_of_poly_next(iter)) {
        customdata_weld(
            &mesh.loop_data, &result->loop_data, group_buffer.data(), iter.group_len, loop_cur);
        dst_corner_verts[loop_cur] = vert_final_map[iter.v];
        dst_corner_edges[loop_cur] = edge_final_map[iter.e];
        loop_cur++;
      }
    }

    CustomData_copy_data(&mesh.face_data, &result->face_data, i, r_i, 1);
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
    while (weld_iter_loop_of_poly_next(iter)) {
      customdata_weld(
          &mesh.loop_data, &result->loop_data, group_buffer.data(), iter.group_len, loop_cur);
      dst_corner_verts[loop_cur] = vert_final_map[iter.v];
      dst_corner_edges[loop_cur] = edge_final_map[iter.e];
      loop_cur++;
    }

    dst_face_offsets[r_i] = loop_start;
    r_i++;
  }

  BLI_assert(int(r_i) == result_nfaces);
  BLI_assert(loop_cur == result_nloops);

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
  Array<int> vert_dest_map(mesh.totvert, OUT_OF_CONTEXT);

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
  Array<int> vert_dest_map(mesh.totvert, OUT_OF_CONTEXT);

  Array<WeldVertexCluster> vert_clusters(mesh.totvert);

  for (const int i : positions.index_range()) {
    WeldVertexCluster &vc = vert_clusters[i];
    copy_v3_v3(vc.co, positions[i]);
    vc.merged_verts = 0;
  }
  const float merge_dist_sq = square_f(merge_distance);

  range_vn_i(vert_dest_map.data(), mesh.totvert, 0);

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

  for (const int i : IndexRange(mesh.totvert)) {
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
                       const bool do_mix_vert_data)
{
  return create_merged_mesh(mesh, vert_dest_map, vert_dest_map_len, do_mix_vert_data);
}

/** \} */

}  // namespace blender::geometry
