/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 *
 * General utils to handle mode switching,
 * actual mode switching logic is per-object type.
 */

#include "DNA_gpencil_legacy_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_workspace_types.h"

#include "BLI_kdopbvh.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "PIL_time.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "ED_armature.h"
#include "ED_gpencil_legacy.h"
#include "ED_screen.h"
#include "ED_transform_snap_object_context.h"
#include "ED_undo.h"
#include "ED_view3d.h"

#include "WM_toolsystem.h"

#include "ED_object.h" /* own include */
#include "object_intern.h"

/* -------------------------------------------------------------------- */
/** \name High Level Mode Operations
 * \{ */

static const char *object_mode_op_string(eObjectMode mode)
{
  if (mode & OB_MODE_EDIT) {
    return "OBJECT_OT_editmode_toggle";
  }
  if (mode == OB_MODE_SCULPT) {
    return "SCULPT_OT_sculptmode_toggle";
  }
  if (mode == OB_MODE_VERTEX_PAINT) {
    return "PAINT_OT_vertex_paint_toggle";
  }
  if (mode == OB_MODE_WEIGHT_PAINT) {
    return "PAINT_OT_weight_paint_toggle";
  }
  if (mode == OB_MODE_TEXTURE_PAINT) {
    return "PAINT_OT_texture_paint_toggle";
  }
  if (mode == OB_MODE_PARTICLE_EDIT) {
    return "PARTICLE_OT_particle_edit_toggle";
  }
  if (mode == OB_MODE_POSE) {
    return "OBJECT_OT_posemode_toggle";
  }
  if (mode == OB_MODE_EDIT_GPENCIL) {
    return "GPENCIL_OT_editmode_toggle";
  }
  if (U.experimental.use_grease_pencil_version3) {
    if (mode == OB_MODE_PAINT_GPENCIL) {
      return "GREASE_PENCIL_OT_draw_mode_toggle";
    }
  }
  if (mode == OB_MODE_PAINT_GPENCIL) {
    return "GPENCIL_OT_paintmode_toggle";
  }
  if (mode == OB_MODE_SCULPT_GPENCIL) {
    return "GPENCIL_OT_sculptmode_toggle";
  }
  if (mode == OB_MODE_WEIGHT_GPENCIL) {
    return "GPENCIL_OT_weightmode_toggle";
  }
  if (mode == OB_MODE_VERTEX_GPENCIL) {
    return "GPENCIL_OT_vertexmode_toggle";
  }
  if (mode == OB_MODE_SCULPT_CURVES) {
    return "CURVES_OT_sculptmode_toggle";
  }
  return nullptr;
}

bool ED_object_mode_compat_test(const Object *ob, eObjectMode mode)
{
  if (mode == OB_MODE_OBJECT) {
    return true;
  }

  switch (ob->type) {
    case OB_MESH:
      if (mode & (OB_MODE_EDIT | OB_MODE_SCULPT | OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT |
                  OB_MODE_TEXTURE_PAINT))
      {
        return true;
      }
      if (mode & OB_MODE_PARTICLE_EDIT) {
        if (ED_object_particle_edit_mode_supported(ob)) {
          return true;
        }
      }
      break;
    case OB_CURVES_LEGACY:
    case OB_SURF:
    case OB_FONT:
    case OB_MBALL:
    case OB_POINTCLOUD:
      if (mode & OB_MODE_EDIT) {
        return true;
      }
      break;
    case OB_LATTICE:
      if (mode & (OB_MODE_EDIT | OB_MODE_WEIGHT_PAINT)) {
        return true;
      }
      break;
    case OB_ARMATURE:
      if (mode & (OB_MODE_EDIT | OB_MODE_POSE)) {
        return true;
      }
      break;
    case OB_GPENCIL_LEGACY:
      if (mode & (OB_MODE_EDIT_GPENCIL | OB_MODE_ALL_PAINT_GPENCIL)) {
        return true;
      }
      break;
    case OB_CURVES:
      if (mode & (OB_MODE_EDIT | OB_MODE_SCULPT_CURVES)) {
        return true;
      }
      break;
    case OB_GREASE_PENCIL:
      if (mode & (OB_MODE_EDIT | OB_MODE_PAINT_GPENCIL)) {
        return true;
      }
      break;
  }

  return false;
}

bool ED_object_mode_compat_set(bContext *C, Object *ob, eObjectMode mode, ReportList *reports)
{
  bool ok;
  if (!ELEM(ob->mode, mode, OB_MODE_OBJECT)) {
    const char *opstring = object_mode_op_string(eObjectMode(ob->mode));

    WM_operator_name_call(C, opstring, WM_OP_EXEC_REGION_WIN, nullptr, nullptr);
    ok = ELEM(ob->mode, mode, OB_MODE_OBJECT);
    if (!ok) {
      wmOperatorType *ot = WM_operatortype_find(opstring, false);
      BKE_reportf(reports, RPT_ERROR, "Unable to execute '%s', error changing modes", ot->name);
    }
  }
  else {
    ok = true;
  }

  return ok;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic Mode Enter/Exit
 *
 * Supports exiting a mode without it being in the current context.
 * This could be done for entering modes too if it's needed.
 *
 * \{ */

bool ED_object_mode_set_ex(bContext *C, eObjectMode mode, bool use_undo, ReportList *reports)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  if (ob == nullptr) {
    return (mode == OB_MODE_OBJECT);
  }

  if ((ob->type == OB_GPENCIL_LEGACY) && (mode == OB_MODE_EDIT)) {
    mode = OB_MODE_EDIT_GPENCIL;
  }

  if (ob->mode == mode) {
    return true;
  }

  if (!ED_object_mode_compat_test(ob, mode)) {
    return false;
  }

  const char *opstring = object_mode_op_string((mode == OB_MODE_OBJECT) ? eObjectMode(ob->mode) :
                                                                          mode);
  wmOperatorType *ot = WM_operatortype_find(opstring, false);

  if (!use_undo) {
    wm->op_undo_depth++;
  }
  WM_operator_name_call_ptr(C, ot, WM_OP_EXEC_REGION_WIN, nullptr, nullptr);
  if (!use_undo) {
    wm->op_undo_depth--;
  }

  if (ob->mode != mode) {
    BKE_reportf(reports, RPT_ERROR, "Unable to execute '%s', error changing modes", ot->name);
    return false;
  }

  return true;
}

bool ED_object_mode_set(bContext *C, eObjectMode mode)
{
  /* Don't do undo push by default, since this may be called by lower level code. */
  return ED_object_mode_set_ex(C, mode, true, nullptr);
}

/**
 * Use for changing works-paces or changing active object.
 * Caller can check #OB_MODE_ALL_MODE_DATA to test if this needs to be run.
 */
static bool ed_object_mode_generic_exit_ex(
    Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob, bool only_test)
{
  BLI_assert((bmain == nullptr) == only_test);
  if (ob->mode & OB_MODE_EDIT) {
    if (BKE_object_is_in_editmode(ob)) {
      if (only_test) {
        return true;
      }
      ED_object_editmode_exit_ex(bmain, scene, ob, EM_FREEDATA);
    }
  }
  else if (ob->mode & OB_MODE_VERTEX_PAINT) {
    if (ob->sculpt && (ob->sculpt->mode_type == OB_MODE_VERTEX_PAINT)) {
      if (only_test) {
        return true;
      }
      ED_object_vpaintmode_exit_ex(ob);
    }
  }
  else if (ob->mode & OB_MODE_WEIGHT_PAINT) {
    if (ob->sculpt && (ob->sculpt->mode_type == OB_MODE_WEIGHT_PAINT)) {
      if (only_test) {
        return true;
      }
      ED_object_wpaintmode_exit_ex(ob);
    }
  }
  else if (ob->mode & OB_MODE_SCULPT) {
    if (ob->sculpt && (ob->sculpt->mode_type == OB_MODE_SCULPT)) {
      if (only_test) {
        return true;
      }
      ED_object_sculptmode_exit_ex(bmain, depsgraph, scene, ob);
    }
  }
  else if (ob->mode & OB_MODE_POSE) {
    if (ob->pose != nullptr) {
      if (only_test) {
        return true;
      }
      ED_object_posemode_exit_ex(bmain, ob);
    }
  }
  else if (ob->mode & OB_MODE_TEXTURE_PAINT) {
    if (only_test) {
      return true;
    }
    ED_object_texture_paint_mode_exit_ex(bmain, scene, ob);
  }
  else if (ob->mode & OB_MODE_PARTICLE_EDIT) {
    if (only_test) {
      return true;
    }
    ED_object_particle_edit_mode_exit_ex(scene, ob);
  }
  else if (ob->type == OB_GPENCIL_LEGACY) {
    /* Accounted for above. */
    BLI_assert((ob->mode & OB_MODE_OBJECT) == 0);
    if (only_test) {
      return true;
    }
    ED_object_gpencil_exit(bmain, ob);
  }
  else {
    if (only_test) {
      return false;
    }
    BLI_assert((ob->mode & OB_MODE_ALL_MODE_DATA) == 0);
  }

  return false;
}

/* When locked, it's almost impossible to select the pose-object
 * then the mesh-object to enter weight paint mode.
 * Even when the object mode is not locked this is inconvenient - so allow in either case.
 *
 * In this case move our pose object in/out of pose mode.
 * This is in fits with the convention of selecting multiple objects and entering a mode.
 */
static void ed_object_posemode_set_for_weight_paint_ex(bContext *C,
                                                       Main *bmain,
                                                       Object *ob_arm,
                                                       const bool is_mode_set)
{
  View3D *v3d = CTX_wm_view3d(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  if (ob_arm != nullptr) {
    BKE_view_layer_synced_ensure(scene, view_layer);
    const Base *base_arm = BKE_view_layer_base_find(view_layer, ob_arm);
    if (base_arm && BASE_VISIBLE(v3d, base_arm)) {
      if (is_mode_set) {
        if ((ob_arm->mode & OB_MODE_POSE) != 0) {
          ED_object_posemode_exit_ex(bmain, ob_arm);
        }
      }
      else {
        /* Only check selected status when entering weight-paint mode
         * because we may have multiple armature objects.
         * Selecting one will de-select the other, which would leave it in pose-mode
         * when exiting weight paint mode. While usable, this looks like inconsistent
         * behavior from a user perspective. */
        if (base_arm->flag & BASE_SELECTED) {
          if ((ob_arm->mode & OB_MODE_POSE) == 0) {
            ED_object_posemode_enter_ex(bmain, ob_arm);
          }
        }
      }
    }
  }
}

void ED_object_posemode_set_for_weight_paint(bContext *C,
                                             Main *bmain,
                                             Object *ob,
                                             const bool is_mode_set)
{
  if (ob->type == OB_GPENCIL_LEGACY) {
    GpencilVirtualModifierData virtualModifierData;
    GpencilModifierData *md = BKE_gpencil_modifiers_get_virtual_modifierlist(ob,
                                                                             &virtualModifierData);
    for (; md; md = md->next) {
      if (md->type == eGpencilModifierType_Armature) {
        ArmatureGpencilModifierData *amd = (ArmatureGpencilModifierData *)md;
        Object *ob_arm = amd->object;
        ed_object_posemode_set_for_weight_paint_ex(C, bmain, ob_arm, is_mode_set);
      }
    }
  }
  else {
    VirtualModifierData virtualModifierData;
    ModifierData *md = BKE_modifiers_get_virtual_modifierlist(ob, &virtualModifierData);
    for (; md; md = md->next) {
      if (md->type == eModifierType_Armature) {
        ArmatureModifierData *amd = (ArmatureModifierData *)md;
        Object *ob_arm = amd->object;
        ed_object_posemode_set_for_weight_paint_ex(C, bmain, ob_arm, is_mode_set);
      }
    }
  }
}

void ED_object_mode_generic_exit(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  ed_object_mode_generic_exit_ex(bmain, depsgraph, scene, ob, false);
}

bool ED_object_mode_generic_has_data(Depsgraph *depsgraph, const Object *ob)
{
  return ed_object_mode_generic_exit_ex(nullptr, depsgraph, nullptr, (Object *)ob, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transfer Mode
 *
 * Enters the same mode of the current active object in another object,
 * leaving the mode of the current object.
 * \{ */

static bool object_transfer_mode_poll(bContext *C)
{
  if (!CTX_wm_region_view3d(C)) {
    return false;
  }
  const Object *ob = CTX_data_active_object(C);
  return ob && (ob->mode != OB_MODE_OBJECT);
}

/* Update the viewport rotation origin to the mouse cursor. */
static void object_transfer_mode_reposition_view_pivot(bContext *C, const int mval[2])
{
  ARegion *region = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);

  float global_loc[3];
  if (!ED_view3d_autodist_simple(region, mval, global_loc, 0, nullptr)) {
    return;
  }
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
  copy_v3_v3(ups->average_stroke_accum, global_loc);
  ups->average_stroke_counter = 1;
  ups->last_stroke_valid = true;
}

static void object_overlay_mode_transfer_animation_start(bContext *C, Object *ob_dst)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob_dst_eval = DEG_get_evaluated_object(depsgraph, ob_dst);
  ob_dst_eval->runtime.overlay_mode_transfer_start_time = PIL_check_seconds_timer();
}

static bool object_transfer_mode_to_base(bContext *C, wmOperator *op, Base *base_dst)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  if (base_dst == nullptr) {
    return false;
  }

  Object *ob_dst = base_dst->object;
  Object *ob_src = CTX_data_active_object(C);

  if (ob_dst == ob_src) {
    return false;
  }

  const eObjectMode last_mode = (eObjectMode)ob_src->mode;
  if (!ED_object_mode_compat_test(ob_dst, last_mode)) {
    return false;
  }

  bool mode_transferred = false;

  ED_undo_group_begin(C);

  if (ED_object_mode_set_ex(C, OB_MODE_OBJECT, true, op->reports)) {
    Object *ob_dst_orig = DEG_get_original_object(ob_dst);
    BKE_view_layer_synced_ensure(scene, view_layer);
    Base *base = BKE_view_layer_base_find(view_layer, ob_dst_orig);
    BKE_view_layer_base_deselect_all(scene, view_layer);
    BKE_view_layer_base_select_and_set_active(view_layer, base);
    DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);

    ED_undo_push(C, "Change Active");

    ob_dst_orig = DEG_get_original_object(ob_dst);
    ED_object_mode_set_ex(C, last_mode, true, op->reports);

    if (RNA_boolean_get(op->ptr, "use_flash_on_transfer")) {
      object_overlay_mode_transfer_animation_start(C, ob_dst);
    }

    WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
    WM_toolsystem_update_from_context_view3d(C);
    mode_transferred = true;
  }

  ED_undo_group_end(C);
  return mode_transferred;
}

static int object_transfer_mode_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob_src = CTX_data_active_object(C);
  const eObjectMode src_mode = (eObjectMode)ob_src->mode;

  Base *base_dst = ED_view3d_give_base_under_cursor(C, event->mval);
  const bool mode_transferred = object_transfer_mode_to_base(C, op, base_dst);
  if (!mode_transferred) {
    return OPERATOR_CANCELLED;
  }

  if (src_mode & OB_MODE_ALL_PAINT) {
    object_transfer_mode_reposition_view_pivot(C, event->mval);
  }

  return OPERATOR_FINISHED;
}

void OBJECT_OT_transfer_mode(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Transfer Mode";
  ot->idname = "OBJECT_OT_transfer_mode";
  ot->description =
      "Switches the active object and assigns the same mode to a new one under the mouse cursor, "
      "leaving the active mode in the current one";

  /* api callbacks */
  ot->invoke = object_transfer_mode_invoke;
  ot->poll = object_transfer_mode_poll;

  /* Undo push is handled by the operator. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_DEPENDS_ON_CURSOR;

  ot->cursor_pending = WM_CURSOR_EYEDROPPER;

  RNA_def_boolean(ot->srna,
                  "use_flash_on_transfer",
                  true,
                  "Flash On Transfer",
                  "Flash the target object when transferring the mode");
}

/** \} */
