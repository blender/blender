/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Functions to work with AnimData.
 */

struct ID;
struct Main;

struct bAnimContext;
struct AnimData;
struct FCurve;
struct bAction;

namespace blender::animrig {

/**
 * Get (or add relevant data to be able to do so) the Active Action for the given
 * Animation Data block, given an ID block where the Animation Data should reside.
 */
bAction *id_action_ensure(Main *bmain, ID *id);

/**
 * Delete the F-Curve from the given AnimData block (if possible),
 * as appropriate according to animation context.
 */
void animdata_fcurve_delete(bAnimContext *ac, AnimData *adt, FCurve *fcu);

/**
 * Unlink the action from animdata if it's empty.
 *
 * If the action has no F-Curves, unlink it from AnimData if it did not
 * come from a NLA Strip being tweaked.
 */
bool animdata_remove_empty_action(AnimData *adt);

}  // namespace blender::animrig
