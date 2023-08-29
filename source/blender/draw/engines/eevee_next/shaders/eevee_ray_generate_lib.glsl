/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Ray generation routines for each BSDF types.
 */

#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_bxdf_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_ray_types_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)

/* Could maybe become parameters. */
#define RAY_BIAS_REFLECTION 0.02
#define RAY_BIAS_REFRACTION 0.02
#define RAY_BIAS_DIFFUSE 0.02

/* Returns view-space ray. */
vec3 ray_generate_direction(vec2 noise, ClosureReflection reflection, vec3 V, out float pdf)
{
  vec2 noise_offset = sampling_rng_2D_get(SAMPLING_RAYTRACE_U);
  vec3 Xi = sample_cylinder(fract(noise_offset + noise));
  /* Bias the rays so we never get really high energy rays almost parallel to the surface. */
  Xi.x = Xi.x * (1.0 - RAY_BIAS_REFLECTION) + RAY_BIAS_REFLECTION;

  float roughness_sqr = max(1e-3, sqr(reflection.roughness));
  /* Gives *perfect* reflection for very small roughness. */
  if (reflection.roughness < 0.0016) {
    Xi = vec3(0.0);
  }

  vec3 T, B, N = reflection.N;
  make_orthonormal_basis(N, T, B);
  return sample_ggx_reflect(Xi, roughness_sqr, V, N, T, B, pdf);
}

/* Returns view-space ray. */
vec3 ray_generate_direction(vec2 noise, ClosureRefraction refraction, vec3 V, out float pdf)
{
  vec2 noise_offset = sampling_rng_2D_get(SAMPLING_RAYTRACE_U);
  vec3 Xi = sample_cylinder(fract(noise_offset + noise));
  /* Bias the rays so we never get really high energy rays almost parallel to the surface. */
  Xi.x = Xi.x * (1.0 - RAY_BIAS_REFRACTION) + RAY_BIAS_REFRACTION;

  float roughness_sqr = max(1e-3, sqr(refraction.roughness));
  /* Gives *perfect* refraction for very small roughness. */
  if (refraction.roughness < 0.0016) {
    Xi = vec3(0.0);
  }

  vec3 T, B, N = refraction.N;
  make_orthonormal_basis(N, T, B);
  return sample_ggx_refract(Xi, roughness_sqr, refraction.ior, V, N, T, B, pdf);
}

/* Returns view-space ray. */
vec3 ray_generate_direction(vec2 noise, ClosureDiffuse diffuse, vec3 V, out float pdf)
{
  vec2 noise_offset = sampling_rng_2D_get(SAMPLING_RAYTRACE_U);
  vec3 Xi = sample_cylinder(fract(noise_offset + noise));
  /* Bias the rays so we never get really high energy rays almost parallel to the surface. */
  Xi.x = Xi.x * (1.0 - RAY_BIAS_DIFFUSE) + RAY_BIAS_DIFFUSE;

  vec3 T, B, N = diffuse.N;
  make_orthonormal_basis(N, T, B);

  return sample_cosine_hemisphere(Xi, N, T, B, pdf);
}
