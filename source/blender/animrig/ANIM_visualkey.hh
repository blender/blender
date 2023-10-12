/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Functions to work with the visal keying system.
 */

struct PointerRNA;
struct PropertyRNA;

namespace blender::animrig {

bool visualkey_can_use(PointerRNA *ptr, PropertyRNA *prop);
float *visualkey_get_values(
    PointerRNA *ptr, PropertyRNA *prop, float *buffer, int buffer_size, int *r_count);

}  // namespace blender::animrig
