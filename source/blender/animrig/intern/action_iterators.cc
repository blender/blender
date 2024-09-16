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
        if (!strip->is<KeyframeStrip>()) {
          continue;
        }
        KeyframeStrip &key_strip = strip->as<KeyframeStrip>();
        for (ChannelBag *bag : key_strip.channelbags()) {
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

  return true;
}

}  // namespace blender::animrig
