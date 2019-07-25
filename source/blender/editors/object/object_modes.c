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
 */

/** \file
 * \ingroup edobj
 *
 * General utils to handle mode switching,
 * actual mode switching logic is per-object type.
 */

#include "DNA_gpencil_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_workspace_types.h"

#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "DEG_depsgraph.h"

#include "ED_armature.h"
#include "ED_gpencil.h"
#include "ED_screen.h"

#include "ED_object.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name High Level Mode Operations
 *
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
  if (mode == OB_MODE_PAINT_GPENCIL) {
    return "GPENCIL_OT_paintmode_toggle";
  }
  if (mode == OB_MODE_SCULPT_GPENCIL) {
    return "GPENCIL_OT_sculptmode_toggle";
  }
  if (mode == OB_MODE_WEIGHT_GPENCIL) {
    return "GPENCIL_OT_weightmode_toggle";
  }
  return NULL;
}

/**
 * Checks the mode to be set is compatible with the object
 * should be made into a generic function
 */
bool ED_object_mode_compat_test(const Object *ob, eObjectMode mode)
{
  if (ob) {
    if (mode == OB_MODE_OBJECT) {
      return true;
    }

    switch (ob->type) {
      case OB_MESH:
        if (mode & (OB_MODE_EDIT | OB_MODE_SCULPT | OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT |
                    OB_MODE_TEXTURE_PAINT | OB_MODE_PARTICLE_EDIT)) {
          return true;
        }
        break;
      case OB_CURVE:
      case OB_SURF:
      case OB_FONT:
      case OB_MBALL:
        if (mode & (OB_MODE_EDIT)) {
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
      case OB_GPENCIL:
        if (mode & (OB_MODE_EDIT | OB_MODE_EDIT_GPENCIL | OB_MODE_PAINT_GPENCIL |
                    OB_MODE_SCULPT_GPENCIL | OB_MODE_WEIGHT_GPENCIL)) {
          return true;
        }
        break;
    }
  }

  return false;
}

/**
 * Sets the mode to a compatible state (use before entering the mode).
 *
 * This is so each mode's exec function can call
 */
bool ED_object_mode_compat_set(bContext *C, Object *ob, eObjectMode mode, ReportList *reports)
{
  bool ok;
  if (!ELEM(ob->mode, mode, OB_MODE_OBJECT)) {
    const char *opstring = object_mode_op_string(ob->mode);

    WM_operator_name_call(C, opstring, WM_OP_EXEC_REGION_WIN, NULL);
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

void ED_object_mode_toggle(bContext *C, eObjectMode mode)
{
  if (mode != OB_MODE_OBJECT) {
    const char *opstring = object_mode_op_string(mode);

    if (opstring) {
      wmOperatorType *ot = WM_operatortype_find(opstring, false);
      WM_operator_name_call_ptr(C, ot, WM_OP_EXEC_REGION_WIN, NULL);
    }
  }
}

/* Wrapper for operator  */
void ED_object_mode_set(bContext *C, eObjectMode mode)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wm->op_undo_depth++;
  /* needed so we don't do undo pushes. */
  ED_object_mode_generic_enter(C, mode);
  wm->op_undo_depth--;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic Mode Enter/Exit
 *
 * Supports exiting a mode without it being in the current context.
 * This could be done for entering modes too if it's needed.
 *
 * \{ */

bool ED_object_mode_generic_enter(struct bContext *C, eObjectMode object_mode)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  if (ob == NULL) {
    return (object_mode == OB_MODE_OBJECT);
  }
  if (ob->mode == object_mode) {
    return true;
  }
  wmOperatorType *ot = WM_operatortype_find("OBJECT_OT_mode_set", false);
  PointerRNA ptr;
  WM_operator_properties_create_ptr(&ptr, ot);
  RNA_enum_set(&ptr, "mode", object_mode);
  WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &ptr);
  WM_operator_properties_free(&ptr);
  return (ob->mode == object_mode);
}

/**
 * Use for changing works-paces or changing active object.
 * Caller can check #OB_MODE_ALL_MODE_DATA to test if this needs to be run.
 */
static bool ed_object_mode_generic_exit_ex(struct Main *bmain,
                                           struct Depsgraph *depsgraph,
                                           struct Scene *scene,
                                           struct Object *ob,
                                           bool only_test)
{
  BLI_assert((bmain == NULL) == only_test);
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
    if (ob->pose != NULL) {
      if (only_test) {
        return true;
      }
      ED_object_posemode_exit_ex(bmain, ob);
    }
  }
  else if ((ob->type == OB_GPENCIL) && ((ob->mode & OB_MODE_OBJECT) == 0)) {
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

void ED_object_mode_generic_exit(struct Main *bmain,
                                 struct Depsgraph *depsgraph,
                                 struct Scene *scene,
                                 struct Object *ob)
{
  ed_object_mode_generic_exit_ex(bmain, depsgraph, scene, ob, false);
}

bool ED_object_mode_generic_has_data(struct Depsgraph *depsgraph, struct Object *ob)
{
  return ed_object_mode_generic_exit_ex(NULL, depsgraph, NULL, ob, true);
}

/** \} */
