/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Functions to work with AnimData.
 */

struct bAnimContext;
struct AnimData;
struct FCurve;

namespace blender::animrig {

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
