/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

#include "gpu_shader_utildefines_lib.glsl"

#include "workbench_common.bsl.hh"

namespace workbench {

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

float3 get_world_lighting([[resource_table]] const workbench::World &world,
                          float3 base_color,
                          float roughness,
                          float metallic,
                          float3 N,
                          float3 I)
{
  const WorldData &world_data = world.world_data;

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

    float4 spec_angle, spec_NL, wrapped_NL;
    for (int i = 0; i < 4; i++) [[unroll]] {
      float3 L = world_data.lights[i].direction.xyz;
      float3 half_dir = normalize(L + I);
      wrapped_NL[i] = dot(L, R);
      spec_angle[i] = saturate(dot(half_dir, N));
      spec_NL[i] = saturate(dot(L, N));
    }

    float4 gloss = float4(1.0f - roughness);
    /* Reduce gloss for smooth light. (simulate bigger light) */
    gloss *= 1.0f - wrap;
    float4 shininess = exp2(10.0f * gloss + 1.0f);

    float4 spec_light = blinn_specular(shininess, spec_angle, spec_NL);

    /* Simulate Env. light. */
    float4 w = mix(wrap, float4(1.0f), roughness);
    float4 spec_env = wrapped_lighting(wrapped_NL, w);

    spec_light = mix(spec_light, spec_env, wrap * wrap);

    /* Multiply result by lights specular colors. */
    for (int i = 0; i < 4; i++) [[unroll]] {
      specular_light += spec_light[i] * world_data.lights[i].specular_color.rgb;
    }

    float NV = saturate(dot(N, I));
    specular_color = brdf_approx(specular_color, roughness, NV);
  }
  specular_light *= specular_color;

  /* Prepare diffuse computation. Eval 4 lights at once. */
  float4 diff_NL;
  for (int i = 0; i < 4; i++) [[unroll]] {
    diff_NL[i] = dot(world_data.lights[i].direction.xyz, N);
  }

  float4 diff_light = wrapped_lighting(diff_NL, wrap);

  /* Multiply result by lights diffuse colors. */
  for (int i = 0; i < 4; i++) [[unroll]] {
    diffuse_light += diff_light[i] * world_data.lights[i].diffuse_color_wrap.rgb;
  }

  /* Energy conservation with colored specular look strange.
   * Limit this strangeness by using mono-chromatic specular intensity. */
  float spec_energy = dot(specular_color, float3(0.33333f));

  diffuse_light *= diffuse_color * (1.0f - spec_energy);

  return diffuse_light + specular_light;
}

float get_shadow([[resource_table]] const workbench::World &world, float3 N, bool force_shadow)
{
  const WorldData &world_data = world.world_data;

  float light_factor = -dot(N, world_data.shadow_direction_vs.xyz);
  float shadow_mix = smoothstep(world_data.shadow_shift, world_data.shadow_focus, light_factor);
  shadow_mix *= force_shadow ? 0.0f : world_data.shadow_mul;
  return shadow_mix + world_data.shadow_add;
}

}  // namespace workbench
