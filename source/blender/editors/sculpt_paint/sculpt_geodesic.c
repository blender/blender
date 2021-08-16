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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_array.h"
#include "BLI_blenlib.h"
#include "BLI_linklist_stack.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_mempool.h"
#include "BLI_sort_utils.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_brush.h"
#include "BKE_ccg.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_multires.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_scene.h"
#include "BKE_subdiv_ccg.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_view3d.h"
#include "paint_intern.h"
#include "sculpt_intern.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"

#include "bmesh.h"

#include <math.h>
#include <stdlib.h>
#define SCULPT_GEODESIC_VERTEX_NONE -1

/* Propagate distance from v1 and v2 to v0. */
static bool sculpt_geodesic_mesh_test_dist_add(MVert *mvert,
                                               const int v0,
                                               const int v1,
                                               const int v2,
                                               float *dists,
                                               GSet *initial_vertices,
                                               SculptVertRef *r_closest_verts,
                                               float (*cos)[3])
{
  if (BLI_gset_haskey(initial_vertices, POINTER_FROM_INT(v0))) {
    return false;
  }

  BLI_assert(dists[v1] != FLT_MAX);
  if (dists[v0] <= dists[v1]) {
    return false;
  }

  float *co0 = cos ? cos[v0] : mvert[v0].co;
  float *co1 = cos ? cos[v1] : mvert[v1].co;
  float *co2 = v2 != SCULPT_GEODESIC_VERTEX_NONE ? (cos ? cos[v2] : mvert[v2].co) : NULL;

  float dist0;
  if (v2 != SCULPT_GEODESIC_VERTEX_NONE) {
    BLI_assert(dists[v2] != FLT_MAX);
    if (dists[v0] <= dists[v2]) {
      return false;
    }
    dist0 = geodesic_distance_propagate_across_triangle(co0, co1, co2, dists[v1], dists[v2]);
  }
  else {
    float vec[3];
    sub_v3_v3v3(vec, co1, co0);
    dist0 = dists[v1] + len_v3(vec);
  }

  if (dist0 < dists[v0]) {
    dists[v0] = dist0;

    if (r_closest_verts) {
      bool tag1 = r_closest_verts[v1].i != -1LL;
      bool tag2 = v2 != SCULPT_GEODESIC_VERTEX_NONE && r_closest_verts[v2].i != -1LL;

      float l1 = len_v3v3(co0, co1);
      float l2 = v2 != SCULPT_GEODESIC_VERTEX_NONE ? len_v3v3(co0, co2) : 0.0f;

      if (tag1 && tag2) {
        if (l1 < l2) {
          r_closest_verts[v0] = r_closest_verts[v1];
        }
        else {
          r_closest_verts[v0] = r_closest_verts[v2];
        }
      }
      else if (tag2) {
        r_closest_verts[v0] = r_closest_verts[v2];
      }
      else if (tag1) {
        r_closest_verts[v0] = r_closest_verts[v1];
      }
    }
    return true;
  }

  return false;
}

/* Propagate distance from v1 and v2 to v0. */
static bool sculpt_geodesic_grids_test_dist_add(SculptSession *ss,
                                                const int v0,
                                                const int v1,
                                                const int v2,
                                                float *dists,
                                                GSet *initial_vertices,
                                                SculptVertRef *r_closest_verts,
                                                float (*cos)[3])
{
  if (BLI_gset_haskey(initial_vertices, POINTER_FROM_INT(v0))) {
    return false;
  }

  BLI_assert(dists[v1] != FLT_MAX);
  if (dists[v0] <= dists[v1]) {
    return false;
  }

  const float *co0 = cos ? cos[v0] :
                           SCULPT_vertex_co_get(ss, BKE_pbvh_table_index_to_vertex(ss->pbvh, v0));
  const float *co1 = cos ? cos[v1] :
                           SCULPT_vertex_co_get(ss, BKE_pbvh_table_index_to_vertex(ss->pbvh, v1));
  const float *co2 = v2 != SCULPT_GEODESIC_VERTEX_NONE ?
                         (cos ? cos[v2] :
                                SCULPT_vertex_co_get(
                                    ss, BKE_pbvh_table_index_to_vertex(ss->pbvh, v2))) :
                         NULL;

  float dist0;
  if (v2 != SCULPT_GEODESIC_VERTEX_NONE) {
    BLI_assert(dists[v2] != FLT_MAX);
    if (dists[v0] <= dists[v2]) {
      return false;
    }
    dist0 = geodesic_distance_propagate_across_triangle(co0, co1, co2, dists[v1], dists[v2]);
  }
  else {
    float vec[3];
    sub_v3_v3v3(vec, co1, co0);
    dist0 = dists[v1] + len_v3(vec);
  }

  if (dist0 < dists[v0]) {
    dists[v0] = dist0;

    if (r_closest_verts) {
      bool tag1 = r_closest_verts[v1].i != -1LL;
      bool tag2 = v2 != SCULPT_GEODESIC_VERTEX_NONE && r_closest_verts[v2].i != -1LL;

      float l1 = len_v3v3(co0, co1);
      float l2 = v2 != SCULPT_GEODESIC_VERTEX_NONE ? len_v3v3(co0, co2) : 0.0f;

      if (tag1 && tag2) {
        if (l1 < l2) {
          r_closest_verts[v0] = r_closest_verts[v1];
        }
        else {
          r_closest_verts[v0] = r_closest_verts[v2];
        }
      }
      else if (tag2) {
        r_closest_verts[v0] = r_closest_verts[v2];
      }
      else if (tag1) {
        r_closest_verts[v0] = r_closest_verts[v1];
      }
    }
    return true;
  }

  return false;
}

#define BMESH_INITIAL_VERT_TAG BM_ELEM_TAG_ALT

static bool sculpt_geodesic_mesh_test_dist_add_bmesh(BMVert *v0,
                                                     BMVert *v1,
                                                     BMVert *v2,
                                                     float *dists,
                                                     GSet *initial_vertices,
                                                     SculptVertRef *r_closest_verts,
                                                     float (*cos)[3])
{
  const int v0_i = BM_elem_index_get(v0);
  const int v1_i = BM_elem_index_get(v1);
  const int v2_i = v2 ? BM_elem_index_get(v2) : SCULPT_GEODESIC_VERTEX_NONE;

  const float *v0co = cos ? cos[BM_elem_index_get(v0)] : v0->co;
  const float *v1co = cos ? cos[BM_elem_index_get(v1)] : v1->co;
  const float *v2co = v2 ? (cos ? cos[BM_elem_index_get(v2)] : v2->co) : NULL;

  if (BM_elem_flag_test(v0, BMESH_INITIAL_VERT_TAG)) {
    return false;
  }

  BLI_assert(dists[v1_i] != FLT_MAX);
  if (dists[v0_i] <= dists[v1_i]) {
    return false;
  }

  float dist0;
  if (v2_i != SCULPT_GEODESIC_VERTEX_NONE) {
    BLI_assert(dists[v2_i] != FLT_MAX);
    if (dists[v0_i] <= dists[v2_i]) {
      return false;
    }

    dist0 = geodesic_distance_propagate_across_triangle(
        v0co, v1co, v2co, dists[v1_i], dists[v2_i]);
  }
  else {
    float vec[3];
    sub_v3_v3v3(vec, v1co, v0co);
    dist0 = dists[v1_i] + len_v3(vec);
  }

  if (dist0 < dists[v0_i]) {
    dists[v0_i] = dist0;

    if (r_closest_verts) {
      bool tag1 = r_closest_verts[v1_i].i != -1LL;
      bool tag2 = v2 && r_closest_verts[v2_i].i != -1LL;

      float l1 = len_v3v3(v0co, v1co);
      float l2 = v2 ? len_v3v3(v0co, v2co) : 0.0f;

      if (!tag1 && !tag2) {
        printf("bad\n");
      }

      if (tag1 && tag2) {
        if (l1 < l2) {  // dists[v1_i] < dists[v2_i]) {
          r_closest_verts[v0_i] = r_closest_verts[v1_i];
        }
        else {
          r_closest_verts[v0_i] = r_closest_verts[v2_i];
        }
      }
      else if (tag2) {
        r_closest_verts[v0_i] = r_closest_verts[v2_i];
      }
      else if (tag1) {
        r_closest_verts[v0_i] = r_closest_verts[v1_i];
      }
    }

    return true;
  }

  return false;
}

static float *SCULPT_geodesic_mesh_create(Object *ob,
                                          GSet *initial_vertices,
                                          const float limit_radius,
                                          SculptVertRef *r_closest_verts,
                                          float (*cos)[3])
{
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = BKE_object_get_original_mesh(ob);

  const int totvert = mesh->totvert;
  const int totedge = mesh->totedge;

  const float limit_radius_sq = limit_radius * limit_radius;

  MEdge *edges = mesh->medge;
  MVert *verts = SCULPT_mesh_deformed_mverts_get(ss);

  float *dists = MEM_malloc_arrayN(totvert, sizeof(float), "distances");
  BLI_bitmap *edge_tag = BLI_BITMAP_NEW(totedge, "edge tag");

  if (!ss->epmap) {
    BKE_mesh_edge_poly_map_create(&ss->epmap,
                                  &ss->epmap_mem,
                                  mesh->medge,
                                  mesh->totedge,
                                  mesh->mpoly,
                                  mesh->totpoly,
                                  mesh->mloop,
                                  mesh->totloop);
  }
  if (!ss->vemap) {
    BKE_mesh_vert_edge_map_create(
        &ss->vemap, &ss->vemap_mem, mesh->mvert, mesh->medge, mesh->totvert, mesh->totedge, true);
  }

  /* Both contain edge indices encoded as *void. */
  BLI_LINKSTACK_DECLARE(queue, void *);
  BLI_LINKSTACK_DECLARE(queue_next, void *);

  BLI_LINKSTACK_INIT(queue);
  BLI_LINKSTACK_INIT(queue_next);

  for (int i = 0; i < totvert; i++) {
    if (BLI_gset_haskey(initial_vertices, POINTER_FROM_INT(i))) {
      if (r_closest_verts) {
        r_closest_verts[i] = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);
      }

      dists[i] = 0.0f;
    }
    else {
      if (r_closest_verts) {
        r_closest_verts[i].i = -1LL;
      }

      dists[i] = FLT_MAX;
    }
  }

  /* Masks vertices that are further than limit radius from an initial vertex. As there is no need
   * to define a distance to them the algorithm can stop earlier by skipping them. */
  BLI_bitmap *affected_vertex = BLI_BITMAP_NEW(totvert, "affected vertex");
  GSetIterator gs_iter;

  if (limit_radius == FLT_MAX) {
    /* In this case, no need to loop through all initial vertices to check distances as they are
     * all going to be affected. */
    BLI_bitmap_set_all(affected_vertex, true, totvert);
  }
  else {
    /* This is an O(n^2) loop used to limit the geodesic distance calculation to a radius. When
     * this optimization is needed, it is expected for the tool to request the distance to a low
     * number of vertices (usually just 1 or 2). */
    GSET_ITER (gs_iter, initial_vertices) {
      const int v = POINTER_AS_INT(BLI_gsetIterator_getKey(&gs_iter));
      float *v_co = cos ? cos[v] : verts[v].co;

      for (int i = 0; i < totvert; i++) {
        if (len_squared_v3v3(v_co, cos ? cos[i] : verts[i].co) <= limit_radius_sq) {
          BLI_BITMAP_ENABLE(affected_vertex, i);
        }
      }
    }
  }

  /* Add edges adjacent to an initial vertex to the queue. */
  for (int i = 0; i < totedge; i++) {
    const int v1 = edges[i].v1;
    const int v2 = edges[i].v2;
    if (!BLI_BITMAP_TEST(affected_vertex, v1) && !BLI_BITMAP_TEST(affected_vertex, v2)) {
      continue;
    }
    if (dists[v1] != FLT_MAX || dists[v2] != FLT_MAX) {
      BLI_LINKSTACK_PUSH(queue, POINTER_FROM_INT(i));
    }
  }

  do {
    while (BLI_LINKSTACK_SIZE(queue)) {
      const int e = POINTER_AS_INT(BLI_LINKSTACK_POP(queue));
      int v1 = edges[e].v1;
      int v2 = edges[e].v2;

      if (dists[v1] == FLT_MAX || dists[v2] == FLT_MAX) {
        if (dists[v1] > dists[v2]) {
          SWAP(int, v1, v2);
        }
        sculpt_geodesic_mesh_test_dist_add(verts,
                                           v2,
                                           v1,
                                           SCULPT_GEODESIC_VERTEX_NONE,
                                           dists,
                                           initial_vertices,
                                           r_closest_verts,
                                           cos);
      }

      if (ss->epmap[e].count != 0) {
        for (int poly_map_index = 0; poly_map_index < ss->epmap[e].count; poly_map_index++) {
          const int poly = ss->epmap[e].indices[poly_map_index];
          if (ss->face_sets[poly] <= 0) {
            continue;
          }
          const MPoly *mpoly = &mesh->mpoly[poly];

          for (int loop_index = 0; loop_index < mpoly->totloop; loop_index++) {
            const MLoop *mloop = &mesh->mloop[loop_index + mpoly->loopstart];
            const int v_other = mloop->v;
            if (ELEM(v_other, v1, v2)) {
              continue;
            }
            if (sculpt_geodesic_mesh_test_dist_add(
                    verts, v_other, v1, v2, dists, initial_vertices, r_closest_verts, cos)) {
              for (int edge_map_index = 0; edge_map_index < ss->vemap[v_other].count;
                   edge_map_index++) {
                const int e_other = ss->vemap[v_other].indices[edge_map_index];
                int ev_other;
                if (edges[e_other].v1 == (uint)v_other) {
                  ev_other = edges[e_other].v2;
                }
                else {
                  ev_other = edges[e_other].v1;
                }

                if (e_other != e && !BLI_BITMAP_TEST(edge_tag, e_other) &&
                    (ss->epmap[e_other].count == 0 || dists[ev_other] != FLT_MAX)) {
                  if (BLI_BITMAP_TEST(affected_vertex, v_other) ||
                      BLI_BITMAP_TEST(affected_vertex, ev_other)) {
                    BLI_BITMAP_ENABLE(edge_tag, e_other);
                    BLI_LINKSTACK_PUSH(queue_next, POINTER_FROM_INT(e_other));
                  }
                }
              }
            }
          }
        }
      }
    }

    for (LinkNode *lnk = queue_next; lnk; lnk = lnk->next) {
      const int e = POINTER_AS_INT(lnk->link);
      BLI_BITMAP_DISABLE(edge_tag, e);
    }

    BLI_LINKSTACK_SWAP(queue, queue_next);

  } while (BLI_LINKSTACK_SIZE(queue));

  BLI_LINKSTACK_FREE(queue);
  BLI_LINKSTACK_FREE(queue_next);
  MEM_SAFE_FREE(edge_tag);
  MEM_SAFE_FREE(affected_vertex);

  return dists;
}

static float *SCULPT_geodesic_bmesh_create(Object *ob,
                                           GSet *initial_vertices,
                                           const float limit_radius,
                                           SculptVertRef *r_closest_verts,
                                           float (*cos)[3])
{
  SculptSession *ss = ob->sculpt;

  if (!ss->bm) {
    return NULL;
  }

  BM_mesh_elem_index_ensure(ss->bm, BM_VERT | BM_EDGE | BM_FACE);

  const int totvert = ss->bm->totvert;
  const int totedge = ss->bm->totedge;

  if (r_closest_verts) {
    for (int i = 0; i < totvert; i++) {
      r_closest_verts[i].i = -1LL;
    }
  }

  const float limit_radius_sq = limit_radius * limit_radius;

  float *dists = MEM_malloc_arrayN(totvert, sizeof(float), "distances");
  BLI_bitmap *edge_tag = BLI_BITMAP_NEW(totedge, "edge tag");

  BLI_LINKSTACK_DECLARE(queue, BMEdge *);
  BLI_LINKSTACK_DECLARE(queue_next, BMEdge *);

  BLI_LINKSTACK_INIT(queue);
  BLI_LINKSTACK_INIT(queue_next);

  for (int i = 0; i < totvert; i++) {
    if (BLI_gset_haskey(initial_vertices, POINTER_FROM_INT(i))) {
      dists[i] = 0.0f;

      if (r_closest_verts) {
        r_closest_verts[i] = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);
      }
    }
    else {
      dists[i] = FLT_MAX;
    }
  }

  /* Masks vertices that are further than limit radius from an initial vertex. As there is no need
   * to define a distance to them the algorithm can stop earlier by skipping them. */
  BLI_bitmap *affected_vertex = BLI_BITMAP_NEW(totvert, "affected vertex");
  GSetIterator gs_iter;

  BMVert *v;
  BMIter iter;

  BM_ITER_MESH (v, &iter, ss->bm, BM_VERTS_OF_MESH) {
    BM_elem_flag_disable(v, BMESH_INITIAL_VERT_TAG);
  }

  if (limit_radius == FLT_MAX) {
    /* In this case, no need to loop through all initial vertices to check distances as they are
     * all going to be affected. */
    BLI_bitmap_set_all(affected_vertex, true, totvert);
  }
  else {
    /* This is an O(n^2) loop used to limit the geodesic distance calculation to a radius. When
     * this optimization is needed, it is expected for the tool to request the distance to a low
     * number of vertices (usually just 1 or 2). */
    GSET_ITER (gs_iter, initial_vertices) {
      const int v_i = POINTER_AS_INT(BLI_gsetIterator_getKey(&gs_iter));
      BMVert *v = (BMVert *)BKE_pbvh_table_index_to_vertex(ss->pbvh, v_i).i;
      float *co1 = cos ? cos[BM_elem_index_get(v)] : v->co;

      BM_elem_flag_enable(v, BMESH_INITIAL_VERT_TAG);

      for (int i = 0; i < totvert; i++) {
        BMVert *v2 = (BMVert *)BKE_pbvh_table_index_to_vertex(ss->pbvh, i).i;
        float *co2 = cos ? cos[BM_elem_index_get(v2)] : v2->co;

        if (len_squared_v3v3(co1, co2) <= limit_radius_sq) {
          BLI_BITMAP_ENABLE(affected_vertex, i);
        }
      }
    }
  }

  BMEdge *e;
  int i = 0;

  BM_ITER_MESH (e, &iter, ss->bm, BM_EDGES_OF_MESH) {
    const int v1_i = BM_elem_index_get(e->v1);
    const int v2_i = BM_elem_index_get(e->v2);

    if (!BLI_BITMAP_TEST(affected_vertex, v1_i) && !BLI_BITMAP_TEST(affected_vertex, v2_i)) {
      i++;
      continue;
    }
    if (dists[v1_i] != FLT_MAX || dists[v2_i] != FLT_MAX) {
      BLI_LINKSTACK_PUSH(queue, e);
    }

    i++;
  }

  do {
    while (BLI_LINKSTACK_SIZE(queue)) {
      BMEdge *e = BLI_LINKSTACK_POP(queue);

      BMVert *v1 = e->v1, *v2 = e->v2;
      int v1_i = BM_elem_index_get(e->v1);
      int v2_i = BM_elem_index_get(e->v2);

      if (dists[v1_i] == FLT_MAX || dists[v2_i] == FLT_MAX) {
        if (dists[v1_i] > dists[v2_i]) {
          SWAP(BMVert *, v1, v2);
          SWAP(int, v1_i, v2_i);
        }
        sculpt_geodesic_mesh_test_dist_add_bmesh(
            v2, v1, NULL, dists, initial_vertices, r_closest_verts, cos);
      }

      BMLoop *l = e->l;
      if (l) {
        do {
          BMFace *f = l->f;
          BMLoop *l2 = f->l_first;

          if (BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset) < 0) {
            l = l->radial_next;
            continue;
          }

          do {
            BMVert *v_other = l2->v;

            if (ELEM(v_other, v1, v2)) {
              l2 = l2->next;
              continue;
            }

            const int v_other_i = BM_elem_index_get(v_other);

            if (sculpt_geodesic_mesh_test_dist_add_bmesh(
                    v_other, v1, v2, dists, initial_vertices, r_closest_verts, cos)) {
              BMIter eiter;
              BMEdge *e_other;

              BM_ITER_ELEM (e_other, &eiter, v_other, BM_EDGES_OF_VERT) {
                BMVert *ev_other;

                if (e_other->v1 == v_other) {
                  ev_other = e_other->v2;
                }
                else {
                  ev_other = e_other->v1;
                }

                const int ev_other_i = BM_elem_index_get(ev_other);
                const int e_other_i = BM_elem_index_get(e_other);

                bool ok = e_other != e && !BLI_BITMAP_TEST(edge_tag, e_other_i);
                ok = ok && (!e_other->l || dists[ev_other_i] != FLT_MAX);
                ok = ok && (BLI_BITMAP_TEST(affected_vertex, v_other_i) ||
                            BLI_BITMAP_TEST(affected_vertex, ev_other_i));

                if (ok) {
                  BLI_BITMAP_ENABLE(edge_tag, e_other_i);
                  BLI_LINKSTACK_PUSH(queue_next, e_other);
                }
              }
            }

            l2 = l2->next;
          } while (l2 != f->l_first);

          l = l->radial_next;
        } while (l != e->l);
      }
    }

    for (LinkNode *lnk = queue_next; lnk; lnk = lnk->next) {
      BMEdge *e = (BMEdge *)lnk->link;
      const int e_i = BM_elem_index_get(e);

      BLI_BITMAP_DISABLE(edge_tag, e_i);
    }

    BLI_LINKSTACK_SWAP(queue, queue_next);

  } while (BLI_LINKSTACK_SIZE(queue));

  BLI_LINKSTACK_FREE(queue);
  BLI_LINKSTACK_FREE(queue_next);
  MEM_SAFE_FREE(edge_tag);
  MEM_SAFE_FREE(affected_vertex);

  return dists;
}

BLI_INLINE void *hash_edge(int v1, int v2, int totvert)
{
  if (v1 > v2) {
    SWAP(int, v1, v2);
  }

  intptr_t ret = (intptr_t)v1 + (intptr_t)v2 * (intptr_t)totvert;
  return (void *)ret;
}

typedef struct TempEdge {
  int v1, v2;
} TempEdge;

int find_quad(TempEdge *edges, MeshElemMap *vmap, int v1, int v2, int v3)
{
  for (int i = 0; i < vmap[v1].count; i++) {
    TempEdge *te = edges + vmap[v1].indices[i];
    int v = v1 == te->v1 ? te->v2 : te->v1;

    if (v == v2) {
      continue;
    }

    for (int j = 0; j < vmap[v].count; j++) {
      TempEdge *te2 = edges + vmap[v].indices[j];
      int v4 = v == te2->v1 ? te2->v2 : te2->v1;

      if (v4 == v3) {
        return v;
      }
    }
  }

  return -1;
}

static float *SCULPT_geodesic_grids_create(Object *ob,
                                           GSet *initial_vertices,
                                           const float limit_radius,
                                           SculptVertRef *r_closest_verts,
                                           float (*cos)[3])
{
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = BKE_object_get_original_mesh(ob);

  const int totvert = SCULPT_vertex_count_get(ss);

  const float limit_radius_sq = limit_radius * limit_radius;

  float *dists = MEM_malloc_arrayN(totvert, sizeof(float), "distances");

  /* Both contain edge indices encoded as *void. */
  BLI_LINKSTACK_DECLARE(queue, void *);
  BLI_LINKSTACK_DECLARE(queue_next, void *);

  BLI_LINKSTACK_INIT(queue);
  BLI_LINKSTACK_INIT(queue_next);

  for (int i = 0; i < totvert; i++) {
    if (BLI_gset_haskey(initial_vertices, POINTER_FROM_INT(i))) {
      if (r_closest_verts) {
        r_closest_verts[i] = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);
      }

      dists[i] = 0.0f;
    }
    else {
      if (r_closest_verts) {
        r_closest_verts[i].i = -1LL;
      }

      dists[i] = FLT_MAX;
    }
  }

  /* Masks vertices that are further than limit radius from an initial vertex. As there is no need
   * to define a distance to them the algorithm can stop earlier by skipping them. */
  BLI_bitmap *affected_vertex = BLI_BITMAP_NEW(totvert, "affected vertex");
  GSetIterator gs_iter;

  if (limit_radius == FLT_MAX) {
    /* In this case, no need to loop through all initial vertices to check distances as they are
     * all going to be affected. */
    BLI_bitmap_set_all(affected_vertex, true, totvert);
  }
  else {
    /* This is an O(n^2) loop used to limit the geodesic distance calculation to a radius. When
     * this optimization is needed, it is expected for the tool to request the distance to a low
     * number of vertices (usually just 1 or 2). */
    GSET_ITER (gs_iter, initial_vertices) {
      const int v = POINTER_AS_INT(BLI_gsetIterator_getKey(&gs_iter));
      SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, v);
      const float *v_co = cos ? cos[v] : SCULPT_vertex_co_get(ss, vertex);

      for (int i = 0; i < totvert; i++) {
        const float *v_co2 = cos ? cos[i] :
                                   SCULPT_vertex_co_get(
                                       ss, BKE_pbvh_table_index_to_vertex(ss->pbvh, i));
        if (len_squared_v3v3(v_co, v_co2) <= limit_radius_sq) {
          BLI_BITMAP_ENABLE(affected_vertex, i);
        }
      }
    }
  }

  SculptVertexNeighborIter ni;

  TempEdge *edges = NULL;
  BLI_array_declare(edges);
  GHash *ehash = BLI_ghash_ptr_new("geodesic multigrids ghash");

  MeshElemMap *vmap = MEM_calloc_arrayN(totvert, sizeof(*vmap), "geodesic grids vmap");

  int totedge = 0;
  MemArena *ma = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, "geodesic grids memarena");

  for (int i = 0; i < totvert; i++) {
    SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);
    MeshElemMap *map = vmap + i;

    int val = SCULPT_vertex_valence_get(ss, vertex);
    map->count = val;
    map->indices = BLI_memarena_alloc(ma, sizeof(int) * val);

    int j = 0;

    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
      void *ekey = hash_edge(i, ni.index, totvert);
      void **val;

      if (!BLI_ghash_ensure_p(ehash, ekey, &val)) {
        *val = POINTER_FROM_INT(totedge);

        TempEdge te = {i, ni.index};
        BLI_array_append(edges, te);
        totedge++;
      }

      map->indices[j] = POINTER_AS_INT(*val);
      j++;
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
  }

  int(*e_otherv_map)[4] = MEM_malloc_arrayN(totedge, sizeof(*e_otherv_map), "e_otherv_map");

  // create an edge map of opposite edge verts in (up to 2) adjacent faces
  for (int i = 0; i < totedge; i++) {
    int v1a = -1, v2a = -1;
    int v1b = -1, v2b = -1;

    TempEdge *te = edges + i;
    SculptVertexNeighborIter ni2;

    for (int j = 0; j < vmap[te->v1].count; j++) {
      TempEdge *te2 = edges + vmap[te->v1].indices[j];
      int v3 = te->v1 == te2->v1 ? te2->v2 : te2->v1;

      if (v3 == te->v2) {
        continue;
      }

      int p = find_quad(edges, vmap, te->v1, te->v2, v3);

      if (p != -1) {
        v1a = p;
        v1b = v3;
      }
    }

    for (int j = 0; j < vmap[te->v2].count; j++) {
      TempEdge *te2 = edges + vmap[te->v2].indices[j];
      int v3 = te->v2 == te2->v1 ? te2->v2 : te2->v1;

      if (v3 == te->v1) {
        continue;
      }

      int p = find_quad(edges, vmap, te->v1, te->v2, v3);

      if (p != -1) {
        if (v1a != -1) {
          v2a = p;
          v2b = v3;
        }
        else {
          v1a = p;
          v1b = v3;
        }
      }
    }

    e_otherv_map[i][0] = v1a;
    e_otherv_map[i][1] = v1b;
    e_otherv_map[i][2] = v2a;
    e_otherv_map[i][3] = v2b;
  }

  BLI_bitmap *edge_tag = BLI_BITMAP_NEW(totedge, "edge tag");

  /* Add edges adjacent to an initial vertex to the queue. */
  for (int i = 0; i < totedge; i++) {
    const int v1 = edges[i].v1;
    const int v2 = edges[i].v2;

    if (!BLI_BITMAP_TEST(affected_vertex, v1) && !BLI_BITMAP_TEST(affected_vertex, v2)) {
      continue;
    }
    if (dists[v1] != FLT_MAX || dists[v2] != FLT_MAX) {
      BLI_LINKSTACK_PUSH(queue, POINTER_FROM_INT(i));
    }
  }

  do {
    while (BLI_LINKSTACK_SIZE(queue)) {
      const int e = POINTER_AS_INT(BLI_LINKSTACK_POP(queue));
      int v1 = edges[e].v1;
      int v2 = edges[e].v2;

      if (dists[v1] == FLT_MAX || dists[v2] == FLT_MAX) {
        if (dists[v1] > dists[v2]) {
          SWAP(int, v1, v2);
        }
        sculpt_geodesic_grids_test_dist_add(ss,
                                            v2,
                                            v1,
                                            SCULPT_GEODESIC_VERTEX_NONE,
                                            dists,
                                            initial_vertices,
                                            r_closest_verts,
                                            cos);
      }

      TempEdge *te = edges + e;

      for (int pi = 0; pi < 4; pi++) {
        int v_other = e_otherv_map[e][pi];

        if (v_other == -1) {
          continue;
        }

        // XXX not sure how to handle face sets here - joeedh
        // if (ss->face_sets[poly] <= 0) {
        //  continue;
        //}

        if (sculpt_geodesic_grids_test_dist_add(
                ss, v_other, v1, v2, dists, initial_vertices, r_closest_verts, cos)) {
          for (int edge_map_index = 0; edge_map_index < vmap[v_other].count; edge_map_index++) {
            const int e_other = vmap[v_other].indices[edge_map_index];
            int ev_other;
            if (edges[e_other].v1 == (uint)v_other) {
              ev_other = edges[e_other].v2;
            }
            else {
              ev_other = edges[e_other].v1;
            }

            if (e_other != e && !BLI_BITMAP_TEST(edge_tag, e_other) &&
                (dists[ev_other] != FLT_MAX)) {
              if (BLI_BITMAP_TEST(affected_vertex, v_other) ||
                  BLI_BITMAP_TEST(affected_vertex, ev_other)) {
                BLI_BITMAP_ENABLE(edge_tag, e_other);
                BLI_LINKSTACK_PUSH(queue_next, POINTER_FROM_INT(e_other));
              }
            }
          }
        }
      }
    }

    for (LinkNode *lnk = queue_next; lnk; lnk = lnk->next) {
      const int e = POINTER_AS_INT(lnk->link);
      BLI_BITMAP_DISABLE(edge_tag, e);
    }

    BLI_LINKSTACK_SWAP(queue, queue_next);

  } while (BLI_LINKSTACK_SIZE(queue));

  BLI_LINKSTACK_FREE(queue);
  BLI_LINKSTACK_FREE(queue_next);
  MEM_SAFE_FREE(edge_tag);
  MEM_SAFE_FREE(affected_vertex);

  BLI_memarena_free(ma);
  BLI_ghash_free(ehash, NULL, NULL);
  MEM_SAFE_FREE(edges);
  MEM_SAFE_FREE(vmap);
  MEM_SAFE_FREE(e_otherv_map);

  return dists;
}

/* For sculpt mesh data that does not support a geodesic distances algorithm, fallback to the
 * distance to each vertex. In this case, only one of the initial vertices will be used to
 * calculate the distance. */
static float *SCULPT_geodesic_fallback_create(Object *ob, GSet *initial_vertices)
{

  SculptSession *ss = ob->sculpt;
  Mesh *mesh = BKE_object_get_original_mesh(ob);
  const int totvert = SCULPT_vertex_count_get(ss);
  float *dists = MEM_malloc_arrayN(totvert, sizeof(float), "distances");
  SculptVertRef first_affected = {SCULPT_GEODESIC_VERTEX_NONE};

  GSetIterator gs_iter;
  GSET_ITER (gs_iter, initial_vertices) {
    first_affected.i = POINTER_AS_INT(BLI_gsetIterator_getKey(&gs_iter));
    break;
  }

  if (first_affected.i == SCULPT_GEODESIC_VERTEX_NONE) {
    for (int i = 0; i < totvert; i++) {
      dists[i] = FLT_MAX;
    }
    return dists;
  }

  const float *first_affected_co = SCULPT_vertex_co_get(ss, first_affected);
  for (int i = 0; i < totvert; i++) {
    dists[i] = len_v3v3(first_affected_co,
                        SCULPT_vertex_co_get(ss, BKE_pbvh_table_index_to_vertex(ss->pbvh, i)));
  }

  return dists;
}

float *SCULPT_geodesic_distances_create(Object *ob,
                                        GSet *initial_vertices,
                                        const float limit_radius,
                                        SculptVertRef *r_closest_verts,
                                        float (*vertco_override)[3])
{
  SculptSession *ss = ob->sculpt;
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      return SCULPT_geodesic_mesh_create(
          ob, initial_vertices, limit_radius, r_closest_verts, vertco_override);
    case PBVH_BMESH:
      return SCULPT_geodesic_bmesh_create(
          ob, initial_vertices, limit_radius, r_closest_verts, vertco_override);
    case PBVH_GRIDS:
      return SCULPT_geodesic_grids_create(
          ob, initial_vertices, limit_radius, r_closest_verts, vertco_override);
      // return SCULPT_geodesic_fallback_create(ob, initial_vertices);
  }
  BLI_assert(false);
  return NULL;
}

float *SCULPT_geodesic_from_vertex_and_symm(Sculpt *sd,
                                            Object *ob,
                                            const SculptVertRef vertex,
                                            const float limit_radius)
{
  SculptSession *ss = ob->sculpt;
  GSet *initial_vertices = BLI_gset_int_new("initial_vertices");

  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  for (char i = 0; i <= symm; ++i) {
    if (SCULPT_is_symmetry_iteration_valid(i, symm)) {
      SculptVertRef v = {-1};

      if (i == 0) {
        v = vertex;
      }
      else {
        float location[3];
        flip_v3_v3(location, SCULPT_vertex_co_get(ss, vertex), i);
        v = SCULPT_nearest_vertex_get(sd, ob, location, FLT_MAX, false);
      }

      const int v_i = BKE_pbvh_vertex_index_to_table(ss->pbvh, v);

      if (v_i != -1) {
        BLI_gset_add(initial_vertices, POINTER_FROM_INT(v_i));
      }
    }
  }

  float *dists = SCULPT_geodesic_distances_create(ob, initial_vertices, limit_radius, NULL, NULL);
  BLI_gset_free(initial_vertices, NULL);
  return dists;
}

float *SCULPT_geodesic_from_vertex(Object *ob,
                                   const SculptVertRef vertex,
                                   const float limit_radius)
{
  SculptSession *ss = ob->sculpt;

  SCULPT_vertex_random_access_ensure(ss);

  GSet *initial_vertices = BLI_gset_int_new("initial_vertices");

  BLI_gset_add(initial_vertices,
               POINTER_FROM_INT(BKE_pbvh_vertex_index_to_table(ss->pbvh, vertex)));

  float *dists = SCULPT_geodesic_distances_create(ob, initial_vertices, limit_radius, NULL, NULL);
  BLI_gset_free(initial_vertices, NULL);
  return dists;
}
