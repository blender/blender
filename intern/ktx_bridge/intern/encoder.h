/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "common.h"

#include <ktx.h>

/* Encodes a tightly packed RGB(A)8 pixel buffer (`channels` is 3 or 4, rows bottom-to-top,
 * matching Blender's `Image.pixels` convention) as a Basis Universal KTX2 file.
 * `quality` is in [0, 100]. `is_data` selects a UNORM VkFormat instead of sRGB, for data
 * textures (normal maps, roughness, etc.) that must not be gamma-encoded. `use_uastc` selects
 * UASTC (higher quality, larger output) instead of ETC1S (smaller, lower quality, faster).
 * `compress` in [0, 22] applies additional Zstandard supercompression on top of UASTC, as a
 * raw Zstd level (0 disables it, 1 = fastest/lowest ratio, 22 = slowest/highest ratio); ignored
 * when `use_uastc` is false, since ETC1S cannot be Zstd-compressed.
 * Returns a `ktx_error_code_e` (0 = success). */
API(int)
encodeToFile(const char *filename,
             const unsigned char *pixels,
             unsigned int width,
             unsigned int height,
             unsigned int channels,
             int quality,
             int is_data,
             int use_uastc,
             int compress);
