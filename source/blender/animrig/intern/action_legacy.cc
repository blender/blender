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

}  // namespace blender::animrig::legacy
