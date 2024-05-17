/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_vector_types.hh"

#pragma once

namespace blender::fftw {

/* FFTW's real to complex and complex to real transforms are more efficient when their input has a
 * specific size. This function finds the most optimal size that is more than or equal the given
 * size. The input data can then be zero padded to the optimal size for better performance. See
 * Section 4.3.3 Real-data DFTs in the FFTW manual for more information. */
int optimal_size_for_real_transform(int size);
int2 optimal_size_for_real_transform(int2 size);

/* Initialize the float variant of FFTW. This essentially setup the multithreading hooks to enable
 * multithreading using TBB's parallel_for and makes the FFTW planner thread safe. */
void initialize_float();

}  // namespace blender::fftw
