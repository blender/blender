/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_hash.h"
#include "BLI_task.h"
#include "BLI_time.h"

#include "DNA_object_types.h"

#include "BKE_ccg.h"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_multires.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "sculpt_intern.hh"

#include "bmesh.hh"

#include <cmath>
#include <cstdlib>

/* Mask Init operator. */
/* Initializes mask values for the entire mesh depending on the mode. */

namespace blender::ed::sculpt_paint::mask {

enum eSculptMaskInitMode {
  SCULPT_MASK_INIT_RANDOM_PER_VERTEX,
  SCULPT_MASK_INIT_RANDOM_PER_FACE_SET,
  SCULPT_MASK_INIT_RANDOM_PER_LOOSE_PART,
};

static void mask_init_task(Object *ob,
                           const int mode,
                           const int seed,
                           const SculptMaskWriteInfo mask_write,
                           PBVHNode *node)
{
  SculptSession *ss = ob->sculpt;
  PBVHVertexIter vd;
  undo::push_node(*ob, node, undo::Type::Mask);
  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    float mask;
    switch (mode) {
      case SCULPT_MASK_INIT_RANDOM_PER_VERTEX:
        mask = BLI_hash_int_01(vd.index + seed);
        break;
      case SCULPT_MASK_INIT_RANDOM_PER_FACE_SET: {
        const int face_set = face_set::vert_face_set_get(ss, vd.vertex);
        mask = BLI_hash_int_01(face_set + seed);
        break;
      }
      case SCULPT_MASK_INIT_RANDOM_PER_LOOSE_PART:
        mask = BLI_hash_int_01(SCULPT_vertex_island_get(ss, vd.vertex) + seed);
        break;
    }
    SCULPT_mask_vert_set(BKE_pbvh_type(ss->pbvh), mask_write, mask, vd);
  }
  BKE_pbvh_vertex_iter_end;
  BKE_pbvh_node_mark_update_mask(node);
}

static int sculpt_mask_init_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  const int mode = RNA_enum_get(op->ptr, "mode");

  MultiresModifierData *mmd = BKE_sculpt_multires_active(CTX_data_scene(C), ob);
  BKE_sculpt_mask_layers_ensure(depsgraph, CTX_data_main(C), ob, mmd);

  BKE_sculpt_update_object_for_edit(depsgraph, ob, false);

  PBVH *pbvh = ob->sculpt->pbvh;
  Vector<PBVHNode *> nodes = bke::pbvh::search_gather(pbvh, {});

  if (nodes.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  undo::push_begin(ob, op);

  if (mode == SCULPT_MASK_INIT_RANDOM_PER_LOOSE_PART) {
    SCULPT_topology_islands_ensure(ob);
  }

  const int mask_init_seed = BLI_time_now_seconds();

  const SculptMaskWriteInfo mask_write = SCULPT_mask_get_for_write(ss);
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      mask_init_task(ob, mode, mask_init_seed, mask_write, nodes[i]);
    }
  });

  multires_stitch_grids(ob);

  undo::push_end(ob);

  bke::pbvh::update_mask(*ss->pbvh);
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

  static EnumPropertyItem modes[] = {
      {SCULPT_MASK_INIT_RANDOM_PER_VERTEX, "RANDOM_PER_VERTEX", 0, "Random per Vertex", ""},
      {SCULPT_MASK_INIT_RANDOM_PER_FACE_SET, "RANDOM_PER_FACE_SET", 0, "Random per Face Set", ""},
      {SCULPT_MASK_INIT_RANDOM_PER_LOOSE_PART,
       "RANDOM_PER_LOOSE_PART",
       0,
       "Random per Loose Part",
       ""},
      {0, nullptr, 0, nullptr, nullptr},
  };
  RNA_def_enum(ot->srna, "mode", modes, SCULPT_MASK_INIT_RANDOM_PER_VERTEX, "Mode", "");
}

}  // namespace blender::ed::sculpt_paint::mask
