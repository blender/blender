/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/workbench_volume_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(workbench_volume)
FRAGMENT_SHADER_CREATE_INFO(workbench_volume_slice)
FRAGMENT_SHADER_CREATE_INFO(workbench_volume_coba)
FRAGMENT_SHADER_CREATE_INFO(workbench_volume_cubic)
FRAGMENT_SHADER_CREATE_INFO(workbench_volume_smoke)

#include "draw_model_lib.glsl"
#include "draw_object_infos_lib.glsl"
#include "draw_view_lib.glsl"
#include "gpu_shader_math_constants_lib.glsl"
#include "gpu_shader_math_vector_compare_lib.glsl"
#include "gpu_shader_math_vector_reduce_lib.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"
#include "workbench_common_lib.glsl"

float phase_function_isotropic()
{
  return 1.0f / (4.0f * M_PI);
}

float line_unit_box_intersect_dist(float3 lineorigin, float3 linedirection)
{
  /* https://seblagarde.wordpress.com/2012/09/29/image-based-lighting-approaches-and-parallax-corrected-cubemap/
   */
  float3 firstplane = (float3(1.0f) - lineorigin) * safe_rcp(linedirection);
  float3 secondplane = (float3(-1.0f) - lineorigin) * safe_rcp(linedirection);
  float3 furthestplane = min(firstplane, secondplane);
  return reduce_max(furthestplane);
}

#define sample_trilinear(ima, co) texture(ima, co)

float4 sample_tricubic(sampler3D ima, float3 co)
{
  float3 tex_size = float3(textureSize(ima, 0).xyz);

  co *= tex_size;
  /* texel center */
  float3 tc = floor(co - 0.5f) + 0.5f;
  float3 f = co - tc;
  float3 f2 = f * f;
  float3 f3 = f2 * f;
  /* Bspline coefficients (optimized). */
  float3 w3 = f3 / 6.0f;
  float3 w0 = -w3 + f2 * 0.5f - f * 0.5f + 1.0f / 6.0f;
  float3 w1 = f3 * 0.5f - f2 + 2.0f / 3.0f;
  float3 w2 = 1.0f - w0 - w1 - w3;

  float3 s0 = w0 + w1;
  float3 s1 = w2 + w3;

  float3 f0 = w1 / (w0 + w1);
  float3 f1 = w3 / (w2 + w3);

  float2 final_z;
  float4 final_co;
  final_co.xy = tc.xy - 1.0f + f0.xy;
  final_co.zw = tc.xy + 1.0f + f1.xy;
  final_z = tc.zz + float2(-1.0f, 1.0f) + float2(f0.z, f1.z);

  final_co /= tex_size.xyxy;
  final_z /= tex_size.zz;

  float4 color;
  color = texture(ima, float3(final_co.xy, final_z.x)) * s0.x * s0.y * s0.z;
  color += texture(ima, float3(final_co.zy, final_z.x)) * s1.x * s0.y * s0.z;
  color += texture(ima, float3(final_co.xw, final_z.x)) * s0.x * s1.y * s0.z;
  color += texture(ima, float3(final_co.zw, final_z.x)) * s1.x * s1.y * s0.z;

  color += texture(ima, float3(final_co.xy, final_z.y)) * s0.x * s0.y * s1.z;
  color += texture(ima, float3(final_co.zy, final_z.y)) * s1.x * s0.y * s1.z;
  color += texture(ima, float3(final_co.xw, final_z.y)) * s0.x * s1.y * s1.z;
  color += texture(ima, float3(final_co.zw, final_z.y)) * s1.x * s1.y * s1.z;

  return color;
}

/* Nearest-neighbor interpolation */
float4 sample_closest(sampler3D ima, float3 co)
{
  /* Unnormalize coordinates */
  int3 cell_co = int3(co * float3(textureSize(ima, 0).xyz));

  return texelFetch(ima, cell_co, 0);
}

float4 flag_to_color(uint flag)
{
  /* Color mapping for flags */
  float4 color = float4(0.0f, 0.0f, 0.0f, 0.06f);
  /* Cell types: 1 is Fluid, 2 is Obstacle, 4 is Empty, 8 is Inflow, 16 is Outflow */
  if (bool(flag & uint(1))) {
    color.rgb += float3(0.0f, 0.0f, 0.75f); /* blue */
  }
  if (bool(flag & uint(2))) {
    color.rgb += float3(0.2f, 0.2f, 0.2f); /* dark gray */
  }
  if (bool(flag & uint(4))) {
    color.rgb += float3(0.25f, 0.0f, 0.2f); /* dark purple */
  }
  if (bool(flag & uint(8))) {
    color.rgb += float3(0.0f, 0.5f, 0.0f); /* dark green */
  }
  if (bool(flag & uint(16))) {
    color.rgb += float3(0.9f, 0.3f, 0.0f); /* orange */
  }
  if (is_zero(color.rgb)) {
    color.rgb += float3(0.5f, 0.0f, 0.0f); /* medium red */
  }
  return color;
}

#ifdef USE_TRICUBIC
#  define sample_volume_texture sample_tricubic
#elif defined(USE_TRILINEAR)
#  define sample_volume_texture sample_trilinear
#elif defined(USE_CLOSEST)
#  define sample_volume_texture sample_closest
#endif

void volume_properties(float3 ls_pos, out float3 scattering, out float extinction)
{
  float3 co = ls_pos * 0.5f + 0.5f;
#ifdef USE_COBA
  float4 tval;
  if (show_phi) {
    /* Color mapping for level-set representation */
    float val = sample_volume_texture(density_tx, co).r * grid_scale;

    val = max(min(val * 0.2f, 1.0f), -1.0f);

    if (val >= 0.0f) {
      tval = float4(val, 0.0f, 0.5f, 0.06f);
    }
    else {
      tval = float4(0.5f, 1.0f + val, 0.0f, 0.06f);
    }
  }
  else if (show_flags) {
    /* Color mapping for flags */
    uint flag = texture(flag_tx, co).r;
    tval = flag_to_color(flag);
  }
  else if (show_pressure) {
    /* Color mapping for pressure */
    float val = sample_volume_texture(density_tx, co).r * grid_scale;

    if (val > 0) {
      tval = float4(val, val, val, 0.06f);
    }
    else {
      tval = float4(-val, 0.0f, 0.0f, 0.06f);
    }
  }
  else {
    float val = sample_volume_texture(density_tx, co).r * grid_scale;
    tval = texture(transfer_tx, val);
  }
  tval *= density_fac;
  tval.rgb = pow(tval.rgb, float3(2.2f));
  scattering = tval.rgb * 1500.0f;
  extinction = max(1e-4f, tval.a * 50.0f);
#else
#  ifdef VOLUME_SMOKE
  float flame = sample_volume_texture(flame_tx, co).r;
  float4 emission = texture(flame_color_tx, flame);
#  endif
  float3 density = sample_volume_texture(density_tx, co).rgb;
  float shadows = sample_volume_texture(shadow_tx, co).r;

  scattering = density * density_fac;
  extinction = max(1e-4f, dot(scattering, float3(0.33333f)));
  scattering *= active_color;

  /* Scale shadows in log space and clamp them to avoid completely black shadows. */
  scattering *= exp(clamp(log(shadows) * density_fac * 0.1f, -2.5f, 0.0f)) * M_PI;

#  ifdef VOLUME_SMOKE
  /* 800 is arbitrary and here to mimic old viewport. TODO: make it a parameter. */
  scattering += emission.rgb * emission.a * 800.0f;
#  endif
#endif
}

void eval_volume_step(inout float3 Lscat, float extinction, float step_len, out float Tr)
{
  Lscat *= phase_function_isotropic();
  /* Evaluate Scattering */
  Tr = exp(-extinction * step_len);
  /* integrate along the current step segment */
  Lscat = (Lscat - Lscat * Tr) / extinction;
}

#define P(x) ((x + 0.5f) * (1.0f / 16.0f))

float4 volume_integration(
    float3 ray_ori, float3 ray_dir, float ray_inc, float ray_max, float step_len)
{
  /* NOTE: Constant array declared inside function scope to reduce shader core thread memory
   * pressure on Apple Silicon. */
  constexpr float4 dither_mat[4] = float4_array(float4(P(0.0f), P(8.0f), P(2.0f), P(10.0f)),
                                                float4(P(12.0f), P(4.0f), P(14.0f), P(6.0f)),
                                                float4(P(3.0f), P(11.0f), P(1.0f), P(9.0f)),
                                                float4(P(15.0f), P(7.0f), P(13.0f), P(5.0f)));
  /* Start with full transmittance and no scattered light. */
  float3 final_scattering = float3(0.0f);
  float final_transmittance = 1.0f;

  int2 tx = int2(gl_FragCoord.xy) % 4;
  float noise = fract(dither_mat[tx.x][tx.y] + noise_ofs);

  float ray_len = noise * ray_inc;
  for (int i = 0; i < samples_len && ray_len < ray_max; i++, ray_len += ray_inc) {
    float3 ls_pos = ray_ori + ray_dir * ray_len;

    float3 Lscat;
    float s_extinction, Tr;
    volume_properties(ls_pos, Lscat, s_extinction);
    eval_volume_step(Lscat, s_extinction, step_len, Tr);
    /* accumulate and also take into account the transmittance from previous steps */
    final_scattering += final_transmittance * Lscat;
    final_transmittance *= Tr;

    if (final_transmittance <= 0.01f) {
      /* Early out */
      final_transmittance = 0.0f;
      break;
    }
  }

  return float4(final_scattering, final_transmittance);
}

void main()
{
  uint stencil = texelFetch(stencil_tx, int2(gl_FragCoord.xy), 0).r;
  constexpr uint in_front_stencil_bits = 1u << 1;
  if (do_depth_test && (stencil & in_front_stencil_bits) != 0) {
    /* Don't draw on top of "in front" objects. */
    gpu_discard_fragment();
    return;
  }

#ifdef VOLUME_SLICE
  /* Manual depth test. TODO: remove. */
  float depth = texelFetch(depth_buffer, int2(gl_FragCoord.xy), 0).r;
  if (do_depth_test && gl_FragCoord.z >= depth) {
    /* NOTE: In the Metal API, prior to Metal 2.3, Discard is not an explicit return and can
     * produce undefined behavior. This is especially prominent with derivatives if control-flow
     * divergence is present.
     *
     * Adding a return call eliminates undefined behavior and a later out-of-bounds read causing
     * a crash on AMD platforms.
     * This behavior can also affect OpenGL on certain devices. */
    gpu_discard_fragment();
    return;
  }

  float3 Lscat;
  float s_extinction, Tr;
  volume_properties(local_position, Lscat, s_extinction);
  eval_volume_step(Lscat, s_extinction, step_length, Tr);

  frag_color = float4(Lscat, Tr);
#else
  float2 screen_uv = gl_FragCoord.xy / float2(textureSize(depth_buffer, 0).xy);
  bool is_persp = drw_view().winmat[3][3] == 0.0f;

  float3 volume_center = drw_modelmat()[3].xyz;

  float depth = do_depth_test ? texelFetch(depth_buffer, int2(gl_FragCoord.xy), 0).r : 1.0f;
  float depth_end = min(depth, gl_FragCoord.z);
  float3 vs_ray_end = drw_point_screen_to_view(float3(screen_uv, depth_end));
  float3 vs_ray_ori = drw_point_screen_to_view(float3(screen_uv, 0.0f));
  float3 vs_ray_dir = (is_persp) ? (vs_ray_end - vs_ray_ori) : float3(0.0f, 0.0f, -1.0f);
  vs_ray_dir /= abs(vs_ray_dir.z);

  float3 ls_ray_dir = drw_point_view_to_object(vs_ray_ori + vs_ray_dir);
  float3 ls_ray_ori = drw_point_view_to_object(vs_ray_ori);
  float3 ls_ray_end = drw_point_view_to_object(vs_ray_end);

#  ifdef VOLUME_SMOKE
  ls_ray_dir = (drw_object_orco(ls_ray_dir)) * 2.0f - 1.0f;
  ls_ray_ori = (drw_object_orco(ls_ray_ori)) * 2.0f - 1.0f;
  ls_ray_end = (drw_object_orco(ls_ray_end)) * 2.0f - 1.0f;
#  else
  ls_ray_dir = (volume_object_to_texture * float4(ls_ray_dir, 1.0f)).xyz * 2.0f - 1.0f;
  ls_ray_ori = (volume_object_to_texture * float4(ls_ray_ori, 1.0f)).xyz * 2.0f - 1.0f;
  ls_ray_end = (volume_object_to_texture * float4(ls_ray_end, 1.0f)).xyz * 2.0f - 1.0f;
#  endif

  ls_ray_dir -= ls_ray_ori;

  /* TODO: Align rays to volume center so that it mimics old behavior of slicing the volume. */

  float dist = line_unit_box_intersect_dist(ls_ray_ori, ls_ray_dir);
  if (dist > 0.0f) {
    ls_ray_ori = ls_ray_dir * dist + ls_ray_ori;
  }

  float3 ls_vol_isect = ls_ray_end - ls_ray_ori;
  if (dot(ls_ray_dir, ls_vol_isect) < 0.0f) {
    /* Start is further away than the end.
     * That means no volume is intersected. */
    gpu_discard_fragment();
    return;
  }

  frag_color = volume_integration(ls_ray_ori,
                                  ls_ray_dir,
                                  step_length,
                                  length(ls_vol_isect) / length(ls_ray_dir),
                                  length(vs_ray_dir) * step_length);
#endif

  /* Convert transmittance to alpha so we can use pre-multiply blending. */
  frag_color.a = 1.0f - frag_color.a;
}
