/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Postprocess diffuse radiance output from the diffuse evaluation pass to mimic subsurface
 * transmission.
 *
 * This implementation follows the technique described in the siggraph presentation:
 * "Efficient screen space subsurface scattering Siggraph 2018"
 * by Evgenii Golubev
 *
 * But, instead of having all the precomputed weights for all three color primaries,
 * we precompute a weight profile texture to be able to support per pixel AND per channel radius.
 */

#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_geom_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)

void main(void)
{
  vec2 center_uv = uvcoordsvar.xy;
  ivec2 texel = ivec2(gl_FragCoord.xy);

  float gbuffer_depth = texelFetch(hiz_tx, texel, 0).r;
  vec3 vP = get_view_space_from_depth(center_uv, gbuffer_depth);

  vec4 color_1_packed = texelFetch(gbuffer_color_tx, ivec3(texel, 1), 0);
  vec4 gbuffer_2_packed = texelFetch(gbuffer_closure_tx, ivec3(texel, 2), 0);

  ClosureDiffuse diffuse;
  diffuse.sss_radius = gbuffer_sss_radii_unpack(gbuffer_2_packed.xyz);
  diffuse.sss_id = gbuffer_object_id_unorm16_unpack(gbuffer_2_packed.w);
  diffuse.color = gbuffer_color_unpack(color_1_packed);

  if (diffuse.sss_id == 0u) {
    /* Normal diffuse is already in combined pass. */
    /* Refraction also go into this case. */
    out_combined = vec4(0.0);
    return;
  }

  float max_radius = max_v3(diffuse.sss_radius);

  float homcoord = ProjectionMatrix[2][3] * vP.z + ProjectionMatrix[3][3];
  vec2 sample_scale = vec2(ProjectionMatrix[0][0], ProjectionMatrix[1][1]) *
                      (0.5 * max_radius / homcoord);

  float pixel_footprint = sample_scale.x * textureSize(hiz_tx, 0).x;
  if (pixel_footprint <= 1.0) {
    /* Early out. */
    out_combined = vec4(0.0);
    return;
  }

  diffuse.sss_radius = max(vec3(1e-4), diffuse.sss_radius / max_radius) * max_radius;
  /* Scale albedo because we can have HDR value caused by BSDF sampling. */
  vec3 albedo = diffuse.color / max(1e-6, max_v3(diffuse.color));
  vec3 d = burley_setup(diffuse.sss_radius, albedo);

  /* Do not rotate too much to avoid too much cache misses. */
  float golden_angle = M_PI * (3.0 - sqrt(5.0));
  float theta = interlieved_gradient_noise(gl_FragCoord.xy, 0, 0.0) * golden_angle;
  float cos_theta = cos(theta);
  float sin_theta = sqrt(1.0 - sqr(cos_theta));
  mat2 rot = mat2(cos_theta, sin_theta, -sin_theta, cos_theta);

  mat2 scale = mat2(sample_scale.x, 0.0, 0.0, sample_scale.y);
  mat2 sample_space = scale * rot;

  vec3 accum_weight = vec3(0.0);
  vec3 accum = vec3(0.0);

  /* TODO/OPTI(fclem) Make separate sample set for lower radius. */

  for (int i = 0; i < uniform_buf.subsurface.sample_len; i++) {
    vec2 sample_uv = center_uv + sample_space * uniform_buf.subsurface.samples[i].xy;
    float pdf_inv = uniform_buf.subsurface.samples[i].z;

    float sample_depth = textureLod(hiz_tx, sample_uv * uniform_buf.hiz.uv_scale, 0.0).r;
    vec3 sample_vP = get_view_space_from_depth(sample_uv, sample_depth);

    vec4 sample_data = texture(radiance_tx, sample_uv);
    vec3 sample_radiance = sample_data.rgb;
    uint sample_sss_id = uint(sample_data.a);

    if (sample_sss_id != diffuse.sss_id) {
      continue;
    }

    /* Discard out of bounds samples. */
    if (any(lessThan(sample_uv, vec2(0.0))) || any(greaterThan(sample_uv, vec2(1.0)))) {
      continue;
    }

    /* Slide 34. */
    float r = distance(sample_vP, vP);
    vec3 weight = burley_eval(d, r) * pdf_inv;

    accum += sample_radiance * weight;
    accum_weight += weight;
  }
  /* Normalize the sum (slide 34). */
  accum /= accum_weight;

  if (uniform_buf.render_pass.diffuse_light_id >= 0) {
    imageStore(
        rp_color_img, ivec3(texel, uniform_buf.render_pass.diffuse_light_id), vec4(accum, 1.0));
  }

  /* This pass uses additive blending.
   * Subtract the surface diffuse radiance so it's not added twice. */
  accum -= texelFetch(radiance_tx, texel, 0).rgb;

  /* Apply surface color on final radiance. */
  accum *= diffuse.color;

  /* Debug, detect NaNs. */
  if (any(isnan(accum))) {
    accum = vec3(1.0, 0.0, 1.0);
  }

  out_combined = vec4(accum, 0.0);
}
