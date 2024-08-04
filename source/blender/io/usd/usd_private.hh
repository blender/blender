/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <pxr/usd/usd/common.h>

#include <string>

#include "usd.hh"

struct Depsgraph;

namespace blender::io::usd {

pxr::UsdStageRefPtr export_to_stage(const USDExportParams &params,
                                    Depsgraph *depsgraph,
                                    const char *filepath);

std::string image_cache_file_path();
std::string get_image_cache_file(const std::string &file_name, bool mkdir = true);
std::string cache_image_color(const float color[4]);

};  // namespace blender::io::usd
