
/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Versioning of old animation data. Most of this lives in the
 * versioning_xxx.cc files, but some is broken out and placed here.
 */

struct bAction;
struct BlendFileReadReport;
struct Main;

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
 * The handling of pre-Animato actions is _only_ done here in this function, in the
 * `...::versioning` namespace, as that needs access to depreacted DNA fields.
 *
 * NOTE: this is semi-duplicated from `Action::is_action_layered()`, but with
 * tweaks to also recognize ultra-legacy (pre-Animato) data.
 *
 * \see Action::is_action_layered()
 */
bool action_is_layered(const bAction &dna_action);

/**
 * Convert all legacy (Animato) Actions to slotted Actions, in-place.
 *
 * This function does *not* work on pre-Animato actions.
 */
void convert_legacy_actions(Main &bmain);

/**
 * Convert legacy (Animato) Action to slotted Action, in-place.
 *
 * \note This function does *not* work on pre-Animato actions.
 *
 * This always creates a slot and a layer for the Action, even when the Action doesn't actually
 * contain any animation data. This ensures that versioned Actions all look the same, and there's
 * just less variations to keep track of. */
void convert_legacy_action(bAction &dna_action);

}  // namespace blender::animrig::versioning
