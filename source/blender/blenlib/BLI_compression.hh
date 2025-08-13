/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * Utilities for lossless data compression.
 */

#include <cstddef>
#include <cstdint>

namespace blender {

/**
 * Transforms array of data, making it more compressible,
 * especially if data is smoothly varying. Typically you do
 * this before compression with a general purpose compressor.
 *
 * Transposes input array so that output data is first byte of
 * all items, then 2nd byte of all items, etc. And successive
 * items within each "byte stream" are stored as difference
 * from previous byte.
 *
 * See https://aras-p.info/blog/2023/03/01/Float-Compression-7-More-Filtering-Optimization/
 * for details.
 */
void filter_transpose_delta(const uint8_t *src, uint8_t *dst, size_t items_num, size_t item_size);

/**
 * Reverses the data transform done by #unfilter_transpose_delta.
 * Typically you do this after decompression with a general purpose
 * compressor.
 */
void unfilter_transpose_delta(const uint8_t *src,
                              uint8_t *dst,
                              size_t items_num,
                              size_t item_size);

}  // namespace blender
