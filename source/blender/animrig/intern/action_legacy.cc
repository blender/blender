/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "ANIM_action.hh"
#include "ANIM_action_legacy.hh"

#include "BLI_listbase_wrapper.hh"

#include "BKE_fcurve.hh"

#include "BLT_translation.hh"

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

Slot &slot_ensure(Action &action)
{
  assert_baklava_phase_1_invariants(action);

  if (!action.slots().is_empty()) {
    return *action.slot(0);
  }

  Slot &slot = action.slot_add();
  action.slot_display_name_define(slot, DATA_(DEFAULT_LEGACY_SLOT_NAME));
  return slot;
}

Channelbag *channelbag_get(Action &action)
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

Channelbag &channelbag_ensure(Action &action)
{
  assert_baklava_phase_1_invariants(action);

  Slot &slot = slot_ensure(action);

  /* Ensure a Layer + keyframe Strip.
   *
   * Normally we would use `Action::layer_keystrip_ensure()` for this, but that
   * doesn't let us specify the name of the layer if newly created. */
  if (action.layers().is_empty()) {
    action.layer_add(DATA_(DEFAULT_LEGACY_LAYER_NAME));
  }
  if (action.layer(0)->strips().is_empty()) {
    action.layer(0)->strip_add(action, Strip::Type::Keyframe);
  }

  Strip &keystrip = *action.layer(0)->strip(0);

  /* Ensure a Channelbag. */
  return keystrip.data<StripKeyframeData>(action).channelbag_for_slot_ensure(slot);
}

/* Lots of template args to support transparent non-const and const versions. */
template<typename ActionType,
         typename FCurveType,
         typename LayerType,
         typename StripType,
         typename StripKeyframeDataType,
         typename ChannelbagType>
static Vector<FCurveType *> fcurves_all_templated(ActionType &action)
{
  /* Legacy Action. */
  if (action.is_action_legacy()) {
    Vector<FCurveType *> legacy_fcurves;
    LISTBASE_FOREACH (FCurveType *, fcurve, &action.curves) {
      legacy_fcurves.append(fcurve);
    }
    return legacy_fcurves;
  }

  /* Layered Action. */
  BLI_assert(action.is_action_layered());

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

Vector<FCurve *> fcurves_first_slot(bAction *action)
{
  if (!action) {
    return {};
  }
  Action &action_wrap = action->wrap();

  if (action_wrap.is_action_legacy()) {
    return fcurves_all(action);
  }

  if (action_wrap.slots().is_empty()) {
    return {};
  }
  return fcurves_for_action_slot(action, action_wrap.slot(0)->handle);
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
  /* Legacy Action. */
  if (action.is_action_legacy()) {
    return listbase_to_vector<FCurveType>(action.curves);
  }

  /* Layered Action. */
  Vector<FCurveType *> as_vector(animrig::fcurves_for_action_slot(action, slot_handle));
  return as_vector;
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
                                           Channelbag>(action->wrap(), slot_handle);
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
                                           const Channelbag>(action->wrap(), slot_handle);
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

Vector<bActionGroup *> channel_groups_all(bAction *action)
{
  if (!action) {
    return {};
  }

  Action &action_wrap = action->wrap();

  /* Legacy Action. */
  if (action_wrap.is_action_legacy()) {
    Vector<bActionGroup *> legacy_groups;
    LISTBASE_FOREACH (bActionGroup *, group, &action_wrap.groups) {
      legacy_groups.append(group);
    }
    return legacy_groups;
  }

  /* Layered Action. */
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

  /* Legacy Action. */
  if (action.is_action_legacy()) {
    return channel_groups_all(adt->action);
  }

  /* Layered Action. */
  Channelbag *bag = channelbag_for_action_slot(action, adt->slot_handle);
  if (!bag) {
    return {};
  }

  Vector<bActionGroup *> slot_groups(bag->channel_groups());
  return slot_groups;
}

bool action_treat_as_legacy(const bAction &action)
{
  /* At runtime, legacy Actions should have been versioned to layered/slotted Actions. However,
   * unit tests can still create legacy Actions, and so this function still has to distinguish
   * between them.
   *
   * Note that empty Actions also count as 'layered'. */
  return !action.wrap().is_action_layered();
}

bool action_fcurves_remove(bAction &action,
                           const slot_handle_t slot_handle,
                           const StringRefNull rna_path_prefix)
{
  BLI_assert(!rna_path_prefix.is_empty());
  if (rna_path_prefix.is_empty()) {
    return false;
  }

  Action &action_wrapped = action.wrap();

  /* Legacy Action. Code is 'borrowed' from fcurves_path_remove_fix() in
   * blenkernel/intern/anim_data.cc */
  if (action_wrapped.is_action_legacy()) {
    bool any_removed = false;
    LISTBASE_FOREACH_MUTABLE (FCurve *, fcurve, &action.curves) {
      if (!fcurve->rna_path) {
        continue;
      }

      if (STRPREFIX(fcurve->rna_path, rna_path_prefix.c_str())) {
        BLI_remlink(&action.curves, fcurve);
        BKE_fcurve_free(fcurve);
        any_removed = true;
      }
    }
    return any_removed;
  }

  /* Layered Action. */
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
