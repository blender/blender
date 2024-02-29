/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once

#include "DNA_ID.h"

struct ID;

namespace blender::ed::asset {

bool id_type_is_non_experimental(const ID *id);
#define ED_ASSET_TYPE_IDS_NON_EXPERIMENTAL_FLAGS \
  (FILTER_ID_BR | FILTER_ID_MA | FILTER_ID_GR | FILTER_ID_OB | FILTER_ID_AC | FILTER_ID_WO | \
   FILTER_ID_NT)

/**
 * Check if the asset type for \a id (which doesn't need to be an asset right now) can be an asset,
 * respecting the "Extended Asset Browser" experimental feature flag.
 */
bool id_type_is_supported(const ID *id);

/**
 * Get the filter flags (subset of #FILTER_ID_ALL) representing the asset ID types that may be
 * turned into assets, respecting the "Extended Asset Browser" experimental feature flag.
 * \note Does not check for #BKE_id_can_be_asset(), so may return filter flags for IDs that can
 *       never be assets.
 */
int64_t types_supported_as_filter_flags();

/**
 * Utility: A string enumerating the non-experimental asset types. This is useful info to
 * the user, it should be displayed in tooltips or messages. Macro to support concatenating static
 * strings with this (not all UI code supports dynamic strings nicely).
 * Should start with a consonant, so usages can prefix it with "a" (not "an").
 */
#define ED_ASSET_TYPE_IDS_NON_EXPERIMENTAL_UI_STRING \
  "Material, Collection, Object, Brush, Pose Action, Node Group or World"

}  // namespace blender::ed::asset
