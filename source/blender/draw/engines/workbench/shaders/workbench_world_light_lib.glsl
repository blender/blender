/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/workbench_prepass_infos.hh"

#ifdef GPU_LIBRARY_SHADER
SHADER_LIBRARY_CREATE_INFO(workbench_prepass)
#endif

/* [Drobot2014a] Low Level Optimizations for GCN */
float4 fast_rcp(float4 v)
{
  return intBitsToFloat(0x7eef370b - floatBitsToInt(v));
}

float3 brdf_approx(float3 spec_color, float roughness, float NV)
{
  /* Very rough approximation. We don't need it to be correct, just fast.
   * Just simulate fresnel effect with roughness attenuation. */
  float fresnel = exp2(-8.35f * NV) * (1.0f - roughness);
  return mix(spec_color, float3(1.0f), fresnel);
}

void prep_specular(float3 L,
                   float3 I,
                   float3 N,
                   float3 R,
                   out float NL,
                   out float wrapped_NL,
                   out float spec_angle)
{
  wrapped_NL = dot(L, R);
  float3 half_dir = normalize(L + I);
  spec_angle = clamp(dot(half_dir, N), 0.0f, 1.0f);
  NL = clamp(dot(L, N), 0.0f, 1.0f);
}

/* Normalized Blinn shading */
float4 blinn_specular(float4 shininess, float4 spec_angle, float4 NL)
{
  /* Pi is already divided in the light power.
   * normalization_factor = (shininess + 8.0f) / (8.0f * M_PI) */
  float4 normalization_factor = shininess * 0.125f + 1.0f;
  float4 spec_light = pow(spec_angle, shininess) * NL * normalization_factor;

  return spec_light;
}

/* NL need to be unclamped. w in [0..1] range. */
float4 wrapped_lighting(float4 NL, float4 w)
{
  float4 w_1 = w + 1.0f;
  float4 denom = fast_rcp(w_1 * w_1);
  return clamp((NL + w) * denom, 0.0f, 1.0f);
}

float3 get_world_lighting(float3 base_color, float roughness, float metallic, float3 N, float3 I)
{
  float3 specular_color, diffuse_color;

  if (world_data.use_specular) {
    diffuse_color = mix(base_color, float3(0.0f), metallic);
    specular_color = mix(float3(0.05f), base_color, metallic);
  }
  else {
    diffuse_color = base_color;
    specular_color = float3(0.0f);
  }

  float3 specular_light = world_data.ambient_color.rgb;
  float3 diffuse_light = world_data.ambient_color.rgb;
  float4 wrap = float4(world_data.lights[0].diffuse_color_wrap.a,
                       world_data.lights[1].diffuse_color_wrap.a,
                       world_data.lights[2].diffuse_color_wrap.a,
                       world_data.lights[3].diffuse_color_wrap.a);

  if (world_data.use_specular) {
    /* Prepare Specular computation. Eval 4 lights at once. */
    float3 R = -reflect(I, N);

#ifdef GPU_METAL
    /* Split vectors into arrays of floats. Partial vector references are unsupported by MSL. */
    float spec_angle[4], spec_NL[4], wrap_NL[4];
#  define AS_VEC4(a) float4(a[0], a[1], a[2], a[3])
#else
    float4 spec_angle, spec_NL, wrap_NL;
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

    float4 gloss = float4(1.0f - roughness);
    /* Reduce gloss for smooth light. (simulate bigger light) */
    gloss *= 1.0f - wrap;
    float4 shininess = exp2(10.0f * gloss + 1.0f);

    float4 spec_light = blinn_specular(shininess, AS_VEC4(spec_angle), AS_VEC4(spec_NL));

    /* Simulate Env. light. */
    float4 w = mix(wrap, float4(1.0f), roughness);
    float4 spec_env = wrapped_lighting(AS_VEC4(wrap_NL), w);
#undef AS_VEC4

    spec_light = mix(spec_light, spec_env, wrap * wrap);

    /* Multiply result by lights specular colors. */
    specular_light += spec_light.x * world_data.lights[0].specular_color.rgb;
    specular_light += spec_light.y * world_data.lights[1].specular_color.rgb;
    specular_light += spec_light.z * world_data.lights[2].specular_color.rgb;
    specular_light += spec_light.w * world_data.lights[3].specular_color.rgb;

    float NV = clamp(dot(N, I), 0.0f, 1.0f);
    specular_color = brdf_approx(specular_color, roughness, NV);
  }
  specular_light *= specular_color;

  /* Prepare diffuse computation. Eval 4 lights at once. */
  float4 diff_NL;
  diff_NL.x = dot(world_data.lights[0].direction.xyz, N);
  diff_NL.y = dot(world_data.lights[1].direction.xyz, N);
  diff_NL.z = dot(world_data.lights[2].direction.xyz, N);
  diff_NL.w = dot(world_data.lights[3].direction.xyz, N);

  float4 diff_light = wrapped_lighting(diff_NL, wrap);

  /* Multiply result by lights diffuse colors. */
  diffuse_light += diff_light.x * world_data.lights[0].diffuse_color_wrap.rgb;
  diffuse_light += diff_light.y * world_data.lights[1].diffuse_color_wrap.rgb;
  diffuse_light += diff_light.z * world_data.lights[2].diffuse_color_wrap.rgb;
  diffuse_light += diff_light.w * world_data.lights[3].diffuse_color_wrap.rgb;

  /* Energy conservation with colored specular look strange.
   * Limit this strangeness by using mono-chromatic specular intensity. */
  float spec_energy = dot(specular_color, float3(0.33333f));

  diffuse_light *= diffuse_color * (1.0f - spec_energy);

  return diffuse_light + specular_light;
}

float get_shadow(float3 N, bool force_shadow)
{
  float light_factor = -dot(N, world_data.shadow_direction_vs.xyz);
  float shadow_mix = smoothstep(world_data.shadow_shift, world_data.shadow_focus, light_factor);
  shadow_mix *= force_shadow ? 0.0f : world_data.shadow_mul;
  return shadow_mix + world_data.shadow_add;
}
