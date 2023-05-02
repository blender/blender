/* SPDX-License-Identifier: GPL-2.0-or-later */

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

// #define USE_WELD_DEBUG

namespace blender::geometry {

/* Indicates when the element was not computed. */
#define OUT_OF_CONTEXT int(-1)
/* Indicates if the edge or face will be collapsed. */
#define ELEM_COLLAPSED int(-2)
/* indicates whether an edge or vertex in groups_map will be merged. */
#define ELEM_MERGED int(-2)

struct WeldVert {
  /* Indices relative to the original Mesh. */
  int vert_dest;
  int vert_orig;
};

struct WeldEdge {
  union {
    int flag;
    struct {
      /* Indices relative to the original Mesh. */
      int edge_dest;
      int edge_orig;
      int vert_a;
      int vert_b;
    };
  };
};

struct WeldLoop {
  union {
    int flag;
    struct {
      /* Indices relative to the original Mesh. */
      int vert;
      int edge;
      int loop_orig;
      int loop_skip_to;
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
      /* Final Polygon Size. */
      int loop_len;
      /* Group of loops that will be affected. */
      struct {
        int len;
        int offs;
      } loops;
    };
  };
};

struct WeldMesh {
  /* Group of vertices to be merged. */
  Array<int> vert_groups_offs;
  Array<int> vert_groups_buffer;

  /* Group of edges to be merged. */
  Array<int> edge_groups_offs;
  Array<int> edge_groups_buffer;
  /* From the original edge index, this indicates which group it is going to be merged. */
  Array<int> edge_groups_map;
  Array<int2> edge_groups_verts;

  /* References all polygons and loops that will be affected. */
  Vector<WeldLoop> wloop;
  Vector<WeldPoly> wpoly;
  int wpoly_new_len;

  /* From the actual index of the element in the mesh, it indicates what is the index of the Weld
   * element above. */
  Array<int> loop_map;
  Array<int> poly_map;

  int vert_kill_len;
  int edge_kill_len;
  int loop_kill_len;
  int poly_kill_len; /* Including the new polygons. */

  /* Size of the affected polygon with more sides. */
  int max_poly_len;
};

struct WeldLoopOfPolyIter {
  int loop_start;
  int loop_end;
  Span<WeldLoop> wloop;
  Span<int> corner_verts;
  Span<int> corner_edges;
  Span<int> loop_map;
  /* Weld group. */
  int *group;

  int l_curr;
  int l_next;

  /* Return */
  int group_len;
  int v;
  int e;
  char type;
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

static void weld_assert_edge_kill_len(Span<WeldEdge> wedge, const int supposed_kill_len)
{
  int kills = 0;
  const WeldEdge *we = &wedge[0];
  for (int i = wedge.size(); i--; we++) {
    int edge_dest = we->edge_dest;
    /* Magically includes collapsed edges. */
    if (edge_dest != OUT_OF_CONTEXT) {
      kills++;
    }
  }
  BLI_assert(kills == supposed_kill_len);
}

static void weld_assert_poly_and_loop_kill_len(WeldMesh *weld_mesh,
                                               const Span<int> corner_verts,
                                               const Span<int> corner_edges,
                                               const OffsetIndices<int> polys,
                                               const int supposed_poly_kill_len,
                                               const int supposed_loop_kill_len)
{
  int poly_kills = 0;
  int loop_kills = corner_verts.size();
  for (const int i : polys.index_range()) {
    int poly_ctx = weld_mesh->poly_map[i];
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
            if (wl->loop_skip_to != OUT_OF_CONTEXT) {
              l_next = wl->loop_skip_to;
            }
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
      loop_kills -= polys[i].size();
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
        if (wl->loop_skip_to != OUT_OF_CONTEXT) {
          l_next = wl->loop_skip_to;
        }
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

  BLI_assert(poly_kills == supposed_poly_kill_len);
  BLI_assert(loop_kills == supposed_loop_kill_len);
}

static void weld_assert_poly_no_vert_repetition(const WeldPoly &wp,
                                                Span<WeldLoop> wloop,
                                                const Span<int> corner_verts,
                                                const Span<int> corner_edges,
                                                Span<int> loop_map)
{
  const int loop_len = wp.loop_len;
  Array<int, 64> verts(loop_len);
  WeldLoopOfPolyIter iter;
  if (!weld_iter_loop_of_poly_begin(
          iter, wp, wloop, corner_verts, corner_edges, loop_map, nullptr)) {
    return;
  }
  else {
    int i = 0;
    while (weld_iter_loop_of_poly_next(iter)) {
      verts[i++] = iter.v;
    }
  }
  for (int i = 0; i < loop_len; i++) {
    int va = verts[i];
    for (int j = i + 1; j < loop_len; j++) {
      int vb = verts[j];
      BLI_assert(va != vb);
    }
  }
}

static void weld_assert_poly_len(const WeldPoly *wp, const Span<WeldLoop> wloop)
{
  if (wp->flag == ELEM_COLLAPSED) {
    return;
  }

  int loop_len = wp->loop_len;
  const WeldLoop *wl = &wloop[wp->loops.offs];
  BLI_assert(wp->loop_start <= wl->loop_orig);

  int end_wloop = wp->loops.offs + wp->loops.len;
  const WeldLoop *wl_end = &wloop[end_wloop - 1];

  int min_len = 0;
  for (; wl <= wl_end; wl++) {
    BLI_assert(wl->loop_skip_to == OUT_OF_CONTEXT); /* Not for this case. */
    if (wl->flag != ELEM_COLLAPSED) {
      min_len++;
    }
  }
  BLI_assert(loop_len >= min_len);

  int max_len = wp->loop_end - wp->loop_start + 1;
  BLI_assert(loop_len <= max_len);
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
static Vector<WeldVert> weld_vert_ctx_alloc_and_setup(MutableSpan<int> vert_dest_map,
                                                      const int vert_kill_len)
{
  Vector<WeldVert> wvert;
  wvert.reserve(std::min<int>(2 * vert_kill_len, vert_dest_map.size()));

  for (const int i : vert_dest_map.index_range()) {
    if (vert_dest_map[i] != OUT_OF_CONTEXT) {
      const int vert_dest = vert_dest_map[i];
      WeldVert wv{};
      wv.vert_dest = vert_dest;
      wv.vert_orig = i;
      wvert.append(wv);

      if (vert_dest_map[vert_dest] != vert_dest) {
        /* The target vertex is also part of the context and needs to be referenced.
         * #vert_dest_map could already indicate this from the beginning, but for better
         * compatibility, it is done here as well. */
        vert_dest_map[vert_dest] = vert_dest;
        wv.vert_orig = vert_dest;
        wvert.append(wv);
      }
    }
  }
  return wvert;
}

/**
 * Create groups of vertices to merge.
 *
 * \return r_vert_groups_map: Map that points out the group of vertices that a vertex belongs to.
 * \return r_vert_groups_buffer: Buffer containing the indices of all vertices that merge.
 * \return r_vert_groups_offs: Array that indicates where each vertex group starts in the buffer.
 */
static void weld_vert_groups_setup(Span<WeldVert> wvert,
                                   Span<int> vert_dest_map,
                                   const int vert_kill_len,
                                   MutableSpan<int> r_vert_groups_map,
                                   Array<int> &r_vert_groups_buffer,
                                   Array<int> &r_vert_groups_offs)
{
  /**
   * Since `r_vert_groups_map` comes from `vert_dest_map`, we don't need to reset vertices out of
   * context again.
   *
   * \code{.c}
   * for (const int i : vert_dest_map.index_range()) {
   *   r_vert_groups_map[i] = OUT_OF_CONTEXT;
   * }
   * \endcode
   */
  BLI_assert(r_vert_groups_map.data() == vert_dest_map.data());
  UNUSED_VARS_NDEBUG(vert_dest_map);

  const int vert_groups_len = wvert.size() - vert_kill_len;

  /* Add +1 to allow calculation of the length of the last group. */
  r_vert_groups_offs.reinitialize(vert_groups_len + 1);
  r_vert_groups_offs.fill(0);

  int wgroups_len = 0;
  for (const WeldVert &wv : wvert) {
    if (wv.vert_dest == wv.vert_orig) {
      /* Indicate the index of the vertex group */
      r_vert_groups_map[wv.vert_orig] = wgroups_len;
      wgroups_len++;
    }
    else {
      r_vert_groups_map[wv.vert_orig] = ELEM_MERGED;
    }
  }

  for (const WeldVert &wv : wvert) {
    int group_index = r_vert_groups_map[wv.vert_dest];
    r_vert_groups_offs[group_index]++;
  }

  int offs = 0;
  for (const int i : IndexRange(vert_groups_len)) {
    offs += r_vert_groups_offs[i];
    r_vert_groups_offs[i] = offs;
  }
  r_vert_groups_offs[vert_groups_len] = offs;

  BLI_assert(offs == wvert.size());

  r_vert_groups_buffer.reinitialize(offs);

  /* Use a reverse for loop to ensure that indexes are assigned in ascending order. */
  for (int i = wvert.size(); i--;) {
    const WeldVert &wv = wvert[i];
    int group_index = r_vert_groups_map[wv.vert_dest];
    r_vert_groups_buffer[--r_vert_groups_offs[group_index]] = wv.vert_orig;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edge API
 * \{ */

/**
 * Alloc Weld Edges.
 *
 * \return r_edge_dest_map: First step to create map of indices pointing edges that will be merged.
 * \return r_edge_ctx_map: Map of indices pointing original edges to weld context edges.
 */
static Vector<WeldEdge> weld_edge_ctx_alloc_and_find_collapsed(Span<int2> edges,
                                                               Span<int> vert_dest_map,
                                                               MutableSpan<int> r_edge_dest_map,
                                                               MutableSpan<int> r_edge_ctx_map,
                                                               int *r_edge_collapsed_len)
{
  /* Edge Context. */
  int wedge_len = 0;
  int edge_collapsed_len = 0;

  Vector<WeldEdge> wedge;
  wedge.reserve(edges.size());

  for (const int i : edges.index_range()) {
    int v1 = edges[i][0];
    int v2 = edges[i][1];
    int v_dest_1 = vert_dest_map[v1];
    int v_dest_2 = vert_dest_map[v2];
    if ((v_dest_1 != OUT_OF_CONTEXT) || (v_dest_2 != OUT_OF_CONTEXT)) {
      WeldEdge we{};
      we.vert_a = (v_dest_1 != OUT_OF_CONTEXT) ? v_dest_1 : v1;
      we.vert_b = (v_dest_2 != OUT_OF_CONTEXT) ? v_dest_2 : v2;
      we.edge_dest = OUT_OF_CONTEXT;
      we.edge_orig = i;

      if (we.vert_a == we.vert_b) {
        we.flag = ELEM_COLLAPSED;
        edge_collapsed_len++;
        r_edge_dest_map[i] = ELEM_COLLAPSED;
      }
      else {
        r_edge_dest_map[i] = i;
      }

      wedge.append(we);
      r_edge_ctx_map[i] = wedge_len++;
    }
    else {
      r_edge_dest_map[i] = OUT_OF_CONTEXT;
      r_edge_ctx_map[i] = OUT_OF_CONTEXT;
    }
  }

  *r_edge_collapsed_len = edge_collapsed_len;
  return wedge;
}

/**
 * Configure Weld Edges.
 *
 * \param r_vlinks: An uninitialized buffer used to compute groups of WeldEdges attached to each
 *                  weld target vertex. It doesn't need to be passed as a parameter but this is
 *                  done to reduce allocations.
 * \return r_edge_dest_map: Map of indices pointing edges that will be merged.
 * \return r_wedge: Weld edges. `flag` and `edge_dest` members will be set here.
 * \return r_edge_kill_len: Number of edges to be destroyed by merging or collapsing.
 */
static void weld_edge_find_doubles(int remain_edge_ctx_len,
                                   int mvert_num,
                                   MutableSpan<int> r_edge_dest_map,
                                   MutableSpan<WeldEdge> r_wedge,
                                   int *r_edge_kill_len)
{
  if (remain_edge_ctx_len == 0) {
    return;
  }

  /* Setup Edge Overlap. */
  int edge_double_len = 0;

  /* Add +1 to allow calculation of the length of the last group. */
  Array<int> v_links(mvert_num + 1, 0);

  for (WeldEdge &we : r_wedge) {
    if (we.flag == ELEM_COLLAPSED) {
      BLI_assert(r_edge_dest_map[we.edge_orig] == ELEM_COLLAPSED);
      continue;
    }

    BLI_assert(we.vert_a != we.vert_b);
    v_links[we.vert_a]++;
    v_links[we.vert_b]++;
  }

  int link_len = 0;
  for (const int i : IndexRange(v_links.size() - 1)) {
    link_len += v_links[i];
    v_links[i] = link_len;
  }
  v_links.last() = link_len;

  BLI_assert(link_len > 0);
  Array<int> link_edge_buffer(link_len);

  /* Use a reverse for loop to ensure that indexes are assigned in ascending order. */
  for (int i = r_wedge.size(); i--;) {
    const WeldEdge &we = r_wedge[i];
    if (we.flag == ELEM_COLLAPSED) {
      continue;
    }

    int dst_vert_a = we.vert_a;
    int dst_vert_b = we.vert_b;

    link_edge_buffer[--v_links[dst_vert_a]] = i;
    link_edge_buffer[--v_links[dst_vert_b]] = i;
  }

  for (const int i : r_wedge.index_range()) {
    const WeldEdge &we = r_wedge[i];
    if (we.edge_dest != OUT_OF_CONTEXT) {
      /* No need to retest edges.
       * (Already includes collapsed edges). */
      continue;
    }

    int dst_vert_a = we.vert_a;
    int dst_vert_b = we.vert_b;

    const int link_a = v_links[dst_vert_a];
    const int link_b = v_links[dst_vert_b];

    int edges_len_a = v_links[dst_vert_a + 1] - link_a;
    int edges_len_b = v_links[dst_vert_b + 1] - link_b;

    if (edges_len_a <= 1 || edges_len_b <= 1) {
      continue;
    }

    int *edges_ctx_a = &link_edge_buffer[link_a];
    int *edges_ctx_b = &link_edge_buffer[link_b];
    int edge_orig = we.edge_orig;

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
        WeldEdge *we_b = &r_wedge[e_ctx_b];
        BLI_assert(ELEM(we_b->vert_a, dst_vert_a, dst_vert_b));
        BLI_assert(ELEM(we_b->vert_b, dst_vert_a, dst_vert_b));
        BLI_assert(we_b->edge_dest == OUT_OF_CONTEXT);
        BLI_assert(we_b->edge_orig != edge_orig);
        r_edge_dest_map[we_b->edge_orig] = edge_orig;
        we_b->edge_dest = edge_orig;
        edge_double_len++;
      }
    }
  }

  *r_edge_kill_len += edge_double_len;

#ifdef USE_WELD_DEBUG
  weld_assert_edge_kill_len(r_wedge, *r_edge_kill_len);
#endif
}

/**
 * Create groups of edges to merge.
 *
 * \return r_edge_groups_map: Map that points out the group of edges that an edge belongs to.
 * \return r_edge_groups_buffer: Buffer containing the indices of all edges that merge.
 * \return r_edge_groups_offs: Array that indicates where each edge group starts in the buffer.
 */
static void weld_edge_groups_setup(const int edges_len,
                                   const int edge_kill_len,
                                   MutableSpan<WeldEdge> wedge,
                                   Span<int> wedge_map,
                                   MutableSpan<int> r_edge_groups_map,
                                   Array<int> &r_edge_groups_buffer,
                                   Array<int> &r_edge_groups_offs,
                                   Array<int2> &r_edge_groups_verts)
{
  int wgroups_len = wedge.size() - edge_kill_len;

  r_edge_groups_verts.reinitialize(wgroups_len);

  wgroups_len = 0;
  for (const int i : IndexRange(edges_len)) {
    int edge_ctx = wedge_map[i];
    if (edge_ctx != OUT_OF_CONTEXT) {
      WeldEdge *we = &wedge[edge_ctx];
      int edge_dest = we->edge_dest;
      if (edge_dest != OUT_OF_CONTEXT) {
        BLI_assert(edge_dest != we->edge_orig);
        r_edge_groups_map[i] = ELEM_MERGED;
      }
      else {
        we->edge_dest = we->edge_orig;
        r_edge_groups_verts[wgroups_len] = {we->vert_a, we->vert_b};
        r_edge_groups_map[i] = wgroups_len;
        wgroups_len++;
      }
    }
    else {
      r_edge_groups_map[i] = OUT_OF_CONTEXT;
    }
  }

  BLI_assert(wgroups_len == wedge.size() - edge_kill_len);

  if (wgroups_len == 0) {
    /* All edges in the context are collapsed. */
    return;
  }

  /* Add +1 to allow calculation of the length of the last group. */
  r_edge_groups_offs.reinitialize(wgroups_len + 1);
  r_edge_groups_offs.fill(0);

  for (const WeldEdge &we : wedge) {
    if (we.flag == ELEM_COLLAPSED) {
      continue;
    }
    int group_index = r_edge_groups_map[we.edge_dest];
    r_edge_groups_offs[group_index]++;
  }

  int offs = 0;
  for (const int i : IndexRange(wgroups_len)) {
    offs += r_edge_groups_offs[i];
    r_edge_groups_offs[i] = offs;
  }
  r_edge_groups_offs[wgroups_len] = offs;

  r_edge_groups_buffer.reinitialize(offs);

  /* Use a reverse for loop to ensure that indexes are assigned in ascending order. */
  for (int i = wedge.size(); i--;) {
    const WeldEdge &we = wedge[i];
    if (we.flag == ELEM_COLLAPSED) {
      continue;
    }
    int group_index = r_edge_groups_map[we.edge_dest];
    r_edge_groups_buffer[--r_edge_groups_offs[group_index]] = we.edge_orig;
  }
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

  iter.loop_start = wp.loop_start;
  iter.loop_end = wp.loop_end;
  iter.wloop = wloop;
  iter.corner_verts = corner_verts;
  iter.corner_edges = corner_edges;
  iter.loop_map = loop_map;
  iter.group = group_buffer;

  int group_len = 0;
  if (group_buffer) {
    /* First loop group needs more attention. */
    int loop_start, loop_end, l;
    loop_start = iter.loop_start;
    loop_end = l = iter.loop_end;
    while (l >= loop_start) {
      const int loop_ctx = loop_map[l];
      if (loop_ctx != OUT_OF_CONTEXT) {
        const WeldLoop *wl = &wloop[loop_ctx];
        if (wl->flag == ELEM_COLLAPSED) {
          l--;
          continue;
        }
      }
      break;
    }
    if (l != loop_end) {
      group_len = loop_end - l;
      int i = 0;
      while (l < loop_end) {
        iter.group[i++] = ++l;
      }
    }
  }
  iter.group_len = group_len;

  iter.l_next = iter.loop_start;
#ifdef USE_WELD_DEBUG
  iter.v = OUT_OF_CONTEXT;
#endif
  return true;
}

static bool weld_iter_loop_of_poly_next(WeldLoopOfPolyIter &iter)
{
  const int loop_end = iter.loop_end;
  Span<WeldLoop> wloop = iter.wloop;
  Span<int> loop_map = iter.loop_map;
  int l = iter.l_curr = iter.l_next;
  if (l == iter.loop_start) {
    /* `grupo_len` is already calculated in the first loop */
  }
  else {
    iter.group_len = 0;
  }
  while (l <= loop_end) {
    int l_next = l + 1;
    const int loop_ctx = loop_map[l];
    if (loop_ctx != OUT_OF_CONTEXT) {
      const WeldLoop *wl = &wloop[loop_ctx];
      if (wl->loop_skip_to != OUT_OF_CONTEXT) {
        l_next = wl->loop_skip_to;
      }
      if (wl->flag == ELEM_COLLAPSED) {
        if (iter.group) {
          iter.group[iter.group_len++] = l;
        }
        l = l_next;
        continue;
      }
#ifdef USE_WELD_DEBUG
      BLI_assert(iter.v != wl->vert);
#endif
      iter.v = wl->vert;
      iter.e = wl->edge;
      iter.type = 1;
    }
    else {
#ifdef USE_WELD_DEBUG
      BLI_assert(iter.v != iter.corner_verts[l]);
#endif
      iter.v = iter.corner_verts[l];
      iter.e = iter.corner_edges[l];
      iter.type = 0;
    }
    if (iter.group) {
      iter.group[iter.group_len++] = l;
    }
    iter.l_next = l_next;
    return true;
  }

  return false;
}

/**
 * Alloc Weld Polygons and Weld Loops.
 *
 * \return r_weld_mesh: Loop and poly members will be allocated here.
 */
static void weld_poly_loop_ctx_alloc(const OffsetIndices<int> polys,
                                     const Span<int> corner_verts,
                                     const Span<int> corner_edges,
                                     Span<int> vert_dest_map,
                                     Span<int> edge_dest_map,
                                     WeldMesh *r_weld_mesh)
{
  /* Loop/Poly Context. */
  Array<int> loop_map(corner_verts.size());
  Array<int> poly_map(polys.size());
  int wloop_len = 0;
  int wpoly_len = 0;
  int max_ctx_poly_len = 4;

  Vector<WeldLoop> wloop;
  wloop.reserve(corner_verts.size());

  Vector<WeldPoly> wpoly;
  wpoly.reserve(polys.size());

  int maybe_new_poly = 0;

  for (const int i : polys.index_range()) {
    const int loopstart = polys[i].start();
    const int totloop = polys[i].size();

    int prev_wloop_len = wloop_len;
    for (const int i_loop : polys[i]) {
      int v = corner_verts[i_loop];
      int e = corner_edges[i_loop];
      int v_dest = vert_dest_map[v];
      int e_dest = edge_dest_map[e];
      bool is_vert_ctx = v_dest != OUT_OF_CONTEXT;
      bool is_edge_ctx = e_dest != OUT_OF_CONTEXT;
      if (is_vert_ctx || is_edge_ctx) {
        WeldLoop wl{};
        wl.vert = is_vert_ctx ? v_dest : v;
        wl.edge = is_edge_ctx ? e_dest : e;
        wl.loop_orig = i_loop;
        wl.loop_skip_to = OUT_OF_CONTEXT;
        wloop.append(wl);

        loop_map[i_loop] = wloop_len++;
      }
      else {
        loop_map[i_loop] = OUT_OF_CONTEXT;
      }
    }
    if (wloop_len != prev_wloop_len) {
      int loops_len = wloop_len - prev_wloop_len;
      WeldPoly wp{};
      wp.poly_dst = OUT_OF_CONTEXT;
      wp.poly_orig = i;
      wp.loops.len = loops_len;
      wp.loops.offs = prev_wloop_len;
      wp.loop_start = loopstart;
      wp.loop_end = loopstart + totloop - 1;
      wp.loop_len = totloop;
      wpoly.append(wp);

      poly_map[i] = wpoly_len++;
      if (totloop > 5 && loops_len > 1) {
        /* We could be smarter here and actually count how many new polygons will be created.
         * But counting this can be inefficient as it depends on the number of non-consecutive
         * self polygon merges. For now just estimate a maximum value. */
        int max_new = std::min((totloop / 3), loops_len) - 1;
        maybe_new_poly += max_new;
        CLAMP_MIN(max_ctx_poly_len, totloop);
      }
    }
    else {
      poly_map[i] = OUT_OF_CONTEXT;
    }
  }

  wpoly.reserve(wpoly.size() + maybe_new_poly);

  r_weld_mesh->wloop = std::move(wloop);
  r_weld_mesh->wpoly = std::move(wpoly);
  r_weld_mesh->wpoly_new_len = 0;
  r_weld_mesh->loop_map = std::move(loop_map);
  r_weld_mesh->poly_map = std::move(poly_map);
  r_weld_mesh->max_poly_len = max_ctx_poly_len;
}

static void weld_poly_split_recursive(Span<int> vert_dest_map,
#ifdef USE_WELD_DEBUG
                                      const Span<int> corner_verts,
                                      const Span<int> corner_edges,
#endif
                                      int ctx_verts_len,
                                      WeldPoly *r_wp,
                                      WeldMesh *r_weld_mesh,
                                      int *r_poly_kill,
                                      int *r_loop_kill)
{
  int poly_loop_len = r_wp->loop_len;
  if (poly_loop_len < 3 || ctx_verts_len < 1) {
    return;
  }

  const int ctx_loops_len = r_wp->loops.len;
  const int ctx_loops_ofs = r_wp->loops.offs;
  MutableSpan<WeldLoop> wloop = r_weld_mesh->wloop;

  int loop_kill = 0;

  WeldLoop *poly_loops = &wloop[ctx_loops_ofs];
  WeldLoop *wla = &poly_loops[0];
  WeldLoop *wla_prev = &poly_loops[ctx_loops_len - 1];
  while (wla_prev->flag == ELEM_COLLAPSED) {
    wla_prev--;
  }
  const int la_len = ctx_loops_len - 1;
  for (int la = 0; la < la_len; la++, wla++) {
  wa_continue:
    if (wla->flag == ELEM_COLLAPSED) {
      continue;
    }
    int vert_a = wla->vert;
    /* Only test vertices that will be merged. */
    if (vert_dest_map[vert_a] != OUT_OF_CONTEXT) {
      int lb = la + 1;
      WeldLoop *wlb = wla + 1;
      WeldLoop *wlb_prev = wla;
      int killed_ab = 0;
      ctx_verts_len = 1;
      for (; lb < ctx_loops_len; lb++, wlb++) {
        BLI_assert(wlb->loop_skip_to == OUT_OF_CONTEXT);
        if (wlb->flag == ELEM_COLLAPSED) {
          killed_ab++;
          continue;
        }
        int vert_b = wlb->vert;
        if (vert_dest_map[vert_b] != OUT_OF_CONTEXT) {
          ctx_verts_len++;
        }
        if (vert_a == vert_b) {
          const int dist_a = wlb->loop_orig - wla->loop_orig - killed_ab;
          const int dist_b = poly_loop_len - dist_a;

          BLI_assert(dist_a != 0 && dist_b != 0);
          if (dist_a == 1 || dist_b == 1) {
            BLI_assert(dist_a != dist_b);
            BLI_assert((wla->flag == ELEM_COLLAPSED) || (wlb->flag == ELEM_COLLAPSED));
          }
          else {
            WeldLoop *wl_tmp = nullptr;
            if (dist_a == 2) {
              wl_tmp = wlb_prev;
              BLI_assert(wla->flag != ELEM_COLLAPSED);
              BLI_assert(wl_tmp->flag != ELEM_COLLAPSED);
              wla->flag = ELEM_COLLAPSED;
              wl_tmp->flag = ELEM_COLLAPSED;
              loop_kill += 2;
              poly_loop_len -= 2;
            }
            if (dist_b == 2) {
              if (wl_tmp != nullptr) {
                r_wp->flag = ELEM_COLLAPSED;
                *r_poly_kill += 1;
              }
              else {
                wl_tmp = wla_prev;
                BLI_assert(wlb->flag != ELEM_COLLAPSED);
                BLI_assert(wl_tmp->flag != ELEM_COLLAPSED);
                wlb->flag = ELEM_COLLAPSED;
                wl_tmp->flag = ELEM_COLLAPSED;
              }
              loop_kill += 2;
              poly_loop_len -= 2;
            }
            if (wl_tmp == nullptr) {
              const int new_loops_len = lb - la;
              const int new_loops_ofs = ctx_loops_ofs + la;

              r_weld_mesh->wpoly.increase_size_by_unchecked(1);
              WeldPoly *new_wp = &r_weld_mesh->wpoly.last();
              new_wp->poly_dst = OUT_OF_CONTEXT;
              new_wp->poly_orig = r_wp->poly_orig;
              new_wp->loops.len = new_loops_len;
              new_wp->loops.offs = new_loops_ofs;
              new_wp->loop_start = wla->loop_orig;
              new_wp->loop_end = wlb_prev->loop_orig;
              new_wp->loop_len = dist_a;
              r_weld_mesh->wpoly_new_len++;
              weld_poly_split_recursive(vert_dest_map,
#ifdef USE_WELD_DEBUG
                                        corner_verts,
                                        corner_edges,
#endif
                                        ctx_verts_len,
                                        new_wp,
                                        r_weld_mesh,
                                        r_poly_kill,
                                        r_loop_kill);
              BLI_assert(dist_b == poly_loop_len - dist_a);
              poly_loop_len = dist_b;
              if (wla_prev->loop_orig > wla->loop_orig) {
                /* New start. */
                r_wp->loop_start = wlb->loop_orig;
              }
              else {
                /* The `loop_start` doesn't change but some loops must be skipped. */
                wla_prev->loop_skip_to = wlb->loop_orig;
              }
              wla = wlb;
              la = lb;
              goto wa_continue;
            }
            break;
          }
        }
        if (wlb->flag != ELEM_COLLAPSED) {
          wlb_prev = wlb;
        }
      }
    }
    if (wla->flag != ELEM_COLLAPSED) {
      wla_prev = wla;
    }
  }
  r_wp->loop_len = poly_loop_len;
  *r_loop_kill += loop_kill;

#ifdef USE_WELD_DEBUG
  weld_assert_poly_no_vert_repetition(
      *r_wp, wloop, corner_verts, corner_edges, r_weld_mesh->loop_map);
#endif
}

/**
 * Alloc Weld Polygons and Weld Loops.
 *
 * \param remain_edge_ctx_len: Context weld edges that won't be destroyed by merging or collapsing.
 * \param r_vlinks: An uninitialized buffer used to compute groups of WeldPolys attached to each
 *                  weld target vertex. It doesn't need to be passed as a parameter but this is
 *                  done to reduce allocations.
 * \return r_weld_mesh: Loop and poly members will be configured here.
 */
static void weld_poly_loop_ctx_setup_collapsed_and_split(
#ifdef USE_WELD_DEBUG
    const Span<int> corner_verts,
    const Span<int> corner_edges,
    const OffsetIndices<int> polys,
#endif
    Span<int> vert_dest_map,
    const int remain_edge_ctx_len,
    WeldMesh *r_weld_mesh)
{
  if (remain_edge_ctx_len == 0) {
    r_weld_mesh->poly_kill_len = r_weld_mesh->wpoly.size();
    r_weld_mesh->loop_kill_len = r_weld_mesh->wloop.size();

    for (WeldPoly &wp : r_weld_mesh->wpoly) {
      wp.flag = ELEM_COLLAPSED;
    }

    return;
  }

  WeldPoly *wpoly = r_weld_mesh->wpoly.data();
  MutableSpan<WeldLoop> wloop = r_weld_mesh->wloop;

  int poly_kill_len = 0;
  int loop_kill_len = 0;

  /* Setup Poly/Loop. */
  /* `wpoly.size()` may change during the loop,
   * so make it clear that we are only working with the original `wpoly` items. */
  IndexRange wpoly_original_range = r_weld_mesh->wpoly.index_range();
  for (const int i : wpoly_original_range) {
    WeldPoly &wp = wpoly[i];
    const int ctx_loops_len = wp.loops.len;
    const int ctx_loops_ofs = wp.loops.offs;

    int poly_loop_len = wp.loop_len;
    int ctx_verts_len = 0;
    WeldLoop *wl = &wloop[ctx_loops_ofs];
    for (int l = ctx_loops_len; l--; wl++) {
      const int edge_dest = wl->edge;
      if (edge_dest == ELEM_COLLAPSED) {
        wl->flag = ELEM_COLLAPSED;
        if (poly_loop_len == 3) {
          wp.flag = ELEM_COLLAPSED;
          poly_kill_len++;
          loop_kill_len += 3;
          poly_loop_len = 0;
          break;
        }
        loop_kill_len++;
        poly_loop_len--;
      }
      else {
        const int vert_dst = wl->vert;
        if (vert_dest_map[vert_dst] != OUT_OF_CONTEXT) {
          ctx_verts_len++;
        }
      }
    }

    if (poly_loop_len) {
      wp.loop_len = poly_loop_len;
#ifdef USE_WELD_DEBUG
      weld_assert_poly_len(&wp, wloop);
#endif

      weld_poly_split_recursive(vert_dest_map,
#ifdef USE_WELD_DEBUG
                                mloop,
#endif
                                ctx_verts_len,
                                &wp,
                                r_weld_mesh,
                                &poly_kill_len,
                                &loop_kill_len);
    }
  }

  r_weld_mesh->poly_kill_len = poly_kill_len;
  r_weld_mesh->loop_kill_len = loop_kill_len;

#ifdef USE_WELD_DEBUG
  weld_assert_poly_and_loop_kill_len(
      r_weld_mesh, mloop, polys, r_weld_mesh->poly_kill_len, r_weld_mesh->loop_kill_len);
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
  Array<int> linked_polys_offset(corner_index_max + 1, 0);

  for (const int elem_index : corners) {
    linked_polys_offset[elem_index]++;
  }

  int link_polys_buffer_len = 0;
  for (const int elem_index : IndexRange(corner_index_max)) {
    link_polys_buffer_len += linked_polys_offset[elem_index];
    linked_polys_offset[elem_index] = link_polys_buffer_len;
  }
  linked_polys_offset[corner_index_max] = link_polys_buffer_len;

  if (link_polys_buffer_len == 0) {
    return 0;
  }

  Array<int> linked_polys_buffer(link_polys_buffer_len);

  /* Use a reverse for loop to ensure that indexes are assigned in ascending order. */
  for (int poly_index = poly_num; poly_index--;) {
    if (poly_corners_offsets[poly_index].size() == 0) {
      continue;
    }

    for (int corner_index = poly_corners_offsets[poly_index].last();
         corner_index >= poly_corners_offsets[poly_index].first();
         corner_index--)
    {
      const int elem_index = corners[corner_index];
      linked_polys_buffer[--linked_polys_offset[elem_index]] = poly_index;
    }
  }

  Array<int> doubles_buffer(poly_num);

  Vector<int> doubles_offsets;
  doubles_offsets.reserve((poly_num / 2) + 1);
  doubles_offsets.append(0);

  BitVector<> is_double(poly_num, false);

  int doubles_buffer_num = 0;
  int doubles_num = 0;
  for (const int poly_index : IndexRange(poly_num)) {
    if (is_double[poly_index]) {
      continue;
    }

    int corner_num = poly_corners_offsets[poly_index].size();
    if (corner_num == 0) {
      continue;
    }

    /* Set or overwrite the first slot of the possible group. */
    doubles_buffer[doubles_buffer_num] = poly_index;

    int corner_first = poly_corners_offsets[poly_index].first();
    int elem_index = corners[corner_first];
    int link_offs = linked_polys_offset[elem_index];
    int polys_a_num = linked_polys_offset[elem_index + 1] - link_offs;
    if (polys_a_num == 1) {
      BLI_assert(linked_polys_buffer[linked_polys_offset[elem_index]] == poly_index);
      continue;
    }

    const int *polys_a = &linked_polys_buffer[link_offs];
    int poly_to_test;

    /* Skip polygons with lower index as these have already been checked. */
    do {
      poly_to_test = *polys_a;
      polys_a++;
      polys_a_num--;
    } while (poly_to_test != poly_index);

    int *isect_result = doubles_buffer.data() + doubles_buffer_num + 1;

    /* `polys_a` are the polygons connected to the first corner. So skip the first corner. */
    for (int corner_index : IndexRange(corner_first + 1, corner_num - 1)) {
      elem_index = corners[corner_index];
      link_offs = linked_polys_offset[elem_index];
      int polys_b_num = linked_polys_offset[elem_index + 1] - link_offs;
      const int *polys_b = &linked_polys_buffer[link_offs];

      /* Skip polygons with lower index as these have already been checked. */
      do {
        poly_to_test = *polys_b;
        polys_b++;
        polys_b_num--;
      } while (poly_to_test != poly_index);

      doubles_num = intersect(Span<int>{polys_a, polys_a_num},
                              Span<int>{polys_b, polys_b_num},
                              is_double,
                              isect_result);

      if (doubles_num == 0) {
        break;
      }

      /* Intersect the last result. */
      polys_a = isect_result;
      polys_a_num = doubles_num;
    }

    if (doubles_num) {
      for (const int poly_double : Span<int>{isect_result, doubles_num}) {
        BLI_assert(poly_double > poly_index);
        is_double[poly_double].set();
      }
      doubles_buffer_num += doubles_num;
      doubles_offsets.append(++doubles_buffer_num);

      if ((doubles_buffer_num + 1) == poly_num) {
        /* The last slot is the remaining unduplicated polygon.
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
#ifdef USE_WELD_DEBUG
                                   const OffsetIndices<int> polys,
#endif
                                   const int medge_len,
                                   WeldMesh *r_weld_mesh)
{
  if (r_weld_mesh->poly_kill_len == r_weld_mesh->wpoly.size()) {
    return;
  }

  WeldPoly *wpoly = r_weld_mesh->wpoly.data();
  MutableSpan<WeldLoop> wloop = r_weld_mesh->wloop;
  Span<int> loop_map = r_weld_mesh->loop_map;
  int poly_index = 0;

  const int poly_len = r_weld_mesh->wpoly.size();
  Array<int> poly_offs(poly_len + 1);
  Vector<int> new_corner_edges;
  new_corner_edges.reserve(corner_verts.size() - r_weld_mesh->loop_kill_len);

  for (const WeldPoly &wp : r_weld_mesh->wpoly) {
    poly_offs[poly_index++] = new_corner_edges.size();

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

  poly_offs[poly_len] = new_corner_edges.size();

  Vector<int> doubles_offsets;
  Array<int> doubles_buffer;
  const int doubles_num = poly_find_doubles(OffsetIndices<int>(poly_offs),
                                            poly_len,
                                            new_corner_edges,
                                            medge_len,
                                            doubles_offsets,
                                            doubles_buffer);

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
        loop_kill_num += wp.loop_len;
      }
    }

    r_weld_mesh->poly_kill_len += doubles_num;
    r_weld_mesh->loop_kill_len += loop_kill_num;
  }

#ifdef USE_WELD_DEBUG
  weld_assert_poly_and_loop_kill_len(
      r_weld_mesh, mloop, polys, r_weld_mesh->poly_kill_len, r_weld_mesh->loop_kill_len);
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh API
 * \{ */

static void weld_mesh_context_create(const Mesh &mesh,
                                     MutableSpan<int> vert_dest_map,
                                     const int vert_kill_len,
                                     MutableSpan<int> r_vert_group_map,
                                     WeldMesh *r_weld_mesh)
{
  const Span<int2> edges = mesh.edges();
  const OffsetIndices polys = mesh.polys();
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<int> corner_edges = mesh.corner_edges();

  Vector<WeldVert> wvert = weld_vert_ctx_alloc_and_setup(vert_dest_map, vert_kill_len);
  r_weld_mesh->vert_kill_len = vert_kill_len;

  Array<int> edge_dest_map(edges.size());
  Array<int> edge_ctx_map(edges.size());
  Vector<WeldEdge> wedge = weld_edge_ctx_alloc_and_find_collapsed(
      edges, vert_dest_map, edge_dest_map, edge_ctx_map, &r_weld_mesh->edge_kill_len);

  weld_edge_find_doubles(wedge.size() - r_weld_mesh->edge_kill_len,
                         mesh.totvert,
                         edge_dest_map,
                         wedge,
                         &r_weld_mesh->edge_kill_len);

  weld_poly_loop_ctx_alloc(
      polys, corner_verts, corner_edges, vert_dest_map, edge_dest_map, r_weld_mesh);

  weld_poly_loop_ctx_setup_collapsed_and_split(
#ifdef USE_WELD_DEBUG
      corner_verts,
      corner_edges,
      polys,
#endif
      vert_dest_map,
      wedge.size() - r_weld_mesh->edge_kill_len,
      r_weld_mesh);

  weld_poly_find_doubles(corner_verts,
                         corner_edges,
#ifdef USE_WELD_DEBUG
                         polys,
#endif
                         edges.size(),
                         r_weld_mesh);

  weld_vert_groups_setup(wvert,
                         vert_dest_map,
                         vert_kill_len,
                         r_vert_group_map,
                         r_weld_mesh->vert_groups_buffer,
                         r_weld_mesh->vert_groups_offs);

  weld_edge_groups_setup(edges.size(),
                         r_weld_mesh->edge_kill_len,
                         wedge,
                         edge_ctx_map,
                         edge_dest_map,
                         r_weld_mesh->edge_groups_buffer,
                         r_weld_mesh->edge_groups_offs,
                         r_weld_mesh->edge_groups_verts);

  r_weld_mesh->edge_groups_map = std::move(edge_dest_map);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name CustomData
 * \{ */

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
      if (CustomData_layer_has_interp(dest, dest_i)) {
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
    if (CustomData_layer_has_interp(dest, dest_i)) {
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Vertex Merging
 * \{ */

static Mesh *create_merged_mesh(const Mesh &mesh,
                                MutableSpan<int> vert_dest_map,
                                const int removed_vertex_count)
{
  const OffsetIndices src_polys = mesh.polys();
  const Span<int> src_corner_verts = mesh.corner_verts();
  const Span<int> src_corner_edges = mesh.corner_edges();
  const int totvert = mesh.totvert;
  const int totedge = mesh.totedge;

  /* Reuse the same buffer as #vert_dest_map.
   * NOTE: the caller must be made aware of it changes. */
  MutableSpan<int> vert_group_map = vert_dest_map;

  WeldMesh weld_mesh;
  weld_mesh_context_create(mesh, vert_dest_map, removed_vertex_count, vert_group_map, &weld_mesh);

  const int result_nverts = totvert - weld_mesh.vert_kill_len;
  const int result_nedges = totedge - weld_mesh.edge_kill_len;
  const int result_nloops = src_corner_verts.size() - weld_mesh.loop_kill_len;
  const int result_npolys = src_polys.size() - weld_mesh.poly_kill_len + weld_mesh.wpoly_new_len;

  Mesh *result = BKE_mesh_new_nomain_from_template(
      &mesh, result_nverts, result_nedges, result_npolys, result_nloops);
  MutableSpan<int2> dst_edges = result->edges_for_write();
  MutableSpan<int> dst_poly_offsets = result->poly_offsets_for_write();
  MutableSpan<int> dst_corner_verts = result->corner_verts_for_write();
  MutableSpan<int> dst_corner_edges = result->corner_edges_for_write();

  /* Vertices. */

  /* Be careful when setting values to this array as it uses the same buffer as #vert_group_map.
   * This map will be used to adjust edges and loops to point to new vertex indices. */
  MutableSpan<int> vert_final_map = vert_group_map;

  int dest_index = 0;
  for (int i = 0; i < totvert; i++) {
    int source_index = i;
    int count = 0;
    while (i < totvert && vert_group_map[i] == OUT_OF_CONTEXT) {
      vert_final_map[i] = dest_index + count;
      count++;
      i++;
    }
    if (count) {
      CustomData_copy_data(&mesh.vdata, &result->vdata, source_index, dest_index, count);
      dest_index += count;
    }
    if (i == totvert) {
      break;
    }
    if (vert_group_map[i] != ELEM_MERGED) {
      const int *wgroup = &weld_mesh.vert_groups_offs[vert_group_map[i]];
      customdata_weld(&mesh.vdata,
                      &result->vdata,
                      &weld_mesh.vert_groups_buffer[*wgroup],
                      *(wgroup + 1) - *wgroup,
                      dest_index);
      vert_final_map[i] = dest_index;
      dest_index++;
    }
  }

  BLI_assert(dest_index == result_nverts);

  /* Edges. */

  /* Be careful when editing this array as it uses the same buffer as #WeldMesh::edge_groups_map.
   * This map will be used to adjust edges and loops to point to new edge indices. */
  MutableSpan<int> edge_final_map = weld_mesh.edge_groups_map;

  dest_index = 0;
  for (int i = 0; i < totedge; i++) {
    const int source_index = i;
    int count = 0;
    while (i < totedge && weld_mesh.edge_groups_map[i] == OUT_OF_CONTEXT) {
      edge_final_map[i] = dest_index + count;
      count++;
      i++;
    }
    if (count) {
      CustomData_copy_data(&mesh.edata, &result->edata, source_index, dest_index, count);
      int2 *edge = &dst_edges[dest_index];
      dest_index += count;
      for (; count--; edge++) {
        (*edge)[0] = vert_final_map[(*edge)[0]];
        (*edge)[1] = vert_final_map[(*edge)[1]];
      }
    }
    if (i == totedge) {
      break;
    }
    if (weld_mesh.edge_groups_map[i] != ELEM_MERGED) {
      const int wegpr_index = weld_mesh.edge_groups_map[i];
      const int wegrp_offs = weld_mesh.edge_groups_offs[wegpr_index];
      const int wegrp_len = weld_mesh.edge_groups_offs[wegpr_index + 1] - wegrp_offs;
      int2 &wegrp_verts = weld_mesh.edge_groups_verts[wegpr_index];
      customdata_weld(&mesh.edata,
                      &result->edata,
                      &weld_mesh.edge_groups_buffer[wegrp_offs],
                      wegrp_len,
                      dest_index);
      int2 &edge = dst_edges[dest_index];
      edge[0] = vert_final_map[wegrp_verts[0]];
      edge[1] = vert_final_map[wegrp_verts[1]];

      edge_final_map[i] = dest_index;
      dest_index++;
    }
  }

  BLI_assert(dest_index == result_nedges);

  /* Polys/Loops. */

  int r_i = 0;
  int loop_cur = 0;
  Array<int, 64> group_buffer(weld_mesh.max_poly_len);
  for (const int i : src_polys.index_range()) {
    const int loop_start = loop_cur;
    const int poly_ctx = weld_mesh.poly_map[i];
    if (poly_ctx == OUT_OF_CONTEXT) {
      int mp_loop_len = src_polys[i].size();
      CustomData_copy_data(
          &mesh.ldata, &result->ldata, src_polys[i].start(), loop_cur, src_polys[i].size());
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
            &mesh.ldata, &result->ldata, group_buffer.data(), iter.group_len, loop_cur);
        dst_corner_verts[loop_cur] = vert_final_map[iter.v];
        dst_corner_edges[loop_cur] = edge_final_map[iter.e];
        loop_cur++;
      }
    }

    CustomData_copy_data(&mesh.pdata, &result->pdata, i, r_i, 1);
    dst_poly_offsets[r_i] = loop_start;
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
      customdata_weld(&mesh.ldata, &result->ldata, group_buffer.data(), iter.group_len, loop_cur);
      dst_corner_verts[loop_cur] = vert_final_map[iter.v];
      dst_corner_edges[loop_cur] = edge_final_map[iter.e];
      loop_cur++;
    }

    dst_poly_offsets[r_i] = loop_start;
    r_i++;
  }

  BLI_assert(int(r_i) == result_npolys);
  BLI_assert(loop_cur == result_nloops);

  return result;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Merge Map Creation
 * \{ */

std::optional<Mesh *> mesh_merge_by_distance_all(const Mesh &mesh,
                                                 const IndexMask selection,
                                                 const float merge_distance)
{
  Array<int> vert_dest_map(mesh.totvert, OUT_OF_CONTEXT);

  KDTree_3d *tree = BLI_kdtree_3d_new(selection.size());

  const Span<float3> positions = mesh.vert_positions();
  for (const int i : selection) {
    BLI_kdtree_3d_insert(tree, i, positions[i]);
  }

  BLI_kdtree_3d_balance(tree);
  const int vert_kill_len = BLI_kdtree_3d_calc_duplicates_fast(
      tree, merge_distance, true, vert_dest_map.data());
  BLI_kdtree_3d_free(tree);

  if (vert_kill_len == 0) {
    return std::nullopt;
  }

  return create_merged_mesh(mesh, vert_dest_map, vert_kill_len);
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

  return create_merged_mesh(mesh, vert_dest_map, vert_kill_len);
}

Mesh *mesh_merge_verts(const Mesh &mesh, MutableSpan<int> vert_dest_map, int vert_dest_map_len)
{
  return create_merged_mesh(mesh, vert_dest_map, vert_dest_map_len);
}

/** \} */

}  // namespace blender::geometry
