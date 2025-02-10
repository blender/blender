/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "DNA_material_types.h"

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_material.hh"

#include "BLI_vector.hh"

#include "DEG_depsgraph.hh"

#include "ED_grease_pencil.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

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
      DEG_id_tag_update(&ma->id, ID_RECALC_SYNC_TO_EVAL);
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
/** \name Hide Others Materials Operator
 * \{ */

static int grease_pencil_material_hide_exec(bContext *C, wmOperator *op)
{
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  const bool invert = RNA_boolean_get(op->ptr, "invert");

  bool changed = false;
  const int material_index = object->actcol - 1;

  for (const int i : IndexRange(object->totcol)) {
    if (invert && i == material_index) {
      continue;
    }
    if (!invert && i != material_index) {
      continue;
    }
    if (Material *ma = BKE_object_material_get(object, i + 1)) {
      MaterialGPencilStyle &gp_style = *ma->gp_style;
      gp_style.flag |= GP_MATERIAL_HIDE;
      DEG_id_tag_update(&ma->id, ID_RECALC_SYNC_TO_EVAL);
      changed = true;
    }
  }

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA | NA_EDITED, &grease_pencil);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_material_hide(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Hide Materials";
  ot->idname = "GREASE_PENCIL_OT_material_hide";
  ot->description = "Hide active/inactive Grease Pencil material(s)";

  /* Callbacks. */
  ot->exec = grease_pencil_material_hide_exec;
  ot->poll = active_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_boolean(
      ot->srna, "invert", false, "Invert", "Hide inactive materials instead of the active one");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lock All Materials Operator
 * \{ */

static int grease_pencil_material_lock_all_exec(bContext *C, wmOperator * /*op*/)
{
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  bool changed = false;
  for (const int i : IndexRange(object->totcol)) {
    if (Material *ma = BKE_object_material_get(object, i + 1)) {
      MaterialGPencilStyle &gp_style = *ma->gp_style;
      gp_style.flag |= GP_MATERIAL_LOCKED;
      DEG_id_tag_update(&ma->id, ID_RECALC_SYNC_TO_EVAL);
      changed = true;
    }
  }

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA | NA_EDITED, &grease_pencil);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_material_lock_all(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Lock All Materials";
  ot->idname = "GREASE_PENCIL_OT_material_lock_all";
  ot->description =
      "Lock all Grease Pencil materials to prevent them from being accidentally modified";

  /* Callbacks. */
  ot->exec = grease_pencil_material_lock_all_exec;
  ot->poll = active_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Unlock All Materials Operator
 * \{ */

static int grease_pencil_material_unlock_all_exec(bContext *C, wmOperator * /*op*/)
{
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  bool changed = false;
  for (const int i : IndexRange(object->totcol)) {
    if (Material *ma = BKE_object_material_get(object, i + 1)) {
      MaterialGPencilStyle &gp_style = *ma->gp_style;
      gp_style.flag &= ~GP_MATERIAL_LOCKED;
      DEG_id_tag_update(&ma->id, ID_RECALC_SYNC_TO_EVAL);
      changed = true;
    }
  }

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA | NA_EDITED, &grease_pencil);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_material_unlock_all(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Unlock All Materials";
  ot->idname = "GREASE_PENCIL_OT_material_unlock_all";
  ot->description = "Unlock all Grease Pencil materials so that they can be edited";

  /* Callbacks. */
  ot->exec = grease_pencil_material_unlock_all_exec;
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
        DEG_id_tag_update(&ma->id, ID_RECALC_SYNC_TO_EVAL);
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

/* -------------------------------------------------------------------- */
/** \name Lock Unselected Materials Operator
 * \{ */

static int grease_pencil_material_lock_unselected_exec(bContext *C, wmOperator * /*op*/)
{
  using namespace blender;
  using namespace blender::bke;

  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  bool changed = false;
  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);

  Set<int> materials_used;

  for (const MutableDrawingInfo &info : drawings) {
    IndexMaskMemory memory;
    const IndexMask strokes = ed::greasepencil::retrieve_editable_and_selected_strokes(
        *object, info.drawing, info.layer_index, memory);
    if (strokes.is_empty()) {
      return OPERATOR_CANCELLED;
    }

    AttributeAccessor attributes = info.drawing.strokes().attributes();
    const VArray<int> material_indices = *attributes.lookup_or_default<int>(
        "material_index", AttrDomain::Curve, 0);

    if (const std::optional<int> single = material_indices.get_if_single()) {
      materials_used.add(*single);
    }
    else {
      strokes.foreach_index([&](const int i) { materials_used.add(material_indices[i]); });
    }
  };

  /* The material lock must be done outside of the drawing loop to prevent
   * 'retrieve_editable_and_selected_strokes' from returning an incorrect IndexMask.
   */
  for (const int i : IndexRange(object->totcol)) {
    if (!materials_used.contains(i)) {
      if (Material *ma = BKE_object_material_get(object, i + 1)) {
        MaterialGPencilStyle &gp_style = *ma->gp_style;
        gp_style.flag |= GP_MATERIAL_LOCKED;
        DEG_id_tag_update(&ma->id, ID_RECALC_SYNC_TO_EVAL);
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

static void GREASE_PENCIL_OT_material_lock_unselected(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Lock Unselected Materials";
  ot->idname = "GREASE_PENCIL_OT_material_lock_unselected";
  ot->description = "Lock any material not used in any selected stroke";

  /* Callbacks. */
  ot->exec = grease_pencil_material_lock_unselected_exec;
  ot->poll = active_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy Materials to Selected Objects
 * \{ */

static int grease_pencil_material_copy_to_object_exec(bContext *C, wmOperator *op)
{
  using namespace blender;
  using namespace blender::bke;

  Main *bmain = CTX_data_main(C);
  const bool only_active = RNA_boolean_get(op->ptr, "only_active");
  Object *ob_src = CTX_data_active_object(C);
  Material *ma_active = BKE_object_material_get(ob_src, ob_src->actcol);
  if (ma_active == nullptr) {
    return OPERATOR_CANCELLED;
  }

  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    if ((ob == ob_src) || (ob->type != OB_GREASE_PENCIL)) {
      continue;
    }
    /* Duplicate materials. */
    for (const int i : IndexRange(ob_src->totcol)) {
      Material *ma_src = BKE_object_material_get(ob_src, i + 1);
      if (only_active && ma_src != ma_active) {
        continue;
      }

      if (ma_src == nullptr) {
        continue;
      }

      BKE_object_material_ensure(bmain, ob, ma_src);
    }

    DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);
  }
  CTX_DATA_END;

  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_material_copy_to_object(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Copy Materials to Selected Object";
  ot->idname = "GREASE_PENCIL_OT_material_copy_to_object";
  ot->description = "Append Materials of the active Grease Pencil to other object";

  /* Callbacks. */
  ot->exec = grease_pencil_material_copy_to_object_exec;
  ot->poll = active_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  prop = RNA_def_boolean(ot->srna,
                         "only_active",
                         true,
                         "Only Active",
                         "Append only active material, uncheck to append all materials");

  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

static int material_isolate_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob->data);
  Material *active_ma = BKE_gpencil_material(ob, ob->actcol);
  MaterialGPencilStyle *active_material = BKE_gpencil_material_settings(ob, ob->actcol);
  MaterialGPencilStyle *gp_style;

  int flags = GP_MATERIAL_LOCKED;
  bool isolate = false;

  if (RNA_boolean_get(op->ptr, "affect_visibility")) {
    flags |= GP_MATERIAL_HIDE;
  }

  if (active_material == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* Test whether to isolate or clear all flags */
  Material *ma = nullptr;
  short *totcol = BKE_object_material_len_p(ob);
  for (short i = 0; i < *totcol; i++) {
    ma = BKE_gpencil_material(ob, i + 1);
    /* Skip if this is the active one */
    if (ELEM(ma, nullptr, active_ma)) {
      continue;
    }

    /* If the flags aren't set, that means that the color is
     * not alone, so we have some colors to isolate still
     */
    gp_style = ma->gp_style;
    if ((gp_style->flag & flags) == 0) {
      isolate = true;
      break;
    }
  }

  /* Set/Clear flags as appropriate */
  if (isolate) {
    /* Set flags on all "other" colors */
    for (short i = 0; i < *totcol; i++) {
      ma = BKE_gpencil_material(ob, i + 1);
      if (ma == nullptr) {
        continue;
      }
      gp_style = ma->gp_style;
      if (gp_style == active_material) {
        continue;
      }
      gp_style->flag |= flags;
      DEG_id_tag_update(&ma->id, ID_RECALC_SYNC_TO_EVAL);
    }
  }
  else {
    /* Clear flags - Restore everything else */
    for (short i = 0; i < *totcol; i++) {
      ma = BKE_gpencil_material(ob, i + 1);
      if (ma == nullptr) {
        continue;
      }
      gp_style = ma->gp_style;
      gp_style->flag &= ~flags;
      DEG_id_tag_update(&ma->id, ID_RECALC_SYNC_TO_EVAL);
    }
  }

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_material_isolate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Isolate Material";
  ot->idname = "GREASE_PENCIL_OT_material_isolate";
  ot->description =
      "Toggle whether the active material is the only one that is editable and/or visible";

  /* callbacks */
  ot->exec = material_isolate_exec;
  ot->poll = active_grease_pencil_material_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna,
                  "affect_visibility",
                  false,
                  "Affect Visibility",
                  "In addition to toggling "
                  "the editability, also affect the visibility");
}

/** \} */

}  // namespace blender::ed::greasepencil

void ED_operatortypes_grease_pencil_material()
{
  using namespace blender::ed::greasepencil;
  WM_operatortype_append(GREASE_PENCIL_OT_material_reveal);
  WM_operatortype_append(GREASE_PENCIL_OT_material_hide);
  WM_operatortype_append(GREASE_PENCIL_OT_material_lock_all);
  WM_operatortype_append(GREASE_PENCIL_OT_material_unlock_all);
  WM_operatortype_append(GREASE_PENCIL_OT_material_lock_unused);
  WM_operatortype_append(GREASE_PENCIL_OT_material_lock_unselected);
  WM_operatortype_append(GREASE_PENCIL_OT_material_copy_to_object);
  WM_operatortype_append(GREASE_PENCIL_OT_material_isolate);
}
