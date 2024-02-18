/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once

#include "DNA_asset_types.h"

namespace blender::ed::asset {

/**
 * Return an index that can be used to uniquely identify \a library, assuming
 * that all relevant indices were created with this function.
 */
int library_reference_to_enum_value(const AssetLibraryReference *library);
/**
 * Return an asset library reference matching the index returned by
 * #library_reference_to_enum_value().
 */
AssetLibraryReference library_reference_from_enum_value(int value);
/**
 * Translate all available asset libraries to an RNA enum, whereby the enum values match the result
 * of #library_reference_to_enum_value() for any given library.
 *
 * Since this is meant for UI display, skips non-displayable libraries, that is, libraries with an
 * empty name or path.
 *
 * \param include_generated: Whether to include libraries that are generated and thus cannot be
 *                           written to. Setting this to false means only custom libraries will be
 *                           included, since they are stored on disk with a single root directory,
 *                           thus have a well defined location that can be written to.
 */
const EnumPropertyItem *library_reference_to_rna_enum_itemf(bool include_generated);

}  // namespace blender::ed::asset
