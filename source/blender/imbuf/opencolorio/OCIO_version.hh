/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

namespace blender::ocio {

struct Version {
  int major = 0;
  int minor = 0;
  int patch = 0;
};

/**
 * Get OpenColorIO library version.
 * When compiled without OpenColorIO library returns {0, 0, 0}.
 */
Version get_version();

}  // namespace blender::ocio
