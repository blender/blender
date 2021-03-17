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
#include "BKE_pbvh.h"
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

typedef enum eSculptFaceSetByTopologyMode {
  SCULPT_FACE_SET_TOPOLOGY_LOOSE_PART = 0,
  SCULPT_FACE_SET_TOPOLOGY_POLY_LOOP = 1,
};


static EnumPropertyItem prop_sculpt_face_set_by_topology[] = {
    {
        SCULPT_FACE_SET_TOPOLOGY_LOOSE_PART,
        "LOOSE_PART",
        0,
        "Loose Part",
        "",
    },
    {
        SCULPT_FACE_SET_TOPOLOGY_POLY_LOOP,
        "POLY_LOOP",
        0,
        "Face Loop",
        "",
    },
    {0, NULL, 0, NULL, NULL},
};




#define SCULPT_FACE_SET_LOOP_STEP_NONE -1
static bool sculpt_poly_loop_step(SculptSession *ss, const int from_poly, const int edge, int *r_next_poly) {
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

static int sculpt_poly_loop_opposite_edge_in_quad(SculptSession *ss, const int poly, const int edge) {
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

static int sculpt_poly_loop_initial_edge_from_cursor(Object *ob) {
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = BKE_object_get_original_mesh(ob);

  MVert *mvert = SCULPT_mesh_deformed_mverts_get(ss);
  MPoly *initial_poly = &mesh->mpoly[ss->active_face_index];

  if (initial_poly->totloop != 4) {
    return 0;
  }

  int closest_vert_index = mesh->mloop[initial_poly->loopstart].v;
  for (int i = 0; i < initial_poly->totloop; i++) {
    if (len_squared_v3v3(mvert[ss->mloop[initial_poly->loopstart + i].v].co, ss->cursor_location) < len_squared_v3v3(mvert[closest_vert_index].co, ss->cursor_location)) {
      closest_vert_index = ss->mloop[initial_poly->loopstart + i].v;
    }
  }

  int initial_edge_index = ss->vemap[closest_vert_index].indices[0];
  int closest_vert_on_initial_edge_index = mesh->medge[initial_edge_index].v1 == closest_vert_index ? mesh->medge[initial_edge_index].v2 : mesh->medge[initial_edge_index].v1;
  for (int i = 0; i < ss->vemap[closest_vert_index].count; i++) {
    const int edge_index = ss->vemap[closest_vert_index].indices[i];
    const int other_vert = mesh->medge[edge_index].v1 == closest_vert_index ? mesh->medge[edge_index].v2 : mesh->medge[edge_index].v1;
    if (len_squared_v3v3(mvert[other_vert].co, ss->cursor_location) < len_squared_v3v3(mvert[closest_vert_on_initial_edge_index].co, ss->cursor_location)) {
      initial_edge_index = edge_index;
      closest_vert_on_initial_edge_index = other_vert;
    }
  }
  printf("CLOSEST VERT INDEX %d\n", closest_vert_index);
  printf("INITIAL EDGE INDEX %d\n", initial_edge_index);
  return initial_edge_index;
}

static void sculpt_poly_loop_topology_data_ensure(Object *ob) {
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
    BKE_mesh_vert_edge_map_create(
        &ss->vemap, &ss->vemap_mem, mesh->medge, mesh->totvert, mesh->totedge);
  }
}

static void sculpt_poly_loop_iterate_and_fill(SculptSession *ss, const int initial_poly, const int initial_edge, BLI_bitmap *poly_loop) {
  int current_poly = initial_poly;
  int current_edge = initial_edge;
  int next_poly = SCULPT_FACE_SET_LOOP_STEP_NONE;
  int max_steps = ss->totfaces;

  BLI_BITMAP_ENABLE(poly_loop, initial_poly);

  while(max_steps && sculpt_poly_loop_step(ss, current_poly, current_edge, &next_poly)) {
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

BLI_bitmap * sculpt_poly_loop_from_cursor(Object *ob) {
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = BKE_object_get_original_mesh(ob);
  BLI_bitmap *poly_loop = BLI_BITMAP_NEW(mesh->totpoly, "poly loop");

  sculpt_poly_loop_topology_data_ensure(ob);
  const int initial_edge = sculpt_poly_loop_initial_edge_from_cursor(ob);
  const int initial_poly = ss->active_face_index;
  const int initial_edge_opposite = sculpt_poly_loop_opposite_edge_in_quad(ss, initial_poly, initial_edge);
  sculpt_poly_loop_iterate_and_fill(ss, initial_poly, initial_edge, poly_loop);
  sculpt_poly_loop_iterate_and_fill(ss, initial_poly, initial_edge_opposite, poly_loop);

  return poly_loop;
}
