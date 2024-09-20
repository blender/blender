/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

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
 */

/**
 * Converts float (FP32) number to half-precision (FP16).
 */
uint16_t float_to_half(float v);

/**
 * Converts half-precision (FP16) number to float (FP32).
 */
float half_to_float(uint16_t v);

}  // namespace blender::math
