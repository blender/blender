/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Used to generate Look Up Tables. Not used in default configuration as the tables are stored in
 * the blender executable. This is only used for reference or to update them.
 */

#pragma BLENDER_REQUIRE(gpu_shader_math_base_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_bxdf_sampling_lib.glsl)

/* Generate BRDF LUT following "Real shading in unreal engine 4" by Brian Karis
 * https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf
 * Parametrizing with `x = roughness` and `y = sqrt(1.0 - cos(theta))`.
 * The result is interpreted as: `integral = F0 * scale + F90 * bias`. */
vec4 ggx_brdf_split_sum(vec3 lut_coord)
{
  /* Squaring for perceptually linear roughness, see [Physically Based Shading at Disney]
   * (https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf)
   * Section 5.4. */
  float roughness = square(lut_coord.x);
  float roughness_sq = square(roughness);

  float NV = clamp(1.0 - square(lut_coord.y), 1e-4, 0.9999);
  vec3 V = vec3(sqrt(1.0 - square(NV)), 0.0, NV);

  /* Integrating BRDF. */
  float scale = 0.0;
  float bias = 0.0;
  const uint sample_count = 512u * 512u;
  for (uint i = 0u; i < sample_count; i++) {
    vec2 rand = hammersley_2d(i, sample_count);
    vec3 Xi = sample_cylinder(rand);

    /* Microfacet normal. */
    vec3 H = sample_ggx(Xi, roughness, V);
    vec3 L = -reflect(V, H);
    float NL = L.z;

    if (NL > 0.0) {
      /* Assuming sample visible normals, `weight = brdf * NV / (pdf * fresnel).` */
      float weight = bxdf_ggx_smith_G1(NL, roughness_sq);
      /* Schlick's Fresnel. */
      float s = saturate(pow5f(1.0 - saturate(dot(V, H))));
      scale += (1.0 - s) * weight;
      bias += s * weight;
    }
  }
  scale /= float(sample_count);
  bias /= float(sample_count);

  return vec4(scale, bias, 0.0, 0.0);
}

/* Generate BSDF LUT for `IOR < 1` using Schlick's approximation. Returns the transmittance and the
 * scale and bias for reflectance.
 *
 * The result is interpreted as:
 * `reflectance = F0 * scale + F90 * bias`,
 * `transmittance = (1 - F0) * transmission_factor`. */
vec4 ggx_bsdf_split_sum(vec3 lut_coord)
{
  float ior = sqrt(lut_coord.x);
  /* ior is sin of critical angle. */
  float critical_cos = sqrt(1.0 - saturate(square(ior)));

  lut_coord.y = lut_coord.y * 2.0 - 1.0;
  /* Maximize texture usage on both sides of the critical angle. */
  lut_coord.y *= (lut_coord.y > 0.0) ? (1.0 - critical_cos) : critical_cos;
  /* Center LUT around critical angle to avoid strange interpolation issues when the critical
   * angle is changing. */
  lut_coord.y += critical_cos;
  float NV = clamp(lut_coord.y, 1e-4, 0.9999);

  /* Squaring for perceptually linear roughness, see [Physically Based Shading at Disney]
   * (https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf)
   * Section 5.4. */
  float roughness = square(lut_coord.z);
  float roughness_sq = square(roughness);

  vec3 V = vec3(sqrt(1.0 - square(NV)), 0.0, NV);

  /* Integrating BSDF */
  float scale = 0.0;
  float bias = 0.0;
  float transmission_factor = 0.0;
  const uint sample_count = 512u * 512u;
  for (uint i = 0u; i < sample_count; i++) {
    vec2 rand = hammersley_2d(i, sample_count);
    vec3 Xi = sample_cylinder(rand);

    /* Microfacet normal. */
    vec3 H = sample_ggx(Xi, roughness, V);
    float HL = 1.0 - (1.0 - square(dot(V, H))) / square(ior);
    float s = saturate(pow5f(1.0 - saturate(HL)));

    /* Reflection. */
    vec3 R = -reflect(V, H);
    float NR = R.z;
    if (NR > 0.0) {
      /* Assuming sample visible normals, `weight = brdf * NV / (pdf * fresnel).` */
      float weight = bxdf_ggx_smith_G1(NR, roughness_sq);
      scale += (1.0 - s) * weight;
      bias += s * weight;
    }

    /* Refraction. */
    vec3 T = refract(-V, H, 1.0 / ior);
    float NT = T.z;
    /* In the case of TIR, `T == vec3(0)`. */
    if (NT < 0.0) {
      /* Assuming sample visible normals, accumulating `btdf * NV / (pdf * (1 - F0)).` */
      transmission_factor += (1.0 - s) * bxdf_ggx_smith_G1(NT, roughness_sq);
    }
  }
  transmission_factor /= float(sample_count);
  scale /= float(sample_count);
  bias /= float(sample_count);

  return vec4(scale, bias, transmission_factor, 0.0);
}

/* Generate BTDF LUT for `IOR > 1` using Schlick's approximation. Only the transmittance is needed
 * because the scale and bias does not depend on the IOR and can be obtained from the BRDF LUT.
 *
 * Parameterize with `x = sqrt((ior - 1) / (ior + 1))` for higher precision in 1 < IOR < 2,
 * and `y = sqrt(1.0 - cos(theta))`, `z = roughness` similar to BRDF LUT.
 *
 * The result is interpreted as:
 * `transmittance = (1 - F0) * transmission_factor`. */
vec4 ggx_btdf_gt_one(vec3 lut_coord)
{
  float f0 = square(lut_coord.x);
  float inv_ior = (1.0 - f0) / (1.0 + f0);

  float NV = clamp(1.0 - square(lut_coord.y), 1e-4, 0.9999);
  vec3 V = vec3(sqrt(1.0 - square(NV)), 0.0, NV);

  /* Squaring for perceptually linear roughness, see [Physically Based Shading at Disney]
   * (https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf)
   * Section 5.4. */
  float roughness = square(lut_coord.z);
  float roughness_sq = square(roughness);

  /* Integrating BTDF. */
  float transmission_factor = 0.0;
  const uint sample_count = 512u * 512u;
  for (uint i = 0u; i < sample_count; i++) {
    vec2 rand = hammersley_2d(i, sample_count);
    vec3 Xi = sample_cylinder(rand);

    /* Microfacet normal. */
    vec3 H = sample_ggx(Xi, roughness, V);

    /* Refraction. */
    vec3 L = refract(-V, H, inv_ior);
    float NL = L.z;

    if (NL < 0.0) {
      /* Schlick's Fresnel. */
      float s = saturate(pow5f(1.0 - saturate(dot(V, H))));
      /* Assuming sample visible normals, accumulating `btdf * NV / (pdf * (1 - F0)).` */
      transmission_factor += (1.0 - s) * bxdf_ggx_smith_G1(NL, roughness_sq);
    }
  }
  transmission_factor /= float(sample_count);

  return vec4(transmission_factor, 0.0, 0.0, 0.0);
}

/* Generate SSS translucency profile.
 * We precompute the exit radiance for a slab of homogenous material backface-lit by a directional
 * light. We only integrate for a single color primary since the profile will be applied to each
 * primary independantly.
 * For each distance `d` we compute the radiance incoming from an hypothetical parallel plane. */
vec4 burley_sss_translucency(vec3 lut_coord)
{
  /* Note that we only store the 1st (radius == 1) component.
   * The others are here for debugging overall appearance. */
  vec3 radii = vec3(1.0, 0.2, 0.1);
  float thickness = lut_coord.x * SSS_TRANSMIT_LUT_RADIUS;
  vec3 r = thickness / radii;
  /* Manual fit based on cycles render of a backlit slab of varying thickness.
   * Mean Error: 0.003
   * Max Error: 0.015 */
  vec3 exponential = exp(-3.6 * pow(r, vec3(1.11)));
  vec3 gaussian = exp(-pow(3.4 * r, vec3(1.6)));
  vec3 fac = square(saturate(0.5 + r / 0.6));
  vec3 profile = saturate(mix(gaussian, exponential, fac));
  /* Mask off the end progressively to 0. */
  profile *= saturate(1.0 - pow5f(lut_coord.x));

  return vec4(profile, 0.0);
}

vec4 random_walk_sss_translucency(vec3 lut_coord)
{
  /* Note that we only store the 1st (radius == 1) component.
   * The others are here for debugging overall appearance. */
  vec3 radii = vec3(1.0, 0.2, 0.1);
  float thickness = lut_coord.x * SSS_TRANSMIT_LUT_RADIUS;
  vec3 r = thickness / radii;
  /* Manual fit based on cycles render of a backlit slab of varying thickness.
   * Mean Error: 0.003
   * Max Error: 0.016 */
  vec3 scale = vec3(0.31, 0.47, 0.32);
  vec3 exponent = vec3(-22.0, -5.8, -0.5);
  vec3 profile = vec3(dot(scale, exp(exponent * r.r)),
                      dot(scale, exp(exponent * r.g)),
                      dot(scale, exp(exponent * r.b)));
  profile = saturate(profile - 0.1);
  /* Mask off the end progressively to 0. */
  profile *= saturate(1.0 - pow5f(lut_coord.x));

  return vec4(profile, 0.0);
}

void main()
{
  /* Make sure coordinates are covering the whole [0..1] range at texel center. */
  vec3 lut_normalized_coordinate = vec3(gl_GlobalInvocationID) / vec3(table_extent - 1);
  /* Make sure missing cases are noticeable. */
  vec4 result = vec4(-1);
  switch (uint(table_type)) {
    case LUT_GGX_BRDF_SPLIT_SUM:
      result = ggx_brdf_split_sum(lut_normalized_coordinate);
      break;
    case LUT_GGX_BTDF_IOR_GT_ONE:
      result = ggx_btdf_gt_one(lut_normalized_coordinate);
      break;
    case LUT_GGX_BSDF_SPLIT_SUM:
      result = ggx_bsdf_split_sum(lut_normalized_coordinate);
      break;
    case LUT_BURLEY_SSS_PROFILE:
      result = burley_sss_translucency(lut_normalized_coordinate);
      break;
    case LUT_RANDOM_WALK_SSS_PROFILE:
      result = random_walk_sss_translucency(lut_normalized_coordinate);
      break;
  }
  imageStore(table_img, ivec3(gl_GlobalInvocationID), result);
}
