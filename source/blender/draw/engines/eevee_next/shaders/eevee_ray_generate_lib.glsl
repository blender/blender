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
#pragma BLENDER_REQUIRE(eevee_thickness_lib.glsl)

struct BsdfSample {
  vec3 direction;
  float pdf;
};

bool is_singular_ray(float roughness)
{
  return roughness < BSDF_ROUGHNESS_THRESHOLD;
}

/* Returns view-space ray. */
BsdfSample ray_generate_direction(vec2 noise, ClosureUndetermined cl, vec3 V, float thickness)
{
  vec3 random_point_on_cylinder = sample_cylinder(noise);
  /* Bias the rays so we never get really high energy rays almost parallel to the surface. */
  const float rng_bias = 0.08;
  random_point_on_cylinder.x = random_point_on_cylinder.x * (1.0 - rng_bias);

  mat3 world_to_tangent = from_up_axis(cl.N);

  BsdfSample samp;
  switch (cl.type) {
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      if (thickness != 0.0) {
        /* When modeling object thickness as a sphere, the outgoing rays are distributed uniformly
         * over the sphere. We don't need the RAY_BIAS in this case. */
        samp.direction = sample_sphere(noise);
        samp.pdf = sample_pdf_uniform_sphere();
      }
      else {
        samp.direction = sample_cosine_hemisphere(random_point_on_cylinder,
                                                  -world_to_tangent[2],
                                                  world_to_tangent[1],
                                                  world_to_tangent[0],
                                                  samp.pdf);
      }
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
      float ior = to_closure_refraction(cl).ior;
      float roughness = to_closure_refraction(cl).roughness;
      if (thickness != 0.0) {
        float apparent_roughness = refraction_roughness_remapping(roughness, ior);
        vec3 L = refraction_dominant_dir(cl.N, V, ior, apparent_roughness);
        /* NOTE(fclem): Tracing origin is modified in the trace shader. */
        cl.N = -thickness_shape_intersect(thickness, cl.N, L).hit_N;
        ior = 1.0 / ior;
        V = -L;
        world_to_tangent = from_up_axis(cl.N);
      }

      if (is_singular_ray(roughness)) {
        samp.direction = refract(-V, cl.N, 1.0 / ior);
        samp.pdf = 1.0;
      }
      else {
        samp.direction = sample_ggx_refract(random_point_on_cylinder,
                                            square(roughness),
                                            ior,
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
