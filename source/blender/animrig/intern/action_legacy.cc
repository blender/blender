/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "ANIM_action.hh"
#include "ANIM_action_legacy.hh"

namespace blender::animrig::legacy {

static Strip *first_keyframe_strip(Action &action)
{
  for (Layer *layer : action.layers()) {
    for (Strip *strip : layer->strips()) {
      if (strip->type() == Strip::Type::Keyframe) {
        return strip;
      }
    }
  }

  return nullptr;
}

ChannelBag *channelbag_get(Action &action)
{
  if (action.slots().is_empty()) {
    return nullptr;
  }

  Strip *keystrip = first_keyframe_strip(action);
  if (!keystrip) {
    return nullptr;
  }

  return keystrip->data<StripKeyframeData>(action).channelbag_for_slot(*action.slot(0));
}

ChannelBag &channelbag_ensure(Action &action)
{
  assert_baklava_phase_1_invariants(action);

  /* Ensure a Slot. */
  Slot *slot;
  if (action.slots().is_empty()) {
    slot = &action.slot_add();
  }
  else {
    slot = action.slot(0);
  }

  /* Ensure a Layer + keyframe Strip. */
  action.layer_keystrip_ensure();
  Strip &keystrip = *action.layer(0)->strip(0);

  /* Ensure a ChannelBag. */
  return keystrip.data<StripKeyframeData>(action).channelbag_for_slot_ensure(*slot);
}

/* Lots of template args to support transparent non-const and const versions. */
template<typename ActionType,
         typename FCurveType,
         typename LayerType,
         typename StripType,
         typename StripKeyframeDataType,
         typename ChannelBagType>
static Vector<FCurveType *> fcurves_all_templated(ActionType &action)
{
#ifdef WITH_ANIM_BAKLAVA
  /* Legacy Action. */
  if (action.is_action_legacy()) {
#endif /* WITH_ANIM_BAKLAVA */
    Vector<FCurveType *> legacy_fcurves;
    LISTBASE_FOREACH (FCurveType *, fcurve, &action.curves) {
      legacy_fcurves.append(fcurve);
    }
    return legacy_fcurves;
#ifdef WITH_ANIM_BAKLAVA
  }

  /* Layered Action. */
  BLI_assert(action.is_action_layered());

  Vector<FCurveType *> all_fcurves;
  for (LayerType *layer : action.layers()) {
    for (StripType *strip : layer->strips()) {
      switch (strip->type()) {
        case Strip::Type::Keyframe: {
          StripKeyframeDataType &strip_data = strip->template data<StripKeyframeData>(action);
          for (ChannelBagType *bag : strip_data.channelbags()) {
            for (FCurveType *fcurve : bag->fcurves()) {
              all_fcurves.append(fcurve);
            }
          }
        }
      }
    }
  }
  return all_fcurves;
#endif /* WITH_ANIM_BAKLAVA */
}

Vector<FCurve *> fcurves_all(bAction *action)
{
  if (!action) {
    return {};
  }
  return fcurves_all_templated<Action, FCurve, Layer, Strip, StripKeyframeData, ChannelBag>(
      action->wrap());
}

Vector<const FCurve *> fcurves_all(const bAction *action)
{
  if (!action) {
    return {};
  }
  return fcurves_all_templated<const Action,
                               const FCurve,
                               const Layer,
                               const Strip,
                               const StripKeyframeData,
                               const ChannelBag>(action->wrap());
}

/* Lots of template args to support transparent non-const and const versions. */
template<typename ActionType,
         typename FCurveType,
         typename LayerType,
         typename StripType,
         typename StripKeyframeDataType,
         typename ChannelBagType>
static Vector<FCurveType *> fcurves_for_action_slot_templated(ActionType &action,
                                                              const slot_handle_t slot_handle)
{
#ifndef WITH_ANIM_BAKLAVA
  UNUSED_VARS(slot_handle);
#endif /* !WITH_ANIM_BAKLAVA */

#ifdef WITH_ANIM_BAKLAVA
  /* Legacy Action. */
  if (action.is_action_legacy()) {
#endif /* WITH_ANIM_BAKLAVA */
    Vector<FCurveType *> legacy_fcurves;
    LISTBASE_FOREACH (FCurveType *, fcurve, &action.curves) {
      legacy_fcurves.append(fcurve);
    }
    return legacy_fcurves;
#ifdef WITH_ANIM_BAKLAVA
  }

  /* Layered Action. */
  Vector<FCurveType *> as_vector(animrig::fcurves_for_action_slot(action, slot_handle));
  return as_vector;
#endif /* WITH_ANIM_BAKLAVA */
}

Vector<FCurve *> fcurves_for_action_slot(bAction *action, const slot_handle_t slot_handle)
{
  if (!action) {
    return {};
  }
  return fcurves_for_action_slot_templated<Action,
                                           FCurve,
                                           Layer,
                                           Strip,
                                           StripKeyframeData,
                                           ChannelBag>(action->wrap(), slot_handle);
}
Vector<const FCurve *> fcurves_for_action_slot(const bAction *action,
                                               const slot_handle_t slot_handle)
{
  if (!action) {
    return {};
  }
  return fcurves_for_action_slot_templated<const Action,
                                           const FCurve,
                                           const Layer,
                                           const Strip,
                                           const StripKeyframeData,
                                           const ChannelBag>(action->wrap(), slot_handle);
}

Vector<FCurve *> fcurves_for_assigned_action(AnimData *adt)
{
  if (!adt || !adt->action) {
    return {};
  }
  return legacy::fcurves_for_action_slot(adt->action, adt->slot_handle);
}
Vector<const FCurve *> fcurves_for_assigned_action(const AnimData *adt)
{
  if (!adt || !adt->action) {
    return {};
  }
  return legacy::fcurves_for_action_slot(const_cast<const bAction *>(adt->action),
                                         adt->slot_handle);
}

bool assigned_action_has_keyframes(AnimData *adt)
{
  if (adt == nullptr || adt->action == nullptr) {
    return false;
  }

  Action &action = adt->action->wrap();

  if (action.is_action_legacy()) {
    return action.curves.first != nullptr;
  }

  return action.has_keyframes(adt->slot_handle);
}

}  // namespace blender::animrig::legacy
