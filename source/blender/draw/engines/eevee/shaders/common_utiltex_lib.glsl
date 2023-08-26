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

/* Layers 4 to 20 are for BTDF Lut. */
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

/* Returns GGX BTDF in first component and fresnel in second. */
vec2 btdf_lut(float cos_theta, float roughness, float ior)
{
  if (ior <= 1e-5) {
    return vec2(0.0);
  }

  if (ior >= 1.0) {
    vec2 split_sum = brdf_lut(cos_theta, roughness);
    float f0 = f0_from_ior(ior);
    /* Baked IOR for GGX BRDF. */
    const float specular = 1.0;
    const float eta_brdf = (2.0 / (1.0 - sqrt(0.08 * specular))) - 1.0;
    /* Avoid harsh transition coming from ior == 1. */
    float f90 = fast_sqrt(saturate(f0 / (f0_from_ior(eta_brdf) * 0.25)));
    float fresnel = F_brdf_single_scatter(vec3(f0), vec3(f90), split_sum).r;
    /* Setting the BTDF to one is not really important since it is only used for multi-scatter
     * and it's already quite close to ground truth. */
    float btdf = 1.0;
    return vec2(btdf, fresnel);
  }

  vec3 coords = lut_coords_btdf(cos_theta, roughness, ior);

  float layer = coords.z * lut_btdf_layer_count;
  float layer_floored = floor(layer);

  coords.z = lut_btdf_layer_first + layer_floored;
  vec2 btdf_low = textureLod(utilTex, coords, 0.0).rg;

  coords.z += 1.0;
  vec2 btdf_high = textureLod(utilTex, coords, 0.0).rg;

  /* Manual trilinear interpolation. */
  vec2 btdf = mix(btdf_low, btdf_high, layer - layer_floored);

  return btdf;
}

/** \} */
