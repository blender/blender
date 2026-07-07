/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include <cstddef>
#include <cstdint>

namespace blender::math {

/**
 * Float (FP32) <-> Half (FP16) conversion functions.
 *
 * Behavior matches hardware (x64 F16C, ARM NEON FCVT),
 * including handling of denormals, infinities, NaNs, rounding
 * is to nearest even, etc. When NaNs are produced, the exact
 * bit pattern might not match hardware, but it will still be a NaN.
 *
 * When compiling for ARM NEON (e.g. Apple Silicon),
 * hardware VCVT instructions are used.
 *
 * For anything involving more than a handful of numbers,
 * prefer #float_to_half_array and #half_to_float_array for
 * performance.
 */

/**
 * Converts float (FP32) number to half-precision (FP16).
 */
uint16_t float_to_half(float v);

/**
 * Converts float (FP32) number to half-precision (FP16),
 * also ensuring the result is finite.
 *
 * Changes +/- infinities to +/- maximum value (65504),
 * and NaNs to zeroes.
 */
uint16_t float_to_half_make_finite(float v);

/**
 * Converts half-precision (FP16) number to float (FP32).
 */
float half_to_float(uint16_t v);

void float_to_half_array(const float *src, uint16_t *dst, size_t length);
void float_to_half_make_finite_array(const float *src, uint16_t *dst, size_t length);
void half_to_float_array(const uint16_t *src, float *dst, size_t length);

}  // namespace blender::math
