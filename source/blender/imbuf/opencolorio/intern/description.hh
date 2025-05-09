/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_string_ref.hh"

#include <string>

namespace blender::ocio {

/**
 * Cleanup description making it possible to easily show in the interface as a tooltip.
 *
 * This includes:
 * - Stripping all trailing line break character.
 * - Replacing all inner line break character with space.
 */
std::string cleanup_description(StringRef description);

}  // namespace blender::ocio
