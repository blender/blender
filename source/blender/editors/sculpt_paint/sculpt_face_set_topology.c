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
static bool sculpt_face_set_loop_step(SculptSession *ss, const int from_poly, const int edge, int *r_next_poly) {
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
}

static int sculpt_face_set_loop_opposite_edge_in_quad(SculptSession *ss, const int poly, const int edge) {
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

static void sculpt_face_set_by_topology_poly_loop(Object *ob, const int next_face_set_id) {
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = BKE_object_get_original_mesh(ob);


  MVert *mvert = SCULPT_mesh_deformed_mverts_get(ss);
  MPoly *initial_poly = &mesh->mpoly[ss->active_face_index];

  if (initial_poly->totloop != 4) {
    return;
  }

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

  int closest_vert_index = mesh->mloop[initial_poly->loopstart].v;
  for (int i = 0; i < initial_poly->totloop; i++) {
    if (len_squared_v3v3(mvert[initial_poly->loopstart + i].co, ss->cursor_location) < len_squared_v3v3(mvert[closest_vert_index].co, ss->cursor_location)) {
      closest_vert_index = initial_poly->loopstart + i;
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

  ss->face_sets[ss->active_face_index] = next_face_set_id;

  int current_poly = ss->active_face_index;
  int current_edge = initial_edge_index;
  int next_poly = SCULPT_FACE_SET_LOOP_STEP_NONE;
  while(sculpt_face_set_loop_step(ss, current_poly, current_edge, &next_poly)) {
    if (ss->face_sets[next_poly] == next_face_set_id) {
      break;
    }
    if (ss->face_sets[next_poly] < 0) {
      break;
    }
    if (ss->mpoly[next_poly].totloop != 4) {
      break;
    }

    ss->face_sets[next_poly] = next_face_set_id;
    current_edge = sculpt_face_set_loop_opposite_edge_in_quad(ss, next_poly, current_edge);
    current_poly = next_poly;
  }
}

static int sculpt_face_set_by_topology_invok(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  const int mode = RNA_enum_get(op->ptr, "mode");
  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, false, false);

  /* Update the current active Face Set and Vertex as the operator can be used directly from the
   * tool without brush cursor. */
  SculptCursorGeometryInfo sgi;
  const float mouse[2] = {event->mval[0], event->mval[1]};
  if (!SCULPT_cursor_geometry_info_update(C, &sgi, mouse, false, false)) {
    /* The cursor is not over the mesh. Cancel to avoid editing the last updated Face Set ID. */
    return OPERATOR_CANCELLED;
  }

  PBVHNode **nodes;
  int totnode;
  BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);
  SCULPT_undo_push_begin(ob, "face set edit");
  SCULPT_undo_push_node(ob, nodes[0], SCULPT_UNDO_FACE_SETS);
  MEM_freeN(nodes);

  Mesh *mesh = BKE_object_get_original_mesh(ob);
  const int next_face_set = ED_sculpt_face_sets_find_next_available_id(mesh);

  switch (mode) {
    case SCULPT_FACE_SET_TOPOLOGY_LOOSE_PART:
      break;

    case SCULPT_FACE_SET_TOPOLOGY_POLY_LOOP:
      sculpt_face_set_by_topology_poly_loop(ob, next_face_set);
      break;
  }

  SCULPT_undo_push_end();
  SCULPT_tag_update_overlays(C);

  return OPERATOR_FINISHED;
}

void SCULPT_OT_face_set_by_topology(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Face Set by Topology";
  ot->idname = "SCULPT_OT_face_set_by_topology";
  ot->description = "Create a new Face Set following the mesh topology";

  /* Api callbacks. */
  ot->invoke = sculpt_face_set_by_topology_invoke;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(
      ot->srna, "mode", prop_sculpt_face_set_by_topology, SCULPT_FACE_SET_TOPOLOGY_POLY_LOOP, "Mode", "");
}
