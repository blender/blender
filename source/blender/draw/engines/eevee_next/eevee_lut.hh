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
extern const float ltc_mat_ggx[64][64][4];
/* LTC magnitude components for isotropic GGX. */
extern const float ltc_mag_ggx[64][64][2];
/* Precomputed Disk integral for different elevation angles and solid angle. */
extern const float ltc_disk_integral[64][64][1];
/* Precomputed integrated split fresnel term of the GGX BRDF. */
extern const float bsdf_split_sum_ggx[64][64][2];
/* Precomputed reflectance and transmission of glass material. */
extern const float btdf_split_sum_ggx[16][64][64][2];
/* 4 different blue noise, one per channel. */
extern const float blue_noise[64][64][4];

}  // namespace blender::eevee::lut
