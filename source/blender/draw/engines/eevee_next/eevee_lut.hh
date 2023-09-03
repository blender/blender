/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_math_vector_types.hh"

namespace blender::eevee::lut {

/* LTC matrix components for isotropic GGX. */
extern const VecBase<float, 4> ltc_mat_ggx[64][64];
/* LTC magnitude components for isotropic GGX. */
extern const VecBase<float, 2> ltc_mag_ggx[64][64];
/* Precomputed Disk integral for different elevation angles and solid angle. */
extern const VecBase<float, 1> ltc_disk_integral[64][64];
/* Precomputed integrated split fresnel term of the GGX brdf. */
extern const VecBase<float, 2> bsdf_split_sum_ggx[64][64];
/* Precomputed reflectance and transmission of glass material. */
extern const VecBase<float, 2> btdf_split_sum_ggx[16][64][64];
/* 4 different blue noise, one per channel. */
extern const VecBase<float, 4> blue_noise[64][64];

}  // namespace blender::eevee::lut
