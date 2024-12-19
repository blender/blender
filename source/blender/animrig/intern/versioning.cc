/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

/* This is versioning code, so it's allowed to touch on deprecated DNA fields. */

#define DNA_DEPRECATED_ALLOW

#include "ANIM_action.hh"
#include "ANIM_action_iterators.hh"
#include "ANIM_versioning.hh"

#include "DNA_action_defaults.h"
#include "DNA_action_types.h"

#include "BKE_main.hh"
#include "BKE_node.hh"
#include "BKE_report.hh"

#include "BLI_listbase.h"
#include "BLI_string_utf8.h"

#include "BLT_translation.hh"

namespace blender::animrig::versioning {

constexpr const char *DEFAULT_VERSIONED_SLOT_NAME = "Legacy Slot";

bool action_is_layered(const bAction &dna_action)
{
  const animrig::Action &action = dna_action.wrap();

  const bool has_layered_data = action.layer_array_num > 0 || action.slot_array_num > 0;
  const bool has_animato_data = !(BLI_listbase_is_empty(&action.curves) &&
                                  BLI_listbase_is_empty(&action.groups));
  const bool has_pre_animato_data = !BLI_listbase_is_empty(&action.chanbase);

  return has_layered_data || (!has_animato_data && !has_pre_animato_data);
}

void convert_legacy_actions(Main &bmain)
{
  LISTBASE_FOREACH (bAction *, dna_action, &bmain.actions) {
    blender::animrig::Action &action = dna_action->wrap();

    if (!action.is_action_legacy()) {
      /* This is just a safety net. Blender files that trigger this versioning code are not
       * expected to have any layered/slotted Actions. */
      continue;
    }

    convert_legacy_action(action);
  }
}

void convert_legacy_action(bAction &dna_action)
{
  if (!BLI_listbase_is_empty(&dna_action.chanbase)) {
    BLI_assert_msg(BLI_listbase_is_empty(&dna_action.chanbase),
                   "Cannot upgrade pre-2.5 Action to slotted Action");
    return;
  }

  Action &action = dna_action.wrap();
  BLI_assert(action.is_action_legacy());

  /* Store this ahead of time, because adding the slot sets the action's idroot
   * to 0. We also set the action's idroot to 0 manually, just to be defensive
   * so we don't depend on esoteric behavior in `slot_add()`. */
  const int16_t idtype = action.idroot;
  action.idroot = 0;

  /* Initialize the Action's last_slot_handle field to its default value, before
   * we create a new slot. */
  action.last_slot_handle = DNA_DEFAULT_ACTION_LAST_SLOT_HANDLE;

  Slot &slot = action.slot_add();
  slot.idtype = idtype;

  const std::string slot_identifier{slot.identifier_prefix_for_idtype() +
                                    DATA_(DEFAULT_VERSIONED_SLOT_NAME)};
  action.slot_identifier_define(slot, slot_identifier);

  Layer &layer = action.layer_add("Layer");
  blender::animrig::Strip &strip = layer.strip_add(action,
                                                   blender::animrig::Strip::Type::Keyframe);
  Channelbag &bag = strip.data<StripKeyframeData>(action).channelbag_for_slot_ensure(slot);
  const int fcu_count = BLI_listbase_count(&action.curves);
  const int group_count = BLI_listbase_count(&action.groups);
  bag.fcurve_array = MEM_cnew_array<FCurve *>(fcu_count, "Action versioning - fcurves");
  bag.fcurve_array_num = fcu_count;
  bag.group_array = MEM_cnew_array<bActionGroup *>(group_count, "Action versioning - groups");
  bag.group_array_num = group_count;

  int group_index = 0;
  int fcurve_index = 0;
  LISTBASE_FOREACH_INDEX (bActionGroup *, group, &action.groups, group_index) {
    bag.group_array[group_index] = group;

    group->channelbag = &bag;
    group->fcurve_range_start = fcurve_index;

    LISTBASE_FOREACH (FCurve *, fcu, &group->channels) {
      if (fcu->grp != group) {
        break;
      }
      bag.fcurve_array[fcurve_index++] = fcu;
    }

    group->fcurve_range_length = fcurve_index - group->fcurve_range_start;
  }

  LISTBASE_FOREACH (FCurve *, fcu, &action.curves) {
    /* Any fcurves with groups have already been added to the fcurve array. */
    if (fcu->grp) {
      continue;
    }
    bag.fcurve_array[fcurve_index++] = fcu;
  }

  BLI_assert(fcurve_index == fcu_count);

  action.curves = {nullptr, nullptr};
  action.groups = {nullptr, nullptr};
}

}  // namespace blender::animrig::versioning
