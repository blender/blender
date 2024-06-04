/* SPDX-FileCopyrightText: 2024 NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd.hh"

#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/timeCode.h>

struct ID;

namespace blender::io::usd {

void set_id_props_from_prim(ID *id,
                            const pxr::UsdPrim &prim,
                            eUSDAttrImportMode attr_import_mode = USD_ATTR_IMPORT_ALL,
                            pxr::UsdTimeCode time_code = pxr::UsdTimeCode::Default());

}  // namespace blender::io::usd
