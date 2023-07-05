/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "BKE_context.h"
#include "BKE_grease_pencil.hh"

#include "DEG_depsgraph.h"

#include "DNA_scene_types.h"

#include "ED_grease_pencil.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"

namespace blender::ed::greasepencil {

static int insert_blank_frame_exec(bContext *C, wmOperator *op)
{
  using namespace blender::bke::greasepencil;
  Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  const int current_frame = scene->r.cfra;
  const bool all_layers = RNA_boolean_get(op->ptr, "all_layers");
  const int duration = RNA_int_get(op->ptr, "duration");

  bool changed = false;
  if (all_layers) {
    for (Layer *layer : grease_pencil.layers_for_write()) {
      if (!layer->is_editable()) {
        continue;
      }
      changed = grease_pencil.insert_blank_frame(
          *layer, current_frame, duration, BEZT_KEYTYPE_KEYFRAME);
    }
  }
  else {
    if (!grease_pencil.has_active_layer()) {
      return OPERATOR_CANCELLED;
    }
    changed = grease_pencil.insert_blank_frame(*grease_pencil.get_active_layer_for_write(),
                                               current_frame,
                                               duration,
                                               BEZT_KEYTYPE_KEYFRAME);
  }

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_insert_blank_frame(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Insert Blank Frame";
  ot->idname = "GREASE_PENCIL_OT_insert_blank_frame";
  ot->description = "Insert a blank frame on the current scene frame";

  /* callbacks */
  ot->exec = insert_blank_frame_exec;
  ot->poll = active_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_boolean(
      ot->srna, "all_layers", false, "All Layers", "Insert a blank frame in all editable layers");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  RNA_def_int(ot->srna, "duration", 1, 1, MAXFRAME, "Duration", "", 1, 100);
}

}  // namespace blender::ed::greasepencil

void ED_operatortypes_grease_pencil_frames()
{
  using namespace blender::ed::greasepencil;
  WM_operatortype_append(GREASE_PENCIL_OT_insert_blank_frame);
}
