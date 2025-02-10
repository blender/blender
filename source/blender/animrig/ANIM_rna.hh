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

struct PointerRNA;
struct PropertyRNA;

namespace blender::animrig {

/** Get the values of the given property. Casts non-float properties to float. */
Vector<float> get_rna_values(PointerRNA *ptr, PropertyRNA *prop);

/** Get the rna path for the given rotation mode. */
StringRef get_rotation_mode_path(eRotationModes rotation_mode);

/**
 * Returns a Vector of ID properties on the given pointer that can be animated. Not all pointer
 * types are supported. Unsupported pointer types will return an empty vector.
 */
Vector<RNAPath> get_keyable_id_property_paths(const PointerRNA &ptr);

}  // namespace blender::animrig
