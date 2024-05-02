/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 */

#include <cmath>
#include <cstring>

#ifndef WIN32
#  include <unistd.h>
#else
#  include <io.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BKE_context.hh"
#include "BKE_key.hh"
#include "BKE_lattice.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "BLI_sys_types.h" /* for intptr_t support */

#include "ED_curve.hh"
#include "ED_lattice.hh"
#include "ED_mesh.hh"
#include "ED_object.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "object_intern.hh"

namespace blender::ed::object {

/* -------------------------------------------------------------------- */
/** \name Shape Key Lock Checks
 * \{ */

bool shape_key_report_if_locked(const Object *obedit, ReportList *reports)
{
  KeyBlock *key_block;

  switch (obedit->type) {
    case OB_MESH:
      key_block = ED_mesh_get_edit_shape_key(static_cast<Mesh *>(obedit->data));
      break;
    case OB_SURF:
    case OB_CURVES_LEGACY:
      key_block = ED_curve_get_edit_shape_key(static_cast<Curve *>(obedit->data));
      break;
    case OB_LATTICE:
      key_block = ED_lattice_get_edit_shape_key(static_cast<Lattice *>(obedit->data));
      break;
    default:
      return false;
  }

  if (key_block && (key_block->flag & KEYBLOCK_LOCKED_SHAPE) != 0) {
    if (reports) {
      BKE_reportf(reports, RPT_ERROR, "The active shape key of %s is locked", obedit->id.name + 2);
    }
    return true;
  }

  return false;
}

bool shape_key_report_if_active_locked(Object *ob, ReportList *reports)
{
  const KeyBlock *kb = BKE_keyblock_from_object(ob);

  if (kb && (kb->flag & KEYBLOCK_LOCKED_SHAPE) != 0) {
    if (reports) {
      BKE_reportf(reports, RPT_ERROR, "The active shape key of %s is locked", ob->id.name + 2);
    }
    return true;
  }

  return false;
}

static bool object_is_any_shape_key_locked(Object *ob)
{
  const Key *key = BKE_key_from_object(ob);

  if (key) {
    LISTBASE_FOREACH (const KeyBlock *, kb, &key->block) {
      if (kb->flag & KEYBLOCK_LOCKED_SHAPE) {
        return true;
      }
    }
  }

  return false;
}

bool shape_key_report_if_any_locked(Object *ob, ReportList *reports)
{
  if (object_is_any_shape_key_locked(ob)) {
    if (reports) {
      BKE_reportf(reports, RPT_ERROR, "The object %s has locked shape keys", ob->id.name + 2);
    }
    return true;
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Shape Key Function
 * \{ */

static void ED_object_shape_key_add(bContext *C, Object *ob, const bool from_mix)
{
  Main *bmain = CTX_data_main(C);
  KeyBlock *kb;
  if ((kb = BKE_object_shapekey_insert(bmain, ob, nullptr, from_mix))) {
    Key *key = BKE_key_from_object(ob);
    /* for absolute shape keys, new keys may not be added last */
    ob->shapenr = BLI_findindex(&key->block, kb) + 1;

    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove Shape Key Function
 * \{ */

static bool object_shapekey_remove(Main *bmain, Object *ob)
{
  KeyBlock *kb;
  Key *key = BKE_key_from_object(ob);

  if (key == nullptr) {
    return false;
  }

  kb = static_cast<KeyBlock *>(BLI_findlink(&key->block, ob->shapenr - 1));
  if (kb) {
    return BKE_object_shapekey_remove(bmain, ob, kb);
  }

  return false;
}

static bool object_shape_key_mirror(
    bContext *C, Object *ob, int *r_totmirr, int *r_totfail, bool use_topology)
{
  KeyBlock *kb;
  Key *key;
  int totmirr = 0, totfail = 0;

  *r_totmirr = *r_totfail = 0;

  key = BKE_key_from_object(ob);
  if (key == nullptr) {
    return false;
  }

  kb = static_cast<KeyBlock *>(BLI_findlink(&key->block, ob->shapenr - 1));

  if (kb) {
    char *tag_elem = static_cast<char *>(
        MEM_callocN(sizeof(char) * kb->totelem, "shape_key_mirror"));

    if (ob->type == OB_MESH) {
      Mesh *mesh = static_cast<Mesh *>(ob->data);
      int i1, i2;
      float *fp1, *fp2;
      float tvec[3];

      ED_mesh_mirror_spatial_table_begin(ob, nullptr, nullptr);

      for (i1 = 0; i1 < mesh->verts_num; i1++) {
        i2 = mesh_get_x_mirror_vert(ob, nullptr, i1, use_topology);
        if (i2 == i1) {
          fp1 = ((float *)kb->data) + i1 * 3;
          fp1[0] = -fp1[0];
          tag_elem[i1] = 1;
          totmirr++;
        }
        else if (i2 != -1) {
          if (tag_elem[i1] == 0 && tag_elem[i2] == 0) {
            fp1 = ((float *)kb->data) + i1 * 3;
            fp2 = ((float *)kb->data) + i2 * 3;

            copy_v3_v3(tvec, fp1);
            copy_v3_v3(fp1, fp2);
            copy_v3_v3(fp2, tvec);

            /* flip x axis */
            fp1[0] = -fp1[0];
            fp2[0] = -fp2[0];
            totmirr++;
          }
          tag_elem[i1] = tag_elem[i2] = 1;
        }
        else {
          totfail++;
        }
      }

      ED_mesh_mirror_spatial_table_end(ob);
    }
    else if (ob->type == OB_LATTICE) {
      const Lattice *lt = static_cast<const Lattice *>(ob->data);
      int i1, i2;
      float *fp1, *fp2;
      int u, v, w;
      /* half but found up odd value */
      const int pntsu_half = (lt->pntsu / 2) + (lt->pntsu % 2);

      /* Currently edit-mode isn't supported by mesh so ignore here for now too. */
#if 0
      if (lt->editlatt) {
        lt = lt->editlatt->latt;
      }
#endif

      for (w = 0; w < lt->pntsw; w++) {
        for (v = 0; v < lt->pntsv; v++) {
          for (u = 0; u < pntsu_half; u++) {
            int u_inv = (lt->pntsu - 1) - u;
            float tvec[3];
            if (u == u_inv) {
              i1 = BKE_lattice_index_from_uvw(lt, u, v, w);
              fp1 = ((float *)kb->data) + i1 * 3;
              fp1[0] = -fp1[0];
              totmirr++;
            }
            else {
              i1 = BKE_lattice_index_from_uvw(lt, u, v, w);
              i2 = BKE_lattice_index_from_uvw(lt, u_inv, v, w);

              fp1 = ((float *)kb->data) + i1 * 3;
              fp2 = ((float *)kb->data) + i2 * 3;

              copy_v3_v3(tvec, fp1);
              copy_v3_v3(fp1, fp2);
              copy_v3_v3(fp2, tvec);
              fp1[0] = -fp1[0];
              fp2[0] = -fp2[0];
              totmirr++;
            }
          }
        }
      }
    }

    MEM_freeN(tag_elem);
  }

  *r_totmirr = totmirr;
  *r_totfail = totfail;

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shared Poll Functions
 * \{ */

static bool shape_key_poll(bContext *C)
{
  Object *ob = context_object(C);
  ID *data = static_cast<ID *>((ob) ? ob->data : nullptr);

  return (ob != nullptr && !ID_IS_LINKED(ob) && !ID_IS_OVERRIDE_LIBRARY(ob) && data != nullptr &&
          !ID_IS_LINKED(data) && !ID_IS_OVERRIDE_LIBRARY(data));
}

static bool shape_key_exists_poll(bContext *C)
{
  Object *ob = context_object(C);

  return (shape_key_poll(C) &&
          /* check a keyblock exists */
          (BKE_keyblock_from_object(ob) != nullptr));
}

static bool shape_key_mode_poll(bContext *C)
{
  Object *ob = context_object(C);

  return (shape_key_poll(C) && ob->mode != OB_MODE_EDIT);
}

static bool shape_key_mode_exists_poll(bContext *C)
{
  Object *ob = context_object(C);

  return (shape_key_mode_poll(C) &&
          /* check a keyblock exists */
          (BKE_keyblock_from_object(ob) != nullptr));
}

static bool shape_key_move_poll(bContext *C)
{
  /* Same as shape_key_mode_exists_poll above, but ensure we have at least two shapes! */
  Object *ob = context_object(C);
  Key *key = BKE_key_from_object(ob);

  return (shape_key_mode_poll(C) && key != nullptr && key->totkey > 1);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shape Key Add Operator
 * \{ */

static int shape_key_add_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_object(C);
  const bool from_mix = RNA_boolean_get(op->ptr, "from_mix");

  ED_object_shape_key_add(C, ob, from_mix);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(CTX_data_main(C));

  return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Shape Key";
  ot->idname = "OBJECT_OT_shape_key_add";
  ot->description = "Add shape key to the object";

  /* api callbacks */
  ot->poll = shape_key_mode_poll;
  ot->exec = shape_key_add_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna,
                  "from_mix",
                  true,
                  "From Mix",
                  "Create the new shape key from the existing mix of keys");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shape Key Remove Operator
 * \{ */

static int shape_key_remove_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = context_object(C);
  bool changed = false;

  if (RNA_boolean_get(op->ptr, "all")) {
    if (shape_key_report_if_any_locked(ob, op->reports)) {
      return OPERATOR_CANCELLED;
    }

    if (RNA_boolean_get(op->ptr, "apply_mix")) {
      float *arr = BKE_key_evaluate_object_ex(
          ob, nullptr, nullptr, 0, static_cast<ID *>(ob->data));
      MEM_freeN(arr);
    }
    changed = BKE_object_shapekey_free(bmain, ob);
  }
  else {
    if (shape_key_report_if_active_locked(ob, op->reports)) {
      return OPERATOR_CANCELLED;
    }

    changed = object_shapekey_remove(bmain, ob);
  }

  if (changed) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    DEG_relations_tag_update(CTX_data_main(C));
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

static bool shape_key_remove_poll_property(const bContext * /*C*/,
                                           wmOperator *op,
                                           const PropertyRNA *prop)
{
  const char *prop_id = RNA_property_identifier(prop);
  const bool do_all = RNA_enum_get(op->ptr, "all");

  /* Only show seed for randomize action! */
  if (STREQ(prop_id, "apply_mix") && !do_all) {
    return false;
  }
  return true;
}

static std::string shape_key_remove_get_description(bContext * /*C*/,
                                                    wmOperatorType * /*ot*/,
                                                    PointerRNA *ptr)
{
  const bool do_apply_mix = RNA_boolean_get(ptr, "apply_mix");
  if (do_apply_mix) {
    return TIP_("Apply current visible shape to the object data, and delete all shape keys");
  }

  return "";
}

void OBJECT_OT_shape_key_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Shape Key";
  ot->idname = "OBJECT_OT_shape_key_remove";
  ot->description = "Remove shape key from the object";

  /* api callbacks */
  ot->poll = shape_key_mode_exists_poll;
  ot->exec = shape_key_remove_exec;
  ot->poll_property = shape_key_remove_poll_property;
  ot->get_description = shape_key_remove_get_description;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna, "all", false, "All", "Remove all shape keys");
  RNA_def_boolean(ot->srna,
                  "apply_mix",
                  false,
                  "Apply Mix",
                  "Apply current mix of shape keys to the geometry before removing them");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shape Key Clear Operator
 * \{ */

static int shape_key_clear_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = context_object(C);
  Key *key = BKE_key_from_object(ob);

  if (!key || BLI_listbase_is_empty(&key->block)) {
    return OPERATOR_CANCELLED;
  }

  LISTBASE_FOREACH (KeyBlock *, kb, &key->block) {
    kb->curval = 0.0f;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Shape Keys";
  ot->description = "Clear weights for all shape keys";
  ot->idname = "OBJECT_OT_shape_key_clear";

  /* api callbacks */
  ot->poll = shape_key_poll;
  ot->exec = shape_key_clear_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* starting point and step size could be optional */
static int shape_key_retime_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = context_object(C);
  Key *key = BKE_key_from_object(ob);
  float cfra = 0.0f;

  if (!key || BLI_listbase_is_empty(&key->block)) {
    return OPERATOR_CANCELLED;
  }

  LISTBASE_FOREACH (KeyBlock *, kb, &key->block) {
    kb->pos = cfra;
    cfra += 0.1f;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_retime(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Re-Time Shape Keys";
  ot->description = "Resets the timing for absolute shape keys";
  ot->idname = "OBJECT_OT_shape_key_retime";

  /* api callbacks */
  ot->poll = shape_key_poll;
  ot->exec = shape_key_retime_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shape Key Mirror Operator
 * \{ */

static int shape_key_mirror_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_object(C);
  int totmirr = 0, totfail = 0;
  bool use_topology = RNA_boolean_get(op->ptr, "use_topology");

  if (shape_key_report_if_active_locked(ob, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  if (!object_shape_key_mirror(C, ob, &totmirr, &totfail, use_topology)) {
    return OPERATOR_CANCELLED;
  }

  ED_mesh_report_mirror(op, totmirr, totfail);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_mirror(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Mirror Shape Key";
  ot->idname = "OBJECT_OT_shape_key_mirror";
  ot->description = "Mirror the current shape key along the local X axis";

  /* api callbacks */
  ot->poll = shape_key_mode_poll;
  ot->exec = shape_key_mirror_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(
      ot->srna,
      "use_topology",
      false,
      "Topology Mirror",
      "Use topology based mirroring (for when both sides of mesh have matching, unique topology)");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shape Key Move (Re-Order) Operator
 * \{ */

enum {
  KB_MOVE_TOP = -2,
  KB_MOVE_UP = -1,
  KB_MOVE_DOWN = 1,
  KB_MOVE_BOTTOM = 2,
};

static int shape_key_move_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_object(C);

  Key *key = BKE_key_from_object(ob);
  const int type = RNA_enum_get(op->ptr, "type");
  const int totkey = key->totkey;
  const int act_index = ob->shapenr - 1;
  int new_index;

  switch (type) {
    case KB_MOVE_TOP:
      /* Replace the ref key only if we're at the top already (only for relative keys) */
      new_index = (ELEM(act_index, 0, 1) || key->type == KEY_NORMAL) ? 0 : 1;
      break;
    case KB_MOVE_BOTTOM:
      new_index = totkey - 1;
      break;
    case KB_MOVE_UP:
    case KB_MOVE_DOWN:
    default:
      new_index = (totkey + act_index + type) % totkey;
      break;
  }

  if (!BKE_keyblock_move(ob, act_index, new_index)) {
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_move(wmOperatorType *ot)
{
  static const EnumPropertyItem slot_move[] = {
      {KB_MOVE_TOP, "TOP", 0, "Top", "Top of the list"},
      {KB_MOVE_UP, "UP", 0, "Up", ""},
      {KB_MOVE_DOWN, "DOWN", 0, "Down", ""},
      {KB_MOVE_BOTTOM, "BOTTOM", 0, "Bottom", "Bottom of the list"},
      {0, nullptr, 0, nullptr, nullptr}};

  /* identifiers */
  ot->name = "Move Shape Key";
  ot->idname = "OBJECT_OT_shape_key_move";
  ot->description = "Move the active shape key up/down in the list";

  /* api callbacks */
  ot->poll = shape_key_move_poll;
  ot->exec = shape_key_move_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna, "type", slot_move, 0, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shape Key Lock (Unlock) Operator
 * \{ */

enum {
  SHAPE_KEY_LOCK,
  SHAPE_KEY_UNLOCK,
};

static int shape_key_lock_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  const int action = RNA_enum_get(op->ptr, "action");
  const Key *keys = BKE_key_from_object(ob);

  if (!keys || BLI_listbase_is_empty(&keys->block)) {
    return OPERATOR_CANCELLED;
  }

  LISTBASE_FOREACH (KeyBlock *, kb, &keys->block) {
    switch (action) {
      case SHAPE_KEY_LOCK:
        kb->flag |= KEYBLOCK_LOCKED_SHAPE;
        break;
      case SHAPE_KEY_UNLOCK:
        kb->flag &= ~KEYBLOCK_LOCKED_SHAPE;
        break;
      default:
        BLI_assert(0);
    }
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

static std::string shape_key_lock_description(bContext * /*C*/,
                                              wmOperatorType * /*op*/,
                                              PointerRNA *params)
{
  const int action = RNA_enum_get(params, "action");

  switch (action) {
    case SHAPE_KEY_LOCK:
      return TIP_("Lock all shape keys of the active object");
      break;
    case SHAPE_KEY_UNLOCK:
      return TIP_("Unlock all shape keys of the active object");
      break;
    default:
      return "";
  }
}

void OBJECT_OT_shape_key_lock(wmOperatorType *ot)
{
  static const EnumPropertyItem shape_key_lock_actions[] = {
      {SHAPE_KEY_LOCK, "LOCK", 0, "Lock", "Lock all shape keys"},
      {SHAPE_KEY_UNLOCK, "UNLOCK", 0, "Unlock", "Unlock all shape keys"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Change the Lock On Shape Keys";
  ot->idname = "OBJECT_OT_shape_key_lock";
  ot->description = "Change the lock state of all shape keys of active object";

  /* api callbacks */
  ot->poll = shape_key_exists_poll;
  ot->exec = shape_key_lock_exec;
  ot->get_description = shape_key_lock_description;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna,
               "action",
               shape_key_lock_actions,
               SHAPE_KEY_LOCK,
               "Action",
               "Lock action to execute on vertex groups");
}

/** \} */

}  // namespace blender::ed::object
