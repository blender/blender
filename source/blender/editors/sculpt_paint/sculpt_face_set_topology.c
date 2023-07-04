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

typedef enum eSculptFaceSetByTopologyMode {
  SCULPT_FACE_SET_TOPOLOGY_LOOSE_PART = 0,
  SCULPT_FACE_SET_TOPOLOGY_POLY_LOOP = 1,
} eSculptFaceSetByTopologyMode;

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

static void sculpt_face_set_by_topology_poly_loop(Object *ob, const int next_face_set_id)
{
  SculptSession *ss = ob->sculpt;
  BLI_bitmap *poly_loop = sculpt_poly_loop_from_cursor(ob);
  for (int i = 0; i < ss->totfaces; i++) {
    if (BLI_BITMAP_TEST(poly_loop, i)) {
      ss->face_sets[i] = next_face_set_id;
    }
  }
  MEM_freeN(poly_loop);
}

static int sculpt_face_set_by_topology_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  const int mode = RNA_enum_get(op->ptr, "mode");
  const bool repeat_previous = RNA_boolean_get(op->ptr, "repeat_previous");
  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, false, false);
  printf("FACE SET TOPOLOGY\n");

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
  SCULPT_undo_push_begin(ob, op);
  SCULPT_undo_push_node(ob, nodes[0], SCULPT_UNDO_FACE_SETS);

  const PBVHFaceRef initial_poly = ss->active_face;
  const PBVHEdgeRef initial_edge = sculpt_poly_loop_initial_edge_from_cursor(ob);

  Mesh *mesh = BKE_object_get_original_mesh(ob);
  int new_face_set = SCULPT_FACE_SET_NONE;

  if (repeat_previous && ss->face_set_last_created != SCULPT_FACE_SET_NONE &&
      initial_poly.i != ss->face_set_last_poly.i && initial_edge.i != ss->face_set_last_edge.i)
  {
    new_face_set = ss->face_set_last_created;
  }
  else {
    new_face_set = ED_sculpt_face_sets_find_next_available_id(mesh);
  }

  switch (mode) {
    case SCULPT_FACE_SET_TOPOLOGY_LOOSE_PART:
      break;
    case SCULPT_FACE_SET_TOPOLOGY_POLY_LOOP:
      sculpt_face_set_by_topology_poly_loop(ob, new_face_set);
      break;
  }

  ss->face_set_last_created = new_face_set;
  ss->face_set_last_edge = initial_edge;
  ss->face_set_last_poly = initial_poly;

  /* Sync face sets visibility and vertex visibility as now all Face Sets are visible. */
  // SCULPT_visibility_sync_all_face_sets_to_vertices(ob);

  for (int i = 0; i < totnode; i++) {
    BKE_pbvh_vert_tag_update_normal_visibility(nodes[i]);
  }

  BKE_pbvh_update_vertex_data(ss->pbvh, PBVH_UpdateVisibility);

  if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES) {
    BKE_mesh_flush_hidden_from_verts(ob->data);
  }

  MEM_freeN(nodes);

  SCULPT_undo_push_end(ob);
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

  RNA_def_enum(ot->srna,
               "mode",
               prop_sculpt_face_set_by_topology,
               SCULPT_FACE_SET_TOPOLOGY_POLY_LOOP,
               "Mode",
               "");

  RNA_def_boolean(ot->srna,
                  "repeat_previous",
                  true,
                  "Repeat previous Face Set",
                  "Repeat the latest created Face Set instead of a new one");
}
