/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct AssetMetaData;
struct ID;
struct Main;
struct bContext;

/**
 * Mark the datablock as asset.
 *
 * To ensure the datablock is saved, this sets Fake User.
 *
 * \return whether the datablock was marked as asset; false when it is not capable of becoming an
 * asset, or when it already was an asset. */
bool ED_asset_mark_id(struct ID *id);

/**
 * Generate preview image for the given datablock.
 *
 * The preview image might be generated using a background thread.
 */
void ED_asset_generate_preview(const struct bContext *C, struct ID *id);

/**
 * Remove the asset metadata, turning the ID into a "normal" ID.
 *
 * This clears the Fake User. If for some reason the datablock is meant to be saved anyway, the
 * caller is responsible for explicitly setting the Fake User.
 *
 * \return whether the asset metadata was actually removed; false when the ID was not an asset.
 */
bool ED_asset_clear_id(struct ID *id);

/**
 * Copy the asset metadata to the given destination ID.
 *
 * The copy is assigned to \a destination, any pre-existing asset metadata is
 * freed before that. If \a destination was not yet marked as asset, it will be
 * after this call.
 *
 * \return true when the copy succeeded, false otherwise. The only reason for
 *  failure is when \a destination is of a type that cannot be an asset.
 */
bool ED_asset_copy_to_id(const struct AssetMetaData *asset_data, struct ID *destination);

void ED_assets_pre_save(struct Main *bmain);

bool ED_asset_can_mark_single_from_context(const struct bContext *C);

#ifdef __cplusplus
}
#endif
