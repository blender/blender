/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
