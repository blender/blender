/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include "ANIM_action.hh"
#include "ANIM_action_iterators.hh"

#include "BLI_assert.h"

#include "BKE_anim_data.hh"
#include "BKE_nla.hh"

#include "DNA_constraint_types.h"

namespace blender::animrig {

void action_foreach_fcurve(Action &action,
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
        for (ChannelBag *bag : strip->data<StripKeyframeData>(action).channelbags()) {
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
  AnimData *adt = BKE_animdata_from_id(&animated_id);

  if (adt) {
    if (adt->action) {
      /* Direct assignment. */
      if (!callback(adt->action->wrap(), adt->slot_handle)) {
        return false;
      }
    }

    /* NLA strips. */
    const bool looped_until_last_strip = bke::nla::foreach_strip_adt(*adt, [&](NlaStrip *strip) {
      if (strip->act) {
        if (!callback(strip->act->wrap(), strip->action_slot_handle)) {
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
    return callback(constraint_data->act->wrap(), constraint_data->action_slot_handle);
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
