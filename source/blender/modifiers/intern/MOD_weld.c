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
 *
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup modifiers
 *
 * Weld modifier: Remove doubles.
 */

/* TODOs:
 * - Review weight and vertex color interpolation.;
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_alloca.h"
#include "BLI_kdopbvh.h"
#include "BLI_math.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_bvhutils.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "DEG_depsgraph.h"

#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h"

//#define USE_WELD_DEBUG
//#define USE_WELD_NORMALS

/* Indicates when the element was not computed. */
#define OUT_OF_CONTEXT (uint)(-1)
/* Indicates if the edge or face will be collapsed. */
#define ELEM_COLLAPSED (uint)(-2)
/* indicates whether an edge or vertex in groups_map will be merged. */
#define ELEM_MERGED (uint)(-2)

/* Used to indicate a range in an array specifying a group. */
struct WeldGroup {
  uint len;
  uint ofs;
};

/* Edge groups that will be merged. Final vertices are also indicated. */
struct WeldGroupEdge {
  struct WeldGroup group;
  uint v1;
  uint v2;
};

typedef struct WeldVert {
  /* Indexes relative to the original Mesh. */
  uint vert_dest;
  uint vert_orig;
} WeldVert;

typedef struct WeldEdge {
  union {
    uint flag;
    struct {
      /* Indexes relative to the original Mesh. */
      uint edge_dest;
      uint edge_orig;
      uint vert_a;
      uint vert_b;
    };
  };
} WeldEdge;

typedef struct WeldLoop {
  union {
    uint flag;
    struct {
      /* Indexes relative to the original Mesh. */
      uint vert;
      uint edge;
      uint loop_orig;
      uint loop_skip_to;
    };
  };
} WeldLoop;

typedef struct WeldPoly {
  union {
    uint flag;
    struct {
      /* Indexes relative to the original Mesh. */
      uint poly_dst;
      uint poly_orig;
      uint loop_start;
      uint loop_end;
      /* Final Polygon Size. */
      uint len;
      /* Group of loops that will be affected. */
      struct WeldGroup loops;
    };
  };
} WeldPoly;

typedef struct WeldMesh {
  /* Group of vertices to be merged. */
  struct WeldGroup *vert_groups;
  uint *vert_groups_buffer;
  /* From the original index of the vertex, this indicates which group it is or is going to be
   * merged. */
  uint *vert_groups_map;

  /* Group of edges to be merged. */
  struct WeldGroupEdge *edge_groups;
  uint *edge_groups_buffer;
  /* From the original index of the vertex, this indicates which group it is or is going to be
   * merged. */
  uint *edge_groups_map;

  /* References all polygons and loops that will be affected. */
  WeldLoop *wloop;
  WeldPoly *wpoly;
  WeldPoly *wpoly_new;
  uint wloop_len;
  uint wpoly_len;
  uint wpoly_new_len;

  /* From the actual index of the element in the mesh, it indicates what is the index of the Weld
   * element above. */
  uint *loop_map;
  uint *poly_map;

  uint vert_kill_len;
  uint edge_kill_len;
  uint loop_kill_len;
  uint poly_kill_len; /* Including the new polygons. */

  /* Size of the affected polygon with more sides. */
  uint max_poly_len;
} WeldMesh;

typedef struct WeldLoopOfPolyIter {
  uint loop_start;
  uint loop_end;
  const WeldLoop *wloop;
  const MLoop *mloop;
  const uint *loop_map;
  /* Weld group. */
  uint *group;

  uint l_curr;
  uint l_next;

  /* Return */
  uint group_len;
  uint v;
  uint e;
  char type;
} WeldLoopOfPolyIter;

/* -------------------------------------------------------------------- */
/** \name Debug Utils
 * \{ */

#ifdef USE_WELD_DEBUG
static bool weld_iter_loop_of_poly_begin(WeldLoopOfPolyIter *iter,
                                         const WeldPoly *wp,
                                         const WeldLoop *wloop,
                                         const MLoop *mloop,
                                         const uint *loop_map,
                                         uint *group_buffer);

static bool weld_iter_loop_of_poly_next(WeldLoopOfPolyIter *iter);

static void weld_assert_vert_dest_map_setup(const BVHTreeOverlap *overlap,
                                            const uint overlap_len,
                                            const uint *vert_dest_map)
{
  const BVHTreeOverlap *overlap_iter = &overlap[0];
  for (uint i = overlap_len; i--; overlap_iter++) {
    uint indexA = overlap_iter->indexA;
    uint indexB = overlap_iter->indexB;
    uint va_dst = vert_dest_map[indexA];
    uint vb_dst = vert_dest_map[indexB];

    BLI_assert(va_dst == vb_dst);
  }
}

static void weld_assert_edge_kill_len(const WeldEdge *wedge,
                                      const uint wedge_len,
                                      const uint supposed_kill_len)
{
  uint kills = 0;
  const WeldEdge *we = &wedge[0];
  for (uint i = wedge_len; i--; we++) {
    uint edge_dest = we->edge_dest;
    /* Magically includes collapsed edges. */
    if (edge_dest != OUT_OF_CONTEXT) {
      kills++;
    }
  }
  BLI_assert(kills == supposed_kill_len);
}

static void weld_assert_poly_and_loop_kill_len(const WeldPoly *wpoly,
                                               const WeldPoly *wpoly_new,
                                               const uint wpoly_new_len,
                                               const WeldLoop *wloop,
                                               const MLoop *mloop,
                                               const uint *loop_map,
                                               const uint *poly_map,
                                               const MPoly *mpoly,
                                               const uint mpoly_len,
                                               const uint mloop_len,
                                               const uint supposed_poly_kill_len,
                                               const uint supposed_loop_kill_len)
{
  uint poly_kills = 0;
  uint loop_kills = mloop_len;
  const MPoly *mp = &mpoly[0];
  for (uint i = 0; i < mpoly_len; i++, mp++) {
    uint poly_ctx = poly_map[i];
    if (poly_ctx != OUT_OF_CONTEXT) {
      const WeldPoly *wp = &wpoly[poly_ctx];
      WeldLoopOfPolyIter iter;
      if (!weld_iter_loop_of_poly_begin(&iter, wp, wloop, mloop, loop_map, NULL)) {
        poly_kills++;
        continue;
      }
      else {
        if (wp->poly_dst != OUT_OF_CONTEXT) {
          poly_kills++;
          continue;
        }
        uint remain = wp->len;
        uint l = wp->loop_start;
        while (remain) {
          uint l_next = l + 1;
          uint loop_ctx = loop_map[l];
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

  const WeldPoly *wp = &wpoly_new[0];
  for (uint i = wpoly_new_len; i--; wp++) {
    if (wp->poly_dst != OUT_OF_CONTEXT) {
      poly_kills++;
      continue;
    }
    uint remain = wp->len;
    uint l = wp->loop_start;
    while (remain) {
      uint l_next = l + 1;
      uint loop_ctx = loop_map[l];
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

static void weld_assert_poly_no_vert_repetition(const WeldPoly *wp,
                                                const WeldLoop *wloop,
                                                const MLoop *mloop,
                                                const uint *loop_map)
{
  const uint len = wp->len;
  uint *verts = BLI_array_alloca(verts, len);
  WeldLoopOfPolyIter iter;
  if (!weld_iter_loop_of_poly_begin(&iter, wp, wloop, mloop, loop_map, NULL)) {
    return;
  }
  else {
    uint i = 0;
    while (weld_iter_loop_of_poly_next(&iter)) {
      verts[i++] = iter.v;
    }
  }
  for (uint i = 0; i < len; i++) {
    uint va = verts[i];
    for (uint j = i + 1; j < len; j++) {
      uint vb = verts[j];
      BLI_assert(va != vb);
    }
  }
}

static void weld_assert_poly_len(const WeldPoly *wp, const WeldLoop *wloop)
{
  if (wp->flag == ELEM_COLLAPSED) {
    return;
  }

  uint len = wp->len;
  const WeldLoop *wl = &wloop[wp->loops.ofs];
  BLI_assert(wp->loop_start <= wl->loop_orig);

  uint end_wloop = wp->loops.ofs + wp->loops.len;
  const WeldLoop *wl_end = &wloop[end_wloop - 1];

  uint min_len = 0;
  for (; wl <= wl_end; wl++) {
    BLI_assert(wl->loop_skip_to == OUT_OF_CONTEXT); /* Not for this case. */
    if (wl->flag != ELEM_COLLAPSED) {
      min_len++;
    }
  }
  BLI_assert(len >= min_len);

  uint max_len = wp->loop_end - wp->loop_start + 1;
  BLI_assert(len <= max_len);
}
#endif
/** \} */

/* -------------------------------------------------------------------- */
/** \name Weld Vert API
 * \{ */

static void weld_vert_ctx_alloc_and_setup(const uint mvert_len,
                                          const BVHTreeOverlap *overlap,
                                          const uint overlap_len,
                                          uint *r_vert_dest_map,
                                          WeldVert **r_wvert,
                                          uint *r_wvert_len,
                                          uint *r_vert_kill_len)
{
  uint *v_dest_iter = &r_vert_dest_map[0];
  for (uint i = mvert_len; i--; v_dest_iter++) {
    *v_dest_iter = OUT_OF_CONTEXT;
  }

  uint vert_kill_len = 0;
  const BVHTreeOverlap *overlap_iter = &overlap[0];
  for (uint i = 0; i < overlap_len; i++, overlap_iter++) {
    uint indexA = overlap_iter->indexA;
    uint indexB = overlap_iter->indexB;

    BLI_assert(indexA < indexB);

    uint va_dst = r_vert_dest_map[indexA];
    uint vb_dst = r_vert_dest_map[indexB];
    if (va_dst == OUT_OF_CONTEXT) {
      if (vb_dst == OUT_OF_CONTEXT) {
        vb_dst = indexA;
        r_vert_dest_map[indexB] = vb_dst;
      }
      r_vert_dest_map[indexA] = vb_dst;
      vert_kill_len++;
    }
    else if (vb_dst == OUT_OF_CONTEXT) {
      r_vert_dest_map[indexB] = va_dst;
      vert_kill_len++;
    }
    else if (va_dst != vb_dst) {
      uint v_new, v_old;
      if (va_dst < vb_dst) {
        v_new = va_dst;
        v_old = vb_dst;
      }
      else {
        v_new = vb_dst;
        v_old = va_dst;
      }
      BLI_assert(r_vert_dest_map[v_old] == v_old);
      BLI_assert(r_vert_dest_map[v_new] == v_new);
      vert_kill_len++;

      const BVHTreeOverlap *overlap_iter_b = &overlap[0];
      for (uint j = i + 1; j--; overlap_iter_b++) {
        indexA = overlap_iter_b->indexA;
        indexB = overlap_iter_b->indexB;
        va_dst = r_vert_dest_map[indexA];
        vb_dst = r_vert_dest_map[indexB];
        if (ELEM(v_old, vb_dst, va_dst)) {
          r_vert_dest_map[indexA] = v_new;
          r_vert_dest_map[indexB] = v_new;
        }
      }
      BLI_assert(r_vert_dest_map[v_old] == v_new);
    }
  }

  /* Vert Context. */
  uint wvert_len = 0;

  WeldVert *wvert, *wv;
  wvert = MEM_mallocN(sizeof(*wvert) * mvert_len, __func__);
  wv = &wvert[0];

  v_dest_iter = &r_vert_dest_map[0];
  for (uint i = 0; i < mvert_len; i++, v_dest_iter++) {
    if (*v_dest_iter != OUT_OF_CONTEXT) {
      wv->vert_dest = *v_dest_iter;
      wv->vert_orig = i;
      wv++;
      wvert_len++;
    }
  }

#ifdef USE_WELD_DEBUG
  weld_assert_vert_dest_map_setup(overlap, overlap_len, r_vert_dest_map);
#endif

  *r_wvert = MEM_reallocN(wvert, sizeof(*wvert) * wvert_len);
  *r_wvert_len = wvert_len;
  *r_vert_kill_len = vert_kill_len;
}

static void weld_vert_groups_setup(const uint mvert_len,
                                   const uint wvert_len,
                                   const WeldVert *wvert,
                                   const uint *vert_dest_map,
                                   uint *r_vert_groups_map,
                                   uint **r_vert_groups_buffer,
                                   struct WeldGroup **r_vert_groups)
{
  /* Get weld vert groups. */

  uint wgroups_len = 0;
  const uint *vert_dest_iter = &vert_dest_map[0];
  uint *group_map_iter = &r_vert_groups_map[0];
  for (uint i = 0; i < mvert_len; i++, group_map_iter++, vert_dest_iter++) {
    uint vert_dest = *vert_dest_iter;
    if (vert_dest != OUT_OF_CONTEXT) {
      if (vert_dest != i) {
        *group_map_iter = ELEM_MERGED;
      }
      else {
        *group_map_iter = wgroups_len;
        wgroups_len++;
      }
    }
    else {
      *group_map_iter = OUT_OF_CONTEXT;
    }
  }

  struct WeldGroup *wgroups = MEM_callocN(sizeof(*wgroups) * wgroups_len, __func__);

  const WeldVert *wv = &wvert[0];
  for (uint i = wvert_len; i--; wv++) {
    uint group_index = r_vert_groups_map[wv->vert_dest];
    wgroups[group_index].len++;
  }

  uint ofs = 0;
  struct WeldGroup *wg_iter = &wgroups[0];
  for (uint i = wgroups_len; i--; wg_iter++) {
    wg_iter->ofs = ofs;
    ofs += wg_iter->len;
  }

  BLI_assert(ofs == wvert_len);

  uint *groups_buffer = MEM_mallocN(sizeof(*groups_buffer) * ofs, __func__);
  wv = &wvert[0];
  for (uint i = wvert_len; i--; wv++) {
    uint group_index = r_vert_groups_map[wv->vert_dest];
    groups_buffer[wgroups[group_index].ofs++] = wv->vert_orig;
  }

  wg_iter = &wgroups[0];
  for (uint i = wgroups_len; i--; wg_iter++) {
    wg_iter->ofs -= wg_iter->len;
  }

  *r_vert_groups = wgroups;
  *r_vert_groups_buffer = groups_buffer;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Weld Edge API
 * \{ */

static void weld_edge_ctx_setup(const uint mvert_len,
                                const uint wedge_len,
                                struct WeldGroup *r_vlinks,
                                uint *r_edge_dest_map,
                                WeldEdge *r_wedge,
                                uint *r_edge_kiil_len)
{
  WeldEdge *we;

  /* Setup Edge Overlap. */
  uint edge_kill_len = 0;

  struct WeldGroup *vl_iter, *v_links;
  v_links = r_vlinks;
  vl_iter = &v_links[0];

  we = &r_wedge[0];
  for (uint i = wedge_len; i--; we++) {
    uint dst_vert_a = we->vert_a;
    uint dst_vert_b = we->vert_b;

    if (dst_vert_a == dst_vert_b) {
      BLI_assert(we->edge_dest == OUT_OF_CONTEXT);
      r_edge_dest_map[we->edge_orig] = ELEM_COLLAPSED;
      we->flag = ELEM_COLLAPSED;
      edge_kill_len++;
      continue;
    }

    v_links[dst_vert_a].len++;
    v_links[dst_vert_b].len++;
  }

  uint link_len = 0;
  vl_iter = &v_links[0];
  for (uint i = mvert_len; i--; vl_iter++) {
    vl_iter->ofs = link_len;
    link_len += vl_iter->len;
  }

  if (link_len) {
    uint *link_edge_buffer = MEM_mallocN(sizeof(*link_edge_buffer) * link_len, __func__);

    we = &r_wedge[0];
    for (uint i = 0; i < wedge_len; i++, we++) {
      if (we->flag == ELEM_COLLAPSED) {
        continue;
      }

      uint dst_vert_a = we->vert_a;
      uint dst_vert_b = we->vert_b;

      link_edge_buffer[v_links[dst_vert_a].ofs++] = i;
      link_edge_buffer[v_links[dst_vert_b].ofs++] = i;
    }

    vl_iter = &v_links[0];
    for (uint i = mvert_len; i--; vl_iter++) {
      /* Fix offset */
      vl_iter->ofs -= vl_iter->len;
    }

    we = &r_wedge[0];
    for (uint i = 0; i < wedge_len; i++, we++) {
      if (we->edge_dest != OUT_OF_CONTEXT) {
        /* No need to retest edges.
         * (Already includes collapsed edges). */
        continue;
      }

      uint dst_vert_a = we->vert_a;
      uint dst_vert_b = we->vert_b;

      struct WeldGroup *link_a = &v_links[dst_vert_a];
      struct WeldGroup *link_b = &v_links[dst_vert_b];

      uint edges_len_a = link_a->len;
      uint edges_len_b = link_b->len;

      if (edges_len_a <= 1 || edges_len_b <= 1) {
        continue;
      }

      uint *edges_ctx_a = &link_edge_buffer[link_a->ofs];
      uint *edges_ctx_b = &link_edge_buffer[link_b->ofs];
      uint edge_orig = we->edge_orig;

      for (; edges_len_a--; edges_ctx_a++) {
        uint e_ctx_a = *edges_ctx_a;
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
        uint e_ctx_b = *edges_ctx_b;
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
    weld_assert_edge_kill_len(r_wedge, wedge_len, edge_kill_len);
#endif

    MEM_freeN(link_edge_buffer);
  }

  *r_edge_kiil_len = edge_kill_len;
}

static void weld_edge_ctx_alloc(const MEdge *medge,
                                const uint medge_len,
                                const uint *vert_dest_map,
                                uint *r_edge_dest_map,
                                uint **r_edge_ctx_map,
                                WeldEdge **r_wedge,
                                uint *r_wedge_len)
{
  /* Edge Context. */
  uint *edge_map = MEM_mallocN(sizeof(*edge_map) * medge_len, __func__);
  uint wedge_len = 0;

  WeldEdge *wedge, *we;
  wedge = MEM_mallocN(sizeof(*wedge) * medge_len, __func__);
  we = &wedge[0];

  const MEdge *me = &medge[0];
  uint *e_dest_iter = &r_edge_dest_map[0];
  uint *iter = &edge_map[0];
  for (uint i = 0; i < medge_len; i++, me++, iter++, e_dest_iter++) {
    uint v1 = me->v1;
    uint v2 = me->v2;
    uint v_dest_1 = vert_dest_map[v1];
    uint v_dest_2 = vert_dest_map[v2];
    if ((v_dest_1 != OUT_OF_CONTEXT) || (v_dest_2 != OUT_OF_CONTEXT)) {
      we->vert_a = (v_dest_1 != OUT_OF_CONTEXT) ? v_dest_1 : v1;
      we->vert_b = (v_dest_2 != OUT_OF_CONTEXT) ? v_dest_2 : v2;
      we->edge_dest = OUT_OF_CONTEXT;
      we->edge_orig = i;
      we++;
      *e_dest_iter = i;
      *iter = wedge_len++;
    }
    else {
      *e_dest_iter = OUT_OF_CONTEXT;
      *iter = OUT_OF_CONTEXT;
    }
  }

  *r_wedge = MEM_reallocN(wedge, sizeof(*wedge) * wedge_len);
  *r_wedge_len = wedge_len;
  *r_edge_ctx_map = edge_map;
}

static void weld_edge_groups_setup(const uint medge_len,
                                   const uint edge_kill_len,
                                   const uint wedge_len,
                                   WeldEdge *wedge,
                                   const uint *wedge_map,
                                   uint *r_edge_groups_map,
                                   uint **r_edge_groups_buffer,
                                   struct WeldGroupEdge **r_edge_groups)
{

  /* Get weld edge groups. */

  struct WeldGroupEdge *wegroups, *wegrp_iter;

  uint wgroups_len = wedge_len - edge_kill_len;
  wegroups = MEM_callocN(sizeof(*wegroups) * wgroups_len, __func__);
  wegrp_iter = &wegroups[0];

  wgroups_len = 0;
  const uint *edge_ctx_iter = &wedge_map[0];
  uint *group_map_iter = &r_edge_groups_map[0];
  for (uint i = medge_len; i--; edge_ctx_iter++, group_map_iter++) {
    uint edge_ctx = *edge_ctx_iter;
    if (edge_ctx != OUT_OF_CONTEXT) {
      WeldEdge *we = &wedge[edge_ctx];
      uint edge_dest = we->edge_dest;
      if (edge_dest != OUT_OF_CONTEXT) {
        BLI_assert(edge_dest != we->edge_orig);
        *group_map_iter = ELEM_MERGED;
      }
      else {
        we->edge_dest = we->edge_orig;
        wegrp_iter->v1 = we->vert_a;
        wegrp_iter->v2 = we->vert_b;
        *group_map_iter = wgroups_len;
        wgroups_len++;
        wegrp_iter++;
      }
    }
    else {
      *group_map_iter = OUT_OF_CONTEXT;
    }
  }

  BLI_assert(wgroups_len == wedge_len - edge_kill_len);

  WeldEdge *we = &wedge[0];
  for (uint i = wedge_len; i--; we++) {
    if (we->flag == ELEM_COLLAPSED) {
      continue;
    }
    uint group_index = r_edge_groups_map[we->edge_dest];
    wegroups[group_index].group.len++;
  }

  uint ofs = 0;
  wegrp_iter = &wegroups[0];
  for (uint i = wgroups_len; i--; wegrp_iter++) {
    wegrp_iter->group.ofs = ofs;
    ofs += wegrp_iter->group.len;
  }

  uint *groups_buffer = MEM_mallocN(sizeof(*groups_buffer) * ofs, __func__);
  we = &wedge[0];
  for (uint i = wedge_len; i--; we++) {
    if (we->flag == ELEM_COLLAPSED) {
      continue;
    }
    uint group_index = r_edge_groups_map[we->edge_dest];
    groups_buffer[wegroups[group_index].group.ofs++] = we->edge_orig;
  }

  wegrp_iter = &wegroups[0];
  for (uint i = wgroups_len; i--; wegrp_iter++) {
    wegrp_iter->group.ofs -= wegrp_iter->group.len;
  }

  *r_edge_groups_buffer = groups_buffer;
  *r_edge_groups = wegroups;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Weld Poly and Loop API
 * \{ */

static bool weld_iter_loop_of_poly_begin(WeldLoopOfPolyIter *iter,
                                         const WeldPoly *wp,
                                         const WeldLoop *wloop,
                                         const MLoop *mloop,
                                         const uint *loop_map,
                                         uint *group_buffer)
{
  if (wp->flag == ELEM_COLLAPSED) {
    return false;
  }

  iter->loop_start = wp->loop_start;
  iter->loop_end = wp->loop_end;
  iter->wloop = wloop;
  iter->mloop = mloop;
  iter->loop_map = loop_map;
  iter->group = group_buffer;

  uint group_len = 0;
  if (group_buffer) {
    /* First loop group needs more attention. */
    uint loop_start, loop_end, l;
    loop_start = iter->loop_start;
    loop_end = l = iter->loop_end;
    while (l >= loop_start) {
      const uint loop_ctx = loop_map[l];
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
        iter->group[i++] = ++l;
      }
    }
  }
  iter->group_len = group_len;

  iter->l_next = iter->loop_start;
#ifdef USE_WELD_DEBUG
  iter->v = OUT_OF_CONTEXT;
#endif
  return true;
}

static bool weld_iter_loop_of_poly_next(WeldLoopOfPolyIter *iter)
{
  uint loop_end = iter->loop_end;
  const WeldLoop *wloop = iter->wloop;
  const uint *loop_map = iter->loop_map;
  uint l = iter->l_curr = iter->l_next;
  if (l == iter->loop_start) {
    /* `grupo_len` is already calculated in the first loop */
  }
  else {
    iter->group_len = 0;
  }
  while (l <= loop_end) {
    uint l_next = l + 1;
    const uint loop_ctx = loop_map[l];
    if (loop_ctx != OUT_OF_CONTEXT) {
      const WeldLoop *wl = &wloop[loop_ctx];
      if (wl->loop_skip_to != OUT_OF_CONTEXT) {
        l_next = wl->loop_skip_to;
      }
      if (wl->flag == ELEM_COLLAPSED) {
        if (iter->group) {
          iter->group[iter->group_len++] = l;
        }
        l = l_next;
        continue;
      }
#ifdef USE_WELD_DEBUG
      BLI_assert(iter->v != wl->vert);
#endif
      iter->v = wl->vert;
      iter->e = wl->edge;
      iter->type = 1;
    }
    else {
      const MLoop *ml = &iter->mloop[l];
#ifdef USE_WELD_DEBUG
      BLI_assert(iter->v != ml->v);
#endif
      iter->v = ml->v;
      iter->e = ml->e;
      iter->type = 0;
    }
    if (iter->group) {
      iter->group[iter->group_len++] = l;
    }
    iter->l_next = l_next;
    return true;
  }

  return false;
}

static void weld_poly_loop_ctx_alloc(const MPoly *mpoly,
                                     const uint mpoly_len,
                                     const MLoop *mloop,
                                     const uint mloop_len,
                                     const uint *vert_dest_map,
                                     const uint *edge_dest_map,
                                     WeldMesh *r_weld_mesh)
{
  /* Loop/Poly Context. */
  uint *loop_map = MEM_mallocN(sizeof(*loop_map) * mloop_len, __func__);
  uint *poly_map = MEM_mallocN(sizeof(*poly_map) * mpoly_len, __func__);
  uint wloop_len = 0;
  uint wpoly_len = 0;
  uint max_ctx_poly_len = 4;

  WeldLoop *wloop, *wl;
  wloop = MEM_mallocN(sizeof(*wloop) * mloop_len, __func__);
  wl = &wloop[0];

  WeldPoly *wpoly, *wp;
  wpoly = MEM_mallocN(sizeof(*wpoly) * mpoly_len, __func__);
  wp = &wpoly[0];

  uint maybe_new_poly = 0;

  const MPoly *mp = &mpoly[0];
  uint *iter = &poly_map[0];
  uint *loop_map_iter = &loop_map[0];
  for (uint i = 0; i < mpoly_len; i++, mp++, iter++) {
    const uint loopstart = mp->loopstart;
    const uint totloop = mp->totloop;

    uint vert_ctx_len = 0;

    uint l = loopstart;
    uint prev_wloop_len = wloop_len;
    const MLoop *ml = &mloop[l];
    for (uint j = totloop; j--; l++, ml++, loop_map_iter++) {
      uint v = ml->v;
      uint e = ml->e;
      uint v_dest = vert_dest_map[v];
      uint e_dest = edge_dest_map[e];
      bool is_vert_ctx = v_dest != OUT_OF_CONTEXT;
      bool is_edge_ctx = e_dest != OUT_OF_CONTEXT;
      if (is_vert_ctx) {
        vert_ctx_len++;
      }
      if (is_vert_ctx || is_edge_ctx) {
        wl->vert = is_vert_ctx ? v_dest : v;
        wl->edge = is_edge_ctx ? e_dest : e;
        wl->loop_orig = l;
        wl->loop_skip_to = OUT_OF_CONTEXT;
        wl++;

        *loop_map_iter = wloop_len++;
      }
      else {
        *loop_map_iter = OUT_OF_CONTEXT;
      }
    }
    if (wloop_len != prev_wloop_len) {
      uint loops_len = wloop_len - prev_wloop_len;

      wp->poly_dst = OUT_OF_CONTEXT;
      wp->poly_orig = i;
      wp->loops.len = loops_len;
      wp->loops.ofs = prev_wloop_len;
      wp->loop_start = loopstart;
      wp->loop_end = loopstart + totloop - 1;
      wp->len = totloop;
      wp++;

      *iter = wpoly_len++;
      if (totloop > 5 && vert_ctx_len > 1) {
        uint max_new = (totloop / 3) - 1;
        vert_ctx_len /= 2;
        maybe_new_poly += MIN2(max_new, vert_ctx_len);
        CLAMP_MIN(max_ctx_poly_len, totloop);
      }
    }
    else {
      *iter = OUT_OF_CONTEXT;
    }
  }

  if (mpoly_len < (wpoly_len + maybe_new_poly)) {
    WeldPoly *wpoly_tmp = wpoly;
    wpoly = MEM_mallocN(sizeof(*wpoly) * ((size_t)wpoly_len + maybe_new_poly), __func__);
    memcpy(wpoly, wpoly_tmp, sizeof(*wpoly) * wpoly_len);
    MEM_freeN(wpoly_tmp);
  }

  WeldPoly *poly_new = &wpoly[wpoly_len];

  r_weld_mesh->wloop = MEM_reallocN(wloop, sizeof(*wloop) * wloop_len);
  r_weld_mesh->wpoly = wpoly;
  r_weld_mesh->wpoly_new = poly_new;
  r_weld_mesh->wloop_len = wloop_len;
  r_weld_mesh->wpoly_len = wpoly_len;
  r_weld_mesh->wpoly_new_len = 0;
  r_weld_mesh->loop_map = loop_map;
  r_weld_mesh->poly_map = poly_map;
  r_weld_mesh->max_poly_len = max_ctx_poly_len;
}

static void weld_poly_split_recursive(const uint *vert_dest_map,
#ifdef USE_WELD_DEBUG
                                      const MLoop *mloop,
#endif
                                      uint ctx_verts_len,
                                      WeldPoly *r_wp,
                                      WeldMesh *r_weld_mesh,
                                      uint *r_poly_kill,
                                      uint *r_loop_kill)
{
  uint poly_len = r_wp->len;
  if (poly_len > 3 && ctx_verts_len > 1) {
    const uint ctx_loops_len = r_wp->loops.len;
    const uint ctx_loops_ofs = r_wp->loops.ofs;
    WeldLoop *wloop = r_weld_mesh->wloop;
    WeldPoly *wpoly_new = r_weld_mesh->wpoly_new;

    uint loop_kill = 0;

    WeldLoop *poly_loops = &wloop[ctx_loops_ofs];
    WeldLoop *wla = &poly_loops[0];
    WeldLoop *wla_prev = &poly_loops[ctx_loops_len - 1];
    while (wla_prev->flag == ELEM_COLLAPSED) {
      wla_prev--;
    }
    const uint la_len = ctx_loops_len - 1;
    for (uint la = 0; la < la_len; la++, wla++) {
    wa_continue:
      if (wla->flag == ELEM_COLLAPSED) {
        continue;
      }
      uint vert_a = wla->vert;
      /* Only test vertices that will be merged. */
      if (vert_dest_map[vert_a] != OUT_OF_CONTEXT) {
        uint lb = la + 1;
        WeldLoop *wlb = wla + 1;
        WeldLoop *wlb_prev = wla;
        uint killed_ab = 0;
        ctx_verts_len = 1;
        for (; lb < ctx_loops_len; lb++, wlb++) {
          BLI_assert(wlb->loop_skip_to == OUT_OF_CONTEXT);
          if (wlb->flag == ELEM_COLLAPSED) {
            killed_ab++;
            continue;
          }
          uint vert_b = wlb->vert;
          if (vert_dest_map[vert_b] != OUT_OF_CONTEXT) {
            ctx_verts_len++;
          }
          if (vert_a == vert_b) {
            const uint dist_a = wlb->loop_orig - wla->loop_orig - killed_ab;
            const uint dist_b = poly_len - dist_a;

            BLI_assert(dist_a != 0 && dist_b != 0);
            if (dist_a == 1 || dist_b == 1) {
              BLI_assert(dist_a != dist_b);
              BLI_assert((wla->flag == ELEM_COLLAPSED) || (wlb->flag == ELEM_COLLAPSED));
            }
            else {
              WeldLoop *wl_tmp = NULL;
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
                if (wl_tmp != NULL) {
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
              if (wl_tmp == NULL) {
                const uint new_loops_len = lb - la;
                const uint new_loops_ofs = ctx_loops_ofs + la;

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
    weld_assert_poly_no_vert_repetition(r_wp, wloop, mloop, r_weld_mesh->loop_map);
#endif
  }
}

static void weld_poly_loop_ctx_setup(const MLoop *mloop,
#ifdef USE_WELD_DEBUG
                                     const MPoly *mpoly,
                                     const uint mpoly_len,
                                     const uint mloop_len,
#endif
                                     const uint mvert_len,
                                     const uint *vert_dest_map,
                                     const uint remain_edge_ctx_len,
                                     struct WeldGroup *r_vlinks,
                                     WeldMesh *r_weld_mesh)
{
  uint poly_kill_len, loop_kill_len, wpoly_len, wpoly_new_len;

  WeldPoly *wpoly_new, *wpoly, *wp;
  WeldLoop *wloop, *wl;

  wpoly = r_weld_mesh->wpoly;
  wloop = r_weld_mesh->wloop;
  wpoly_new = r_weld_mesh->wpoly_new;
  wpoly_len = r_weld_mesh->wpoly_len;
  wpoly_new_len = 0;
  poly_kill_len = 0;
  loop_kill_len = 0;

  const uint *loop_map = r_weld_mesh->loop_map;

  if (remain_edge_ctx_len) {

    /* Setup Poly/Loop. */

    wp = &wpoly[0];
    for (uint i = wpoly_len; i--; wp++) {
      const uint ctx_loops_len = wp->loops.len;
      const uint ctx_loops_ofs = wp->loops.ofs;

      uint poly_len = wp->len;
      uint ctx_verts_len = 0;
      wl = &wloop[ctx_loops_ofs];
      for (uint l = ctx_loops_len; l--; wl++) {
        const uint edge_dest = wl->edge;
        if (edge_dest == ELEM_COLLAPSED) {
          wl->flag = ELEM_COLLAPSED;
          if (poly_len == 3) {
            wp->flag = ELEM_COLLAPSED;
            poly_kill_len++;
            loop_kill_len += 3;
            poly_len = 0;
            break;
          }
          loop_kill_len++;
          poly_len--;
        }
        else {
          const uint vert_dst = wl->vert;
          if (vert_dest_map[vert_dst] != OUT_OF_CONTEXT) {
            ctx_verts_len++;
          }
        }
      }

      if (poly_len) {
        wp->len = poly_len;
#ifdef USE_WELD_DEBUG
        weld_assert_poly_len(wp, wloop);
#endif

        weld_poly_split_recursive(vert_dest_map,
#ifdef USE_WELD_DEBUG
                                  mloop,
#endif
                                  ctx_verts_len,
                                  wp,
                                  r_weld_mesh,
                                  &poly_kill_len,
                                  &loop_kill_len);

        wpoly_new_len = r_weld_mesh->wpoly_new_len;
      }
    }

#ifdef USE_WELD_DEBUG
    weld_assert_poly_and_loop_kill_len(wpoly,
                                       wpoly_new,
                                       wpoly_new_len,
                                       wloop,
                                       mloop,
                                       loop_map,
                                       r_weld_mesh->poly_map,
                                       mpoly,
                                       mpoly_len,
                                       mloop_len,
                                       poly_kill_len,
                                       loop_kill_len);
#endif

    /* Setup Polygon Overlap. */

    uint wpoly_and_new_len = wpoly_len + wpoly_new_len;

    struct WeldGroup *vl_iter, *v_links = r_vlinks;
    memset(v_links, 0, sizeof(*v_links) * mvert_len);

    wp = &wpoly[0];
    for (uint i = wpoly_and_new_len; i--; wp++) {
      WeldLoopOfPolyIter iter;
      if (weld_iter_loop_of_poly_begin(&iter, wp, wloop, mloop, loop_map, NULL)) {
        while (weld_iter_loop_of_poly_next(&iter)) {
          v_links[iter.v].len++;
        }
      }
    }

    uint link_len = 0;
    vl_iter = &v_links[0];
    for (uint i = mvert_len; i--; vl_iter++) {
      vl_iter->ofs = link_len;
      link_len += vl_iter->len;
    }

    if (link_len) {
      uint *link_poly_buffer = MEM_mallocN(sizeof(*link_poly_buffer) * link_len, __func__);

      wp = &wpoly[0];
      for (uint i = 0; i < wpoly_and_new_len; i++, wp++) {
        WeldLoopOfPolyIter iter;
        if (weld_iter_loop_of_poly_begin(&iter, wp, wloop, mloop, loop_map, NULL)) {
          while (weld_iter_loop_of_poly_next(&iter)) {
            link_poly_buffer[v_links[iter.v].ofs++] = i;
          }
        }
      }

      vl_iter = &v_links[0];
      for (uint i = mvert_len; i--; vl_iter++) {
        /* Fix offset */
        vl_iter->ofs -= vl_iter->len;
      }

      uint polys_len_a, polys_len_b, *polys_ctx_a, *polys_ctx_b, p_ctx_a, p_ctx_b;
      polys_len_b = p_ctx_b = 0; /* silence warnings */

      wp = &wpoly[0];
      for (uint i = 0; i < wpoly_and_new_len; i++, wp++) {
        if (wp->poly_dst != OUT_OF_CONTEXT) {
          /* No need to retest poly.
           * (Already includes collapsed polygons). */
          continue;
        }

        WeldLoopOfPolyIter iter;
        weld_iter_loop_of_poly_begin(&iter, wp, wloop, mloop, loop_map, NULL);
        weld_iter_loop_of_poly_next(&iter);
        struct WeldGroup *link_a = &v_links[iter.v];
        polys_len_a = link_a->len;
        if (polys_len_a == 1) {
          BLI_assert(link_poly_buffer[link_a->ofs] == i);
          continue;
        }
        uint wp_len = wp->len;
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
          while (weld_iter_loop_of_poly_next(&iter_b)) {
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
          BLI_assert(wp_tmp != wp);
          wp_tmp->poly_dst = wp->poly_orig;
          loop_kill_len += wp_tmp->len;
          poly_kill_len++;
        }
      }
      MEM_freeN(link_poly_buffer);
    }
  }
  else {
    poly_kill_len = r_weld_mesh->wpoly_len;
    loop_kill_len = r_weld_mesh->wloop_len;

    wp = &wpoly[0];
    for (uint i = wpoly_len; i--; wp++) {
      wp->flag = ELEM_COLLAPSED;
    }
  }

#ifdef USE_WELD_DEBUG
  weld_assert_poly_and_loop_kill_len(wpoly,
                                     wpoly_new,
                                     wpoly_new_len,
                                     wloop,
                                     mloop,
                                     loop_map,
                                     r_weld_mesh->poly_map,
                                     mpoly,
                                     mpoly_len,
                                     mloop_len,
                                     poly_kill_len,
                                     loop_kill_len);
#endif

  r_weld_mesh->wpoly_new = wpoly_new;
  r_weld_mesh->poly_kill_len = poly_kill_len;
  r_weld_mesh->loop_kill_len = loop_kill_len;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Weld Mesh API
 * \{ */

static void weld_mesh_context_create(const Mesh *mesh,
                                     BVHTreeOverlap *overlap,
                                     const uint overlap_len,
                                     WeldMesh *r_weld_mesh)
{
  const MEdge *medge = mesh->medge;
  const MLoop *mloop = mesh->mloop;
  const MPoly *mpoly = mesh->mpoly;
  const uint mvert_len = mesh->totvert;
  const uint medge_len = mesh->totedge;
  const uint mloop_len = mesh->totloop;
  const uint mpoly_len = mesh->totpoly;

  uint *vert_dest_map = MEM_mallocN(sizeof(*vert_dest_map) * mvert_len, __func__);
  uint *edge_dest_map = MEM_mallocN(sizeof(*edge_dest_map) * medge_len, __func__);
  struct WeldGroup *v_links = MEM_callocN(sizeof(*v_links) * mvert_len, __func__);

  WeldVert *wvert;
  uint wvert_len;
  weld_vert_ctx_alloc_and_setup(mvert_len,
                                overlap,
                                overlap_len,
                                vert_dest_map,
                                &wvert,
                                &wvert_len,
                                &r_weld_mesh->vert_kill_len);

  uint *edge_ctx_map;
  WeldEdge *wedge;
  uint wedge_len;
  weld_edge_ctx_alloc(
      medge, medge_len, vert_dest_map, edge_dest_map, &edge_ctx_map, &wedge, &wedge_len);

  weld_edge_ctx_setup(
      mvert_len, wedge_len, v_links, edge_dest_map, wedge, &r_weld_mesh->edge_kill_len);

  weld_poly_loop_ctx_alloc(
      mpoly, mpoly_len, mloop, mloop_len, vert_dest_map, edge_dest_map, r_weld_mesh);

  weld_poly_loop_ctx_setup(mloop,
#ifdef USE_WELD_DEBUG
                           mpoly,
                           mpoly_len,
                           mloop_len,
#endif
                           mvert_len,
                           vert_dest_map,
                           wedge_len - r_weld_mesh->edge_kill_len,
                           v_links,
                           r_weld_mesh);

  weld_vert_groups_setup(mvert_len,
                         wvert_len,
                         wvert,
                         vert_dest_map,
                         vert_dest_map,
                         &r_weld_mesh->vert_groups_buffer,
                         &r_weld_mesh->vert_groups);

  weld_edge_groups_setup(medge_len,
                         r_weld_mesh->edge_kill_len,
                         wedge_len,
                         wedge,
                         edge_ctx_map,
                         edge_dest_map,
                         &r_weld_mesh->edge_groups_buffer,
                         &r_weld_mesh->edge_groups);

  r_weld_mesh->vert_groups_map = vert_dest_map;
  r_weld_mesh->edge_groups_map = edge_dest_map;
  MEM_freeN(v_links);
  MEM_freeN(wvert);
  MEM_freeN(edge_ctx_map);
  MEM_freeN(wedge);
}

static void weld_mesh_context_free(WeldMesh *weld_mesh)
{
  MEM_freeN(weld_mesh->vert_groups);
  MEM_freeN(weld_mesh->vert_groups_buffer);
  MEM_freeN(weld_mesh->vert_groups_map);

  MEM_freeN(weld_mesh->edge_groups);
  MEM_freeN(weld_mesh->edge_groups_buffer);
  MEM_freeN(weld_mesh->edge_groups_map);

  MEM_freeN(weld_mesh->wloop);
  MEM_freeN(weld_mesh->wpoly);
  MEM_freeN(weld_mesh->loop_map);
  MEM_freeN(weld_mesh->poly_map);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Weld CustomData
 * \{ */

static void customdata_weld(
    const CustomData *source, CustomData *dest, const uint *src_indices, int count, int dest_index)
{
  if (count == 1) {
    CustomData_copy_data(source, dest, src_indices[0], dest_index, 1);
    return;
  }

  CustomData_interp(source, dest, (const int *)src_indices, NULL, NULL, count, dest_index);

  int src_i, dest_i;
  int j;

  float co[3] = {0.0f, 0.0f, 0.0f};
#ifdef USE_WELD_NORMALS
  float no[3] = {0.0f, 0.0f, 0.0f};
#endif
  uint crease = 0;
  uint bweight = 0;
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
/** \name Weld Modifier Main
 * \{ */

struct WeldOverlapData {
  const MVert *mvert;
  float merge_dist_sq;
};
static bool bvhtree_weld_overlap_cb(void *userdata, int index_a, int index_b, int UNUSED(thread))
{
  if (index_a < index_b) {
    struct WeldOverlapData *data = userdata;
    const MVert *mvert = data->mvert;
    const float dist_sq = len_squared_v3v3(mvert[index_a].co, mvert[index_b].co);
    BLI_assert(dist_sq <= ((data->merge_dist_sq + FLT_EPSILON) * 3));
    return dist_sq <= data->merge_dist_sq;
  }
  return false;
}

static Mesh *weldModifier_doWeld(WeldModifierData *wmd, const ModifierEvalContext *ctx, Mesh *mesh)
{
  Mesh *result = mesh;

  Object *ob = ctx->object;
  BLI_bitmap *v_mask = NULL;
  int v_mask_act = 0;

  const MVert *mvert;
  const MLoop *mloop;
  const MPoly *mpoly, *mp;
  uint totvert, totedge, totloop, totpoly;
  uint i;

  mvert = mesh->mvert;
  totvert = mesh->totvert;

  /* Vertex Group. */
  const int defgrp_index = BKE_object_defgroup_name_index(ob, wmd->defgrp_name);
  if (defgrp_index != -1) {
    MDeformVert *dvert, *dv;
    dvert = CustomData_get_layer(&mesh->vdata, CD_MDEFORMVERT);
    if (dvert) {
      const bool invert_vgroup = (wmd->flag & MOD_WELD_INVERT_VGROUP) != 0;
      dv = &dvert[0];
      v_mask = BLI_BITMAP_NEW(totvert, __func__);
      for (i = 0; i < totvert; i++, dv++) {
        const bool found = BKE_defvert_find_weight(dv, defgrp_index) > 0.0f;
        if (found != invert_vgroup) {
          BLI_BITMAP_ENABLE(v_mask, i);
          v_mask_act++;
        }
      }
    }
  }

  /* Get overlap map. */
  /* TODO: For a better performanse use KD-Tree. */
  struct BVHTreeFromMesh treedata;
  BVHTree *bvhtree = bvhtree_from_mesh_verts_ex(&treedata,
                                                mvert,
                                                totvert,
                                                false,
                                                v_mask,
                                                v_mask_act,
                                                wmd->merge_dist / 2,
                                                2,
                                                6,
                                                0,
                                                NULL,
                                                NULL);

  if (v_mask) {
    MEM_freeN(v_mask);
  }

  if (bvhtree == NULL) {
    return result;
  }

  struct WeldOverlapData data;
  data.mvert = mvert;
  data.merge_dist_sq = square_f(wmd->merge_dist);

  uint overlap_len;
  BVHTreeOverlap *overlap = BLI_bvhtree_overlap_ex(bvhtree,
                                                   bvhtree,
                                                   &overlap_len,
                                                   bvhtree_weld_overlap_cb,
                                                   &data,
                                                   wmd->max_interactions,
                                                   BVH_OVERLAP_RETURN_PAIRS);

  free_bvhtree_from_mesh(&treedata);

  if (overlap_len) {
    WeldMesh weld_mesh;
    weld_mesh_context_create(mesh, overlap, overlap_len, &weld_mesh);

    mloop = mesh->mloop;
    mpoly = mesh->mpoly;

    totedge = mesh->totedge;
    totloop = mesh->totloop;
    totpoly = mesh->totpoly;

    const int result_nverts = totvert - weld_mesh.vert_kill_len;
    const int result_nedges = totedge - weld_mesh.edge_kill_len;
    const int result_nloops = totloop - weld_mesh.loop_kill_len;
    const int result_npolys = totpoly - weld_mesh.poly_kill_len + weld_mesh.wpoly_new_len;

    result = BKE_mesh_new_nomain_from_template(
        mesh, result_nverts, result_nedges, 0, result_nloops, result_npolys);

    /* Vertices */

    uint *vert_final = weld_mesh.vert_groups_map;
    uint *index_iter = &vert_final[0];
    int dest_index = 0;
    for (i = 0; i < totvert; i++, index_iter++) {
      int source_index = i;
      int count = 0;
      while (i < totvert && *index_iter == OUT_OF_CONTEXT) {
        *index_iter = dest_index + count;
        index_iter++;
        count++;
        i++;
      }
      if (count) {
        CustomData_copy_data(&mesh->vdata, &result->vdata, source_index, dest_index, count);
        dest_index += count;
      }
      if (i == totvert) {
        break;
      }
      if (*index_iter != ELEM_MERGED) {
        struct WeldGroup *wgroup = &weld_mesh.vert_groups[*index_iter];
        customdata_weld(&mesh->vdata,
                        &result->vdata,
                        &weld_mesh.vert_groups_buffer[wgroup->ofs],
                        wgroup->len,
                        dest_index);
        *index_iter = dest_index;
        dest_index++;
      }
    }

    BLI_assert(dest_index == result_nverts);

    /* Edges */

    uint *edge_final = weld_mesh.edge_groups_map;
    index_iter = &edge_final[0];
    dest_index = 0;
    for (i = 0; i < totedge; i++, index_iter++) {
      int source_index = i;
      int count = 0;
      while (i < totedge && *index_iter == OUT_OF_CONTEXT) {
        *index_iter = dest_index + count;
        index_iter++;
        count++;
        i++;
      }
      if (count) {
        CustomData_copy_data(&mesh->edata, &result->edata, source_index, dest_index, count);
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
      if (*index_iter != ELEM_MERGED) {
        struct WeldGroupEdge *wegrp = &weld_mesh.edge_groups[*index_iter];
        customdata_weld(&mesh->edata,
                        &result->edata,
                        &weld_mesh.edge_groups_buffer[wegrp->group.ofs],
                        wegrp->group.len,
                        dest_index);
        MEdge *me = &result->medge[dest_index];
        me->v1 = vert_final[wegrp->v1];
        me->v2 = vert_final[wegrp->v2];
        me->flag |= ME_LOOSEEDGE;

        *index_iter = dest_index;
        dest_index++;
      }
    }

    BLI_assert(dest_index == result_nedges);

    /* Polys/Loops */

    mp = &mpoly[0];
    MPoly *r_mp = &result->mpoly[0];
    MLoop *r_ml = &result->mloop[0];
    uint r_i = 0;
    int loop_cur = 0;
    uint *group_buffer = BLI_array_alloca(group_buffer, weld_mesh.max_poly_len);
    for (i = 0; i < totpoly; i++, mp++) {
      int loop_start = loop_cur;
      uint poly_ctx = weld_mesh.poly_map[i];
      if (poly_ctx == OUT_OF_CONTEXT) {
        uint mp_loop_len = mp->totloop;
        CustomData_copy_data(&mesh->ldata, &result->ldata, mp->loopstart, loop_cur, mp_loop_len);
        loop_cur += mp_loop_len;
        for (; mp_loop_len--; r_ml++) {
          r_ml->v = vert_final[r_ml->v];
          r_ml->e = edge_final[r_ml->e];
        }
      }
      else {
        WeldPoly *wp = &weld_mesh.wpoly[poly_ctx];
        WeldLoopOfPolyIter iter;
        if (!weld_iter_loop_of_poly_begin(
                &iter, wp, weld_mesh.wloop, mloop, weld_mesh.loop_map, group_buffer)) {
          continue;
        }
        else {
          if (wp->poly_dst != OUT_OF_CONTEXT) {
            continue;
          }
          while (weld_iter_loop_of_poly_next(&iter)) {
            customdata_weld(&mesh->ldata, &result->ldata, group_buffer, iter.group_len, loop_cur);
            uint v = vert_final[iter.v];
            uint e = edge_final[iter.e];
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
      }

      CustomData_copy_data(&mesh->pdata, &result->pdata, i, r_i, 1);
      r_mp->loopstart = loop_start;
      r_mp->totloop = loop_cur - loop_start;
      r_mp++;
      r_i++;
    }

    WeldPoly *wp = &weld_mesh.wpoly_new[0];
    for (i = 0; i < weld_mesh.wpoly_new_len; i++, wp++) {
      int loop_start = loop_cur;
      WeldLoopOfPolyIter iter;
      if (!weld_iter_loop_of_poly_begin(
              &iter, wp, weld_mesh.wloop, mloop, weld_mesh.loop_map, group_buffer)) {
        continue;
      }
      else {
        if (wp->poly_dst != OUT_OF_CONTEXT) {
          continue;
        }
        while (weld_iter_loop_of_poly_next(&iter)) {
          customdata_weld(&mesh->ldata, &result->ldata, group_buffer, iter.group_len, loop_cur);
          uint v = vert_final[iter.v];
          uint e = edge_final[iter.e];
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
      r_mp->loopstart = loop_start;
      r_mp->totloop = loop_cur - loop_start;
      r_mp++;
      r_i++;
    }

    BLI_assert((int)r_i == result_npolys);
    BLI_assert(loop_cur == result_nloops);

    /* is this needed? */
    /* recalculate normals */
    result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;

    weld_mesh_context_free(&weld_mesh);
  }

  MEM_freeN(overlap);
  return result;
}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  WeldModifierData *wmd = (WeldModifierData *)md;
  return weldModifier_doWeld(wmd, ctx, mesh);
}

static void initData(ModifierData *md)
{
  WeldModifierData *wmd = (WeldModifierData *)md;

  wmd->merge_dist = 0.001f;
  wmd->max_interactions = 1;
  wmd->defgrp_name[0] = '\0';
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *md,
                             CustomData_MeshMasks *r_cddata_masks)
{
  WeldModifierData *wmd = (WeldModifierData *)md;

  /* Ask for vertexgroups if we need them. */
  if (wmd->defgrp_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "merge_threshold", 0, IFACE_("Distance"), ICON_NONE);
  uiItemR(layout, &ptr, "max_interactions", 0, NULL, ICON_NONE);
  modifier_vgroup_ui(layout, &ptr, &ob_ptr, "vertex_group", "invert_vertex_group", NULL);

  modifier_panel_end(layout, &ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Weld, panel_draw);
}

ModifierTypeInfo modifierType_Weld = {
    /* name */ "Weld",
    /* structName */ "WeldModifierData",
    /* structSize */ sizeof(WeldModifierData),
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_EnableInEditmode |
        eModifierTypeFlag_AcceptsCVs,

    /* copyData */ BKE_modifier_copydata_generic,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ modifyMesh,
    /* modifyHair */ NULL,
    /* modifyPointCloud */ NULL,
    /* modifyVolume */ NULL,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
};

/** \} */
