/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once

#include "DNA_ID_enums.h"

struct Main;

namespace blender::asset_system {
class AssetRepresentation;
}

struct ID *ED_asset_get_local_id_from_asset_or_append_and_reuse(
    Main *bmain, const blender::asset_system::AssetRepresentation &asset, ID_Type idtype);
