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
 *
 * Functions for filtering assets.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct AssetFilterSettings;
struct AssetHandle;

/**
 * Compare \a asset against the settings of \a filter.
 *
 * Individual filter parameters are ORed with the asset properties. That means:
 * * The asset type must be one of the ID types filtered by, and
 * * The asset must contain at least one of the tags filtered by.
 * However for an asset to be matching it must have one match in each of the parameters. I.e. one
 * matching type __and__ at least one matching tag.
 *
 * \returns True if the asset should be visible with these filter settings (parameters match).
 * Otherwise returns false (mismatch).
 */
bool ED_asset_filter_matches_asset(const struct AssetFilterSettings *filter,
                                   const struct AssetHandle *asset);

#ifdef __cplusplus
}
#endif
