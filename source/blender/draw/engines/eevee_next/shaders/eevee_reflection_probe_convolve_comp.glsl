/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Shader to convert cube-map to octahedral projection. */

#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_base_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_matrix_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_reflection_probe_mapping_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)

/* Bypass convolution cascade and projection logic. */
// #define ALWAYS_SAMPLE_CUBEMAP
/* Debugging texel alignment. */
// #define USE_PIXEL_CHECKERBOARD

float roughness_from_relative_mip(float prev_mip_roughness, float curr_mip_roughness)
{
#ifdef ALWAYS_SAMPLE_CUBEMAP
  /* For reference and debugging. */
  return curr_mip_roughness;
#else
  /* The exponent should be 2 but result is a bit less blurry than expected in practice. */
  const float exponent = 3.0;
  /* From linear roughness to GGX roughness input. */
  float m_prev = pow(prev_mip_roughness, exponent);
  float m_curr = pow(curr_mip_roughness, exponent);
  /* Given that spherical gaussians are very close to regular gaussian in 1D,
   * we reuse the same rule for successive convolution (i.e: G(x,a) X G(x,b) = G(x,a+b)).
   * While this isn't technically correct, this still works quite well in practice. */
  float m_target = m_curr - m_prev;
  /* From GGX roughness input to linear roughness. */
  return pow(m_target, 1.0 / exponent);
#endif
}

float cone_cosine_from_roughness(float linear_roughness)
{
  /* From linear roughness to GGX roughness input. */
  float m = square(linear_roughness);
  /* Chosen so that roughness of 1.0 maps to half pi cone aperture. */
  float cutoff_value = mix(0.01, 0.14, m);
  /* Inversion of the spherical gaussian. This gives the cutoff for the half angle from N.H. */
  float half_angle_cos = 1.0 + (log(cutoff_value) * square(m)) / 2.0;
  float half_angle_sin = safe_sqrt(1.0 - square(half_angle_cos));
  /* Use cosine rule to avoid acos. Return cos(2 * half_angle). */
  return square(half_angle_cos) - square(half_angle_sin);
}

int sample_count_get()
{
  /* After experimenting this is likely to be the best value if we keep the max resolution to 2048.
   * This isn't ideal, but the better solution would be to use multiple steps per mip which would
   * reduce the number of sample per step (use sum of gaussian per step). */
  return 196;
}

float sample_weight(vec3 out_direction, vec3 in_direction, float linear_roughness)
{
  out_direction = normalize(out_direction);
  in_direction = normalize(in_direction);

  float cos_theta = saturate(dot(out_direction, in_direction));

  /* From linear roughness to GGX roughness input. */
  float m = square(linear_roughness);
  /* Map GGX roughness to spherical gaussian sharpness.
   * From "SG Series Part 4: Specular Lighting From an SG Light Source" by MJP
   * https://therealmjp.github.io/posts/sg-series-part-4-specular-lighting-from-an-sg-light-source/
   */
  vec3 N = out_direction;
  vec3 H = normalize(out_direction + in_direction);
  float NH = saturate(dot(N, H));
  /* GGX. */
  // return exp(-square(acos(NH) / m));
  /* Spherical Gaussian. */
  return exp(2.0 * (NH - 1.0) / square(m));
}

mat3x3 tangent_basis(vec3 N)
{
  /* TODO(fclem): This create a discontinuity at Z=0. */
  return from_up_axis(N);
}

void main()
{
  SphereProbeUvArea sample_coord = reinterpret_as_atlas_coord(probe_coord_packed);
  SphereProbePixelArea out_texel_area = reinterpret_as_write_coord(write_coord_packed);
  SphereProbePixelArea in_texel_area = reinterpret_as_write_coord(read_coord_packed);

  /* Texel in probe. */
  ivec2 out_local_texel = ivec2(gl_GlobalInvocationID.xy);

  /* Exit when pixel being written doesn't fit in the area reserved for the probe. */
  if (any(greaterThanEqual(out_local_texel, ivec2(out_texel_area.extent)))) {
    return;
  }

  /* From mip to linear roughness (same as UI). */
  float prev_mip_roughness = sphere_probe_lod_to_roughness(float(read_lod));
  float curr_mip_roughness = sphere_probe_lod_to_roughness(float(read_lod + 1));
  /* In order to reduce the sample count, we sample the content of previous mip level.
   * But this one has already been convolved. So we have to derive the equivalent roughness
   * that produces the same result. */
  float mip_roughness = roughness_from_relative_mip(prev_mip_roughness, curr_mip_roughness);
  /* Clamp to avoid numerical imprecision. */
  float mip_roughness_clamped = max(mip_roughness, BSDF_ROUGHNESS_THRESHOLD);
  float cone_cos = cone_cosine_from_roughness(mip_roughness_clamped);

  vec3 out_direction = sphere_probe_texel_to_direction(
      out_local_texel, out_texel_area, sample_coord);
  out_direction = normalize(out_direction);

  mat3x3 basis = tangent_basis(out_direction);

  ivec2 out_texel = out_texel_area.offset + out_local_texel;

  float weight_accum = 0.0;
  vec4 radiance_accum = vec4(0.0);

  int sample_count = sample_count_get();
  for (int i = 0; i < sample_count; i++) {
    vec2 rand = hammersley_2d(i, sample_count);
    vec3 in_direction = basis * sample_uniform_cone(rand, cone_cos);

#ifndef ALWAYS_SAMPLE_CUBEMAP
    vec2 in_uv = sphere_probe_direction_to_uv(in_direction, float(read_lod), sample_coord);
    vec4 radiance = texture(in_atlas_mip_tx, vec3(in_uv, sample_coord.layer));
#else /* For reference and debugging. */
    vec4 radiance = texture(cubemap_tx, in_direction);
#endif

    float weight = sample_weight(out_direction, in_direction, mip_roughness_clamped);
    radiance_accum += radiance * weight;
    weight_accum += weight;
  }
  vec4 out_radiance = radiance_accum * safe_rcp(weight_accum);

#ifdef USE_PIXEL_CHECKERBOARD
  ivec2 a = out_texel % 2;
  out_radiance = vec4(a.x == a.y);
#endif

  imageStore(out_atlas_mip_img, ivec3(out_texel, out_texel_area.layer), out_radiance);
}
