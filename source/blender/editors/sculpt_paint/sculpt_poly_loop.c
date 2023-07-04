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
#include "BLI_hash.h"
#include "BLI_math.h"
#include "BLI_task.h"

#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_brush.h"
#include "BKE_ccg.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_mesh.h"
#include "BKE_mesh_fair.h"
#include "BKE_mesh_mapping.h"
#include "BKE_multires.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh_api.hh"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_view3d.h"
#include "paint_intern.h"
#include "sculpt_intern.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "bmesh.h"

#include <math.h>
#include <stdlib.h>

static void sculpt_poly_loop_topology_data_ensure(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = BKE_object_get_original_mesh(ob);

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
    const float(*vert_positions)[3] = BKE_mesh_vert_positions(mesh);

    BKE_mesh_vert_edge_map_create(&ss->vemap,
                                  &ss->vemap_mem,
                                  vert_positions,
                                  mesh->medge,
                                  mesh->totvert,
                                  mesh->totedge,
                                  false);
  }
}

#define SCULPT_FACE_SET_LOOP_STEP_NONE -1
static bool sculpt_poly_loop_step(SculptSession *ss,
                                  const int from_poly,
                                  const int edge,
                                  int *r_next_poly)
{
  if (!ss->epmap) {
    return false;
  }

  int next_poly = SCULPT_FACE_SET_LOOP_STEP_NONE;
  for (int i = 0; i < ss->epmap[edge].count; i++) {
    if (ss->epmap[edge].indices[i] != from_poly) {
      next_poly = ss->epmap[edge].indices[i];
    }
  }

  if (next_poly == SCULPT_FACE_SET_LOOP_STEP_NONE) {
    return false;
  }

  *r_next_poly = next_poly;
  return true;
}

static int sculpt_poly_loop_opposite_edge_in_quad(SculptSession *ss,
                                                  const int poly,
                                                  const int edge)
{
  if (ss->mpoly[poly].totloop != 4) {
    return edge;
  }

  int edge_index_in_poly = 0;
  for (int i = 0; i < ss->mpoly[poly].totloop; i++) {
    if (edge == ss->mloop[ss->mpoly[poly].loopstart + i].e) {
      edge_index_in_poly = i;
      break;
    }
  }

  const int next_edge_index_in_poly = (edge_index_in_poly + 2) % 4;
  return ss->mloop[ss->mpoly[poly].loopstart + next_edge_index_in_poly].e;
}

PBVHEdgeRef sculpt_poly_loop_initial_edge_from_cursor(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = BKE_object_get_original_mesh(ob);

  sculpt_poly_loop_topology_data_ensure(ob);

  float *location = ss->cursor_location;

  float(*vert_positions)[3] = SCULPT_mesh_deformed_positions_get(ss);
  MPoly *initial_poly = &mesh->mpoly[ss->active_face.i];

  if (initial_poly->totloop != 4) {
    return (PBVHEdgeRef){.i = PBVH_REF_NONE};
  }

  int closest_vert = mesh->mloop[initial_poly->loopstart].v;
  for (int i = 0; i < initial_poly->totloop; i++) {
    if (len_squared_v3v3(vert_positions[ss->mloop[initial_poly->loopstart + i].v], location) <
        len_squared_v3v3(vert_positions[closest_vert], location))
    {
      closest_vert = ss->mloop[initial_poly->loopstart + i].v;
    }
  }

  int initial_edge = ss->vemap[closest_vert].indices[0];
  int closest_vert_on_initial_edge = mesh->medge[initial_edge].v1 == closest_vert ?
                                         mesh->medge[initial_edge].v2 :
                                         mesh->medge[initial_edge].v1;
  for (int i = 0; i < ss->vemap[closest_vert].count; i++) {
    const int edge_index = ss->vemap[closest_vert].indices[i];
    const int other_vert = mesh->medge[edge_index].v1 == closest_vert ?
                               mesh->medge[edge_index].v2 :
                               mesh->medge[edge_index].v1;
    if (dist_to_line_segment_v3(
            location, vert_positions[closest_vert], vert_positions[other_vert]) <
        dist_to_line_segment_v3(
            location, vert_positions[closest_vert], vert_positions[closest_vert_on_initial_edge]))
    {
      initial_edge = edge_index;
      closest_vert_on_initial_edge = other_vert;
    }
  }
  return (PBVHEdgeRef){.i = initial_edge};
}

static void sculpt_poly_loop_iterate_and_fill(SculptSession *ss,
                                              const int initial_poly,
                                              const int initial_edge,
                                              BLI_bitmap *poly_loop)
{
  int current_poly = initial_poly;
  int current_edge = initial_edge;
  int next_poly = SCULPT_FACE_SET_LOOP_STEP_NONE;
  int max_steps = ss->totfaces;

  BLI_BITMAP_ENABLE(poly_loop, initial_poly);

  while (max_steps && sculpt_poly_loop_step(ss, current_poly, current_edge, &next_poly)) {
    if (ss->face_sets[next_poly] == initial_poly) {
      break;
    }
    if (ss->face_sets[next_poly] < 0) {
      break;
    }
    if (ss->mpoly[next_poly].totloop != 4) {
      break;
    }

    BLI_BITMAP_ENABLE(poly_loop, next_poly);
    current_edge = sculpt_poly_loop_opposite_edge_in_quad(ss, next_poly, current_edge);
    current_poly = next_poly;
    max_steps--;
  }
}

static void sculpt_poly_loop_symm_poly_find(Object *ob,
                                            const int poly_index,
                                            const int edge_index,
                                            const char symm_it,
                                            int *r_poly_index,
                                            int *r_edge_index)
{
  if (symm_it == 0) {
    *r_poly_index = poly_index;
    *r_edge_index = edge_index;
    return;
  }

  SculptSession *ss = ob->sculpt;
  Mesh *mesh = BKE_object_get_original_mesh(ob);
  float(*vert_positions)[3] = SCULPT_mesh_deformed_positions_get(ss);

  MPoly *original_poly = &mesh->mpoly[poly_index];
  float original_poly_center[3];
  BKE_mesh_calc_poly_center(
      original_poly, &mesh->mloop[original_poly->loopstart], vert_positions, original_poly_center);

  float symm_poly_center[3];
  flip_v3_v3(symm_poly_center, original_poly_center, symm_it);

  float min_poly_dist = FLT_MAX;
  int search_poly_index = poly_index;

  for (int i = 0; i < mesh->totpoly; i++) {
    MPoly *poly = &mesh->mpoly[i];
    float poly_center[3];
    BKE_mesh_calc_poly_center(poly, &mesh->mloop[poly->loopstart], vert_positions, poly_center);
    const float dist_to_poly_squared = len_squared_v3v3(symm_poly_center, poly_center);
    if (dist_to_poly_squared < min_poly_dist) {
      min_poly_dist = dist_to_poly_squared;
      search_poly_index = i;
    }
  }

  *r_poly_index = search_poly_index;
  MPoly *search_poly = &mesh->mpoly[search_poly_index];

  float original_edge_center[3];
  MEdge *original_edge = &mesh->medge[edge_index];
  mid_v3_v3v3(
      original_edge_center, vert_positions[original_edge->v1], vert_positions[original_edge->v2]);

  float symm_edge_center[3];
  flip_v3_v3(symm_edge_center, original_edge_center, symm_it);

  float min_edge_dist = FLT_MAX;
  int search_edge_index = edge_index;

  for (int i = 0; i < search_poly->totloop; i++) {
    MLoop *loop = &mesh->mloop[search_poly->loopstart + i];
    MEdge *edge = &mesh->medge[loop->e];
    float edge_center[3];
    mid_v3_v3v3(edge_center, vert_positions[edge->v1], vert_positions[edge->v2]);
    const float dist_to_edge_squared = len_squared_v3v3(symm_edge_center, edge_center);
    if (dist_to_edge_squared < min_edge_dist) {
      min_edge_dist = dist_to_edge_squared;
      search_edge_index = loop->e;
    }

    *r_edge_index = search_edge_index;
  }
}

BLI_bitmap *sculpt_poly_loop_from_cursor(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = BKE_object_get_original_mesh(ob);
  BLI_bitmap *poly_loop = BLI_BITMAP_NEW(mesh->totpoly, "poly loop");

  sculpt_poly_loop_topology_data_ensure(ob);
  const PBVHEdgeRef initial_edge = sculpt_poly_loop_initial_edge_from_cursor(ob);
  const PBVHFaceRef initial_poly = ss->active_face;

  const int initial_edge_i = BKE_pbvh_edge_to_index(ss->pbvh, initial_edge);
  const int initial_poly_i = BKE_pbvh_face_to_index(ss->pbvh, initial_poly);

  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  for (char symm_it = 0; symm_it <= symm; symm_it++) {
    if (!SCULPT_is_symmetry_iteration_valid(symm_it, symm)) {
      continue;
    }

    int initial_poly_symm;
    int initial_edge_symm;
    sculpt_poly_loop_symm_poly_find(
        ob, initial_poly_i, initial_edge_i, symm_it, &initial_poly_symm, &initial_edge_symm);

    const int initial_edge_opposite = sculpt_poly_loop_opposite_edge_in_quad(
        ss, initial_poly_symm, initial_edge_symm);

    sculpt_poly_loop_iterate_and_fill(ss, initial_poly_symm, initial_edge_symm, poly_loop);
    sculpt_poly_loop_iterate_and_fill(ss, initial_poly_symm, initial_edge_opposite, poly_loop);
  }

  return poly_loop;
}
