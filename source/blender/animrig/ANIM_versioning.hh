/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Versioning of old animation data. Most animation versioning code lives
 * in the versioning_xxx.cc files, but some is broken out and placed here.
 */

namespace blender {

struct bAction;
struct BlendFileReadReport;
struct ID;
struct Main;
struct ReportList;

namespace animrig {
class Action;
}

namespace animrig::versioning {

/**
 * Return whether an action is layered (as opposed to legacy).
 *
 * This will return false for both Animato and pre-Animato actions. It is used
 * during file read and versioning to determine how forward-compatible and
 * legacy data should be handled.
 */
bool action_is_layered(const bAction &dna_action);

/**
 * Convert all legacy (Animato) Actions to slotted Actions, in-place.
 *
 * This function does *not* work on pre-Animato actions.
 */
void convert_legacy_animato_actions(Main &bmain);

/**
 * Convert legacy (Animato) Action to slotted Action, in-place.
 *
 * \note This function does *not* work on pre-Animato actions.
 *
 * This always creates a slot and a layer for the Action, even when the Action doesn't actually
 * contain any animation data. This ensures that versioned Actions all look the same, and there's
 * just less variations to keep track of. */
void convert_legacy_animato_action(bAction &dna_action);

/**
 * Go over all animated IDs, and tag them whenever they use a legacy Action.
 *
 * \see convert_legacy_action_assignments
 */
void tag_action_users_for_slotted_actions_conversion(Main &bmain);

/**
 * Tag this ID so it'll get its legacy Action assignment converted.
 *
 * \see convert_legacy_action_assignments
 */
void tag_action_user_for_slotted_actions_conversion(ID &animated_id);

/**
 * Convert the Action assignments of all animated IDs.
 *
 * For all IDs that use an Action, this also picks an Action Slot to ensure the ID is still
 * animated.
 *
 * This only visits IDs tagged by #tag_action_users_for_slotted_actions_conversion.
 */
void convert_legacy_action_assignments(Main &bmain, ReportList *reports);

/**
 * Reconstruct channel pointers.
 * Assumes that the groups referred to by the FCurves are already in act->groups.
 * Reorders the main channel list to match group order.
 *
 * Only used in versioning code since this only works with legacy actions which
 * no longer exist in new files.
 */
void action_groups_reconstruct(bAction *act);

}  // namespace animrig::versioning
}  // namespace blender
