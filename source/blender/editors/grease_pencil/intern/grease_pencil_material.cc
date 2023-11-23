/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "DNA_material_types.h"

#include "BKE_context.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_material.h"

#include "DEG_depsgraph.hh"

#include "ED_grease_pencil.hh"

#include "WM_api.hh"

namespace blender::ed::greasepencil {

/* -------------------------------------------------------------------- */
/** \name Show All Materials Operator
 * \{ */

static int grease_pencil_material_reveal_exec(bContext *C, wmOperator * /*op*/)
{
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  bool changed = false;
  for (const int i : IndexRange(object->totcol)) {
    if (Material *ma = BKE_gpencil_material(object, i + 1)) {
      MaterialGPencilStyle &gp_style = *ma->gp_style;
      gp_style.flag &= ~GP_MATERIAL_HIDE;
      DEG_id_tag_update(&ma->id, ID_RECALC_COPY_ON_WRITE);
      changed = true;
    }
  }

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA | NA_EDITED, &grease_pencil);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_material_reveal(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Show All Materials";
  ot->idname = "GREASE_PENCIL_OT_material_reveal";
  ot->description = "Unhide all hidden Grease Pencil materials";

  /* Callbacks. */
  ot->exec = grease_pencil_material_reveal_exec;
  ot->poll = active_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lock Unused Materials Operator
 * \{ */

static int grease_pencil_material_lock_unused_exec(bContext *C, wmOperator * /*op*/)
{
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  bool changed = false;
  for (const int material_index : IndexRange(object->totcol)) {
    if (!BKE_object_material_slot_used(object, material_index + 1)) {
      if (Material *ma = BKE_object_material_get(object, material_index + 1)) {
        MaterialGPencilStyle &gp_style = *ma->gp_style;
        gp_style.flag |= GP_MATERIAL_HIDE | GP_MATERIAL_LOCKED;
        DEG_id_tag_update(&ma->id, ID_RECALC_COPY_ON_WRITE);
        changed = true;
      }
    }
  }
  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA | NA_EDITED, &grease_pencil);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_material_lock_unused(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Lock Unused Materials";
  ot->idname = "GREASE_PENCIL_OT_material_lock_unused";
  ot->description = "Lock and hide any material not used";

  /* Callbacks. */
  ot->exec = grease_pencil_material_lock_unused_exec;
  ot->poll = active_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

}  // namespace blender::ed::greasepencil

void ED_operatortypes_grease_pencil_material()
{
  using namespace blender::ed::greasepencil;
  WM_operatortype_append(GREASE_PENCIL_OT_material_reveal);
  WM_operatortype_append(GREASE_PENCIL_OT_material_lock_unused);
}
