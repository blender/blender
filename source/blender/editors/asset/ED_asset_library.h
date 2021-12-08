/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup edasset
 */

#pragma once

#include "DNA_asset_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return an index that can be used to uniquely identify \a library, assuming
 * that all relevant indices were created with this function.
 */
int ED_asset_library_reference_to_enum_value(const AssetLibraryReference *library);
/**
 * Return an asset library reference matching the index returned by
 * #ED_asset_library_reference_to_enum_value().
 */
AssetLibraryReference ED_asset_library_reference_from_enum_value(int value);
/**
 * Translate all available asset libraries to an RNA enum, whereby the enum values match the result
 * of #ED_asset_library_reference_to_enum_value() for any given library.
 *
 * Since this is meant for UI display, skips non-displayable libraries, that is, libraries with an
 * empty name or path.
 *
 * \param include_local_library: Whether to include the "Current File" library or not.
 */
const struct EnumPropertyItem *ED_asset_library_reference_to_rna_enum_itemf(
    bool include_local_library);

#ifdef __cplusplus
}
#endif
