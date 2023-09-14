/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(bsdf_common_lib.glsl)

/* ---------------------------------------------------------------------- */
/** \name Utiltex
 *
 * Utiltex is a sampler2DArray that stores a number of useful small utilitary textures and lookup
 * tables.
 * \{ */

#if !defined(USE_GPU_SHADER_CREATE_INFO)

uniform sampler2DArray utilTex;

#endif

#define LUT_SIZE 64

#define LTC_MAT_LAYER 0
#define LTC_BRDF_LAYER 1
#define BRDF_LUT_LAYER 1
#define NOISE_LAYER 2
#define LTC_DISK_LAYER 3 /* UNUSED */

/* Layers 4 to 20 are for BTDF LUT. */
#define lut_btdf_layer_first 4.0
#define lut_btdf_layer_count 16.0

/**
 * Reminder: The 4 noise values are based of 3 uncorrelated blue noises:
 * x : Uniformly distributed value [0..1] (noise 1).
 * y : Uniformly distributed value [0..1] (noise 2).
 * z,w : Uniformly distributed point on the unit circle [-1..1] (noise 3).
 */
#define texelfetch_noise_tex(coord) texelFetch(utilTex, ivec3(ivec2(coord) % LUT_SIZE, 2.0), 0)

/* Return texture coordinates to sample Surface LUT. */
vec2 lut_coords(float cos_theta, float roughness)
{
  vec2 coords = vec2(roughness, sqrt(1.0 - cos_theta));
  /* scale and bias coordinates, for correct filtered lookup */
  return coords * (LUT_SIZE - 1.0) / LUT_SIZE + 0.5 / LUT_SIZE;
}

/* Returns the GGX split-sum precomputed in LUT. */
vec2 brdf_lut(float cos_theta, float roughness)
{
  return textureLod(utilTex, vec3(lut_coords(cos_theta, roughness), BRDF_LUT_LAYER), 0.0).rg;
}

/* Return texture coordinates to sample Surface LUT. */
vec3 lut_coords_btdf(float cos_theta, float roughness, float ior)
{
  /* ior is sin of critical angle. */
  float critical_cos = sqrt(1.0 - ior * ior);

  vec3 coords;
  coords.x = sqr(ior);
  coords.y = cos_theta;
  coords.y -= critical_cos;
  coords.y /= (coords.y > 0.0) ? (1.0 - critical_cos) : critical_cos;
  coords.y = coords.y * 0.5 + 0.5;
  coords.z = roughness;

  coords = saturate(coords);

  /* scale and bias coordinates, for correct filtered lookup */
  coords.xy = coords.xy * (LUT_SIZE - 1.0) / LUT_SIZE + 0.5 / LUT_SIZE;

  return coords;
}

/* Returns GGX transmittance in first component and reflectance in second. */
vec2 bsdf_lut(float cos_theta, float roughness, float ior, float do_multiscatter)
{
  if (ior <= 1e-5) {
    return vec2(0.0, 1.0);
  }

  if (ior >= 1.0) {
    vec2 split_sum = brdf_lut(cos_theta, roughness);
    float f0 = F0_from_ior(ior);
    /* Gradually increase `f90` from 0 to 1 when IOR is in the range of [1.0, 1.33], to avoid harsh
     * transition at `IOR == 1`. */
    float f90 = fast_sqrt(saturate(f0 / 0.02));

    float brdf = F_brdf_multi_scatter(vec3(f0), vec3(f90), split_sum).r;
    /* Energy conservation. */
    float btdf = 1.0 - brdf;
    /* Assuming the energy loss caused by single-scattering is distributed proportionally in the
     * reflection and refraction lobes. */
    return vec2(btdf, brdf) * ((do_multiscatter == 0.0) ? sum(split_sum) : 1.0);
  }

  vec3 coords = lut_coords_btdf(cos_theta, roughness, ior);

  float layer = coords.z * lut_btdf_layer_count;
  float layer_floored = floor(layer);

  coords.z = lut_btdf_layer_first + layer_floored;
  vec2 btdf_brdf_low = textureLod(utilTex, coords, 0.0).rg;

  coords.z += 1.0;
  vec2 btdf_brdf_high = textureLod(utilTex, coords, 0.0).rg;

  /* Manual trilinear interpolation. */
  vec2 btdf_brdf = mix(btdf_brdf_low, btdf_brdf_high, layer - layer_floored);

  if (do_multiscatter != 0.0) {
    /* For energy-conserving BSDF the reflection and refraction lobes should sum to one. Assuming
     * the energy loss of single-scattering is distributed proportionally in the two lobes. */
    btdf_brdf /= (btdf_brdf.x + btdf_brdf.y);
  }

  return btdf_brdf;
}

/** \} */
