/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* [Drobot2014a] Low Level Optimizations for GCN */
vec4 fast_rcp(vec4 v)
{
  return intBitsToFloat(0x7eef370b - floatBitsToInt(v));
}

vec3 brdf_approx(vec3 spec_color, float roughness, float NV)
{
  /* Very rough own approx. We don't need it to be correct, just fast.
   * Just simulate fresnel effect with roughness attenuation. */
  float fresnel = exp2(-8.35 * NV) * (1.0 - roughness);
  return mix(spec_color, vec3(1.0), fresnel);
}

void prep_specular(
    vec3 L, vec3 I, vec3 N, vec3 R, out float NL, out float wrapped_NL, out float spec_angle)
{
  wrapped_NL = dot(L, R);
  vec3 half_dir = normalize(L + I);
  spec_angle = clamp(dot(half_dir, N), 0.0, 1.0);
  NL = clamp(dot(L, N), 0.0, 1.0);
}

/* Normalized Blinn shading */
vec4 blinn_specular(vec4 shininess, vec4 spec_angle, vec4 NL)
{
  /* Pi is already divided in the light power.
   * normalization_factor = (shininess + 8.0) / (8.0 * M_PI) */
  vec4 normalization_factor = shininess * 0.125 + 1.0;
  vec4 spec_light = pow(spec_angle, shininess) * NL * normalization_factor;

  return spec_light;
}

/* NL need to be unclamped. w in [0..1] range. */
vec4 wrapped_lighting(vec4 NL, vec4 w)
{
  vec4 w_1 = w + 1.0;
  vec4 denom = fast_rcp(w_1 * w_1);
  return clamp((NL + w) * denom, 0.0, 1.0);
}

vec3 get_world_lighting(vec3 base_color, float roughness, float metallic, vec3 N, vec3 I)
{
  vec3 specular_color, diffuse_color;

  if (world_data.use_specular) {
    diffuse_color = mix(base_color, vec3(0.0), metallic);
    specular_color = mix(vec3(0.05), base_color, metallic);
  }
  else {
    diffuse_color = base_color;
    specular_color = vec3(0.0);
  }

  vec3 specular_light = world_data.ambient_color.rgb;
  vec3 diffuse_light = world_data.ambient_color.rgb;
  vec4 wrap = vec4(world_data.lights[0].diffuse_color_wrap.a,
                   world_data.lights[1].diffuse_color_wrap.a,
                   world_data.lights[2].diffuse_color_wrap.a,
                   world_data.lights[3].diffuse_color_wrap.a);

  if (world_data.use_specular) {
    /* Prepare Specular computation. Eval 4 lights at once. */
    vec3 R = -reflect(I, N);

#ifdef GPU_METAL
    /* Split vectors into arrays of floats. Partial vector references are unsupported by MSL. */
    float spec_angle[4], spec_NL[4], wrap_NL[4];
#  define AS_VEC4(a) vec4(a[0], a[1], a[2], a[3])
#else
    vec4 spec_angle, spec_NL, wrap_NL;
#  define AS_VEC4(a) a
#endif
    prep_specular(
        world_data.lights[0].direction.xyz, I, N, R, spec_NL[0], wrap_NL[0], spec_angle[0]);
    prep_specular(
        world_data.lights[1].direction.xyz, I, N, R, spec_NL[1], wrap_NL[1], spec_angle[1]);
    prep_specular(
        world_data.lights[2].direction.xyz, I, N, R, spec_NL[2], wrap_NL[2], spec_angle[2]);
    prep_specular(
        world_data.lights[3].direction.xyz, I, N, R, spec_NL[3], wrap_NL[3], spec_angle[3]);

    vec4 gloss = vec4(1.0 - roughness);
    /* Reduce gloss for smooth light. (simulate bigger light) */
    gloss *= 1.0 - wrap;
    vec4 shininess = exp2(10.0 * gloss + 1.0);

    vec4 spec_light = blinn_specular(shininess, AS_VEC4(spec_angle), AS_VEC4(spec_NL));

    /* Simulate Env. light. */
    vec4 w = mix(wrap, vec4(1.0), roughness);
    vec4 spec_env = wrapped_lighting(AS_VEC4(wrap_NL), w);
#undef AS_VEC4

    spec_light = mix(spec_light, spec_env, wrap * wrap);

    /* Multiply result by lights specular colors. */
    specular_light += spec_light.x * world_data.lights[0].specular_color.rgb;
    specular_light += spec_light.y * world_data.lights[1].specular_color.rgb;
    specular_light += spec_light.z * world_data.lights[2].specular_color.rgb;
    specular_light += spec_light.w * world_data.lights[3].specular_color.rgb;

    float NV = clamp(dot(N, I), 0.0, 1.0);
    specular_color = brdf_approx(specular_color, roughness, NV);
  }
  specular_light *= specular_color;

  /* Prepare diffuse computation. Eval 4 lights at once. */
  vec4 diff_NL;
  diff_NL.x = dot(world_data.lights[0].direction.xyz, N);
  diff_NL.y = dot(world_data.lights[1].direction.xyz, N);
  diff_NL.z = dot(world_data.lights[2].direction.xyz, N);
  diff_NL.w = dot(world_data.lights[3].direction.xyz, N);

  vec4 diff_light = wrapped_lighting(diff_NL, wrap);

  /* Multiply result by lights diffuse colors. */
  diffuse_light += diff_light.x * world_data.lights[0].diffuse_color_wrap.rgb;
  diffuse_light += diff_light.y * world_data.lights[1].diffuse_color_wrap.rgb;
  diffuse_light += diff_light.z * world_data.lights[2].diffuse_color_wrap.rgb;
  diffuse_light += diff_light.w * world_data.lights[3].diffuse_color_wrap.rgb;

  /* Energy conservation with colored specular look strange.
   * Limit this strangeness by using mono-chromatic specular intensity. */
  float spec_energy = dot(specular_color, vec3(0.33333));

  diffuse_light *= diffuse_color * (1.0 - spec_energy);

  return diffuse_light + specular_light;
}

float get_shadow(vec3 N, bool force_shadowing)
{
  float light_factor = -dot(N, world_data.shadow_direction_vs.xyz);
  float shadow_mix = smoothstep(world_data.shadow_shift, world_data.shadow_focus, light_factor);
  shadow_mix *= force_shadowing ? 0.0 : world_data.shadow_mul;
  return shadow_mix + world_data.shadow_add;
}
