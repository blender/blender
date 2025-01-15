/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Versioning of old animation data. Most animation versioning code lives
 * in the versioning_xxx.cc files, but some is broken out and placed here.
 */

struct bAction;
struct BlendFileReadReport;
struct ID;
struct Main;
struct ReportList;

namespace blender::animrig {
class Action;
}

namespace blender::animrig::versioning {

/**
 * Return whether an action is layered (as opposed to legacy).
 *
 * This will return false for both Animato and pre-Animato actions. It is used
 * during file read and versioning to determine how forward-compatible and
 * legacy data should be handled.
 *
 * NOTE: this is semi-duplicated from `Action::is_action_layered()`, but with
 * tweaks to also recognize ultra-legacy (pre-Animato) data. Because this needs access to
 * deprecated DNA fields, which is ok here in the versioning code, the other "is this legacy or
 * layered?" functions do not check for pre-Animato data.
 *
 * \see Action::is_action_layered()
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

}  // namespace blender::animrig::versioning
