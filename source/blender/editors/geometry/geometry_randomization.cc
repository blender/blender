/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "WM_api.hh"

#include "BKE_context.hh"
#include "BKE_global.h"
#include "BKE_main.hh"

#include "DEG_depsgraph.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "geometry_intern.hh"

namespace blender::ed::geometry {

static int geometry_randomization_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  RNA_boolean_set(op->ptr, "value", G.randomize_geometry_element_order);
  return WM_operator_props_popup(C, op, event);
}

static int geometry_randomization_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);

  G.randomize_geometry_element_order = RNA_boolean_get(op->ptr, "value");

  LISTBASE_FOREACH (Object *, object, &bmain->objects) {
    DEG_id_tag_update(&object->id, ID_RECALC_GEOMETRY);
  }
  WM_event_add_notifier(C, NC_WINDOW, nullptr);
  return OPERATOR_FINISHED;
}

void GEOMETRY_OT_geometry_randomization(wmOperatorType *ot)
{
  ot->name = "Set Geometry Randomization";
  ot->idname = "GEOMETRY_OT_geometry_randomization";
  ot->description = "Toggle geometry randomization for debugging purposes";

  ot->exec = geometry_randomization_exec;
  ot->invoke = geometry_randomization_invoke;
  ot->flag |= OPTYPE_UNDO | OPTYPE_REGISTER;

  RNA_def_boolean(ot->srna,
                  "value",
                  false,
                  "Value",
                  "Randomize the order of geometry elements (e.g. vertices or edges) after some "
                  "operations where there are no guarantees about the order. This avoids "
                  "accidentally depending on something that may change in the future");
}

}  // namespace blender::ed::geometry
