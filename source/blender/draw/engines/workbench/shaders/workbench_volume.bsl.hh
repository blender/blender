/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once
#pragma create_info

#include "gpu_shader_compat.hh"

#include "draw_object_infos_infos.hh"

SHADER_LIBRARY_CREATE_INFO(draw_mesh)

#include "draw_model_lib.glsl"
#include "draw_object_infos_lib.glsl"
#include "draw_view_lib.glsl"
#include "gpu_shader_math_constants_lib.glsl"
#include "gpu_shader_math_vector_compare_lib.glsl"
#include "gpu_shader_math_vector_reduce_lib.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"

namespace workbench::volume {

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

/* Legacy Fluid Simulation Modifier. */
struct Smoke {
  [[legacy_info]] const ShaderCreateInfo draw_modelmat;

  [[sampler(2)]] const sampler3D flame_tx;
  [[sampler(3)]] const sampler1D flame_color_tx;
};

/* Volume Objects. */
struct Volume {
  [[legacy_info]] const ShaderCreateInfo draw_volume;

  [[push_constant]] const float4x4 volume_texture_to_object;
  [[push_constant]] const float4x4 volume_object_to_texture;
};

struct ColorBand {
  [[sampler(4)]] const usampler3D flag_tx;
  [[sampler(5)]] const sampler1D transfer_tx;

  [[push_constant]] const bool show_phi;
  [[push_constant]] const bool show_flags;
  [[push_constant]] const bool show_pressure;
  [[push_constant]] const float grid_scale;
};

struct ColorUniform {
  [[sampler(4)]] const sampler3D shadow_tx;

  [[push_constant]] const float3 active_color;
  [[push_constant]] const bool show_flags;
  [[push_constant]] const bool show_pressure;
  [[push_constant]] const float grid_scale;
};

struct Resources {
  [[legacy_info]] const ShaderCreateInfo draw_view;
  [[legacy_info]] const ShaderCreateInfo draw_object_infos;
  [[legacy_info]] const ShaderCreateInfo draw_resource_id_varying;

  [[compilation_constant]] const bool use_slice;
  [[compilation_constant]] const bool use_color_band;
  [[compilation_constant]] const bool is_legacy_smoke;
  [[compilation_constant]] const int interpolation;

  [[sampler(0)]] const sampler2DDepth depth_buffer;
  [[sampler(1)]] const sampler3D density_tx;
  [[sampler(7)]] const usampler2D stencil_tx;

  [[push_constant]] const int samples_len;
  [[push_constant]] const float noise_ofs;
  [[push_constant]] const float step_length;
  [[push_constant]] const float density_fac;
  [[push_constant]] const bool do_depth_test;

  [[push_constant, condition(use_slice)]] const int slice_axis; /* -1 is no slice. */
  [[push_constant, condition(use_slice)]] const float slice_position;

  [[resource_table, condition(!is_legacy_smoke)]] srt_t<Volume> volume;
  [[resource_table, condition(is_legacy_smoke)]] srt_t<Smoke> smoke;

  [[resource_table, condition(use_color_band)]] srt_t<ColorBand> color_band;
  [[resource_table, condition(!use_color_band)]] srt_t<ColorUniform> color_uniform;

  float4 sample_volume_texture(sampler3D ima, float3 co)
  {
    if (this->interpolation == 0) [[static_branch]] {
      return sample_closest(ima, co);
    }
    if (this->interpolation == 2) [[static_branch]] {
      return sample_tricubic(ima, co);
    }
    /* Use hardware interpolation*/
    return texture(ima, co);
  }
};

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

void volume_properties([[resource_table]] Resources &srt,
                       float3 ls_pos,
                       float3 &scattering,
                       float &extinction)
{
  float3 co = ls_pos * 0.5f + 0.5f;

  if (srt.use_color_band) [[static_branch]] {
    [[resource_table]] ColorBand &color_band = srt.color_band;
    float4 tval;
    if (color_band.show_phi) {
      /* Color mapping for level-set representation */
      float val = srt.sample_volume_texture(srt.density_tx, co).r * color_band.grid_scale;

      val = max(min(val * 0.2f, 1.0f), -1.0f);

      if (val >= 0.0f) {
        tval = float4(val, 0.0f, 0.5f, 0.06f);
      }
      else {
        tval = float4(0.5f, 1.0f + val, 0.0f, 0.06f);
      }
    }
    else if (color_band.show_flags) {
      /* Color mapping for flags */
      uint flag = texture(color_band.flag_tx, co).r;
      tval = flag_to_color(flag);
    }
    else if (color_band.show_pressure) {
      /* Color mapping for pressure */
      float val = srt.sample_volume_texture(srt.density_tx, co).r * color_band.grid_scale;

      if (val > 0) {
        tval = float4(val, val, val, 0.06f);
      }
      else {
        tval = float4(-val, 0.0f, 0.0f, 0.06f);
      }
    }
    else {
      float val = srt.sample_volume_texture(srt.density_tx, co).r * color_band.grid_scale;
      tval = texture(color_band.transfer_tx, val);
    }
    tval *= srt.density_fac;
    tval.rgb = pow(tval.rgb, float3(2.2f));
    scattering = tval.rgb * 1500.0f;
    extinction = max(1e-4f, tval.a * 50.0f);
  }
  else {
    [[resource_table]] ColorUniform &uniform = srt.color_uniform;
    float3 density = srt.sample_volume_texture(srt.density_tx, co).rgb;
    float shadows = srt.sample_volume_texture(uniform.shadow_tx, co).r;

    scattering = density * srt.density_fac;
    extinction = max(1e-4f, dot(scattering, float3(0.33333f)));
    scattering *= uniform.active_color;

    /* Scale shadows in log space and clamp them to avoid completely black shadows. */
    scattering *= exp(clamp(log(shadows) * srt.density_fac * 0.1f, -2.5f, 0.0f)) * M_PI;

    if (srt.is_legacy_smoke) [[static_branch]] {
      [[resource_table]] Smoke &smoke = srt.smoke;
      float flame = srt.sample_volume_texture(smoke.flame_tx, co).r;
      float4 emission = texture(smoke.flame_color_tx, flame);
      /* 800 is arbitrary and here to mimic old viewport. TODO: make it a parameter. */
      scattering += emission.rgb * emission.a * 800.0f;
    }
  }
}

void eval_volume_step(float3 &Lscat, float extinction, float step_len, float &Tr)
{
  Lscat *= phase_function_isotropic();
  /* Evaluate Scattering */
  Tr = exp(-extinction * step_len);
  /* integrate along the current step segment */
  Lscat = (Lscat - Lscat * Tr) / extinction;
}

#define P(x) ((x + 0.5f) * (1.0f / 16.0f))

float4 volume_integration([[resource_table]] Resources &srt,
                          float4 frag_coord,
                          float3 ray_ori,
                          float3 ray_dir,
                          float ray_inc,
                          float ray_max,
                          float step_len)
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

  int2 tx = int2(frag_coord.xy) % 4;
  float noise = fract(dither_mat[tx.x][tx.y] + srt.noise_ofs);

  float ray_len = noise * ray_inc;
  for (int i = 0; i < srt.samples_len && ray_len < ray_max; i++, ray_len += ray_inc) {
    float3 ls_pos = ray_ori + ray_dir * ray_len;

    float3 Lscat;
    float s_extinction, Tr;
    volume_properties(srt, ls_pos, Lscat, s_extinction);
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

struct VertIn {
  [[attribute(0)]] float3 pos;
};

struct VertOut {
  [[smooth]] float3 local_pos;
};

[[vertex]] void vertex_function([[resource_table]] Resources &srt,
                                [[in]] const VertIn &v_in,
                                [[out, condition(use_slice)]] VertOut &v_out,
                                [[position]] float4 &out_position)
{
  drw_ResourceID_iface.resource_index = drw_resource_id_raw();

  float3 final_pos;

  if (srt.use_slice) [[static_branch]] {
    if (srt.slice_axis == 0) {
      v_out.local_pos = float3(srt.slice_position * 2.0f - 1.0f, v_in.pos.xy);
    }
    else if (srt.slice_axis == 1) {
      v_out.local_pos = float3(v_in.pos.x, srt.slice_position * 2.0f - 1.0f, v_in.pos.y);
    }
    else {
      v_out.local_pos = float3(v_in.pos.xy, srt.slice_position * 2.0f - 1.0f);
    }
    final_pos = v_out.local_pos;
  }
  else {
    final_pos = v_in.pos;
  }

  if (srt.is_legacy_smoke) [[static_branch]] {
    ObjectInfos info = drw_object_infos();
    final_pos = ((final_pos * 0.5f + 0.5f) - info.orco_add) / info.orco_mul;
  }
  else {
    [[resource_table]] Volume &volume = srt.volume;
    final_pos = (volume.volume_texture_to_object * float4(final_pos * 0.5f + 0.5f, 1.0f)).xyz;
  }
  out_position = drw_point_world_to_homogenous(drw_point_object_to_world(final_pos));
}

struct FragOut {
  [[frag_color(0)]] float4 color;
};

[[fragment]] void fragment_function([[resource_table]] Resources &srt,
                                    [[frag_coord]] const float4 &frag_coord,
                                    [[in, condition(use_slice)]] const VertOut &v_out,
                                    [[out]] FragOut &frag_out)
{
  uint stencil = texelFetch(srt.stencil_tx, int2(frag_coord.xy), 0).r;

  constexpr uint in_front_stencil_bits = 1u << 1;
  if (srt.do_depth_test && (stencil & in_front_stencil_bits) != 0) {
    /* Don't draw on top of "in front" objects. */
    gpu_discard_fragment();
    return;
  }

  if (srt.use_slice) [[static_branch]] {
    /* Manual depth test. TODO: remove. */
    float depth = texelFetch(srt.depth_buffer, int2(frag_coord.xy), 0).r;
    if (srt.do_depth_test && frag_coord.z >= depth) {
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
    volume_properties(srt, v_out.local_pos, Lscat, s_extinction);
    eval_volume_step(Lscat, s_extinction, srt.step_length, Tr);

    frag_out.color = float4(Lscat, Tr);
  }
  else {
    float2 screen_uv = frag_coord.xy / float2(textureSize(srt.depth_buffer, 0).xy);
    bool is_persp = drw_view().winmat[3][3] == 0.0f;

    float depth = srt.do_depth_test ? texelFetch(srt.depth_buffer, int2(frag_coord.xy), 0).r :
                                      1.0f;
    float depth_end = min(depth, frag_coord.z);
    float3 vs_ray_end = drw_point_screen_to_view(float3(screen_uv, depth_end));
    float3 vs_ray_ori = drw_point_screen_to_view(float3(screen_uv, 0.0f));
    float3 vs_ray_dir = (is_persp) ? (vs_ray_end - vs_ray_ori) : float3(0.0f, 0.0f, -1.0f);
    vs_ray_dir /= abs(vs_ray_dir.z);

    float3 ls_ray_dir = drw_point_view_to_object(vs_ray_ori + vs_ray_dir);
    float3 ls_ray_ori = drw_point_view_to_object(vs_ray_ori);
    float3 ls_ray_end = drw_point_view_to_object(vs_ray_end);

    if (srt.is_legacy_smoke) [[static_branch]] {
      ls_ray_dir = (drw_object_orco(ls_ray_dir)) * 2.0f - 1.0f;
      ls_ray_ori = (drw_object_orco(ls_ray_ori)) * 2.0f - 1.0f;
      ls_ray_end = (drw_object_orco(ls_ray_end)) * 2.0f - 1.0f;
    }
    else {
      [[resource_table]] Volume &volume = srt.volume;
      ls_ray_dir = (volume.volume_object_to_texture * float4(ls_ray_dir, 1.0f)).xyz * 2.0f - 1.0f;
      ls_ray_ori = (volume.volume_object_to_texture * float4(ls_ray_ori, 1.0f)).xyz * 2.0f - 1.0f;
      ls_ray_end = (volume.volume_object_to_texture * float4(ls_ray_end, 1.0f)).xyz * 2.0f - 1.0f;
    }

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

    frag_out.color = volume_integration(srt,
                                        frag_coord,
                                        ls_ray_ori,
                                        ls_ray_dir,
                                        srt.step_length,
                                        length(ls_vol_isect) / length(ls_ray_dir),
                                        length(vs_ray_dir) * srt.step_length);
  }

  /* Convert transmittance to alpha so we can use pre-multiply blending. */
  frag_out.color.a = 1.0f - frag_out.color.a;
}

/* clang-format off */
PipelineGraphic smoke_closest_coba_slice(       vertex_function, fragment_function, Resources{.use_slice = true,  .use_color_band = true,  .is_legacy_smoke = true,   .interpolation = 0});
PipelineGraphic smoke_closest_coba_no_slice(    vertex_function, fragment_function, Resources{.use_slice = false, .use_color_band = true,  .is_legacy_smoke = true,   .interpolation = 0});
PipelineGraphic smoke_linear_coba_slice(        vertex_function, fragment_function, Resources{.use_slice = true,  .use_color_band = true,  .is_legacy_smoke = true,   .interpolation = 1});
PipelineGraphic smoke_linear_coba_no_slice(     vertex_function, fragment_function, Resources{.use_slice = false, .use_color_band = true,  .is_legacy_smoke = true,   .interpolation = 1});
PipelineGraphic smoke_cubic_coba_slice(         vertex_function, fragment_function, Resources{.use_slice = true,  .use_color_band = true,  .is_legacy_smoke = true,   .interpolation = 2});
PipelineGraphic smoke_cubic_coba_no_slice(      vertex_function, fragment_function, Resources{.use_slice = false, .use_color_band = true,  .is_legacy_smoke = true,   .interpolation = 2});
PipelineGraphic smoke_closest_no_coba_slice(    vertex_function, fragment_function, Resources{.use_slice = true,  .use_color_band = false, .is_legacy_smoke = true,   .interpolation = 0});
PipelineGraphic smoke_closest_no_coba_no_slice( vertex_function, fragment_function, Resources{.use_slice = false, .use_color_band = false, .is_legacy_smoke = true,   .interpolation = 0});
PipelineGraphic smoke_linear_no_coba_slice(     vertex_function, fragment_function, Resources{.use_slice = true,  .use_color_band = false, .is_legacy_smoke = true,   .interpolation = 1});
PipelineGraphic smoke_linear_no_coba_no_slice(  vertex_function, fragment_function, Resources{.use_slice = false, .use_color_band = false, .is_legacy_smoke = true,   .interpolation = 1});
PipelineGraphic smoke_cubic_no_coba_slice(      vertex_function, fragment_function, Resources{.use_slice = true,  .use_color_band = false, .is_legacy_smoke = true,   .interpolation = 2});
PipelineGraphic smoke_cubic_no_coba_no_slice(   vertex_function, fragment_function, Resources{.use_slice = false, .use_color_band = false, .is_legacy_smoke = true,   .interpolation = 2});
PipelineGraphic object_closest_coba_slice(      vertex_function, fragment_function, Resources{.use_slice = true,  .use_color_band = true,  .is_legacy_smoke = false,  .interpolation = 0});
PipelineGraphic object_closest_coba_no_slice(   vertex_function, fragment_function, Resources{.use_slice = false, .use_color_band = true,  .is_legacy_smoke = false,  .interpolation = 0});
PipelineGraphic object_linear_coba_slice(       vertex_function, fragment_function, Resources{.use_slice = true,  .use_color_band = true,  .is_legacy_smoke = false,  .interpolation = 1});
PipelineGraphic object_linear_coba_no_slice(    vertex_function, fragment_function, Resources{.use_slice = false, .use_color_band = true,  .is_legacy_smoke = false,  .interpolation = 1});
PipelineGraphic object_cubic_coba_slice(        vertex_function, fragment_function, Resources{.use_slice = true,  .use_color_band = true,  .is_legacy_smoke = false,  .interpolation = 2});
PipelineGraphic object_cubic_coba_no_slice(     vertex_function, fragment_function, Resources{.use_slice = false, .use_color_band = true,  .is_legacy_smoke = false,  .interpolation = 2});
PipelineGraphic object_closest_no_coba_slice(   vertex_function, fragment_function, Resources{.use_slice = true,  .use_color_band = false, .is_legacy_smoke = false,  .interpolation = 0});
PipelineGraphic object_closest_no_coba_no_slice(vertex_function, fragment_function, Resources{.use_slice = false, .use_color_band = false, .is_legacy_smoke = false,  .interpolation = 0});
PipelineGraphic object_linear_no_coba_slice(    vertex_function, fragment_function, Resources{.use_slice = true,  .use_color_band = false, .is_legacy_smoke = false,  .interpolation = 1});
PipelineGraphic object_linear_no_coba_no_slice( vertex_function, fragment_function, Resources{.use_slice = false, .use_color_band = false, .is_legacy_smoke = false,  .interpolation = 1});
PipelineGraphic object_cubic_no_coba_slice(     vertex_function, fragment_function, Resources{.use_slice = true,  .use_color_band = false, .is_legacy_smoke = false,  .interpolation = 2});
PipelineGraphic object_cubic_no_coba_no_slice(  vertex_function, fragment_function, Resources{.use_slice = false, .use_color_band = false, .is_legacy_smoke = false,  .interpolation = 2});
/* clang-format on */

}  // namespace workbench::volume
