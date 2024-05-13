/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Functions to work with AnimData.
 */

#pragma once

#include "BLI_string_ref.hh"

struct ID;
struct Main;

struct bAnimContext;
struct AnimData;
struct FCurve;
struct bAction;

namespace blender::animrig {

class Action;

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

/** Iterate the FCurves of the given bAnimContext and validate the RNA path. Sets the flag
 * FCURVE_DISABLED if the path can't be resolved. */
void reevaluate_fcurve_errors(bAnimContext *ac);

/**
 * Unlink the action from animdata if it's empty.
 *
 * If the action has no F-Curves, unlink it from AnimData if it did not
 * come from a NLA Strip being tweaked.
 */
bool animdata_remove_empty_action(AnimData *adt);

/**
 * Compatibility helper function for `BKE_animadata_fcurve_find_by_rna_path()`.
 *
 * Searches each layer (top to bottom) to find an FCurve that matches the given
 * RNA path & index.
 *
 * \see BKE_animadata_fcurve_find_by_rna_path
 *
 * \note The returned FCurve should NOT be used for keyframe manipulation. Its
 * existence is an indicator for "this property is animated".
 *
 * This function should probably be limited to the active layer (for the given
 * property, once pinning to layers is there), so that the "this is keyed" color
 * is more accurate.
 *
 * Again, this is just to hook up the new Animation data-block to the old
 * Blender UI code.
 */
const FCurve *fcurve_find_by_rna_path(const Action &anim,
                                      const ID &animated_id,
                                      StringRefNull rna_path,
                                      int array_index);

}  // namespace blender::animrig
