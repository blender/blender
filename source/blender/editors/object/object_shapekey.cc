/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 */

#include <cstring>

#ifndef WIN32
#  include <unistd.h>
#else
#  include <io.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
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
#include "BKE_library.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

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

bool shape_key_is_selected(const Object &object, const KeyBlock &kb, const int keyblock_index)
{
  /* The active shape key is always considered selected. */
  return (kb.flag & KEYBLOCK_SEL) || keyblock_index == object.shapenr - 1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Shape Key Function
 * \{ */

static void object_shape_key_add(bContext *C, Object *ob, const bool from_mix)
{
  Main *bmain = CTX_data_main(C);
  KeyBlock *kb = BKE_object_shapekey_insert(bmain, ob, nullptr, from_mix);
  if (kb) {
    /* Shapekeys created via this operator should get default value 1.0. */
    kb->curval = 1.0f;

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

void shape_key_mirror(
    Object *ob, KeyBlock *kb, const bool use_topology, int &totmirr, int &totfail)
{
  char *tag_elem = MEM_calloc_arrayN<char>(kb->totelem, "shape_key_mirror");

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

static bool object_shape_key_mirror(
    bContext *C, Object *ob, int *r_totmirr, int *r_totfail, bool use_topology)
{
  Key *key;
  int totmirr = 0, totfail = 0;

  *r_totmirr = *r_totfail = 0;

  key = BKE_key_from_object(ob);
  if (key == nullptr) {
    return false;
  }

  if (KeyBlock *kb = static_cast<KeyBlock *>(BLI_findlink(&key->block, ob->shapenr - 1))) {
    shape_key_mirror(ob, kb, use_topology, totmirr, totfail);
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

  return (ob != nullptr && ID_IS_EDITABLE(ob) && !ID_IS_OVERRIDE_LIBRARY(ob) && data != nullptr &&
          ID_IS_EDITABLE(data) && !ID_IS_OVERRIDE_LIBRARY(data));
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

static wmOperatorStatus shape_key_add_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_object(C);
  const bool from_mix = RNA_boolean_get(op->ptr, "from_mix");

  object_shape_key_add(C, ob, from_mix);

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

  /* API callbacks. */
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
/** \name Shape Key Duplicate Operator
 * \{ */

static wmOperatorStatus shape_key_copy_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = context_object(C);
  Key *key = BKE_key_from_object(ob);
  KeyBlock *kb_src = BKE_keyblock_from_object(ob);
  KeyBlock *kb_new = BKE_keyblock_duplicate(key, kb_src);
  ob->shapenr = BLI_findindex(&key->block, kb_new) + 1;
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(CTX_data_main(C));
  return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_copy(wmOperatorType *ot)
{
  ot->name = "Duplicate Shape Key";
  ot->idname = "OBJECT_OT_shape_key_copy";
  ot->description = "Duplicate the active shape key";

  ot->poll = shape_key_mode_exists_poll;
  ot->exec = shape_key_copy_exec;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shape Key Remove Operator
 * \{ */

static wmOperatorStatus shape_key_remove_exec(bContext *C, wmOperator *op)
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
    int num_selected_but_locked = 0;
    /* This could be moved into a function of its own at some point. Right now it's only used here,
     * though, since its inner structure is tailored for allowing shapekey deletion. */
    Key &key = *BKE_key_from_object(ob);
    LISTBASE_FOREACH_MUTABLE (KeyBlock *, kb, &key.block) {
      /* Always try to find the keyblock again, as the previous one may have been deleted. For
       * the same reason, ob->shapenr has to be re-evaluated on every loop iteration. */
      const int cur_index = BLI_findindex(&key.block, kb);
      if (!shape_key_is_selected(*ob, *kb, cur_index)) {
        continue;
      }
      if (kb->flag & KEYBLOCK_LOCKED_SHAPE) {
        num_selected_but_locked++;
        continue;
      }

      changed |= BKE_object_shapekey_remove(bmain, ob, kb);

      /* When `BKE_object_shapekey_remove()` deletes the active shapekey, the active shapekeyindex
       * is updated as well. It usually decrements, which means that even when the same index is
       * re-visited, we don't see the active one any more. However, when the basis key (index=0) is
       * deleted AND there are keys remaining, the active index remains set to 0, and so every
       * iteration sees "the active shapekey", effectively deleting all of them. */
      if (cur_index == 0) {
        ob->shapenr = 0;
      }
    }

    if (num_selected_but_locked) {
      BKE_reportf(op->reports,
                  changed ? RPT_WARNING : RPT_ERROR,
                  "Could not delete %d locked shape key(s)",
                  num_selected_but_locked);
    }
  }

  /* Ensure that there is still a shapekey active, if there are any. See the comment above. Be
   * extra careful here, because the deletion of the last shapekey can delete the entire Key ID,
   * making our `key` reference (from the code above) invalid. */
  if (ob->shapenr == 0) {
    Key *key = BKE_key_from_object(ob);
    if (key && key->totkey > 0) {
      ob->shapenr = 1;
    }
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

  /* API callbacks. */
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

static wmOperatorStatus shape_key_clear_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = context_object(C);
  Key *key = BKE_key_from_object(ob);

  if (!key || BLI_listbase_is_empty(&key->block)) {
    return OPERATOR_CANCELLED;
  }

  LISTBASE_FOREACH (KeyBlock *, kb, &key->block) {
    kb->curval = clamp_f(0.0f, kb->slidermin, kb->slidermax);
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Shape Keys";
  ot->description =
      "Reset the weights of all shape keys to 0 or to the closest value respecting the limits";
  ot->idname = "OBJECT_OT_shape_key_clear";

  /* API callbacks. */
  ot->poll = shape_key_poll;
  ot->exec = shape_key_clear_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* starting point and step size could be optional */
static wmOperatorStatus shape_key_retime_exec(bContext *C, wmOperator * /*op*/)
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

  /* API callbacks. */
  ot->poll = shape_key_poll;
  ot->exec = shape_key_retime_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shape Key Mirror Operator
 * \{ */

static wmOperatorStatus shape_key_mirror_exec(bContext *C, wmOperator *op)
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

  ED_mesh_report_mirror(*op->reports, totmirr, totfail);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_mirror(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Mirror Shape Key";
  ot->idname = "OBJECT_OT_shape_key_mirror";
  ot->description = "Mirror the current shape key along the local X axis";

  /* API callbacks. */
  ot->poll = shape_key_mode_exists_poll;
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

enum KeyBlockMove {
  KB_MOVE_TOP = -2,
  KB_MOVE_UP = -1,
  KB_MOVE_DOWN = 1,
  KB_MOVE_BOTTOM = 2,
};

static wmOperatorStatus shape_key_move_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_object(C);

  const Key &key = *BKE_key_from_object(ob);
  const KeyBlockMove type = KeyBlockMove(RNA_enum_get(op->ptr, "type"));
  const int totkey = key.totkey;
  int new_index = 0;
  bool changed = false;

  if (type < 0) { /* Moving upwards. */
    /* Don't move above the position of the basis key */
    int top_index = 1;
    /* Start from index 1 to ignore basis key from being able to move above. */
    for (int index = 1; index < totkey; index++) {
      const KeyBlock &kb = *static_cast<KeyBlock *>(BLI_findlink(&key.block, index));
      if (!shape_key_is_selected(*ob, kb, index)) {
        continue;
      }
      switch (type) {
        case KB_MOVE_TOP:
          new_index = top_index;
          break;
        case KB_MOVE_UP:
          new_index = max_ii(index - 1, top_index);
          break;
        case KB_MOVE_BOTTOM:
        case KB_MOVE_DOWN:
          BLI_assert_unreachable();
          break;
      }
      top_index++;
      if (new_index < 0) {
        continue;
      }
      changed |= BKE_keyblock_move(ob, index, new_index);
    }
  }
  else { /* Moving downwards. */
    int bottom_index = totkey - 1;
    /* Skip basis key to prevent it from moving downwards. */
    for (int index = totkey - 1; index >= 1; index--) {
      const KeyBlock &kb = *static_cast<KeyBlock *>(BLI_findlink(&key.block, index));
      if (!shape_key_is_selected(*ob, kb, index)) {
        continue;
      }
      switch (type) {
        case KB_MOVE_BOTTOM:
          new_index = bottom_index;
          break;
        case KB_MOVE_DOWN:
          new_index = min_ii(index + 1, bottom_index);
          break;
        case KB_MOVE_TOP:
        case KB_MOVE_UP:
          BLI_assert_unreachable();
          break;
      }
      bottom_index--;
      changed |= BKE_keyblock_move(ob, index, new_index);
    }
  }

  if (!changed) {
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
  ot->description = "Move selected shape keys up/down in the list";

  /* API callbacks. */
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

static wmOperatorStatus shape_key_lock_exec(bContext *C, wmOperator *op)
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

static std::string shape_key_lock_get_description(bContext * /*C*/,
                                                  wmOperatorType * /*op*/,
                                                  PointerRNA *ptr)
{
  const int action = RNA_enum_get(ptr, "action");

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

  /* API callbacks. */
  ot->poll = shape_key_exists_poll;
  ot->exec = shape_key_lock_exec;
  ot->get_description = shape_key_lock_get_description;

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

/* -------------------------------------------------------------------- */
/** \name Shape Key Make Basis Operator
 * \{ */

static bool shape_key_make_basis_poll(bContext *C)
{
  if (!shape_key_exists_poll(C)) {
    return false;
  }

  Object *ob = context_object(C);
  /* 0 = nothing active, 1 = basis key active. */
  return ob->shapenr > 1;
}

static wmOperatorStatus shape_key_make_basis_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = CTX_data_active_object(C);
  Key *key = BKE_key_from_object(ob);
  KeyBlock *old_basis_key = static_cast<KeyBlock *>(key->block.first);

  /* Make the new basis by moving the active key to index 0. */
  const int from_index = -1; /* Interpreted as "the active key". */
  const int to_index = 0;    /* Offset by 1 compared to ob->shapenr. */
  const bool changed = BKE_keyblock_move(ob, from_index, to_index);

  if (!changed) {
    /* The poll function should have prevented this operator from being called
     * on the current basis key. */
    BLI_assert_unreachable();
    return OPERATOR_CANCELLED;
  }

  /* Make the old & new basis keys "Relative to" the new basis key. For the new key it doesn't
   * matter much, as it's treated as special anyway, but keeping it relative to another key makes
   * no sense. For the old basis key (which just became a normal key), it would otherwise still be
   * relative to itself, effectively disabling it. */
  KeyBlock *new_basis_key = static_cast<KeyBlock *>(key->block.first);
  new_basis_key->relative = 0;
  old_basis_key->relative = 0;

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_make_basis(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Shape Key the Basis Key";
  ot->idname = "OBJECT_OT_shape_key_make_basis";
  ot->description =
      "Make this shape key the new basis key, effectively applying it to the mesh. Note that this "
      "applies the shape key at its 100% value";

  /* API callbacks. */
  ot->poll = shape_key_make_basis_poll;
  ot->exec = shape_key_make_basis_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

}  // namespace blender::ed::object
