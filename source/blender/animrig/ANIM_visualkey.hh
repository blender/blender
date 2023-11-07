/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Functions to work with the visual keying system.
 */

#include "BLI_vector.hh"

struct PointerRNA;
struct PropertyRNA;

namespace blender::animrig {

/**
 * This helper function determines if visual-keyframing should be used when
 * inserting keyframes for the given channel. As visual-keyframing only works
 * on Object and Pose-Channel blocks, this should only get called for those
 * block-types, when using "standard" keying but 'Visual Keying' option in Auto-Keying
 * settings is on.
 */
bool visualkey_can_use(PointerRNA *ptr, PropertyRNA *prop);

/**
 * This helper function extracts the value to use for visual-keyframing
 * In the event that it is not possible to perform visual keying, try to fall-back
 * to using the default method. Assumes that all data it has been passed is valid.
 */
Vector<float> visualkey_get_values(PointerRNA *ptr, PropertyRNA *prop);

}  // namespace blender::animrig
