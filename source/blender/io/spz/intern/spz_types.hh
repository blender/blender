/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spz
 */

#pragma once

#include <cstdint>

namespace blender::io::spz {

/* Magic number in the file header for the tile identification. Common for all format versions.
 * It is bytes N, G, S, P in the file order. */
constexpr uint32_t SPZ_HEADER_MAGIC = 0x5053474e;

}  // namespace blender::io::spz
