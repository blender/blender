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

#include "BLO_readfile.hh"

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

void convert_legacy_animato_actions(Main &bmain)
{
  LISTBASE_FOREACH (bAction *, dna_action, &bmain.actions) {
    blender::animrig::Action &action = dna_action->wrap();

    if (action_is_layered(action) && !action.is_empty()) {
      /* This is just a safety net. Blender files that trigger this versioning code are not
       * expected to have any layered/slotted Actions.
       *
       * Empty Actions, even though they are valid "layered" Actions, should still get through
       * versioning, though, to ensure they have the default "Legacy Slot" and a zero idroot. */
      continue;
    }

    /* This function should skip pre-2.50 Actions, as those are versioned in a special step (see
     * `do_versions_after_setup()` in `versioning_common.cc`). */
    if (!BLI_listbase_is_empty(&action.chanbase)) {
      continue;
    }

    convert_legacy_animato_action(action);
  }
}

void convert_legacy_animato_action(bAction &dna_action)
{
  BLI_assert_msg(BLI_listbase_is_empty(&dna_action.chanbase),
                 "this function cannot handle pre-2.50 Actions");
  if (!BLI_listbase_is_empty(&dna_action.chanbase)) {
    /* This is a pre-2.5 Action, which cannot be converted here. It's converted in another function
     * to a post-2.5 Action (aka Animato Action), and after that, this function will be called
     * again. */
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

void tag_action_user_for_slotted_actions_conversion(ID &animated_id)
{
  animated_id.runtime.readfile_data->tags.action_assignment_needs_slot = true;
}

void tag_action_users_for_slotted_actions_conversion(Main &bmain)
{
  /* This function is only called when the blend-file is old enough to NOT use
   * slotted Actions, so we can safely tag anything that uses an Action. */

  auto flag_adt = [](ID &animated_id,
                     bAction *& /*action_ptr_ref*/,
                     slot_handle_t & /*slot_handle_ref*/,
                     char * /*slot_name*/) -> bool {
    tag_action_user_for_slotted_actions_conversion(animated_id);

    /* Once tagged, the foreach loop can stop, because more tagging of the same
     * ID doesn't do anything. */
    return false;
  };

  ID *id;
  FOREACH_MAIN_ID_BEGIN (&bmain, id) {
    foreach_action_slot_use_with_references(*id, flag_adt);

    /* Process embedded IDs, as these are not listed in bmain, but still can
     * have their own Action+Slot. Unfortunately there is no generic looper
     * for embedded IDs. At this moment the only animatable embedded ID is a
     * node tree. */
    bNodeTree *node_tree = blender::bke::node_tree_from_id(id);
    if (node_tree) {
      foreach_action_slot_use_with_references(node_tree->id, flag_adt);
    }
  }
  FOREACH_MAIN_ID_END;
}

void convert_legacy_action_assignments(Main &bmain, ReportList *reports)
{
  auto version_slot_assignment = [&](ID &animated_id,
                                     bAction *&action_ptr_ref,
                                     slot_handle_t &slot_handle_ref,
                                     char *last_used_slot_identifier) {
    BLI_assert(action_ptr_ref); /* Ensured by the foreach loop. */
    Action &action = action_ptr_ref->wrap();

    if (action.slot_array_num == 0) {
      /* animated_id is from an older file (because it is in the being-versioned-right-now bmain),
       * and it's referring to an Action from an already-versioned library file. We know this
       * because versioned legacy Actions always have a single slot called "Legacy Slot", and so
       * this Action must have been opened in some Blender and had its slot removed. */

      /* Another reason that there is no slot is that it was a _really_ old (pre-2.50)
       * Action that should have been upgraded already. */
      BLI_assert_msg(BLI_listbase_is_empty(&action.chanbase),
                     "Did not expect pre-2.5 Action at this stage of the versioning code");

      return true;
    }

    /* Reset the "last used slot identifier" to the default "Legacy Slot". That way
     * generic_slot_for_autoassign() will pick up on legacy slots automatically.
     *
     * Note that this function should only run on legacy users of Actions, i.e. they are not
     * expected to have any last-used slot at all. The field in DNA can still be set, though,
     * because the 4.3 code already has the data model for slotted Actions. */

    /* Ensure that the identifier has the correct ID type prefix. */
    *reinterpret_cast<short *>(last_used_slot_identifier) = GS(animated_id.name);

    static_assert(Slot::identifier_length_max > 2); /* Because of the -2 below. */
    BLI_strncpy_utf8(last_used_slot_identifier + 2,
                     DATA_(DEFAULT_VERSIONED_SLOT_NAME),
                     Slot::identifier_length_max - 2);

    Slot *slot_to_assign = generic_slot_for_autoassign(
        animated_id, action, last_used_slot_identifier);
    if (!slot_to_assign) {
      /* This means that there is no slot that can be found by name, not even the "Legacy Slot"
       * name. Keep the ID unanimated, as this means that the referenced Action has changed
       * significantly since this file was opened. */
      BKE_reportf(reports,
                  RPT_WARNING,
                  "\"%s\" is using Action \"%s\", which does not have a slot with identifier "
                  "\"%s\" or \"%s\". Manually assign the right action slot to \"%s\".\n",
                  animated_id.name,
                  action.id.name + 2,
                  last_used_slot_identifier,
                  animated_id.name,
                  animated_id.name + 2);
      return true;
    }

    const ActionSlotAssignmentResult result = generic_assign_action_slot(
        slot_to_assign, animated_id, action_ptr_ref, slot_handle_ref, last_used_slot_identifier);
    switch (result) {
      case ActionSlotAssignmentResult::OK:
        break;
      case ActionSlotAssignmentResult::SlotNotSuitable:
        /* If the slot wasn't suitable for the ID, we force assignment anyway,
         * but with a warning.
         *
         * This happens when the legacy action assigned to the ID had a
         * mismatched idroot, and therefore the created slot does as well.
         * This mismatch can happen in a variety of ways, and we opt to
         * preserve this unusual (but technically valid) state of affairs.
         */
        slot_handle_ref = slot_to_assign->handle;
        BLI_strncpy_utf8(
            last_used_slot_identifier, slot_to_assign->identifier, Slot::identifier_length_max);
        /* Not necessary to add this ID to the slot user list, as that list is
         * going to get refreshed after versioning anyway. */

        BKE_reportf(
            reports,
            RPT_WARNING,
            "Legacy action \"%s\" is assigned to \"%s\", which does not match the "
            "action's id_root \"%s\". The action has been upgraded to a slotted action with "
            "slot \"%s\" with an id_type \"%s\", which has also been assigned to \"%s\" despite "
            "this type mismatch. This likely indicates something odd about the blend file.\n",
            action.id.name + 2,
            animated_id.name,
            slot_to_assign->identifier_prefix_for_idtype().c_str(),
            slot_to_assign->identifier_without_prefix().c_str(),
            slot_to_assign->identifier_prefix_for_idtype().c_str(),
            animated_id.name);
        break;
      case ActionSlotAssignmentResult::SlotNotFromAction:
        BLI_assert_msg(false, "SlotNotFromAction should not be returned here");
        break;
      case ActionSlotAssignmentResult::MissingAction:
        BLI_assert_msg(false, "MissingAction should not be returned here");
        break;
    }

    return true;
  };

  ID *id;
  FOREACH_MAIN_ID_BEGIN (&bmain, id) {
    /* Process the ID itself. */
    if (BLO_readfile_id_runtime_tags(*id).action_assignment_needs_slot) {
      foreach_action_slot_use_with_references(*id, version_slot_assignment);
      id->runtime.readfile_data->tags.action_assignment_needs_slot = false;
    }

    /* Process embedded IDs, as these are not listed in bmain, but still can
     * have their own Action+Slot. Unfortunately there is no generic looper
     * for embedded IDs. At this moment the only animatable embedded ID is a
     * node tree. */
    bNodeTree *node_tree = blender::bke::node_tree_from_id(id);
    if (node_tree && BLO_readfile_id_runtime_tags(node_tree->id).action_assignment_needs_slot) {
      foreach_action_slot_use_with_references(node_tree->id, version_slot_assignment);
      node_tree->id.runtime.readfile_data->tags.action_assignment_needs_slot = false;
    }
  }
  FOREACH_MAIN_ID_END;
}

}  // namespace blender::animrig::versioning
