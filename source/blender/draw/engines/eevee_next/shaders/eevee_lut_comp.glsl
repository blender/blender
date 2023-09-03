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
 * The result is interpreted as: `integral = f0 * scale + f90 * bias`. */
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

/* Generate BSDF LUT for `IOR < 1`. Returns the transmittance and the reflectance. */
vec4 ggx_btdf_split_sum(vec3 lut_coord)
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
  float transmittance = 0.0;
  float reflectance = 0.0;

  const uint sample_count = 512u * 512u;
  for (uint i = 0u; i < sample_count; i++) {
    vec2 rand = hammersley_2d(i, sample_count);
    vec3 Xi = sample_cylinder(rand);

    /* Microfacet normal. */
    vec3 H = sample_ggx(Xi, roughness, V);
    float fresnel = F_eta(ior, dot(V, H));

    /* Reflection. */
    vec3 R = -reflect(V, H);
    float NR = R.z;
    if (NR > 0.0) {
      /* Assuming sample visible normals, accumulating `brdf * NV / pdf.` */
      reflectance += fresnel * bxdf_ggx_smith_G1(NR, roughness_sq);
    }

    /* Refraction. */
    vec3 T = refract(-V, H, ior);
    float NT = T.z;
    /* In the case of TIR, `T == vec3(0)`. */
    if (NT < 0.0) {
      /* Assuming sample visible normals, accumulating `btdf * NV / pdf.` */
      transmittance += (1.0 - fresnel) * bxdf_ggx_smith_G1(NT, roughness_sq);
    }
  }
  transmittance /= float(sample_count);
  reflectance /= float(sample_count);

  /* There is place to put multi-scatter result (which is a little bit different still)
   * and / or lobe fitting for better sampling of. */
  return vec4(transmittance, reflectance, 0.0, 0.0);
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
    case LUT_GGX_BTDF_SPLIT_SUM:
      result = ggx_btdf_split_sum(lut_normalized_coordinate);
      break;
  }
  imageStore(table_img, ivec3(gl_GlobalInvocationID), result);
}
