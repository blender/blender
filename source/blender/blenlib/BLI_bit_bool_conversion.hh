/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#pragma once

#include "BLI_bit_span.hh"
#include "BLI_span.hh"

namespace blender::bits {

/**
 * Converts the bools to bits and `or`s them into the given bits. For pure conversion, the bits
 * should therefore be zero initialized before they are passed into this function.
 *
 * \param allowed_overshoot: How many bools/bits can be read/written after the end of the given
 * spans. This can help with performance because the internal algorithm can process many elements
 * at once.
 *
 * \return True if any of the checked bools were true (this also includes the bools in the
 * overshoot).
 */
bool or_bools_into_bits(Span<bool> bools, MutableBitSpan r_bits, int64_t allowed_overshoot = 0);

}  // namespace blender::bits
