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
extern const float brdf_ggx[64][64][2];
/* Precomputed Schlick reflectance and transmittance factor of glass material with IOR < 1. */
extern const float bsdf_ggx[16][64][64][3];
/* Precomputed Schlick transmittance factor of glass material with IOR > 1. */
extern const float btdf_ggx[16][64][64][1];
/* 4 different blue noise, one per channel. */
extern const float blue_noise[64][64][4];
/* Precomputed translucency profile of a slab of material with varying thickness. */
extern const float burley_sss_profile[64][1];
/* Precomputed translucency profile of a slab of material with varying thickness. */
extern const float random_walk_sss_profile[64][1];

#ifdef CYCLES_REFERENCE_LUT
/* Precomputed translucency profile of a slab of material with varying thickness. */
extern const float cycles_burley_reference_sss_profile[256][3];
/* Precomputed translucency profile of a slab of material with varying thickness. */
extern const float cycles_random_walk_reference_sss_profile[256][3];
#endif

}  // namespace blender::eevee::lut
