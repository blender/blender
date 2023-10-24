/* SPDX-FileCopyrightText: 2023 Blender Foundation
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

bool visualkey_can_use(PointerRNA *ptr, PropertyRNA *prop);
Vector<float> visualkey_get_values(PointerRNA *ptr, PropertyRNA *prop);

}  // namespace blender::animrig
