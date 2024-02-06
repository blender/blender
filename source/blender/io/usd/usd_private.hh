/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <pxr/usd/usd/stage.h>

struct Depsgraph;
struct USDExportParams;

namespace blender::io::usd {

pxr::UsdStageRefPtr export_to_stage(const USDExportParams &params,
                                    Depsgraph *depsgraph,
                                    const char *filepath);

};
