/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * BxDF evaluation functions.
 */

#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_base_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_fast_lib.glsl)

struct BsdfSample {
  vec3 direction;
  float pdf;
};

struct BsdfEval {
  float throughput;
  float pdf;
};

struct ClosureLight {
  /* LTC matrix. */
  vec4 ltc_mat;
  /* Shading normal. */
  vec3 N;
  /* Enum used as index to fetch which light intensity to use [0..3]. */
  LightingType type;
  /* Output both shadowed and unshadowed for shadow denoising. */
  vec3 light_shadowed;
  vec3 light_unshadowed;
};

/* Represent an approximation of a bunch of rays from a BSDF. */
struct LightProbeRay {
  /* Average direction of sampled rays or its approximation.
   * Magnitude will reduce directionality of spherical harmonic evaluation. */
  vec3 dominant_direction;
  /* Perceptual roughness in [0..1] range.
   * Modulate blur level of spherical probe and blend between sphere probe and spherical harmonic
   * evaluation at higher roughness. */
  float perceptual_roughness;
};

/* General purpose 3D ray. */
struct Ray {
  vec3 origin;
  vec3 direction;
  float max_time;
};

/* -------------------------------------------------------------------- */
/** \name Utils
 * \{ */

/* Fresnel monochromatic, perfect mirror */
float F_eta(float eta, float cos_theta)
{
  /* Compute fresnel reflectance without explicitly computing
   * the refracted direction. */
  float c = abs(cos_theta);
  float g = eta * eta - 1.0 + c * c;
  if (g > 0.0) {
    g = sqrt(g);
    float A = (g - c) / (g + c);
    float B = (c * (g + c) - 1.0) / (c * (g - c) + 1.0);
    return 0.5 * A * A * (1.0 + B * B);
  }
  /* Total internal reflections. */
  return 1.0;
}

/** \} */
