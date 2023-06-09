/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once

#include "DNA_ID_enums.h"

struct AssetRepresentation;
struct Main;

#ifdef __cplusplus
extern "C" {
#endif

struct ID *ED_asset_get_local_id_from_asset_or_append_and_reuse(
    struct Main *bmain, const struct AssetRepresentation *asset_c_ptr, ID_Type idtype);

#ifdef __cplusplus
}
#endif
