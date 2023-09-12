/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edarmature
 */

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "DNA_armature_types.h"
#include "DNA_layer_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_array_utils.h"
#include "BLI_listbase.h"
#include "BLI_map.hh"

#include "BKE_armature.h"
#include "BKE_context.h"
#include "BKE_idprop.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_undo_system.h"

#include "DEG_depsgraph.h"

#include "ED_armature.hh"
#include "ED_object.hh"
#include "ED_undo.hh"
#include "ED_util.hh"

#include "ANIM_bone_collections.hh"

#include "WM_api.hh"
#include "WM_types.hh"

using namespace blender::animrig;

/** We only need this locally. */
static CLG_LogRef LOG = {"ed.undo.armature"};

/* Utility functions. */

/**
 * Remaps editbone collection membership.
 *
 * This is intended to be used in combination with ED_armature_ebone_listbase_copy()
 * and ANIM_bonecoll_listbase_copy() to make a full duplicate of both edit
 * bones and collections together.
 */
static void remap_ebone_bone_collection_references(
    ListBase /* EditBone */ *edit_bones,
    const blender::Map<BoneCollection *, BoneCollection *> &bcoll_map)
{
  LISTBASE_FOREACH (EditBone *, ebone, edit_bones) {
    LISTBASE_FOREACH (BoneCollectionReference *, bcoll_ref, &ebone->bone_collections) {
      bcoll_ref->bcoll = bcoll_map.lookup(bcoll_ref->bcoll);
    }
  }
}

/* -------------------------------------------------------------------- */
/** \name Undo Conversion
 * \{ */

struct UndoArmature {
  EditBone *act_edbone;
  BoneCollection *active_collection;
  ListBase /* EditBone */ ebones;
  ListBase /* BoneCollection */ bone_collections;
  size_t undo_size;
};

static void undoarm_to_editarm(UndoArmature *uarm, bArmature *arm)
{
  /* Copy edit bones. */
  ED_armature_ebone_listbase_free(arm->edbo, true);
  ED_armature_ebone_listbase_copy(arm->edbo, &uarm->ebones, true);

  /* Active bone. */
  if (uarm->act_edbone) {
    EditBone *ebone;
    ebone = uarm->act_edbone;
    arm->act_edbone = ebone->temp.ebone;
  }
  else {
    arm->act_edbone = nullptr;
  }

  ED_armature_ebone_listbase_temp_clear(arm->edbo);

  /* Copy bone collections. */
  ANIM_bonecoll_listbase_free(&arm->collections, true);
  auto bcoll_map = ANIM_bonecoll_listbase_copy_no_membership(
      &arm->collections, &uarm->bone_collections, true);
  arm->active_collection = bcoll_map.lookup_default(uarm->active_collection, nullptr);

  remap_ebone_bone_collection_references(arm->edbo, bcoll_map);

  ANIM_armature_runtime_refresh(arm);
}

static void *undoarm_from_editarm(UndoArmature *uarm, bArmature *arm)
{
  BLI_assert(BLI_array_is_zeroed(uarm, 1));

  /* Copy edit bones. */
  ED_armature_ebone_listbase_copy(&uarm->ebones, arm->edbo, false);

  /* Active bone. */
  if (arm->act_edbone) {
    EditBone *ebone = arm->act_edbone;
    uarm->act_edbone = ebone->temp.ebone;
  }

  ED_armature_ebone_listbase_temp_clear(&uarm->ebones);

  /* Copy bone collections. */
  auto bcoll_map = ANIM_bonecoll_listbase_copy_no_membership(
      &uarm->bone_collections, &arm->collections, false);
  uarm->active_collection = bcoll_map.lookup_default(arm->active_collection, nullptr);

  /* Point the new edit bones at the new collections. */
  remap_ebone_bone_collection_references(&uarm->ebones, bcoll_map);

  /* Undo size.
   * TODO: include size of ID-properties. */
  uarm->undo_size = 0;
  LISTBASE_FOREACH (EditBone *, ebone, &uarm->ebones) {
    uarm->undo_size += sizeof(EditBone);
    uarm->undo_size += sizeof(BoneCollectionReference) *
                       BLI_listbase_count(&ebone->bone_collections);
  }
  uarm->undo_size += sizeof(BoneCollection) * BLI_listbase_count(&uarm->bone_collections);

  return uarm;
}

static void undoarm_free_data(UndoArmature *uarm)
{
  ED_armature_ebone_listbase_free(&uarm->ebones, false);
  ANIM_bonecoll_listbase_free(&uarm->bone_collections, false);
}

static Object *editarm_object_from_context(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obedit = BKE_view_layer_edit_object_get(view_layer);
  if (obedit && obedit->type == OB_ARMATURE) {
    bArmature *arm = static_cast<bArmature *>(obedit->data);
    if (arm->edbo != nullptr) {
      return obedit;
    }
  }
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 *
 * \note This is similar for all edit-mode types.
 * \{ */

struct ArmatureUndoStep_Elem {
  ArmatureUndoStep_Elem *next, *prev;
  UndoRefID_Object obedit_ref;
  UndoArmature data;
};

struct ArmatureUndoStep {
  UndoStep step;
  ArmatureUndoStep_Elem *elems;
  uint elems_len;
};

static bool armature_undosys_poll(bContext *C)
{
  return editarm_object_from_context(C) != nullptr;
}

static bool armature_undosys_step_encode(bContext *C, Main *bmain, UndoStep *us_p)
{
  ArmatureUndoStep *us = (ArmatureUndoStep *)us_p;

  /* Important not to use the 3D view when getting objects because all objects
   * outside of this list will be moved out of edit-mode when reading back undo steps. */
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = ED_undo_editmode_objects_from_view_layer(scene, view_layer, &objects_len);

  us->elems = static_cast<ArmatureUndoStep_Elem *>(
      MEM_callocN(sizeof(*us->elems) * objects_len, __func__));
  us->elems_len = objects_len;

  for (uint i = 0; i < objects_len; i++) {
    Object *ob = objects[i];
    ArmatureUndoStep_Elem *elem = &us->elems[i];

    elem->obedit_ref.ptr = ob;
    bArmature *arm = static_cast<bArmature *>(elem->obedit_ref.ptr->data);
    undoarm_from_editarm(&elem->data, arm);
    arm->needs_flush_to_id = 1;
    us->step.data_size += elem->data.undo_size;
  }
  MEM_freeN(objects);

  bmain->is_memfile_undo_flush_needed = true;

  return true;
}

static void armature_undosys_step_decode(
    bContext *C, Main *bmain, UndoStep *us_p, const eUndoStepDir /*dir*/, bool /*is_final*/)
{
  ArmatureUndoStep *us = (ArmatureUndoStep *)us_p;

  ED_undo_object_editmode_restore_helper(
      C, &us->elems[0].obedit_ref.ptr, us->elems_len, sizeof(*us->elems));

  BLI_assert(BKE_object_is_in_editmode(us->elems[0].obedit_ref.ptr));

  for (uint i = 0; i < us->elems_len; i++) {
    ArmatureUndoStep_Elem *elem = &us->elems[i];
    Object *obedit = elem->obedit_ref.ptr;
    bArmature *arm = static_cast<bArmature *>(obedit->data);
    if (arm->edbo == nullptr) {
      /* Should never fail, may not crash but can give odd behavior. */
      CLOG_ERROR(&LOG,
                 "name='%s', failed to enter edit-mode for object '%s', undo state invalid",
                 us_p->name,
                 obedit->id.name);
      continue;
    }
    undoarm_to_editarm(&elem->data, arm);
    arm->needs_flush_to_id = 1;
    DEG_id_tag_update(&arm->id, ID_RECALC_GEOMETRY);
  }

  /* The first element is always active */
  ED_undo_object_set_active_or_warn(
      CTX_data_scene(C), CTX_data_view_layer(C), us->elems[0].obedit_ref.ptr, us_p->name, &LOG);

  /* Check after setting active. */
  BLI_assert(armature_undosys_poll(C));

  bmain->is_memfile_undo_flush_needed = true;

  WM_event_add_notifier(C, NC_GEOM | ND_DATA, nullptr);
}

static void armature_undosys_step_free(UndoStep *us_p)
{
  ArmatureUndoStep *us = (ArmatureUndoStep *)us_p;

  for (uint i = 0; i < us->elems_len; i++) {
    ArmatureUndoStep_Elem *elem = &us->elems[i];
    undoarm_free_data(&elem->data);
  }
  MEM_freeN(us->elems);
}

static void armature_undosys_foreach_ID_ref(UndoStep *us_p,
                                            UndoTypeForEachIDRefFn foreach_ID_ref_fn,
                                            void *user_data)
{
  ArmatureUndoStep *us = (ArmatureUndoStep *)us_p;

  for (uint i = 0; i < us->elems_len; i++) {
    ArmatureUndoStep_Elem *elem = &us->elems[i];
    foreach_ID_ref_fn(user_data, ((UndoRefID *)&elem->obedit_ref));
  }
}

void ED_armature_undosys_type(UndoType *ut)
{
  ut->name = "Edit Armature";
  ut->poll = armature_undosys_poll;
  ut->step_encode = armature_undosys_step_encode;
  ut->step_decode = armature_undosys_step_decode;
  ut->step_free = armature_undosys_step_free;

  ut->step_foreach_ID_ref = armature_undosys_foreach_ID_ref;

  ut->flags = UNDOTYPE_FLAG_NEED_CONTEXT_FOR_ENCODE;

  ut->step_size = sizeof(ArmatureUndoStep);
}

/** \} */
