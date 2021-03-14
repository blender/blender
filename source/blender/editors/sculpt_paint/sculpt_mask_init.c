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
 * The Original Code is Copyright (C) 2021 Blender Foundation.
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

#include "BLT_translation.h"

#include "PIL_time.h"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_brush.h"
#include "BKE_ccg.h"
#include "BKE_context.h"
#include "BKE_mesh.h"
#include "BKE_multires.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_sculpt.h"
#include "paint_intern.h"
#include "sculpt_intern.h"

#include "bmesh.h"

#include <math.h>
#include <stdlib.h>

/* Mask Init operator. */
/* Initializes mask values for the entire mesh depending on the mode. */

typedef enum eSculptMaskInitMode {
  SCULPT_MASK_INIT_RANDOM_PER_VERTEX,
  SCULPT_MASK_INIT_RANDOM_PER_FACE_SET,
  SCULPT_MASK_INIT_RANDOM_PER_LOOSE_PART,
} eSculptMaskInitMode;

static EnumPropertyItem prop_sculpt_mask_init_mode_types[] = {
    {
        SCULPT_MASK_INIT_RANDOM_PER_VERTEX,
        "RANDOM_PER_VERTEX",
        0,
        "Random per Vertex",
        "",
    },
    {
        SCULPT_MASK_INIT_RANDOM_PER_FACE_SET,
        "RANDOM_PER_FACE_SET",
        0,
        "Random per Face Set",
        "",
    },
    {
        SCULPT_MASK_INIT_RANDOM_PER_LOOSE_PART,
        "RANDOM_PER_LOOSE_PART",
        0,
        "Random per Loose Part",
        "",
    },
    {0, NULL, 0, NULL, NULL},
};

static void mask_init_task_cb(void *__restrict userdata,
                              const int i,
                              const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  PBVHNode *node = data->nodes[i];
  PBVHVertexIter vd;
  const int mode = data->mask_init_mode;
  const int seed = data->mask_init_seed;
  SCULPT_undo_push_node(data->ob, node, SCULPT_UNDO_MASK);
  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    switch (mode) {
      case SCULPT_MASK_INIT_RANDOM_PER_VERTEX:
        *vd.mask = BLI_hash_int_01(vd.index + seed);
        break;
      case SCULPT_MASK_INIT_RANDOM_PER_FACE_SET: {
        const int face_set = SCULPT_vertex_face_set_get(ss, vd.index);
        *vd.mask = BLI_hash_int_01(face_set + seed);
        break;
      }
      case SCULPT_MASK_INIT_RANDOM_PER_LOOSE_PART:
        *vd.mask = BLI_hash_int_01(ss->vertex_info.connected_component[vd.index] + seed);
        break;
    }
  }
  BKE_pbvh_vertex_iter_end;
  BKE_pbvh_node_mark_update_mask(data->nodes[i]);
}

static int sculpt_mask_init_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  const int mode = RNA_enum_get(op->ptr, "mode");

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, false);

  PBVH *pbvh = ob->sculpt->pbvh;
  PBVHNode **nodes;
  int totnode;
  BKE_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);

  if (totnode == 0) {
    return OPERATOR_CANCELLED;
  }

  SCULPT_undo_push_begin(ob, "init mask");

  if (mode == SCULPT_MASK_INIT_RANDOM_PER_LOOSE_PART) {
    SCULPT_connected_components_ensure(ob);
  }

  SculptThreadedTaskData data = {
      .ob = ob,
      .nodes = nodes,
      .mask_init_mode = mode,
      .mask_init_seed = PIL_check_seconds_timer(),
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, mask_init_task_cb, &settings);

  multires_stitch_grids(ob);

  SCULPT_undo_push_end();

  BKE_pbvh_update_vertex_data(ss->pbvh, PBVH_UpdateMask);
  MEM_SAFE_FREE(nodes);
  SCULPT_tag_update_overlays(C);
  return OPERATOR_FINISHED;
}

void SCULPT_OT_mask_init(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Init Mask";
  ot->description = "Creates a new mask for the entire mesh";
  ot->idname = "SCULPT_OT_mask_init";

  /* api callbacks */
  ot->exec = sculpt_mask_init_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  RNA_def_enum(ot->srna,
               "mode",
               prop_sculpt_mask_init_mode_types,
               SCULPT_MASK_INIT_RANDOM_PER_VERTEX,
               "Mode",
               "");
}
