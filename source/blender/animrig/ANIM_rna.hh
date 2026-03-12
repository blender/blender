/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Helper functions for animation to interact with the RNA system.
 */

#include "BLI_string_ref.hh"
#include "BLI_vector.hh"
#include "DNA_action_types.h"
#include "RNA_types.hh"

#include "RNA_path.hh"

namespace blender {

struct PointerRNA;
struct PropertyRNA;

namespace animrig {

/** Get the values of the given property. Casts non-float properties to float. */
Vector<float> get_rna_values(PointerRNA *ptr, PropertyRNA *prop);

/** Get the rna path for the given rotation mode. */
StringRef get_rotation_mode_path(eRotationModes rotation_mode);

/**
 * Given an RNA path to a rotation property, return the corresponding rotation mode.
 *
 * \returns the rotation mode of the given rna path or a nullopt if the `rna_path` is not for a
 * rotation property.
 *
 * \note that this returns ROT_MODE_EUL for any euler rotation mode since it cannot determine the
 * rotation order.
 *
 * \note that this function assumes that the rna_path is syntactically valid.
 */
std::optional<eRotationModes> get_rotation_mode_from_path(StringRefNull rna_path);

/**
 * Given a PointerRNA return the rotation mode of the data it points to.
 *
 * \returns the rotation mode of the given rna pointer or a nullopt if the data has no rotation
 * mode.
 */
std::optional<eRotationModes> get_rotation_mode_from_rna_pointer(const PointerRNA &ptr);

/**
 * Given an RNA path, check if it is a path to a rotation property.
 *
 * \returns true if the given rna path is for a rotation property.
 */
bool is_rotation_path(StringRefNull rna_path);

/**
 * Returns a Vector of ID properties on the given pointer that can be animated. Not all pointer
 * types are supported. Unsupported pointer types will return an empty vector.
 */
Vector<RNAPath> get_keyable_id_property_paths(const PointerRNA &ptr);

}  // namespace animrig
}  // namespace blender
