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

#include "BLI_blenlib.h"
#include "BLI_linklist_stack.h"
#include "BLI_math.h"
#include "BLI_task.h"

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

/*

on factor;
off period;

load_package "avector";

comment: if (dist1 != 0.0f && dist2 != 0.0f);

forall x let abs(abs(x)*abs(x)) = x**2;
forall x let abs(x*x) = x**2;
forall x let abs(abs(x)) = abs(x);
forall x let abs(x)**2 = x**2;

let v2len**2 = v2lensqr;
let abs(v2len) = v2len;
let sqrt(the_hh) = the_hh_sqrt;

v0 := avec(v0x, v0y, v0z);
v1 := avec(0, 0, 0); comment: avec(v1x, v1y, v1z);
v2 := avec(v2x, v2y, v2z);

let v2x**2 + v2y**2 + v2z**2 = v2lensqr;
let sqrt(v2lensqr) = v2len;

v10 := v0; comment: v0 - v1;
v12 := v2; comment: v2 - v1;

d12 := v2len;

comment: if d12*d12 > 0.0;

u := v12 / d12;

n := v12 cross v10;
n = n / VMOD n;
v := n cross u;

v0_0 := v10 dot u;
v0_1 := abs(v10 dot v);

a := 0.5 * (1.0 + (dist1 * dist1 - dist2 * dist2) / (d12 * d12));

hh := dist1 * dist1 - a * a * d12 * d12;

commment: if (hh > 0.0f);

h := the_hh**0.5;

S_0 := a*d12;
S_1 := -h;

x_intercept := S_0 + h * (v0_0 - S_0) / (v0_1 + h);

comment: if (x_intercept >= 0.0f && x_intercept <= d12);

on rounded;
on double;
on period;

result := ((S_0-v0_0)**2 + (S_1 - v0_1)**2)**0.5;

load_package "gentran";

gentranlang!* := 'c$

on factor;

gentran begin outhh := eval(hh); outxintercept := eval(x_intercept); outresult := eval(result) end;

*/
static float fast_geodesic_distance_propagate_across_triangle(
    const float v0[3], const float v1[3], const float v2[3], const float dist1, const float dist2)
{
  float the_hh, the_hh_sqrt;
  float v0x = v0[0] - v1[0];
  float v0y = v0[1] - v1[1];
  float v0z = v0[2] - v1[2];

  float v2x = v2[0] - v1[0];
  float v2y = v2[1] - v1[1];
  float v2z = v2[2] - v1[2];

  if (dist1 != 0.0f && dist2 != 0.0f) {
    float v2lensqr = (v2x * v2x + v2y * v2y + v2z * v2z);
    float xintercept;

    if (v2lensqr > 1.0e-35f) {
      float v2len = sqrtf(v2lensqr);

      the_hh = -(0.25 * (dist2 + v2len + dist1) * (dist2 + v2len - dist1) *
                 (dist2 - v2len + dist1) * (dist2 - v2len - dist1)) /
               v2lensqr;

      if (the_hh > 0.0f) {
        the_hh_sqrt = sqrtf(the_hh);

        xintercept =
            -(0.5 * (dist2 * dist2 - v2lensqr - (dist1 * dist1)) *
                  fabsf((double)(((v2lensqr - (v2z * v2z)) * v0z - (2 * v0y * v2y * v2z)) * v0z +
                                 (v2lensqr - (v2y * v2y)) * (v0y * v0y) -
                                 ((2 * (v0y * v2y + v0z * v2z) * v2x -
                                   ((v2y * v2y + v2z * v2z) * v0x)) *
                                  v0x))) -
              ((v0y * v2y + v0z * v2z + v0x * v2x) * the_hh_sqrt * v2len)) /
            ((fabsf(
                  (double)(((v2lensqr - (v2z * v2z)) * v0z - (2 * v0y * v2y * v2z)) * v0z +
                           (v2lensqr - (v2y * v2y)) * (v0y * v0y) -
                           ((2 * (v0y * v2y + v0z * v2z) * v2x - ((v2y * v2y + v2z * v2z) * v0x)) *
                            v0x))) +
              the_hh_sqrt * v2len) *
             v2len);

        if (xintercept >= 0.0 && xintercept <= v2len) {
          float result =
              (0.5 * sqrt((double)((2.0 * (v0y * v2y + v0z * v2z + v0x * v2x) - (dist1 * dist1) +
                                    (dist2 + v2len) * (dist2 - v2len)) *
                                       (2.0 * (v0y * v2y + v0z * v2z + v0x * v2x) -
                                        (dist1 * dist1) + (dist2 + v2len) * (dist2 - v2len)) +
                                   4.0 * ((fabsf((double)(((v2lensqr - (v2z * v2z)) * v0z -
                                                           (2 * v0y * v2y * v2z)) *
                                                              v0z +
                                                          (v2lensqr - (v2y * v2y)) * (v0y * v0y) -
                                                          ((2 * (v0y * v2y + v0z * v2z) * v2x -
                                                            ((v2y * v2y + v2z * v2z) * v0x)) *
                                                           v0x))) +
                                           the_hh_sqrt * v2len) *
                                          (fabsf((double)(((v2lensqr - (v2z * v2z)) * v0z -
                                                           (2 * v0y * v2y * v2z)) *
                                                              v0z +
                                                          (v2lensqr - (v2y * v2y)) * (v0y * v0y) -
                                                          ((2 * (v0y * v2y + v0z * v2z) * v2x -
                                                            ((v2y * v2y + v2z * v2z) * v0x)) *
                                                           v0x))) +
                                           the_hh_sqrt * v2len))))) /
              v2len;
          /*
          printf("%.7f : %.7f\n",
                 result*2.0,
                 geodesic_distance_propagate_across_triangle(v0, v1, v2, dist1, dist2));*/
          return result * 4.0;
        }
      }
    }
  }

  /* Fall back to Dijsktra approximation in trivial case, or if no valid source
   * point found that connects to v0 across the triangle. */
  return min_ff(dist1 + sqrtf(v0x * v0x + v0y * v0y + v0z * v0z), dist2 + len_v3v3(v0, v2));
}

#if 0
float geodesic_distance_propagate_across_triangle(
    const float v0[3], const float v1[3], const float v2[3], const float dist1, const float dist2)
{
  /* Vectors along triangle edges. */
  float v10[3], v12[3];
  sub_v3_v3v3(v10, v0, v1);
  sub_v3_v3v3(v12, v2, v1);

  if (dist1 != 0.0f && dist2 != 0.0f) {
    /* Local coordinate system in the triangle plane. */
    float u[3], v[3], n[3];
    const float d12 = normalize_v3_v3(u, v12);

    if (d12 * d12 > 0.0f) {
      cross_v3_v3v3(n, v12, v10);
      normalize_v3(n);
      cross_v3_v3v3(v, n, u);

      /* v0 in local coordinates */
      const float v0_[2] = {dot_v3v3(v10, u), fabsf(dot_v3v3(v10, v))};

      /* Compute virtual source point in local coordinates, that we estimate the geodesic
       * distance is being computed from. See figure 9 in the paper for the derivation. */
      const float a = 0.5f * (1.0f + (dist1 * dist1 - dist2 * dist2) / (d12 * d12));
      const float hh = dist1 * dist1 - a * a * d12 * d12;

      if (hh > 0.0f) {
        const float h = sqrtf(hh);
        const float S_[2] = {a * d12, -h};

        /* Only valid if the line between the source point and v0 crosses
         * the edge between v1 and v2. */
        const float x_intercept = S_[0] + h * (v0_[0] - S_[0]) / (v0_[1] + h);
        if (x_intercept >= 0.0f && x_intercept <= d12) {
          return len_v2v2(S_, v0_);
        }
      }
    }
  }

  /* Fall back to Dijsktra approximation in trivial case, or if no valid source
   * point found that connects to v0 across the triangle. */
  return min_ff(dist1 + len_v3(v10), dist2 + len_v3v3(v0, v2));
}
#endif

/* Propagate distance from v1 and v2 to v0. */
static bool sculpt_geodesic_mesh_test_dist_add(
    MVert *mvert, const int v0, const int v1, const int v2, float *dists, GSet *initial_vertices)
{
  if (BLI_gset_haskey(initial_vertices, POINTER_FROM_INT(v0))) {
    return false;
  }

  BLI_assert(dists[v1] != FLT_MAX);
  if (dists[v0] <= dists[v1]) {
    return false;
  }

  float dist0;
  if (v2 != SCULPT_GEODESIC_VERTEX_NONE) {
    BLI_assert(dists[v2] != FLT_MAX);
    if (dists[v0] <= dists[v2]) {
      return false;
    }
    dist0 = geodesic_distance_propagate_across_triangle(
        mvert[v0].co, mvert[v1].co, mvert[v2].co, dists[v1], dists[v2]);
  }
  else {
    float vec[3];
    sub_v3_v3v3(vec, mvert[v1].co, mvert[v0].co);
    dist0 = dists[v1] + len_v3(vec);
  }

  if (dist0 < dists[v0]) {
    dists[v0] = dist0;
    return true;
  }

  return false;
}

#define BMESH_INITIAL_VERT_TAG BM_ELEM_TAG_ALT

static bool sculpt_geodesic_mesh_test_dist_add_bmesh(
    BMVert *v0, BMVert *v1, BMVert *v2, float *dists, GSet *initial_vertices)
{
  const int v0_i = BM_elem_index_get(v0);
  const int v1_i = BM_elem_index_get(v1);
  const int v2_i = v2 ? BM_elem_index_get(v2) : SCULPT_GEODESIC_VERTEX_NONE;

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
        v0->co, v1->co, v2->co, dists[v1_i], dists[v2_i]);
  }
  else {
    float vec[3];
    sub_v3_v3v3(vec, v1->co, v0->co);
    dist0 = dists[v1_i] + len_v3(vec);
  }

  if (dist0 < dists[v0_i]) {
    dists[v0_i] = dist0;
    return true;
  }

  return false;
}

static float *SCULPT_geodesic_mesh_create(Object *ob,
                                          GSet *initial_vertices,
                                          const float limit_radius)
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
        &ss->vemap, &ss->vemap_mem, mesh->medge, mesh->totvert, mesh->totedge);
  }

  /* Both contain edge indices encoded as *void. */
  BLI_LINKSTACK_DECLARE(queue, void *);
  BLI_LINKSTACK_DECLARE(queue_next, void *);

  BLI_LINKSTACK_INIT(queue);
  BLI_LINKSTACK_INIT(queue_next);

  for (int i = 0; i < totvert; i++) {
    if (BLI_gset_haskey(initial_vertices, POINTER_FROM_INT(i))) {
      dists[i] = 0.0f;
    }
    else {
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
      float *v_co = verts[v].co;
      for (int i = 0; i < totvert; i++) {
        if (len_squared_v3v3(v_co, verts[i].co) <= limit_radius_sq) {
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
        sculpt_geodesic_mesh_test_dist_add(
            verts, v2, v1, SCULPT_GEODESIC_VERTEX_NONE, dists, initial_vertices);
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
                    verts, v_other, v1, v2, dists, initial_vertices)) {
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
                                           const float limit_radius)
{
  SculptSession *ss = ob->sculpt;

  if (!ss->bm) {
    return NULL;
  }

  const int totvert = ss->bm->totvert;
  const int totedge = ss->bm->totedge;

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
    }
    else {
      dists[i] = FLT_MAX;
    }
  }

  /* Masks vertices that are further than limit radius from an initial vertex. As there is no need
   * to define a distance to them the algorithm can stop earlier by skipping them. */
  BLI_bitmap *affected_vertex = BLI_BITMAP_NEW(totvert, "affected vertex");
  GSetIterator gs_iter;

  SCULPT_vertex_random_access_ensure(ss);

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

      BM_elem_flag_enable(v, BMESH_INITIAL_VERT_TAG);

      for (int i = 0; i < totvert; i++) {
        BMVert *v2 = (BMVert *)BKE_pbvh_table_index_to_vertex(ss->pbvh, i).i;

        if (len_squared_v3v3(v->co, v2->co) <= limit_radius_sq) {
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
        sculpt_geodesic_mesh_test_dist_add_bmesh(v2, v1, NULL, dists, initial_vertices);
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
                    v_other, v1, v2, dists, initial_vertices)) {
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

/* For sculpt mesh data that does not support a geodesic distances algorithm, fallback to the
 * distance to each vertex. In this case, only one of the initial vertices will be used to
 * calculate the distance. */
static float *SCULPT_geodesic_fallback_create(Object *ob, GSet *initial_vertices)
{

  SculptSession *ss = ob->sculpt;
  Mesh *mesh = BKE_object_get_original_mesh(ob);
  const int totvert = mesh->totvert;
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
                                        const float limit_radius)
{
  SculptSession *ss = ob->sculpt;
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      return SCULPT_geodesic_mesh_create(ob, initial_vertices, limit_radius);
    case PBVH_BMESH:
      return SCULPT_geodesic_bmesh_create(ob, initial_vertices, limit_radius);
    case PBVH_GRIDS:
      return SCULPT_geodesic_fallback_create(ob, initial_vertices);
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

  float *dists = SCULPT_geodesic_distances_create(ob, initial_vertices, limit_radius);
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

  float *dists = SCULPT_geodesic_distances_create(ob, initial_vertices, limit_radius);
  BLI_gset_free(initial_vertices, NULL);
  return dists;
}
