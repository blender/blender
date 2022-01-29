/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "BLI_array.hh"
#include "BLI_index_mask.hh"
#include "BLI_kdtree.h"
#include "BLI_math_vector.hh"
#include "BLI_vector.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_customdata.h"
#include "BKE_mesh.h"

#include "GEO_mesh_merge_by_distance.hh"

//#define USE_WELD_DEBUG
//#define USE_WELD_NORMALS

namespace blender::geometry {

/* Indicates when the element was not computed. */
#define OUT_OF_CONTEXT (int)(-1)
/* Indicates if the edge or face will be collapsed. */
#define ELEM_COLLAPSED (int)(-2)
/* indicates whether an edge or vertex in groups_map will be merged. */
#define ELEM_MERGED (int)(-2)

/* Used to indicate a range in an array specifying a group. */
struct WeldGroup {
  int len;
  int ofs;
};

/* Edge groups that will be merged. Final vertices are also indicated. */
struct WeldGroupEdge {
  struct WeldGroup group;
  int v1;
  int v2;
};

struct WeldVert {
  /* Indexes relative to the original Mesh. */
  int vert_dest;
  int vert_orig;
};

struct WeldEdge {
  union {
    int flag;
    struct {
      /* Indexes relative to the original Mesh. */
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
      /* Indexes relative to the original Mesh. */
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
      /* Indexes relative to the original Mesh. */
      int poly_dst;
      int poly_orig;
      int loop_start;
      int loop_end;
      /* Final Polygon Size. */
      int len;
      /* Group of loops that will be affected. */
      struct WeldGroup loops;
    };
  };
};

struct WeldMesh {
  /* Group of vertices to be merged. */
  Array<WeldGroup> vert_groups;
  Array<int> vert_groups_buffer;

  /* Group of edges to be merged. */
  Array<WeldGroupEdge> edge_groups;
  Array<int> edge_groups_buffer;
  /* From the original index of the vertex, this indicates which group it is or is going to be
   * merged. */
  Array<int> edge_groups_map;

  /* References all polygons and loops that will be affected. */
  Vector<WeldLoop> wloop;
  Vector<WeldPoly> wpoly;
  WeldPoly *wpoly_new;
  int wloop_len;
  int wpoly_len;
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
  Span<MLoop> mloop;
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
static bool weld_iter_loop_of_poly_begin(WeldLoopOfPolyIter *iter,
                                         const WeldPoly &wp,
                                         Span<WeldLoop> wloop,
                                         Span<MLoop> mloop,
                                         Span<int> loop_map,
                                         int *group_buffer);

static bool weld_iter_loop_of_poly_next(WeldLoopOfPolyIter *iter);

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

static void weld_assert_poly_and_loop_kill_len(Span<WeldPoly> wpoly,
                                               Span<WeldPoly> wpoly_new,
                                               Span<WeldLoop> wloop,
                                               Span<MLoop> mloop,
                                               Span<int> loop_map,
                                               Span<int> poly_map,
                                               Span<MPoly> mpoly,
                                               const int supposed_poly_kill_len,
                                               const int supposed_loop_kill_len)
{
  int poly_kills = 0;
  int loop_kills = mloop.size();
  const MPoly *mp = &mpoly[0];
  for (int i = 0; i < mpoly.size(); i++, mp++) {
    int poly_ctx = poly_map[i];
    if (poly_ctx != OUT_OF_CONTEXT) {
      const WeldPoly *wp = &wpoly[poly_ctx];
      WeldLoopOfPolyIter iter;
      if (!weld_iter_loop_of_poly_begin(&iter, *wp, wloop, mloop, loop_map, nullptr)) {
        poly_kills++;
        continue;
      }
      else {
        if (wp->poly_dst != OUT_OF_CONTEXT) {
          poly_kills++;
          continue;
        }
        int remain = wp->len;
        int l = wp->loop_start;
        while (remain) {
          int l_next = l + 1;
          int loop_ctx = loop_map[l];
          if (loop_ctx != OUT_OF_CONTEXT) {
            const WeldLoop *wl = &wloop[loop_ctx];
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
      loop_kills -= mp->totloop;
    }
  }

  const WeldPoly *wp = wpoly_new.data();
  for (int i = wpoly_new.size(); i--; wp++) {
    if (wp->poly_dst != OUT_OF_CONTEXT) {
      poly_kills++;
      continue;
    }
    int remain = wp->len;
    int l = wp->loop_start;
    while (remain) {
      int l_next = l + 1;
      int loop_ctx = loop_map[l];
      if (loop_ctx != OUT_OF_CONTEXT) {
        const WeldLoop *wl = &wloop[loop_ctx];
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
                                                Span<MLoop> mloop,
                                                Span<int> loop_map)
{
  const int len = wp.len;
  Array<int, 64> verts(len);
  WeldLoopOfPolyIter iter;
  if (!weld_iter_loop_of_poly_begin(&iter, wp, wloop, mloop, loop_map, nullptr)) {
    return;
  }
  else {
    int i = 0;
    while (weld_iter_loop_of_poly_next(iter)) {
      verts[i++] = iter.v;
    }
  }
  for (int i = 0; i < len; i++) {
    int va = verts[i];
    for (int j = i + 1; j < len; j++) {
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

  int len = wp->len;
  const WeldLoop *wl = &wloop[wp->loops.ofs];
  BLI_assert(wp->loop_start <= wl->loop_orig);

  int end_wloop = wp->loops.ofs + wp->loops.len;
  const WeldLoop *wl_end = &wloop[end_wloop - 1];

  int min_len = 0;
  for (; wl <= wl_end; wl++) {
    BLI_assert(wl->loop_skip_to == OUT_OF_CONTEXT); /* Not for this case. */
    if (wl->flag != ELEM_COLLAPSED) {
      min_len++;
    }
  }
  BLI_assert(len >= min_len);

  int max_len = wp->loop_end - wp->loop_start + 1;
  BLI_assert(len <= max_len);
}

#endif /* USE_WELD_DEBUG */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vert API
 * \{ */

static Vector<WeldVert> weld_vert_ctx_alloc_and_setup(Span<int> vert_dest_map,
                                                      const int vert_kill_len)
{
  Vector<WeldVert> wvert;
  wvert.reserve(std::min<int>(2 * vert_kill_len, vert_dest_map.size()));

  for (const int i : vert_dest_map.index_range()) {
    if (vert_dest_map[i] != OUT_OF_CONTEXT) {
      WeldVert wv{};
      wv.vert_dest = vert_dest_map[i];
      wv.vert_orig = i;
      wvert.append(wv);
    }
  }
  return wvert;
}

static void weld_vert_groups_setup(Span<WeldVert> wvert,
                                   Span<int> vert_dest_map,
                                   MutableSpan<int> r_vert_groups_map,
                                   Array<int> &r_vert_groups_buffer,
                                   Array<WeldGroup> &r_vert_groups)
{
  /* Get weld vert groups. */

  int wgroups_len = 0;
  for (const int i : vert_dest_map.index_range()) {
    const int vert_dest = vert_dest_map[i];
    if (vert_dest != OUT_OF_CONTEXT) {
      if (vert_dest != i) {
        r_vert_groups_map[i] = ELEM_MERGED;
      }
      else {
        r_vert_groups_map[i] = wgroups_len;
        wgroups_len++;
      }
    }
    else {
      r_vert_groups_map[i] = OUT_OF_CONTEXT;
    }
  }

  r_vert_groups.reinitialize(wgroups_len);
  r_vert_groups.fill({0, 0});
  MutableSpan<WeldGroup> wgroups = r_vert_groups;

  for (const WeldVert &wv : wvert) {
    int group_index = r_vert_groups_map[wv.vert_dest];
    wgroups[group_index].len++;
  }

  int ofs = 0;
  for (WeldGroup &wg : wgroups) {
    wg.ofs = ofs;
    ofs += wg.len;
  }

  BLI_assert(ofs == wvert.size());

  r_vert_groups_buffer.reinitialize(ofs);
  for (const WeldVert &wv : wvert) {
    int group_index = r_vert_groups_map[wv.vert_dest];
    r_vert_groups_buffer[wgroups[group_index].ofs++] = wv.vert_orig;
  }

  for (WeldGroup &wg : wgroups) {
    wg.ofs -= wg.len;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edge API
 * \{ */

static Vector<WeldEdge> weld_edge_ctx_alloc(Span<MEdge> medge,
                                            Span<int> vert_dest_map,
                                            MutableSpan<int> r_edge_dest_map,
                                            MutableSpan<int> r_edge_ctx_map)
{
  /* Edge Context. */
  int wedge_len = 0;

  Vector<WeldEdge> wedge;
  wedge.reserve(medge.size());

  for (const int i : medge.index_range()) {
    int v1 = medge[i].v1;
    int v2 = medge[i].v2;
    int v_dest_1 = vert_dest_map[v1];
    int v_dest_2 = vert_dest_map[v2];
    if ((v_dest_1 != OUT_OF_CONTEXT) || (v_dest_2 != OUT_OF_CONTEXT)) {
      WeldEdge we{};
      we.vert_a = (v_dest_1 != OUT_OF_CONTEXT) ? v_dest_1 : v1;
      we.vert_b = (v_dest_2 != OUT_OF_CONTEXT) ? v_dest_2 : v2;
      we.edge_dest = OUT_OF_CONTEXT;
      we.edge_orig = i;
      wedge.append(we);
      r_edge_dest_map[i] = i;
      r_edge_ctx_map[i] = wedge_len++;
    }
    else {
      r_edge_dest_map[i] = OUT_OF_CONTEXT;
      r_edge_ctx_map[i] = OUT_OF_CONTEXT;
    }
  }

  return wedge;
}

static void weld_edge_ctx_setup(MutableSpan<WeldGroup> r_vlinks,
                                MutableSpan<int> r_edge_dest_map,
                                MutableSpan<WeldEdge> r_wedge,
                                int *r_edge_kiil_len)
{
  /* Setup Edge Overlap. */
  int edge_kill_len = 0;

  MutableSpan<WeldGroup> v_links = r_vlinks;

  for (WeldEdge &we : r_wedge) {
    int dst_vert_a = we.vert_a;
    int dst_vert_b = we.vert_b;

    if (dst_vert_a == dst_vert_b) {
      BLI_assert(we.edge_dest == OUT_OF_CONTEXT);
      r_edge_dest_map[we.edge_orig] = ELEM_COLLAPSED;
      we.flag = ELEM_COLLAPSED;
      edge_kill_len++;
      continue;
    }

    v_links[dst_vert_a].len++;
    v_links[dst_vert_b].len++;
  }

  int link_len = 0;
  for (WeldGroup &vl : r_vlinks) {
    vl.ofs = link_len;
    link_len += vl.len;
  }

  if (link_len > 0) {
    Array<int> link_edge_buffer(link_len);

    for (const int i : r_wedge.index_range()) {
      const WeldEdge &we = r_wedge[i];
      if (we.flag == ELEM_COLLAPSED) {
        continue;
      }

      int dst_vert_a = we.vert_a;
      int dst_vert_b = we.vert_b;

      link_edge_buffer[v_links[dst_vert_a].ofs++] = i;
      link_edge_buffer[v_links[dst_vert_b].ofs++] = i;
    }

    for (WeldGroup &vl : r_vlinks) {
      /* Fix offset */
      vl.ofs -= vl.len;
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

      struct WeldGroup *link_a = &v_links[dst_vert_a];
      struct WeldGroup *link_b = &v_links[dst_vert_b];

      int edges_len_a = link_a->len;
      int edges_len_b = link_b->len;

      if (edges_len_a <= 1 || edges_len_b <= 1) {
        continue;
      }

      int *edges_ctx_a = &link_edge_buffer[link_a->ofs];
      int *edges_ctx_b = &link_edge_buffer[link_b->ofs];
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
          edge_kill_len++;
        }
      }
    }

#ifdef USE_WELD_DEBUG
    weld_assert_edge_kill_len(r_wedge, edge_kill_len);
#endif
  }

  *r_edge_kiil_len = edge_kill_len;
}

static void weld_edge_groups_setup(const int medge_len,
                                   const int edge_kill_len,
                                   MutableSpan<WeldEdge> wedge,
                                   Span<int> wedge_map,
                                   MutableSpan<int> r_edge_groups_map,
                                   Array<int> &r_edge_groups_buffer,
                                   Array<WeldGroupEdge> &r_edge_groups)
{
  /* Get weld edge groups. */
  int wgroups_len = wedge.size() - edge_kill_len;
  r_edge_groups.reinitialize(wgroups_len);
  r_edge_groups.fill({{0}});
  MutableSpan<WeldGroupEdge> wegroups = r_edge_groups;

  wgroups_len = 0;
  for (const int i : IndexRange(medge_len)) {
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
        wegroups[wgroups_len].v1 = we->vert_a;
        wegroups[wgroups_len].v2 = we->vert_b;
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

  for (const WeldEdge &we : wedge) {
    if (we.flag == ELEM_COLLAPSED) {
      continue;
    }
    int group_index = r_edge_groups_map[we.edge_dest];
    wegroups[group_index].group.len++;
  }

  int ofs = 0;
  for (WeldGroupEdge &wegrp : wegroups) {
    wegrp.group.ofs = ofs;
    ofs += wegrp.group.len;
  }

  r_edge_groups_buffer.reinitialize(ofs);
  for (const WeldEdge &we : wedge) {
    if (we.flag == ELEM_COLLAPSED) {
      continue;
    }
    int group_index = r_edge_groups_map[we.edge_dest];
    r_edge_groups_buffer[wegroups[group_index].group.ofs++] = we.edge_orig;
  }

  for (WeldGroupEdge &wegrp : wegroups) {
    wegrp.group.ofs -= wegrp.group.len;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Poly and Loop API
 * \{ */

static bool weld_iter_loop_of_poly_begin(WeldLoopOfPolyIter &iter,
                                         const WeldPoly &wp,
                                         Span<WeldLoop> wloop,
                                         Span<MLoop> mloop,
                                         Span<int> loop_map,
                                         int *group_buffer)
{
  if (wp.flag == ELEM_COLLAPSED) {
    return false;
  }

  iter.loop_start = wp.loop_start;
  iter.loop_end = wp.loop_end;
  iter.wloop = wloop;
  iter.mloop = mloop;
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
      const MLoop &ml = iter.mloop[l];
#ifdef USE_WELD_DEBUG
      BLI_assert((uint)iter.v != ml.v);
#endif
      iter.v = ml.v;
      iter.e = ml.e;
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

static void weld_poly_loop_ctx_alloc(Span<MPoly> mpoly,
                                     Span<MLoop> mloop,
                                     Span<int> vert_dest_map,
                                     Span<int> edge_dest_map,
                                     WeldMesh *r_weld_mesh)
{
  /* Loop/Poly Context. */
  Array<int> loop_map(mloop.size());
  Array<int> poly_map(mpoly.size());
  int wloop_len = 0;
  int wpoly_len = 0;
  int max_ctx_poly_len = 4;

  Vector<WeldLoop> wloop;
  wloop.reserve(mloop.size());

  Vector<WeldPoly> wpoly;
  wpoly.reserve(mpoly.size());

  int maybe_new_poly = 0;

  for (const int i : mpoly.index_range()) {
    const MPoly &mp = mpoly[i];
    const int loopstart = mp.loopstart;
    const int totloop = mp.totloop;

    int vert_ctx_len = 0;

    int prev_wloop_len = wloop_len;
    for (const int i_loop : mloop.index_range().slice(loopstart, totloop)) {
      int v = mloop[i_loop].v;
      int e = mloop[i_loop].e;
      int v_dest = vert_dest_map[v];
      int e_dest = edge_dest_map[e];
      bool is_vert_ctx = v_dest != OUT_OF_CONTEXT;
      bool is_edge_ctx = e_dest != OUT_OF_CONTEXT;
      if (is_vert_ctx) {
        vert_ctx_len++;
      }
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
      wp.loops.ofs = prev_wloop_len;
      wp.loop_start = loopstart;
      wp.loop_end = loopstart + totloop - 1;
      wp.len = totloop;
      wpoly.append(wp);

      poly_map[i] = wpoly_len++;
      if (totloop > 5 && vert_ctx_len > 1) {
        int max_new = (totloop / 3) - 1;
        vert_ctx_len /= 2;
        maybe_new_poly += MIN2(max_new, vert_ctx_len);
        CLAMP_MIN(max_ctx_poly_len, totloop);
      }
    }
    else {
      poly_map[i] = OUT_OF_CONTEXT;
    }
  }

  if (mpoly.size() < (wpoly_len + maybe_new_poly)) {
    wpoly.resize(wpoly_len + maybe_new_poly);
  }

  WeldPoly *poly_new = wpoly.data() + wpoly_len;

  r_weld_mesh->wloop = std::move(wloop);
  r_weld_mesh->wpoly = std::move(wpoly);
  r_weld_mesh->wpoly_new = poly_new;
  r_weld_mesh->wloop_len = wloop_len;
  r_weld_mesh->wpoly_len = wpoly_len;
  r_weld_mesh->wpoly_new_len = 0;
  r_weld_mesh->loop_map = std::move(loop_map);
  r_weld_mesh->poly_map = std::move(poly_map);
  r_weld_mesh->max_poly_len = max_ctx_poly_len;
}

static void weld_poly_split_recursive(Span<int> vert_dest_map,
#ifdef USE_WELD_DEBUG
                                      const Span<MLoop> mloop,
#endif
                                      int ctx_verts_len,
                                      WeldPoly *r_wp,
                                      WeldMesh *r_weld_mesh,
                                      int *r_poly_kill,
                                      int *r_loop_kill)
{
  int poly_len = r_wp->len;
  if (poly_len > 3 && ctx_verts_len > 1) {
    const int ctx_loops_len = r_wp->loops.len;
    const int ctx_loops_ofs = r_wp->loops.ofs;
    MutableSpan<WeldLoop> wloop = r_weld_mesh->wloop;
    WeldPoly *wpoly_new = r_weld_mesh->wpoly_new;

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
            const int dist_b = poly_len - dist_a;

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
                poly_len -= 2;
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
                poly_len -= 2;
              }
              if (wl_tmp == nullptr) {
                const int new_loops_len = lb - la;
                const int new_loops_ofs = ctx_loops_ofs + la;

                WeldPoly *new_wp = &wpoly_new[r_weld_mesh->wpoly_new_len++];
                new_wp->poly_dst = OUT_OF_CONTEXT;
                new_wp->poly_orig = r_wp->poly_orig;
                new_wp->loops.len = new_loops_len;
                new_wp->loops.ofs = new_loops_ofs;
                new_wp->loop_start = wla->loop_orig;
                new_wp->loop_end = wlb_prev->loop_orig;
                new_wp->len = dist_a;
                weld_poly_split_recursive(vert_dest_map,
#ifdef USE_WELD_DEBUG
                                          mloop,
#endif
                                          ctx_verts_len,
                                          new_wp,
                                          r_weld_mesh,
                                          r_poly_kill,
                                          r_loop_kill);
                BLI_assert(dist_b == poly_len - dist_a);
                poly_len = dist_b;
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
    r_wp->len = poly_len;
    *r_loop_kill += loop_kill;

#ifdef USE_WELD_DEBUG
    weld_assert_poly_no_vert_repetition(*r_wp, wloop, mloop, r_weld_mesh->loop_map);
#endif
  }
}

static void weld_poly_loop_ctx_setup(Span<MLoop> mloop,
#ifdef USE_WELD_DEBUG
                                     Span<MPoly> mpoly,
#endif
                                     const int mvert_len,
                                     Span<int> vert_dest_map,
                                     const int remain_edge_ctx_len,
                                     MutableSpan<WeldGroup> r_vlinks,
                                     WeldMesh *r_weld_mesh)
{
  MutableSpan<WeldPoly> wpoly = r_weld_mesh->wpoly;
  MutableSpan<WeldLoop> wloop = r_weld_mesh->wloop;
  WeldPoly *wpoly_new = r_weld_mesh->wpoly_new;
  int wpoly_len = r_weld_mesh->wpoly_len;
  int wpoly_new_len = 0;
  int poly_kill_len = 0;
  int loop_kill_len = 0;

  Span<int> loop_map = r_weld_mesh->loop_map;

  if (remain_edge_ctx_len) {

    /* Setup Poly/Loop. Note that `wpoly_len` may be different than `wpoly.size()` here. */
    for (const int i : IndexRange(wpoly_len)) {
      WeldPoly &wp = wpoly[i];
      const int ctx_loops_len = wp.loops.len;
      const int ctx_loops_ofs = wp.loops.ofs;

      int poly_len = wp.len;
      int ctx_verts_len = 0;
      WeldLoop *wl = &wloop[ctx_loops_ofs];
      for (int l = ctx_loops_len; l--; wl++) {
        const int edge_dest = wl->edge;
        if (edge_dest == ELEM_COLLAPSED) {
          wl->flag = ELEM_COLLAPSED;
          if (poly_len == 3) {
            wp.flag = ELEM_COLLAPSED;
            poly_kill_len++;
            loop_kill_len += 3;
            poly_len = 0;
            break;
          }
          loop_kill_len++;
          poly_len--;
        }
        else {
          const int vert_dst = wl->vert;
          if (vert_dest_map[vert_dst] != OUT_OF_CONTEXT) {
            ctx_verts_len++;
          }
        }
      }

      if (poly_len) {
        wp.len = poly_len;
#ifdef USE_WELD_DEBUG
        weld_assert_poly_len(wp, wloop);
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

        wpoly_new_len = r_weld_mesh->wpoly_new_len;
      }
    }

#ifdef USE_WELD_DEBUG
    weld_assert_poly_and_loop_kill_len(wpoly,
                                       {wpoly_new, wpoly_new_len},
                                       wloop,
                                       mloop,
                                       loop_map,
                                       r_weld_mesh->poly_map,
                                       mpoly,
                                       poly_kill_len,
                                       loop_kill_len);
#endif

    /* Setup Polygon Overlap. */

    const int wpoly_and_new_len = wpoly_len + wpoly_new_len;

    r_vlinks.fill({0, 0});
    MutableSpan<WeldGroup> v_links = r_vlinks;

    for (const int i : IndexRange(wpoly_and_new_len)) {
      const WeldPoly &wp = wpoly[i];
      WeldLoopOfPolyIter iter;
      if (weld_iter_loop_of_poly_begin(iter, wp, wloop, mloop, loop_map, nullptr)) {
        while (weld_iter_loop_of_poly_next(iter)) {
          v_links[iter.v].len++;
        }
      }
    }

    int link_len = 0;
    for (const int i : IndexRange(mvert_len)) {
      v_links[i].ofs = link_len;
      link_len += v_links[i].len;
    }

    if (link_len) {
      Array<int> link_poly_buffer(link_len);

      for (const int i : IndexRange(wpoly_and_new_len)) {
        const WeldPoly &wp = wpoly[i];
        WeldLoopOfPolyIter iter;
        if (weld_iter_loop_of_poly_begin(iter, wp, wloop, mloop, loop_map, nullptr)) {
          while (weld_iter_loop_of_poly_next(iter)) {
            link_poly_buffer[v_links[iter.v].ofs++] = i;
          }
        }
      }

      for (WeldGroup &vl : r_vlinks) {
        /* Fix offset */
        vl.ofs -= vl.len;
      }

      int polys_len_a, polys_len_b, *polys_ctx_a, *polys_ctx_b, p_ctx_a, p_ctx_b;
      polys_len_b = p_ctx_b = 0; /* silence warnings */

      for (const int i : IndexRange(wpoly_and_new_len)) {
        const WeldPoly &wp = wpoly[i];
        if (wp.poly_dst != OUT_OF_CONTEXT) {
          /* No need to retest poly.
           * (Already includes collapsed polygons). */
          continue;
        }

        WeldLoopOfPolyIter iter;
        weld_iter_loop_of_poly_begin(iter, wp, wloop, mloop, loop_map, nullptr);
        weld_iter_loop_of_poly_next(iter);
        struct WeldGroup *link_a = &v_links[iter.v];
        polys_len_a = link_a->len;
        if (polys_len_a == 1) {
          BLI_assert(link_poly_buffer[link_a->ofs] == i);
          continue;
        }
        int wp_len = wp.len;
        polys_ctx_a = &link_poly_buffer[link_a->ofs];
        for (; polys_len_a--; polys_ctx_a++) {
          p_ctx_a = *polys_ctx_a;
          if (p_ctx_a == i) {
            continue;
          }

          WeldPoly *wp_tmp = &wpoly[p_ctx_a];
          if (wp_tmp->len != wp_len) {
            continue;
          }

          WeldLoopOfPolyIter iter_b = iter;
          while (weld_iter_loop_of_poly_next(iter_b)) {
            struct WeldGroup *link_b = &v_links[iter_b.v];
            polys_len_b = link_b->len;
            if (polys_len_b == 1) {
              BLI_assert(link_poly_buffer[link_b->ofs] == i);
              polys_len_b = 0;
              break;
            }

            polys_ctx_b = &link_poly_buffer[link_b->ofs];
            for (; polys_len_b; polys_len_b--, polys_ctx_b++) {
              p_ctx_b = *polys_ctx_b;
              if (p_ctx_b < p_ctx_a) {
                continue;
              }
              if (p_ctx_b >= p_ctx_a) {
                if (p_ctx_b > p_ctx_a) {
                  polys_len_b = 0;
                }
                break;
              }
            }
            if (polys_len_b == 0) {
              break;
            }
          }
          if (polys_len_b == 0) {
            continue;
          }
          BLI_assert(p_ctx_a > i);
          BLI_assert(p_ctx_a == p_ctx_b);
          BLI_assert(wp_tmp->poly_dst == OUT_OF_CONTEXT);
          BLI_assert(wp_tmp != &wp);
          wp_tmp->poly_dst = wp.poly_orig;
          loop_kill_len += wp_tmp->len;
          poly_kill_len++;
        }
      }
    }
  }
  else {
    poly_kill_len = r_weld_mesh->wpoly_len;
    loop_kill_len = r_weld_mesh->wloop_len;

    for (WeldPoly &wp : wpoly) {
      wp.flag = ELEM_COLLAPSED;
    }
  }

#ifdef USE_WELD_DEBUG
  weld_assert_poly_and_loop_kill_len(wpoly,
                                     {wpoly_new, wpoly_new_len},
                                     wloop,
                                     mloop,
                                     loop_map,
                                     r_weld_mesh->poly_map,
                                     mpoly,
                                     poly_kill_len,
                                     loop_kill_len);
#endif

  r_weld_mesh->wpoly_new = wpoly_new;
  r_weld_mesh->poly_kill_len = poly_kill_len;
  r_weld_mesh->loop_kill_len = loop_kill_len;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh API
 * \{ */

static void weld_mesh_context_create(const Mesh &mesh,
                                     MutableSpan<int> vert_dest_map,
                                     const int vert_kill_len,
                                     WeldMesh *r_weld_mesh)
{
  Span<MEdge> medge{mesh.medge, mesh.totedge};
  Span<MPoly> mpoly{mesh.mpoly, mesh.totpoly};
  Span<MLoop> mloop{mesh.mloop, mesh.totloop};
  const int mvert_len = mesh.totvert;

  Vector<WeldVert> wvert = weld_vert_ctx_alloc_and_setup(vert_dest_map, vert_kill_len);
  r_weld_mesh->vert_kill_len = vert_kill_len;

  Array<int> edge_dest_map(medge.size());
  Array<int> edge_ctx_map(medge.size());
  Vector<WeldEdge> wedge = weld_edge_ctx_alloc(medge, vert_dest_map, edge_dest_map, edge_ctx_map);

  Array<WeldGroup> v_links(mvert_len, {0, 0});
  weld_edge_ctx_setup(v_links, edge_dest_map, wedge, &r_weld_mesh->edge_kill_len);

  weld_poly_loop_ctx_alloc(mpoly, mloop, vert_dest_map, edge_dest_map, r_weld_mesh);

  weld_poly_loop_ctx_setup(mloop,
#ifdef USE_WELD_DEBUG
                           mpoly,

#endif
                           mvert_len,
                           vert_dest_map,
                           wedge.size() - r_weld_mesh->edge_kill_len,
                           v_links,
                           r_weld_mesh);

  weld_vert_groups_setup(wvert,
                         vert_dest_map,
                         vert_dest_map,
                         r_weld_mesh->vert_groups_buffer,
                         r_weld_mesh->vert_groups);

  weld_edge_groups_setup(medge.size(),
                         r_weld_mesh->edge_kill_len,
                         wedge,
                         edge_ctx_map,
                         edge_dest_map,
                         r_weld_mesh->edge_groups_buffer,
                         r_weld_mesh->edge_groups);

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

  float co[3] = {0.0f, 0.0f, 0.0f};
#ifdef USE_WELD_NORMALS
  float no[3] = {0.0f, 0.0f, 0.0f};
#endif
  int crease = 0;
  int bweight = 0;
  short flag = 0;

  /* interpolates a layer at a time */
  dest_i = 0;
  for (src_i = 0; src_i < source->totlayer; src_i++) {
    const int type = source->layers[src_i].type;

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

      if (type == CD_MVERT) {
        for (j = 0; j < count; j++) {
          MVert *mv_src = &((MVert *)src_data)[src_indices[j]];
          add_v3_v3(co, mv_src->co);
#ifdef USE_WELD_NORMALS
          short *mv_src_no = mv_src->no;
          no[0] += mv_src_no[0];
          no[1] += mv_src_no[1];
          no[2] += mv_src_no[2];
#endif
          bweight += mv_src->bweight;
          flag |= mv_src->flag;
        }
      }
      else if (type == CD_MEDGE) {
        for (j = 0; j < count; j++) {
          MEdge *me_src = &((MEdge *)src_data)[src_indices[j]];
          crease += me_src->crease;
          bweight += me_src->bweight;
          flag |= me_src->flag;
        }
      }
      else if (CustomData_layer_has_interp(dest, dest_i)) {
        /* Already calculated.
         * TODO: Optimize by exposing `typeInfo->interp`. */
      }
      else if (CustomData_layer_has_math(dest, dest_i)) {
        const int size = CustomData_sizeof(type);
        void *dst_data = dest->layers[dest_i].data;
        void *v_dst = POINTER_OFFSET(dst_data, (size_t)dest_index * size);
        for (j = 0; j < count; j++) {
          CustomData_data_add(
              type, v_dst, POINTER_OFFSET(src_data, (size_t)src_indices[j] * size));
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
    const int type = layer_dst->type;
    if (type == CD_MVERT) {
      MVert *mv = &((MVert *)layer_dst->data)[dest_index];
      mul_v3_fl(co, fac);
      bweight *= fac;
      CLAMP_MAX(bweight, 255);

      copy_v3_v3(mv->co, co);
#ifdef USE_WELD_NORMALS
      mul_v3_fl(no, fac);
      short *mv_no = mv->no;
      mv_no[0] = (short)no[0];
      mv_no[1] = (short)no[1];
      mv_no[2] = (short)no[2];
#endif

      mv->flag = (char)flag;
      mv->bweight = (char)bweight;
    }
    else if (type == CD_MEDGE) {
      MEdge *me = &((MEdge *)layer_dst->data)[dest_index];
      crease *= fac;
      bweight *= fac;
      CLAMP_MAX(crease, 255);
      CLAMP_MAX(bweight, 255);

      me->crease = (char)crease;
      me->bweight = (char)bweight;
      me->flag = flag;
    }
    else if (CustomData_layer_has_interp(dest, dest_i)) {
      /* Already calculated. */
    }
    else if (CustomData_layer_has_math(dest, dest_i)) {
      const int size = CustomData_sizeof(type);
      void *dst_data = layer_dst->data;
      void *v_dst = POINTER_OFFSET(dst_data, (size_t)dest_index * size);
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
  Span<MPoly> mpoly{mesh.mpoly, mesh.totpoly};
  Span<MLoop> mloop{mesh.mloop, mesh.totloop};
  const int totvert = mesh.totvert;
  const int totedge = mesh.totedge;
  const int totloop = mesh.totloop;
  const int totpoly = mesh.totpoly;

  WeldMesh weld_mesh;
  weld_mesh_context_create(mesh, vert_dest_map, removed_vertex_count, &weld_mesh);

  const int result_nverts = totvert - weld_mesh.vert_kill_len;
  const int result_nedges = totedge - weld_mesh.edge_kill_len;
  const int result_nloops = totloop - weld_mesh.loop_kill_len;
  const int result_npolys = totpoly - weld_mesh.poly_kill_len + weld_mesh.wpoly_new_len;

  Mesh *result = BKE_mesh_new_nomain_from_template(
      &mesh, result_nverts, result_nedges, 0, result_nloops, result_npolys);

  /* Vertices. */

  /* Be careful when editing this array, to avoid new allocations it uses the same buffer as
   * #vert_dest_map. This map will be used to adjust the edges, polys and loops. */
  MutableSpan<int> vert_final = vert_dest_map;

  int dest_index = 0;
  for (int i = 0; i < totvert; i++) {
    int source_index = i;
    int count = 0;
    while (i < totvert && vert_dest_map[i] == OUT_OF_CONTEXT) {
      vert_final[i] = dest_index + count;
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
    if (vert_dest_map[i] != ELEM_MERGED) {
      struct WeldGroup *wgroup = &weld_mesh.vert_groups[vert_dest_map[i]];
      customdata_weld(&mesh.vdata,
                      &result->vdata,
                      &weld_mesh.vert_groups_buffer[wgroup->ofs],
                      wgroup->len,
                      dest_index);
      vert_final[i] = dest_index;
      dest_index++;
    }
  }

  BLI_assert(dest_index == result_nverts);

  /* Edges. */

  /* Be careful when editing this array, to avoid new allocations it uses the same buffer as
   * #edge_groups_map. This map will be used to adjust the polys and loops. */
  MutableSpan<int> edge_final = weld_mesh.edge_groups_map;

  dest_index = 0;
  for (int i = 0; i < totedge; i++) {
    const int source_index = i;
    int count = 0;
    while (i < totedge && weld_mesh.edge_groups_map[i] == OUT_OF_CONTEXT) {
      edge_final[i] = dest_index + count;
      count++;
      i++;
    }
    if (count) {
      CustomData_copy_data(&mesh.edata, &result->edata, source_index, dest_index, count);
      MEdge *me = &result->medge[dest_index];
      dest_index += count;
      for (; count--; me++) {
        me->v1 = vert_final[me->v1];
        me->v2 = vert_final[me->v2];
      }
    }
    if (i == totedge) {
      break;
    }
    if (weld_mesh.edge_groups_map[i] != ELEM_MERGED) {
      struct WeldGroupEdge *wegrp = &weld_mesh.edge_groups[weld_mesh.edge_groups_map[i]];
      customdata_weld(&mesh.edata,
                      &result->edata,
                      &weld_mesh.edge_groups_buffer[wegrp->group.ofs],
                      wegrp->group.len,
                      dest_index);
      MEdge *me = &result->medge[dest_index];
      me->v1 = vert_final[wegrp->v1];
      me->v2 = vert_final[wegrp->v2];
      /* "For now, assume that all merged edges are loose. This flag will be cleared in the
       * Polys/Loops step". */
      me->flag |= ME_LOOSEEDGE;

      edge_final[i] = dest_index;
      dest_index++;
    }
  }

  BLI_assert(dest_index == result_nedges);

  /* Polys/Loops. */

  MPoly *r_mp = &result->mpoly[0];
  MLoop *r_ml = &result->mloop[0];
  int r_i = 0;
  int loop_cur = 0;
  Array<int, 64> group_buffer(weld_mesh.max_poly_len);
  for (const int i : mpoly.index_range()) {
    const MPoly &mp = mpoly[i];
    const int loop_start = loop_cur;
    const int poly_ctx = weld_mesh.poly_map[i];
    if (poly_ctx == OUT_OF_CONTEXT) {
      int mp_loop_len = mp.totloop;
      CustomData_copy_data(&mesh.ldata, &result->ldata, mp.loopstart, loop_cur, mp_loop_len);
      loop_cur += mp_loop_len;
      for (; mp_loop_len--; r_ml++) {
        r_ml->v = vert_final[r_ml->v];
        r_ml->e = edge_final[r_ml->e];
      }
    }
    else {
      const WeldPoly &wp = weld_mesh.wpoly[poly_ctx];
      WeldLoopOfPolyIter iter;
      if (!weld_iter_loop_of_poly_begin(
              iter, wp, weld_mesh.wloop, mloop, weld_mesh.loop_map, group_buffer.data())) {
        continue;
      }

      if (wp.poly_dst != OUT_OF_CONTEXT) {
        continue;
      }
      while (weld_iter_loop_of_poly_next(iter)) {
        customdata_weld(
            &mesh.ldata, &result->ldata, group_buffer.data(), iter.group_len, loop_cur);
        int v = vert_final[iter.v];
        int e = edge_final[iter.e];
        r_ml->v = v;
        r_ml->e = e;
        r_ml++;
        loop_cur++;
        if (iter.type) {
          result->medge[e].flag &= ~ME_LOOSEEDGE;
        }
        BLI_assert((result->medge[e].flag & ME_LOOSEEDGE) == 0);
      }
    }

    CustomData_copy_data(&mesh.pdata, &result->pdata, i, r_i, 1);
    r_mp->loopstart = loop_start;
    r_mp->totloop = loop_cur - loop_start;
    r_mp++;
    r_i++;
  }

  for (const int i : IndexRange(weld_mesh.wpoly_new_len)) {
    const WeldPoly &wp = weld_mesh.wpoly_new[i];
    const int loop_start = loop_cur;
    WeldLoopOfPolyIter iter;
    if (!weld_iter_loop_of_poly_begin(
            iter, wp, weld_mesh.wloop, mloop, weld_mesh.loop_map, group_buffer.data())) {
      continue;
    }

    if (wp.poly_dst != OUT_OF_CONTEXT) {
      continue;
    }
    while (weld_iter_loop_of_poly_next(iter)) {
      customdata_weld(&mesh.ldata, &result->ldata, group_buffer.data(), iter.group_len, loop_cur);
      int v = vert_final[iter.v];
      int e = edge_final[iter.e];
      r_ml->v = v;
      r_ml->e = e;
      r_ml++;
      loop_cur++;
      if (iter.type) {
        result->medge[e].flag &= ~ME_LOOSEEDGE;
      }
      BLI_assert((result->medge[e].flag & ME_LOOSEEDGE) == 0);
    }

    r_mp->loopstart = loop_start;
    r_mp->totloop = loop_cur - loop_start;
    r_mp++;
    r_i++;
  }

  BLI_assert((int)r_i == result_npolys);
  BLI_assert(loop_cur == result_nloops);

  /* We could only update the normals of the elements in context, but the next modifier can make it
   * dirty anyway which would make the work useless. */
  BKE_mesh_normals_tag_dirty(result);

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

  for (const int i : selection) {
    BLI_kdtree_3d_insert(tree, i, mesh.mvert[i].co);
  }

  BLI_kdtree_3d_balance(tree);
  const int vert_kill_len = BLI_kdtree_3d_calc_duplicates_fast(
      tree, merge_distance, false, vert_dest_map.data());
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
  Span<MVert> verts{mesh.mvert, mesh.totvert};
  Span<MEdge> edges{mesh.medge, mesh.totedge};

  int vert_kill_len = 0;

  /* From the original index of the vertex.
   * This indicates which vert it is or is going to be merged. */
  Array<int> vert_dest_map(mesh.totvert, OUT_OF_CONTEXT);

  Array<WeldVertexCluster> vert_clusters(mesh.totvert);

  for (const int i : verts.index_range()) {
    WeldVertexCluster &vc = vert_clusters[i];
    copy_v3_v3(vc.co, verts[i].co);
    vc.merged_verts = 0;
  }
  const float merge_dist_sq = square_f(merge_distance);

  range_vn_i(vert_dest_map.data(), mesh.totvert, 0);

  /* Collapse Edges that are shorter than the threshold. */
  for (const int i : edges.index_range()) {
    int v1 = edges[i].v1;
    int v2 = edges[i].v2;

    if (only_loose_edges && (edges[i].flag & ME_LOOSEEDGE) == 0) {
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
      SWAP(int, v1, v2);
    }
    WeldVertexCluster *v1_cluster = &vert_clusters[v1];
    WeldVertexCluster *v2_cluster = &vert_clusters[v2];

    float edgedir[3];
    sub_v3_v3v3(edgedir, v2_cluster->co, v1_cluster->co);
    const float dist_sq = len_squared_v3(edgedir);
    if (dist_sq <= merge_dist_sq) {
      float influence = (v2_cluster->merged_verts + 1) /
                        (float)(v1_cluster->merged_verts + v2_cluster->merged_verts + 2);
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

/** \} */

}  // namespace blender::geometry
