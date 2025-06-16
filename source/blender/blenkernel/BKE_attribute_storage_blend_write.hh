/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_attribute_storage.hh"

namespace blender::bke {

/**
 * Prepare an #AttributeStorage struct embedded in another struct to be written. This is necessary
 * because the #AttributeStorage implementation doesn't use the DNA structs at runtime, they are
 * created just for the writing process. Creating them mutates the struct, which must be done
 * before writing the struct that embeds it.
 */
void attribute_storage_blend_write_prepare(AttributeStorage &data,
                                           AttributeStorage::BlendWriteData &write_data);

}  // namespace blender::bke
