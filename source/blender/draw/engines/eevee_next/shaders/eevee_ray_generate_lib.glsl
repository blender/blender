/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Ray generation routines for each BSDF types.
 */

#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_matrix_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_bxdf_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_ray_types_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)

struct BsdfSample {
  vec3 direction;
  float pdf;
};

/* Could maybe become parameters. */
#define RAY_BIAS 0.05

bool is_singular_ray(float roughness)
{
  return roughness < BSDF_ROUGHNESS_THRESHOLD;
}

/* Returns view-space ray. */
BsdfSample ray_generate_direction(vec2 noise, ClosureUndetermined cl, vec3 V)
{
  vec3 random_point_on_cylinder = sample_cylinder(noise);
  /* Bias the rays so we never get really high energy rays almost parallel to the surface. */
  random_point_on_cylinder.x = random_point_on_cylinder.x * (1.0 - RAY_BIAS) + RAY_BIAS;

  mat3 world_to_tangent = from_up_axis(cl.N);

  BsdfSample samp;
  switch (cl.type) {
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      samp.direction = sample_cosine_hemisphere(random_point_on_cylinder,
                                                -world_to_tangent[2],
                                                world_to_tangent[1],
                                                world_to_tangent[0],
                                                samp.pdf);
      break;
    case CLOSURE_BSSRDF_BURLEY_ID:
    case CLOSURE_BSDF_DIFFUSE_ID:
      samp.direction = sample_cosine_hemisphere(random_point_on_cylinder,
                                                world_to_tangent[2],
                                                world_to_tangent[1],
                                                world_to_tangent[0],
                                                samp.pdf);
      break;
    case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID: {
      if (is_singular_ray(to_closure_reflection(cl).roughness)) {
        samp.direction = reflect(-V, cl.N);
        samp.pdf = 1.0;
      }
      else {
        samp.direction = sample_ggx_reflect(random_point_on_cylinder,
                                            square(to_closure_reflection(cl).roughness),
                                            V,
                                            world_to_tangent[2],
                                            world_to_tangent[1],
                                            world_to_tangent[0],
                                            samp.pdf);
      }
      break;
    }
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID: {
      if (is_singular_ray(to_closure_refraction(cl).roughness)) {
        samp.direction = refract(-V, cl.N, 1.0 / to_closure_refraction(cl).ior);
        samp.pdf = 1.0;
      }
      else {
        samp.direction = sample_ggx_refract(random_point_on_cylinder,
                                            square(to_closure_refraction(cl).roughness),
                                            to_closure_refraction(cl).ior,
                                            V,
                                            world_to_tangent[2],
                                            world_to_tangent[1],
                                            world_to_tangent[0],
                                            samp.pdf);
      }
      break;
    }
    case CLOSURE_NONE_ID:
      /* TODO(fclem): Assert. */
      samp.pdf = 0.0;
      break;
  }

  return samp;
}
