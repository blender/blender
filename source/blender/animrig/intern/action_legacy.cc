/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "ANIM_action.hh"
#include "ANIM_action_legacy.hh"

#include "BLI_listbase_wrapper.hh"

#include "BKE_fcurve.hh"

#include "BLT_translation.hh"

namespace blender::animrig::legacy {

/* Lots of template args to support transparent non-const and const versions. */
template<typename ActionType,
         typename FCurveType,
         typename LayerType,
         typename StripType,
         typename StripKeyframeDataType,
         typename ChannelbagType>
static Vector<FCurveType *> fcurves_all_templated(ActionType &action)
{
  Vector<FCurveType *> all_fcurves;
  for (LayerType *layer : action.layers()) {
    for (StripType *strip : layer->strips()) {
      switch (strip->type()) {
        case Strip::Type::Keyframe: {
          StripKeyframeDataType &strip_data = strip->template data<StripKeyframeData>(action);
          for (ChannelbagType *bag : strip_data.channelbags()) {
            for (FCurveType *fcurve : bag->fcurves()) {
              all_fcurves.append(fcurve);
            }
          }
        }
      }
    }
  }
  return all_fcurves;
}

Vector<FCurve *> fcurves_all(bAction *action)
{
  if (!action) {
    return {};
  }
  return fcurves_all_templated<Action, FCurve, Layer, Strip, StripKeyframeData, Channelbag>(
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
                               const Channelbag>(action->wrap());
}

/* Lots of template args to support transparent non-const and const versions. */
template<typename ActionType,
         typename FCurveType,
         typename LayerType,
         typename StripType,
         typename StripKeyframeDataType,
         typename ChannelbagType>
static Vector<FCurveType *> fcurves_for_action_slot_templated(ActionType &action,
                                                              const slot_handle_t slot_handle)
{
  Vector<FCurveType *> as_vector(animrig::fcurves_for_action_slot(action, slot_handle));
  return as_vector;
}

bool assigned_action_has_keyframes(AnimData *adt)
{
  if (adt == nullptr || adt->action == nullptr) {
    return false;
  }

  Action &action = adt->action->wrap();
  return action.has_keyframes(adt->slot_handle);
}

Vector<bActionGroup *> channel_groups_all(bAction *action)
{
  if (!action) {
    return {};
  }

  Action &action_wrap = action->wrap();
  Vector<bActionGroup *> all_groups;
  for (Layer *layer : action_wrap.layers()) {
    for (Strip *strip : layer->strips()) {
      switch (strip->type()) {
        case Strip::Type::Keyframe: {
          StripKeyframeData &strip_data = strip->template data<StripKeyframeData>(action_wrap);
          for (Channelbag *bag : strip_data.channelbags()) {
            all_groups.extend(bag->channel_groups());
          }
        }
      }
    }
  }
  return all_groups;
}

Vector<bActionGroup *> channel_groups_for_assigned_slot(AnimData *adt)
{
  if (!adt || !adt->action) {
    return {};
  }

  Action &action = adt->action->wrap();
  Channelbag *bag = channelbag_for_action_slot(action, adt->slot_handle);
  if (!bag) {
    return {};
  }

  Vector<bActionGroup *> slot_groups(bag->channel_groups());
  return slot_groups;
}

bool action_fcurves_remove(bAction &action,
                           const slot_handle_t slot_handle,
                           const StringRefNull rna_path_prefix)
{
  BLI_assert(!rna_path_prefix.is_empty());
  if (rna_path_prefix.is_empty()) {
    return false;
  }

  Channelbag *bag = channelbag_for_action_slot(action.wrap(), slot_handle);
  if (!bag) {
    return false;
  }

  bool any_removed = false;
  for (int64_t fcurve_index = 0; fcurve_index < bag->fcurve_array_num; fcurve_index++) {
    FCurve *fcurve = bag->fcurve(fcurve_index);
    if (!fcurve->rna_path) {
      continue;
    }

    if (STRPREFIX(fcurve->rna_path, rna_path_prefix.c_str())) {
      bag->fcurve_remove_by_index(fcurve_index);
      fcurve_index--;
      any_removed = true;
    }
  }
  return any_removed;
}

}  // namespace blender::animrig::legacy
