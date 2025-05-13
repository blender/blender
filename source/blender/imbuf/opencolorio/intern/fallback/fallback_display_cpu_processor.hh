/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>

namespace blender::ocio {

class Config;
class CPUProcessor;
struct DisplayParameters;

std::shared_ptr<const CPUProcessor> create_fallback_display_cpu_processor(
    const Config &config, const DisplayParameters &display_parameters);

}  // namespace blender::ocio
