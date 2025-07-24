/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include "ANIM_action.hh"
#include "ANIM_action_iterators.hh"

#include "BLI_assert.h"
#include "BLI_listbase.h"

#include "BKE_anim_data.hh"
#include "BKE_nla.hh"

#include "DNA_constraint_types.h"
#include "DNA_object_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

namespace blender::animrig {

void foreach_fcurve_in_action(Action &action, FunctionRef<void(FCurve &fcurve)> callback)
{
  if (action.is_action_legacy()) {
    LISTBASE_FOREACH (FCurve *, fcurve, &action.curves) {
      callback(*fcurve);
    }
    return;
  }

  for (Layer *layer : action.layers()) {
    for (Strip *strip : layer->strips()) {
      if (strip->type() != Strip::Type::Keyframe) {
        continue;
      }
      for (Channelbag *bag : strip->data<StripKeyframeData>(action).channelbags()) {
        for (FCurve *fcu : bag->fcurves()) {
          callback(*fcu);
        }
      }
    }
  }
}

void foreach_fcurve_in_action_slot(Action &action,
                                   slot_handle_t handle,
                                   FunctionRef<void(FCurve &fcurve)> callback)
{
  if (action.is_action_legacy()) {
    LISTBASE_FOREACH (FCurve *, fcurve, &action.curves) {
      callback(*fcurve);
    }
  }
  else if (action.is_action_layered()) {
    for (Layer *layer : action.layers()) {
      for (Strip *strip : layer->strips()) {
        if (strip->type() != Strip::Type::Keyframe) {
          continue;
        }
        for (Channelbag *bag : strip->data<StripKeyframeData>(action).channelbags()) {
          if (bag->slot_handle != handle) {
            continue;
          }
          for (FCurve *fcu : bag->fcurves()) {
            BLI_assert(fcu != nullptr);
            callback(*fcu);
          }
        }
      }
    }
  }
}

bool foreach_action_slot_use(
    const ID &animated_id,
    FunctionRef<bool(const Action &action, slot_handle_t slot_handle)> callback)
{

  const auto forward_to_callback = [&](ID & /* animated_id */,
                                       bAction *&action_ptr_ref,
                                       const slot_handle_t &slot_handle_ref,
                                       char * /*last_slot_identifier*/) -> bool {
    if (!action_ptr_ref) {
      return true;
    }
    return callback(const_cast<const Action &>(action_ptr_ref->wrap()), slot_handle_ref);
  };

  return foreach_action_slot_use_with_references(const_cast<ID &>(animated_id),
                                                 forward_to_callback);
}

bool foreach_action_slot_use_with_references(
    ID &animated_id,
    FunctionRef<bool(ID &animated_id,
                     bAction *&action_ptr_ref,
                     slot_handle_t &slot_handle_ref,
                     char *last_slot_identifier)> callback)
{
  AnimData *adt = BKE_animdata_from_id(&animated_id);

  if (adt) {
    if (adt->action) {
      /* Direct assignment. */
      if (!callback(animated_id, adt->action, adt->slot_handle, adt->last_slot_identifier)) {
        return false;
      }
    }

    /* NLA strips. */
    const bool looped_until_last_strip = bke::nla::foreach_strip_adt(*adt, [&](NlaStrip *strip) {
      if (strip->act) {
        if (!callback(
                animated_id, strip->act, strip->action_slot_handle, strip->last_slot_identifier))
        {
          return false;
        }
      }
      return true;
    });
    if (!looped_until_last_strip) {
      return false;
    }
  }

  /* The rest of the code deals with constraints, so only relevant when this is an Object. */
  if (GS(animated_id.name) != ID_OB) {
    return true;
  }

  const Object &object = reinterpret_cast<const Object &>(animated_id);

  /**
   * Visit a constraint, and call the callback if it's an Action constraint.
   *
   * \returns whether to continue looping over possible uses of Actions, i.e.
   * the return value of the callback.
   */
  auto visit_constraint = [&](const bConstraint &constraint) -> bool {
    if (constraint.type != CONSTRAINT_TYPE_ACTION) {
      return true;
    }
    bActionConstraint *constraint_data = static_cast<bActionConstraint *>(constraint.data);
    if (!constraint_data->act) {
      return true;
    }
    return callback(animated_id,
                    constraint_data->act,
                    constraint_data->action_slot_handle,
                    constraint_data->last_slot_identifier);
  };

  /* Visit Object constraints. */
  LISTBASE_FOREACH (bConstraint *, con, &object.constraints) {
    if (!visit_constraint(*con)) {
      return false;
    }
  }

  /* Visit Pose Bone constraints. */
  if (object.type == OB_ARMATURE) {
    LISTBASE_FOREACH (bPoseChannel *, pchan, &object.pose->chanbase) {
      LISTBASE_FOREACH (bConstraint *, con, &pchan->constraints) {
        if (!visit_constraint(*con)) {
          return false;
        }
      }
    }
  }

  return true;
}

bool foreach_action_slot_use_with_rna(ID &animated_id,
                                      FunctionRef<bool(ID &animated_id,
                                                       bAction *action,
                                                       PointerRNA &action_slot_ptr,
                                                       PropertyRNA &action_slot_prop,
                                                       char *last_slot_identifier)> callback)
{
  /* This function has to copy the logic of #foreach_action_slot_use_with_references(),
   * as it needs to know where exactly those pointers came from. */

  AnimData *adt = BKE_animdata_from_id(&animated_id);

  if (adt) {
    if (adt->action) {
      /* Direct assignment. */
      PointerRNA ptr = RNA_pointer_create_discrete(&animated_id, &RNA_AnimData, adt);
      PropertyRNA *prop = RNA_struct_find_property(&ptr, "action_slot");
      if (!callback(animated_id, adt->action, ptr, *prop, adt->last_slot_identifier)) {
        return false;
      }
    }

    /* NLA strips. */
    const bool looped_until_last_strip = bke::nla::foreach_strip_adt(*adt, [&](NlaStrip *strip) {
      if (strip->act) {
        PointerRNA ptr = RNA_pointer_create_discrete(&animated_id, &RNA_NlaStrip, strip);
        PropertyRNA *prop = RNA_struct_find_property(&ptr, "action_slot");

        if (!callback(animated_id, strip->act, ptr, *prop, strip->last_slot_identifier)) {
          return false;
        }
      }
      return true;
    });
    if (!looped_until_last_strip) {
      return false;
    }
  }

  /* The rest of the code deals with constraints, so only relevant when this is an Object. */
  if (GS(animated_id.name) != ID_OB) {
    return true;
  }

  const Object &object = reinterpret_cast<const Object &>(animated_id);

  /**
   * Visit a constraint, and call the callback if it's an Action constraint.
   *
   * \returns whether to continue looping over possible uses of Actions, i.e.
   * the return value of the callback.
   */
  auto visit_constraint = [&](bConstraint &constraint) -> bool {
    if (constraint.type != CONSTRAINT_TYPE_ACTION) {
      return true;
    }
    bActionConstraint *constraint_data = static_cast<bActionConstraint *>(constraint.data);
    if (!constraint_data->act) {
      return true;
    }

    PointerRNA ptr = RNA_pointer_create_discrete(&animated_id, &RNA_ActionConstraint, &constraint);
    PropertyRNA *prop = RNA_struct_find_property(&ptr, "action_slot");

    return callback(
        animated_id, constraint_data->act, ptr, *prop, constraint_data->last_slot_identifier);
  };

  /* Visit Object constraints. */
  LISTBASE_FOREACH (bConstraint *, con, &object.constraints) {
    if (!visit_constraint(*con)) {
      return false;
    }
  }

  /* Visit Pose Bone constraints. */
  if (object.type == OB_ARMATURE) {
    LISTBASE_FOREACH (bPoseChannel *, pchan, &object.pose->chanbase) {
      LISTBASE_FOREACH (bConstraint *, con, &pchan->constraints) {
        if (!visit_constraint(*con)) {
          return false;
        }
      }
    }
  }

  return true;
}

}  // namespace blender::animrig
