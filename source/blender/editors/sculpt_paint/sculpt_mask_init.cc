/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_hash.h"
#include "BLI_math.h"
#include "BLI_task.h"

#include "PIL_time.h"

#include "DNA_brush_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BKE_ccg.h"
#include "BKE_context.h"
#include "BKE_multires.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "sculpt_intern.hh"

#include "bmesh.h"

#include <cmath>
#include <cstdlib>

/* Mask Init operator. */
/* Initializes mask values for the entire mesh depending on the mode. */

enum eSculptMaskInitMode {
  SCULPT_MASK_INIT_RANDOM_PER_VERTEX,
  SCULPT_MASK_INIT_RANDOM_PER_FACE_SET,
  SCULPT_MASK_INIT_RANDOM_PER_LOOSE_PART,
};

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
    {0, nullptr, 0, nullptr, nullptr},
};

static void mask_init_task_cb(void *__restrict userdata,
                              const int i,
                              const TaskParallelTLS *__restrict /*tls*/)
{
  SculptThreadedTaskData *data = static_cast<SculptThreadedTaskData *>(userdata);
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
        const int face_set = SCULPT_vertex_face_set_get(ss, vd.vertex);
        *vd.mask = BLI_hash_int_01(face_set + seed);
        break;
      }
      case SCULPT_MASK_INIT_RANDOM_PER_LOOSE_PART:
        *vd.mask = BLI_hash_int_01(SCULPT_vertex_island_get(ss, vd.vertex) + seed);
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

  MultiresModifierData *mmd = BKE_sculpt_multires_active(CTX_data_scene(C), ob);
  BKE_sculpt_mask_layers_ensure(depsgraph, CTX_data_main(C), ob, mmd);

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, false);

  PBVH *pbvh = ob->sculpt->pbvh;
  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(pbvh, nullptr, nullptr);

  if (nodes.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  SCULPT_undo_push_begin(ob, op);

  if (mode == SCULPT_MASK_INIT_RANDOM_PER_LOOSE_PART) {
    SCULPT_topology_islands_ensure(ob);
  }

  SculptThreadedTaskData data{};
  data.ob = ob;
  data.nodes = nodes;
  data.mask_init_mode = mode;
  data.mask_init_seed = PIL_check_seconds_timer();

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, nodes.size());
  BLI_task_parallel_range(0, nodes.size(), &data, mask_init_task_cb, &settings);

  multires_stitch_grids(ob);

  SCULPT_undo_push_end(ob);

  BKE_pbvh_update_vertex_data(ss->pbvh, PBVH_UpdateMask);
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
