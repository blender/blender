/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(eevee_bxdf_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_thickness_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_matrix_lib.glsl)

/* -------------------------------------------------------------------- */
/** \name Diffuse BSDF
 * \{ */

/**
 * Returns a tangent space diffuse direction following the Lambertian distribution.
 *
 * \param rand: random point on the unit cylinder (result of sample_cylinder).
 *              The Z component can be biased towards 1.
 * \return pdf: the pdf of sampling the reflected/refracted ray. 0 if ray is invalid.
 */
BsdfSample bxdf_diffuse_sample(vec3 rand)
{
  float cos_theta = safe_sqrt(rand.x);
  BsdfSample samp;
  samp.direction = vec3(rand.yz * sin_from_cos(cos_theta), cos_theta);
  samp.pdf = cos_theta * M_1_PI;
  return samp;
}

BsdfEval bxdf_diffuse_eval(vec3 N, vec3 L)
{
  BsdfEval eval;
  eval.throughput = eval.pdf = saturate(dot(N, L));
  return eval;
}

float bxdf_diffuse_perceived_roughness()
{
  return 1.0;
}

LightProbeRay bxdf_diffuse_lightprobe(vec3 N)
{
  LightProbeRay probe;
  probe.perceptual_roughness = bxdf_diffuse_perceived_roughness();
  probe.dominant_direction = N;
  return probe;
}

#ifdef EEVEE_UTILITY_TX

ClosureLight bxdf_diffuse_light(ClosureUndetermined cl)
{
  ClosureLight light;
  light.ltc_mat = vec4(1.0, 0.0, 0.0, 1.0); /* No transform, just plain cosine distribution. */
  light.N = cl.N;
  light.type = LIGHT_DIFFUSE;
  return light;
}

#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Translucent BSDF
 * \{ */

/**
 * Returns a tangent space diffuse direction following and inverted Lambertian distribution.
 *
 * \param rand: random point on the unit cylinder (result of sample_cylinder).
 *              The Z component can be biased towards 1.
 * \param thickness: Thickness of the object. 0 is considered thin.
 * \return pdf: the pdf of sampling the reflected/refracted ray. 0 if ray is invalid.
 */
BsdfSample bxdf_translucent_sample(vec3 rand, float thickness)
{
  if (thickness > 0.0) {
    /* Two transmission events inside a sphere is a uniform sphere distribution. */
    float cos_theta = rand.x * 2.0 - 1.0;
    BsdfSample samp;
    samp.direction = vec3(rand.yz * sin_from_cos(cos_theta), -cos_theta);
    samp.pdf = 0.25 * M_1_PI;
    return samp;
  }

  /* Inverted cosine distribution. */
  BsdfSample samp = bxdf_diffuse_sample(rand);
  samp.direction = -samp.direction;
  return samp;
}

BsdfEval bxdf_translucent_eval(vec3 N, vec3 L, float thickness)
{
  if (thickness > 0.0) {
    /* Two transmission events inside a sphere is a uniform sphere distribution. */
    BsdfEval eval;
    eval.throughput = eval.pdf = 0.25 * M_1_PI;
    return eval;
  }

  BsdfEval eval;
  eval.throughput = eval.pdf = saturate(dot(-N, L));
  return eval;
}

float bxdf_translucent_perceived_roughness()
{
  return 1.0;
}

LightProbeRay bxdf_translucent_lightprobe(vec3 N, float thickness)
{
  LightProbeRay probe;
  probe.perceptual_roughness = bxdf_translucent_perceived_roughness();
  /* If using the spherical assumption, discard any directionality from the lighting. */
  probe.dominant_direction = (thickness > 0.0) ? vec3(0.0) : -N;
  return probe;
}

Ray bxdf_translucent_ray_amend(ClosureUndetermined cl, vec3 V, Ray ray, float thickness)
{
  if (thickness > 0.0) {
    /* Ray direction is distributed on the whole sphere.
     * Move the ray origin to the sphere surface (with bias to avoid self-intersection). */
    ray.origin += (ray.direction - cl.N) * thickness * 0.505;
  }
  return ray;
}

#ifdef EEVEE_UTILITY_TX

ClosureLight bxdf_translucent_light(ClosureUndetermined cl, vec3 V, float thickness)
{
  /* A translucent sphere lit by a light outside the sphere transmits the
   * light uniformly over the sphere. To mimic this phenomenon, we use the light vector
   * as normal. This is done inside `light_eval_single`.
   *
   * For slab model, the approximation has little to no impact on the lighting in practice,
   * only focusing the light a tiny bit. Using the flipped normal is good enough approximation.
   */
  ClosureLight light;
  light.ltc_mat = vec4(1.0, 0.0, 0.0, 1.0); /* No transform, just plain cosine distribution. */
  /* Tag for `is_translucent_with_thickness`. */
  light.N = (thickness > 0.0) ? vec3(2.0) : -cl.N;
  light.type = LIGHT_DIFFUSE;
  return light;
}

#endif

/** \} */
