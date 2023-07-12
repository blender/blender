/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once

#include "DNA_ID_enums.h"

struct ID;
struct Main;

namespace blender::asset_system {
class AssetRepresentation;
}

namespace blender::ed::asset {

/**
 * If the asset already has a corresponding local #ID, return it. Otherwise, link or append the
 * asset's data-block, using "Append & Reuse" if the method is unspecified.
 */
ID *asset_local_id_ensure_imported(Main &bmain, const asset_system::AssetRepresentation &asset);

}  // namespace blender::ed::asset
