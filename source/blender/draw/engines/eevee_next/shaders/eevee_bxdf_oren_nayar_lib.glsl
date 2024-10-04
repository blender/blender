/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_bxdf_lib.glsl"
#include "eevee_thickness_lib.glsl"
#include "gpu_shader_math_matrix_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"

/* -------------------------------------------------------------------- */
/** \name Oren Nayar BSDF
 * \{ */

/**
 * Returns a tangent space diffuse direction following the Oren-Nayar distribution.
 *
 * \return pdf: the pdf of sampling the reflected/refracted ray. 0 if ray is invalid.
 */
BsdfSample bxdf_oren_nayar_sample(vec3 random_point_cos_hemisphere)
{
  /* Bias the rays so we never get really high energy rays almost parallel to the surface.
   * Also reduces shadow terminator artifacts. */
  const float bias = 0.05;
  random_point_cos_hemisphere = normalize(random_point_cos_hemisphere + vec3(0.0, 0.0, bias));

  float cos_theta = random_point_cos_hemisphere.z;
  BsdfSample samp;
  samp.direction = random_point_cos_hemisphere;
  samp.pdf = cos_theta * M_1_PI;
  return samp;
}

Spectrum bsdf_oren_nayar_get_intensity(ShaderClosure *sc, float3 n, float3 v, float3 l)
{
  OrenNayarBsdf *bsdf = (OrenNayarBsdf *)sc;
  float NL = max(dot(n, l), 0.0);
  float NV = max(dot(n, v), 0.0);
  float t = dot(l, v) - NL * NV;

  if (t > 0.0) {
    t /= max(NL, NV) + FLT_MIN;
  }

  float single_scatter = bsdf->a + bsdf->b * t;

  float El = bsdf->a * M_PI_F + bsdf->b * bsdf_oren_nayar_G(NL);
  Spectrum multi_scatter = bsdf->multiscatter_term * (1.0 - El);

  return NL * (make_spectrum(single_scatter) + multi_scatter);
}

BsdfEval bxdf_oren_nayar_eval(vec3 N, vec3 L)
{
  BsdfEval eval;
  eval.throughput = eval.pdf = saturate(dot(N, L));
  return eval;
}

float bxdf_oren_nayar_perceived_roughness()
{
  return 1.0;
}

LightProbeRay bxdf_oren_nayar_lightprobe(vec3 N)
{
  LightProbeRay probe;
  probe.perceptual_roughness = bxdf_oren_nayar_perceived_roughness();
  probe.dominant_direction = N;
  return probe;
}

#ifdef EEVEE_UTILITY_TX

ClosureLight bxdf_oren_nayar_light(ClosureUndetermined cl)
{
  ClosureLight light;
  /* TODO(fclem): LTC fit. */
  light.ltc_mat = vec4(1.0, 0.0, 0.0, 1.0); /* No transform, just plain cosine distribution. */
  light.N = cl.N;
  light.type = LIGHT_DIFFUSE;
  return light;
}

#endif

/** \} */
