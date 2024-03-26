/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once

struct AssetMetaData;
struct ID;
struct Main;
struct bContext;

namespace blender::ed::asset {

/**
 * Mark the data-block as asset.
 *
 * To ensure the data-block is saved, this sets Fake User.
 *
 * \return whether the data-block was marked as asset; false when it is not capable of becoming an
 * asset, or when it already was an asset. */
bool mark_id(ID *id);

/**
 * Generate preview image for the given data-block.
 *
 * The preview image might be generated using a background thread.
 */
void generate_preview(const bContext *C, ID *id);

/**
 * Remove the asset metadata, turning the ID into a "normal" ID.
 *
 * This clears the Fake User. If for some reason the data-block is meant to be saved anyway, the
 * caller is responsible for explicitly setting the Fake User.
 *
 * \return whether the asset metadata was actually removed; false when the ID was not an asset.
 */
bool clear_id(ID *id);

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
bool copy_to_id(const AssetMetaData *asset_data, ID *destination);

void pre_save_assets(Main *bmain);

bool can_mark_single_from_context(const bContext *C);

}  // namespace blender::ed::asset
