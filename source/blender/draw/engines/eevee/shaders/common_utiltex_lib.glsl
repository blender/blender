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
#define LTC_BRDF_LAYER 3
#define LTC_DISK_LAYER 3
#define BRDF_LUT_LAYER 1
#define NOISE_LAYER 2

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

void brdf_f82_tint_lut(vec3 F0,
                       vec3 F82,
                       float cos_theta,
                       float roughness,
                       bool do_multiscatter,
                       out vec3 reflectance)
{
  vec2 uv = lut_coords(cos_theta, roughness);
  vec3 split_sum = textureLod(utilTex, vec3(uv, BRDF_LUT_LAYER), 0.0).rgb;

  reflectance = do_multiscatter ? F_brdf_multi_scatter(F0, vec3(1.0), split_sum.xy) :
                                  F_brdf_single_scatter(F0, vec3(1.0), split_sum.xy);

  /* Precompute the F82 term factor for the Fresnel model.
   * In the classic F82 model, the F82 input directly determines the value of the Fresnel
   * model at ~82°, similar to F0 and F90.
   * With F82-Tint, on the other hand, the value at 82° is the value of the classic Schlick
   * model multiplied by the tint input.
   * Therefore, the factor follows by setting `F82Tint(cosI) = FSchlick(cosI) - b*cosI*(1-cosI)^6`
   * and `F82Tint(acos(1/7)) = FSchlick(acos(1/7)) * f82_tint` and solving for `b`. */
  const float f = 6.0 / 7.0;
  const float f5 = (f * f) * (f * f) * f;
  const float f6 = (f * f) * (f * f) * (f * f);
  vec3 F_schlick = mix(F0, vec3(1.0), f5);
  vec3 b = F_schlick * (7.0 / f6) * (1.0 - F82);
  reflectance -= b * split_sum.z;
}

vec4 sample_3D_texture(sampler2DArray tex, vec3 coords)
{
  float layer_floored;
  float interp = modf(coords.z, layer_floored);

  coords.z = layer_floored;
  vec4 tex_low = textureLod(tex, coords, 0.0);

  coords.z += 1.0;
  vec4 tex_high = textureLod(tex, coords, 0.0);

  /* Manual trilinear interpolation. */
  return mix(tex_low, tex_high, interp);
}

/* Return texture coordinates to sample Surface LUT. */
vec3 lut_coords_btdf(float cos_theta, float roughness, float ior)
{
  vec3 coords = vec3(sqrt((ior - 1.0) / (ior + 1.0)), sqrt(1.0 - cos_theta), roughness);

  /* scale and bias coordinates, for correct filtered lookup */
  coords.xy = coords.xy * (LUT_SIZE - 1.0) / LUT_SIZE + 0.5 / LUT_SIZE;
  coords.z = coords.z * lut_btdf_layer_count + lut_btdf_layer_first;

  return coords;
}

/* Return texture coordinates to sample BSDF LUT. */
vec3 lut_coords_bsdf(float cos_theta, float roughness, float ior)
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
  coords.z = coords.z * lut_btdf_layer_count + lut_btdf_layer_first;

  return coords;
}

/* Computes the reflectance and transmittance based on the tint (`f0`, `f90`, `transmission_tint`)
 * and the BSDF LUT. */
void bsdf_lut(vec3 F0,
              vec3 F90,
              vec3 transmission_tint,
              float cos_theta,
              float roughness,
              float ior,
              bool do_multiscatter,
              out vec3 reflectance,
              out vec3 transmittance)
{
  if (ior == 1.0) {
    reflectance = vec3(0.0);
    transmittance = transmission_tint;
    return;
  }

  vec2 split_sum;
  float transmission_factor;

  if (ior > 1.0) {
    split_sum = brdf_lut(cos_theta, roughness);
    transmission_factor = sample_3D_texture(utilTex, lut_coords_btdf(cos_theta, roughness, ior)).a;
    /* Gradually increase `f90` from 0 to 1 when IOR is in the range of [1.0, 1.33], to avoid harsh
     * transition at `IOR == 1`. */
    if (all(equal(F90, vec3(1.0)))) {
      F90 = vec3(saturate(2.33 / 0.33 * (ior - 1.0) / (ior + 1.0)));
    }
  }
  else {
    vec3 bsdf = sample_3D_texture(utilTex, lut_coords_bsdf(cos_theta, roughness, ior)).rgb;
    split_sum = bsdf.rg;
    transmission_factor = bsdf.b;
  }

  reflectance = F_brdf_single_scatter(F0, F90, split_sum);
  transmittance = (vec3(1.0) - F0) * transmission_factor * transmission_tint;

  if (do_multiscatter) {
    float real_F0 = F0_from_ior(ior);
    float Ess = real_F0 * split_sum.x + split_sum.y + (1.0 - real_F0) * transmission_factor;
    float Ems = 1.0 - Ess;
    /* Assume that the transmissive tint makes up most of the overall color if it's not zero. */
    vec3 Favg = all(equal(transmission_tint, vec3(0.0))) ? F0 + (F90 - F0) / 21.0 :
                                                           transmission_tint;

    vec3 scale = 1.0 / (1.0 - Ems * Favg);
    reflectance *= scale;
    transmittance *= scale;
  }

  return;
}

/* Computes the reflectance and transmittance based on the BSDF LUT. */
vec2 bsdf_lut(float cos_theta, float roughness, float ior, bool do_multiscatter)
{
  float F0 = F0_from_ior(ior);
  vec3 color = vec3(1.0);
  vec3 reflectance, transmittance;
  bsdf_lut(vec3(F0),
           color,
           color,
           cos_theta,
           roughness,
           ior,
           do_multiscatter,
           reflectance,
           transmittance);
  return vec2(reflectance.r, transmittance.r);
}

/** \} */
