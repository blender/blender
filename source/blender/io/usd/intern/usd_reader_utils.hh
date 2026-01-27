/* SPDX-FileCopyrightText: 2024 NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd.hh"

#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/timeCode.h>

namespace blender {

struct ID;

namespace io::usd {

void set_id_props_from_prim(ID *id,
                            const pxr::UsdPrim &prim,
                            PropertyImportMode property_import_mode = PropertyImportMode::All,
                            pxr::UsdTimeCode time_code = pxr::UsdTimeCode::Default());

}  // namespace io::usd
}  // namespace blender
