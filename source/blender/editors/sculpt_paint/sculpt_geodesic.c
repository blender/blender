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

  BLI_LINKSTACK_DECLARE(queue, int);
  BLI_LINKSTACK_DECLARE(queue_next, int);

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

  BLI_bitmap *affected_vertex = BLI_BITMAP_NEW(totvert, "affected vertex");
  GSetIterator gs_iter;
  GSET_ITER (gs_iter, initial_vertices) {
    const int v = POINTER_AS_INT(BLI_gsetIterator_getKey(&gs_iter));
    float *v_co = verts[v].co;
    for (int i = 0; i < totvert; i++) {
      if (len_squared_v3v3(v_co, verts[i].co) <= limit_radius_sq) {
        BLI_BITMAP_ENABLE(affected_vertex, i);
      }
    }
  }

  for (int i = 0; i < totedge; i++) {
    const int v1 = edges[i].v1;
    const int v2 = edges[i].v2;
    if (!BLI_BITMAP_TEST(affected_vertex, v1) && !BLI_BITMAP_TEST(affected_vertex, v2)) {
      continue;
    }
    if (dists[v1] != FLT_MAX || dists[v2] != FLT_MAX) {
      BLI_LINKSTACK_PUSH(queue, i);
    }
  }

  do {
    int e;
    while (e = BLI_LINKSTACK_POP(queue)) {
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
        for (int pi = 0; pi < ss->epmap[e].count; pi++) {
          const int poly = ss->epmap[e].indices[pi];
          if (ss->face_sets[poly] <= 0) {
            continue;
          }
          const MPoly *mpoly = &mesh->mpoly[poly];

          for (int li = 0; li < mpoly->totloop; li++) {
            const MLoop *mloop = &mesh->mloop[li + mpoly->loopstart];
            const int v_other = mloop->v;
            if (ELEM(v_other, v1, v2)) {
              continue;
            }
            if (sculpt_geodesic_mesh_test_dist_add(
                    verts, v_other, v1, v2, dists, initial_vertices)) {
              for (int ei = 0; ei < ss->vemap[v_other].count; ei++) {
                const int e_other = ss->vemap[v_other].indices[ei];
                int ev_other;
                if (edges[e_other].v1 == v_other) {
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
                    BLI_LINKSTACK_PUSH(queue_next, e_other);
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

static float *SCULPT_geodesic_fallback_create(Object *ob, GSet *initial_vertices)
{

  SculptSession *ss = ob->sculpt;
  Mesh *mesh = BKE_object_get_original_mesh(ob);
  const int totvert = mesh->totvert;
  float *dists = MEM_malloc_arrayN(totvert, sizeof(float), "distances");
  int first_affected = SCULPT_GEODESIC_VERTEX_NONE;
  GSetIterator gs_iter;
  GSET_ITER (gs_iter, initial_vertices) {
    first_affected = POINTER_AS_INT(BLI_gsetIterator_getKey(&gs_iter));
    break;
  }
  if (first_affected == SCULPT_GEODESIC_VERTEX_NONE) {
    return dists;
  }

  const float *first_affected_co = SCULPT_vertex_co_get(ss, first_affected);
  for (int i = 0; i < totvert; i++) {
    dists[i] = len_v3v3(first_affected_co, SCULPT_vertex_co_get(ss, i));
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
    case PBVH_GRIDS:
      return SCULPT_geodesic_fallback_create(ob, initial_vertices);
  }
  BLI_assert(false);
  return NULL;
}

float *SCULPT_geodesic_from_vertex_and_symm(Sculpt *sd,
                                            Object *ob,
                                            const int vertex,
                                            const float limit_radius)
{
  SculptSession *ss = ob->sculpt;
  GSet *initial_vertices = BLI_gset_int_new("initial_vertices");

  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  for (char i = 0; i <= symm; ++i) {
    if (SCULPT_is_symmetry_iteration_valid(i, symm)) {
      int v = -1;
      if (i == 0) {
        v = vertex;
      }
      else {
        float location[3];
        flip_v3_v3(location, SCULPT_vertex_co_get(ss, vertex), i);
        v = SCULPT_nearest_vertex_get(sd, ob, location, FLT_MAX, false);
      }
      if (v != -1) {
        BLI_gset_add(initial_vertices, POINTER_FROM_INT(v));
      }
    }
  }

  float *dists = SCULPT_geodesic_distances_create(ob, initial_vertices, limit_radius);
  BLI_gset_free(initial_vertices, NULL);
  return dists;
}

float *SCULPT_geodesic_from_vertex(Object *ob, const int vertex, const float limit_radius)
{
  GSet *initial_vertices = BLI_gset_int_new("initial_vertices");
  BLI_gset_add(initial_vertices, POINTER_FROM_INT(vertex));
  float *dists = SCULPT_geodesic_distances_create(ob, initial_vertices, limit_radius);
  BLI_gset_free(initial_vertices, NULL);
  return dists;
}
