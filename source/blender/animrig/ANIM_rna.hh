/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Helper functions for animation to interact with the RNA system.
 */

#include "BLI_array.hh"
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
StringRefNull get_rotation_mode_path(eRotationModes rotation_mode);

/**
 * Returns the full pose bone rna path. For example "pose.bones["bone_name"]".
 */
std::string get_pose_bone_rna_path(const bPoseChannel &pose_bone);

/**
 * Returns the name of the pose bone encoded in this rna path.
 * If the given path is not to a pose bone, `std::nullopt` will be returned.
 * This function does the unescaping of the string, which is why it has to return
 * a copy of the string, and not just a StringRef.
 */
std::optional<std::string> pose_bone_name_from_rna_path(StringRefNull rna_path);

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

/**
 * Returns the given property values as a float array. In case the property is not an array, the
 * array length is 1. Casts non-float values to float.
 * Calling this with unsupported property types is invalid and returns an array of length 0.
 *
 * \note Only PROP_BOOLEAN, PROP_INT and PROP_FLOAT are supported.
 */
Array<float> rna_property_get_as_float(PointerRNA &ptr, PropertyRNA &prop);

/**
 * Sets the given property to the given `values`. The size of values has to match the property
 * array length. In case the property is not an array, only the first index is used. This is an
 * abstraction around RNA properties to deal with them as float regardless of their actual type.
 *
 * \note Only PROP_BOOLEAN, PROP_INT and PROP_FLOAT are supported.
 */
void rna_property_set_as_float(PointerRNA &ptr, PropertyRNA &prop, Span<float> values);

}  // namespace animrig
}  // namespace blender
