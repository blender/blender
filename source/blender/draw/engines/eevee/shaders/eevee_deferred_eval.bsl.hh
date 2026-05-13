/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Compute light objects lighting contribution using captured Gbuffer data.
 */

#pragma once

#include "infos/eevee_deferred_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(draw_view)
FRAGMENT_SHADER_CREATE_INFO(draw_object_infos)
FRAGMENT_SHADER_CREATE_INFO(eevee_gbuffer_data)
FRAGMENT_SHADER_CREATE_INFO(eevee_utility_texture)
FRAGMENT_SHADER_CREATE_INFO(eevee_sampling_data)
FRAGMENT_SHADER_CREATE_INFO(eevee_light_data)
FRAGMENT_SHADER_CREATE_INFO(eevee_shadow_data)
FRAGMENT_SHADER_CREATE_INFO(eevee_hiz_data)
FRAGMENT_SHADER_CREATE_INFO(eevee_volume_probe_data)

#include "draw_view_lib.glsl"
#include "eevee_closure_lib.glsl"
#include "eevee_gbuffer_read_lib.glsl"
#include "eevee_light_eval_lib.glsl"
#include "eevee_lightprobe_eval_lib.glsl"
#include "eevee_lightprobe_volume_eval_lib.glsl"
#include "eevee_renderpass_lib.glsl"
#include "eevee_subsurface_lib.glsl"
#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_fullscreen_lib.glsl"
#include "gpu_shader_shared_exponent_lib.glsl"

namespace eevee::deferred {

struct VertOut {
  [[smooth]] float2 screen_uv;
};

[[vertex]]
void fullscreen_vert([[vertex_id]] const int vert_id,
                     [[position]] float4 &out_position,
                     [[out]] VertOut &v_out)
{
  fullscreen_vertex(vert_id, out_position, v_out.screen_uv);
}

struct FragOut {
  [[frag_color(0)]] float4 radiance;
};

/* -------------------------------------------------------------------- */
/** \name Deferred Pipeline
 * \{ */
struct LightEval {
  [[legacy_info]] ShaderCreateInfo eevee_gbuffer_data;
  [[legacy_info]] ShaderCreateInfo eevee_utility_texture;
  [[legacy_info]] ShaderCreateInfo eevee_sampling_data;
  [[legacy_info]] ShaderCreateInfo eevee_light_data;
  [[legacy_info]] ShaderCreateInfo eevee_shadow_data;
  [[legacy_info]] ShaderCreateInfo eevee_hiz_data;
  [[legacy_info]] ShaderCreateInfo eevee_lightprobe_data;
  [[legacy_info]] ShaderCreateInfo eevee_render_pass_out;
  [[legacy_info]] ShaderCreateInfo draw_object_infos;
  [[legacy_info]] ShaderCreateInfo draw_view;

  [[specialization_constant(true)]] bool use_split_indirect;
  [[specialization_constant(true)]] bool use_lightprobe_eval;
  [[specialization_constant(false)]] bool use_transmission;
  [[specialization_constant(-1)]] int render_pass_shadow_id;
  [[specialization_constant(1)]] int shadow_ray_count;
  [[specialization_constant(6)]] int shadow_ray_step_count;

  [[compilation_constant]] int light_closure_eval_count;

  /* Chaining to next pass. */
  [[image(2, write, DEFERRED_RADIANCE_FORMAT)]] uimage2D direct_radiance_1_img;
  [[image(3, write, DEFERRED_RADIANCE_FORMAT)]] uimage2D direct_radiance_2_img;
  [[image(4, write, DEFERRED_RADIANCE_FORMAT)]] uimage2D direct_radiance_3_img;
  /* Optimized out if use_split_indirect is false. */
  [[image(5, write, RAYTRACE_RADIANCE_FORMAT)]] image2D indirect_radiance_1_img;
  [[image(6, write, RAYTRACE_RADIANCE_FORMAT)]] image2D indirect_radiance_2_img;
  [[image(7, write, RAYTRACE_RADIANCE_FORMAT)]] image2D indirect_radiance_3_img;

  void write_radiance_direct(uchar layer_index, int2 texel, float3 radiance)
  {
    /* TODO(fclem): Layered texture. */
    uint data = rgb9e5_encode(radiance);
    if (layer_index == 0u) {
      imageStore(direct_radiance_1_img, texel, uint4(data));
    }
    else if (layer_index == 1u) {
      imageStore(direct_radiance_2_img, texel, uint4(data));
    }
    else if (layer_index == 2u) {
      imageStore(direct_radiance_3_img, texel, uint4(data));
    }
  }

  void write_radiance_indirect(uchar layer_index, int2 texel, float3 radiance)
  {
    /* TODO(fclem): Layered texture. */
    if (layer_index == 0u) {
      imageStore(indirect_radiance_1_img, texel, float4(radiance, 1.0f));
    }
    else if (layer_index == 1u) {
      imageStore(indirect_radiance_2_img, texel, float4(radiance, 1.0f));
    }
    else if (layer_index == 2u) {
      imageStore(indirect_radiance_3_img, texel, float4(radiance, 1.0f));
    }
  }
};

/* Deferred direct light evaluation function.
 * Load all BSDFs closures and evaluate LTC for each selected lights. */
[[fragment, early_fragment_tests]]
void light_eval_frag([[resource_table]] LightEval &srt,
                     [[frag_coord]] const float4 frag_co,
                     [[in]] const VertOut v_out)
{
  const int2 texel = int2(frag_co.xy);

  /* Bias the shading point position because of depth buffer precision.
   * Constant is taken from https://www.terathon.com/gdc07_lengyel.pdf. */
  constexpr float bias = 2.4e-7f;

  const float depth = texelFetch(hiz_tx, texel, 0).r - bias;
  const gbuffer::Layers gbuf = gbuffer::read_layers(texel);
  const Thickness thickness = gbuffer::read_thickness(gbuf.header, texel);
  const uchar closure_count = gbuf.header.closure_len();

  const float3 P = drw_point_screen_to_world(float3(v_out.screen_uv, depth));
  const float3 Ng = gbuf.header.geometry_normal(gbuf.surface_N());
  const float3 V = drw_world_incident_vector(P);
  const float vPz = dot(drw_view_forward(), P) - dot(drw_view_forward(), drw_view_position());

  ClosureLightStack stack;
  /* Unroll light stack array assignments to avoid non-constant indexing. */
  closure_light_set(stack, 0, closure_light_new(gbuf.layer[0], V));
  if (srt.light_closure_eval_count > 1) [[static_branch]] {
    closure_light_set(stack, 1, closure_light_new(gbuf.layer[1], V));
  }
  if (srt.light_closure_eval_count > 2) [[static_branch]] {
    closure_light_set(stack, 2, closure_light_new(gbuf.layer[2], V));
  }

  uchar receiver_light_set = 0;
  float normal_offset = 0.0f;
  float geometry_offset = 0.0f;
  if (gbuf.header.use_object_id()) {
    uint object_id = gbuffer::read_object_id(texel);
    ObjectInfos object_infos = drw_infos[object_id];
    receiver_light_set = receiver_light_set_get(object_infos);
    normal_offset = object_infos.shadow_terminator_normal_offset;
    geometry_offset = object_infos.shadow_terminator_geometry_offset;
  }

  /* TODO(fclem): If transmission (no SSS) is present, we could reduce LIGHT_CLOSURE_EVAL_COUNT
   * by 1 for this evaluation and skip evaluating the transmission closure twice. */
  light_eval_reflection(stack, P, Ng, V, vPz, receiver_light_set, normal_offset, geometry_offset);

  if (srt.use_transmission) {
    ClosureUndetermined cl_transmit = gbuf.layer[0];
#if 1 /* TODO Limit to SSS. */
    float3 sss_reflect_shadowed, sss_reflect_unshadowed;
    if (cl_transmit.type == CLOSURE_BSSRDF_BURLEY_ID) {
      sss_reflect_shadowed = stack.cl[0].light_shadowed;
      sss_reflect_unshadowed = stack.cl[0].light_unshadowed;
    }
#endif

    stack.cl[0] = closure_light_new(cl_transmit, V, thickness);

    /* NOTE: Only evaluates `stack.cl[0]`. */
    light_eval_transmission(
        stack, P, Ng, V, vPz, thickness, receiver_light_set, normal_offset, geometry_offset);

#if 1 /* TODO Limit to SSS. */
    if (cl_transmit.type == CLOSURE_BSSRDF_BURLEY_ID) {
      /* Apply transmission profile onto transmitted light and sum with reflected light. */
      float3 sss_profile = subsurface_transmission(to_closure_subsurface(cl_transmit).sss_radius,
                                                   thickness.value());
      stack.cl[0].light_shadowed *= sss_profile;
      stack.cl[0].light_unshadowed *= sss_profile;
      stack.cl[0].light_shadowed += sss_reflect_shadowed;
      stack.cl[0].light_unshadowed += sss_reflect_unshadowed;
    }
#endif
  }

  if (srt.render_pass_shadow_id != -1) {
    float3 radiance_shadowed = float3(0);
    float3 radiance_unshadowed = float3(0);
    for (uchar i = 0; i < LIGHT_CLOSURE_EVAL_COUNT && i < closure_count; i++) {
      radiance_shadowed += closure_light_get(stack, i).light_shadowed;
      radiance_unshadowed += closure_light_get(stack, i).light_unshadowed;
    }
    float3 shadows = radiance_shadowed * safe_rcp(radiance_unshadowed);
    output_renderpass_value(srt.render_pass_shadow_id, average(shadows));
  }

  if (srt.use_lightprobe_eval) {
    LightProbeSample samp = lightprobe_load(gl_FragCoord.xy, P, Ng, V);

    float clamp_indirect = uniform_buf.clamp.surface_indirect;
    samp.volume_irradiance = spherical_harmonics::clamp_energy(samp.volume_irradiance,
                                                               clamp_indirect);

    uint3 bin_indices = gbuf.header.bin_index_per_layer();
    for (uchar i = 0; i < LIGHT_CLOSURE_EVAL_COUNT && i < closure_count; i++) {
      float3 indirect_light = lightprobe_eval(samp, gbuf.layer[i], P, V, thickness);
      float3 direct_light = closure_light_get(stack, i).light_shadowed;
      if (srt.use_split_indirect) {
        srt.write_radiance_indirect(bin_indices[i], texel, indirect_light);
        srt.write_radiance_direct(bin_indices[i], texel, direct_light);
      }
      else {
        srt.write_radiance_direct(bin_indices[i], texel, direct_light + indirect_light);
      }
    }
  }
  else {
    uint3 bin_indices = gbuf.header.bin_index_per_layer();
    for (uchar i = 0; i < LIGHT_CLOSURE_EVAL_COUNT && i < closure_count; i++) {
      float3 direct_light = closure_light_get(stack, i).light_shadowed;
      srt.write_radiance_direct(bin_indices[i], texel, direct_light);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Probe Capture Pipeline
 * \{ */

struct SphereProbeEval {
  [[compilation_constant]] int light_closure_eval_count;

  [[legacy_info]] ShaderCreateInfo draw_view;
  [[legacy_info]] ShaderCreateInfo draw_object_infos;
  [[legacy_info]] ShaderCreateInfo eevee_gbuffer_data;
  [[legacy_info]] ShaderCreateInfo eevee_utility_texture;
  [[legacy_info]] ShaderCreateInfo eevee_sampling_data;
  [[legacy_info]] ShaderCreateInfo eevee_light_data;
  [[legacy_info]] ShaderCreateInfo eevee_shadow_data;
  [[legacy_info]] ShaderCreateInfo eevee_hiz_data;
  [[legacy_info]] ShaderCreateInfo eevee_volume_probe_data;
};

/* Sphere probe evaluate everything as diffuse since they can only rely on volume light-probes
 * being available. */
[[fragment, early_fragment_tests]]
void sphere_eval_frag([[resource_table]] SphereProbeEval & /*srt*/,
                      [[frag_coord]] const float4 frag_co,
                      [[in]] const VertOut v_out,
                      [[out]] FragOut &frag_out)
{
  int2 texel = int2(frag_co.xy);

  float depth = texelFetch(hiz_tx, texel, 0).r;

  const gbuffer::Layers gbuf = gbuffer::read_layers(texel);

  if (gbuf.has_no_closure()) {
    frag_out.radiance = float4(0.0f);
    return;
  }

  const uchar closure_count = gbuf.header.closure_len();
  const Thickness thickness = gbuffer::read_thickness(gbuf.header, texel);

  float3 albedo_front = float3(0.0f);
  float3 albedo_back = float3(0.0f);

  for (uchar i = 0; i < GBUFFER_LAYER_MAX && i < closure_count; i++) {
    ClosureUndetermined cl = gbuf.layer_get(i);
    switch (cl.type) {
      case CLOSURE_BSSRDF_BURLEY_ID:
      case CLOSURE_BSDF_DIFFUSE_ID:
      case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
        albedo_front += cl.color;
        break;
      case CLOSURE_BSDF_TRANSLUCENT_ID:
      case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
        albedo_back += (thickness.value() != 0.0f) ? square(cl.color) : cl.color;
        break;
      case CLOSURE_NONE_ID:
        /* TODO(fclem): Assert. */
        break;
    }
  }

  float3 P = drw_point_screen_to_world(float3(v_out.screen_uv, depth));
  float3 Ng = gbuf.header.geometry_normal(gbuf.surface_N());
  float3 V = drw_world_incident_vector(P);
  float vPz = dot(drw_view_forward(), P) - dot(drw_view_forward(), drw_view_position());

  ClosureUndetermined cl;
  cl.N = gbuf.surface_N();
  cl.type = CLOSURE_BSDF_DIFFUSE_ID;

  ClosureUndetermined cl_transmit;
  cl_transmit.N = gbuf.surface_N();
  cl_transmit.type = CLOSURE_BSDF_TRANSLUCENT_ID;

  uchar receiver_light_set = 0;
  float normal_offset = 0.0f;
  float geometry_offset = 0.0f;
  if (gbuf.header.use_object_id()) {
    uint object_id = gbuffer::read_object_id(texel);
    ObjectInfos object_infos = drw_infos[object_id];
    receiver_light_set = receiver_light_set_get(object_infos);
    normal_offset = object_infos.shadow_terminator_normal_offset;
    geometry_offset = object_infos.shadow_terminator_geometry_offset;
  }

  /* Direct light. */
  ClosureLightStack stack;
  stack.cl[0] = closure_light_new(cl, V);
  light_eval_reflection(stack, P, Ng, V, vPz, receiver_light_set, normal_offset, geometry_offset);

  float3 radiance_front = stack.cl[0].light_shadowed;

  stack.cl[0] = closure_light_new(cl_transmit, V, thickness);
  light_eval_transmission(
      stack, P, Ng, V, vPz, thickness, receiver_light_set, normal_offset, geometry_offset);

  float3 radiance_back = stack.cl[0].light_shadowed;

  /* Indirect light. */
  /* Can only load irradiance to avoid dependency loop with the reflection probe. */
  SphericalHarmonicL1<float4> sh = lightprobe_volume_sample(P, V, Ng);

  radiance_front += sh.evaluate_lambert(Ng).rgb;
  /* TODO(fclem): Correct transmission eval. */
  radiance_back += sh.evaluate_lambert(-Ng).rgb;

  frag_out.radiance = float4(radiance_front * albedo_front + radiance_back * albedo_back, 0.0f);
}

struct PlanarProbeEval {
  /* TODO(fclem): Workaround waiting for this lib to be ported to BSL.  */
  [[compilation_constant]] bool legacy_sphere_probe_enable;
  [[compilation_constant]] int light_closure_eval_count;

  [[legacy_info]] ShaderCreateInfo draw_view;
  [[legacy_info]] ShaderCreateInfo draw_object_infos;
  [[legacy_info]] ShaderCreateInfo eevee_gbuffer_data;
  [[legacy_info]] ShaderCreateInfo eevee_utility_texture;
  [[legacy_info]] ShaderCreateInfo eevee_sampling_data;
  [[legacy_info]] ShaderCreateInfo eevee_light_data;
  [[legacy_info]] ShaderCreateInfo eevee_lightprobe_data;
  [[legacy_info]] ShaderCreateInfo eevee_shadow_data;
  [[legacy_info]] ShaderCreateInfo eevee_hiz_data;
};

[[fragment, early_fragment_tests]]
void planar_eval_frag([[resource_table]] PlanarProbeEval & /*srt*/,
                      [[frag_coord]] const float4 frag_co,
                      [[in]] const VertOut v_out,
                      [[out]] FragOut &frag_out)
{
  int2 texel = int2(frag_co.xy);

  float depth = texelFetch(hiz_tx, texel, 0).r;

  const gbuffer::Layers gbuf = gbuffer::read_layers(texel);
  const uchar closure_count = gbuf.header.closure_len();
  const Thickness thickness = gbuffer::read_thickness(gbuf.header, texel);

  float3 albedo_front = float3(0.0f);
  float3 albedo_back = float3(0.0f);

  ClosureUndetermined cl_reflect;
  cl_reflect.type = CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID;
  cl_reflect.color = float3(0.0);
  cl_reflect.N = float3(0.0);
  cl_reflect.data = float4(0.0);
  float reflect_weight = 0.0;

  ClosureUndetermined cl_refract;
  cl_refract.type = CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;
  cl_refract.color = float3(0.0);
  cl_refract.N = float3(0.0);
  cl_refract.data = float4(0.0);
  float refract_weight = 0.0;

  for (uchar i = 0; i < GBUFFER_LAYER_MAX && i < closure_count; i++) {
    ClosureUndetermined cl = gbuf.layer_get(i);
    switch (cl.type) {
      case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID: {
        cl_reflect.color += cl.color;
        /* Average roughness and normals. */
        float weight = reduce_add(cl.color);
        cl_reflect.N += cl.N * weight;
        cl_reflect.data += cl.data * weight;
        reflect_weight += weight;
        break;
      }
      case CLOSURE_BSSRDF_BURLEY_ID:
      case CLOSURE_BSDF_DIFFUSE_ID:
        albedo_front += cl.color;
        break;
      case CLOSURE_BSDF_TRANSLUCENT_ID:
        albedo_back += (thickness.value() != 0.0f) ? square(cl.color) : cl.color;
        break;
      case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID: {
        cl_refract.color += (thickness.value() != 0.0f) ? square(cl.color) : cl.color;
        /* Average roughness and normals. */
        float weight = reduce_add(cl.color);
        cl_refract.N += cl.N * weight;
        cl_refract.data += cl.data * weight;
        refract_weight += weight;
        break;
      }
      case CLOSURE_NONE_ID:
        /* TODO(fclem): Assert. */
        break;
    }
  }

  {
    float inv_weight = safe_rcp(reflect_weight);
    cl_reflect.N *= inv_weight;
    cl_reflect.data *= inv_weight;
  }
  {
    float inv_weight = safe_rcp(refract_weight);
    cl_refract.N *= inv_weight;
    cl_refract.data *= inv_weight;
  }

  float3 P = drw_point_screen_to_world(float3(v_out.screen_uv, depth));
  float3 Ng = gbuf.header.geometry_normal(gbuf.surface_N());
  float3 V = drw_world_incident_vector(P);
  float vPz = dot(drw_view_forward(), P) - dot(drw_view_forward(), drw_view_position());

  ClosureUndetermined cl;
  cl.N = gbuf.surface_N();
  cl.type = CLOSURE_BSDF_DIFFUSE_ID;

  ClosureUndetermined cl_transmit;
  cl_transmit.N = gbuf.surface_N();
  cl_transmit.type = CLOSURE_BSDF_TRANSLUCENT_ID;

  uchar receiver_light_set = 0;
  float normal_offset = 0.0f;
  float geometry_offset = 0.0f;
  if (gbuf.header.use_object_id()) {
    uint object_id = gbuffer::read_object_id(texel);
    ObjectInfos object_infos = drw_infos[object_id];
    receiver_light_set = receiver_light_set_get(object_infos);
    normal_offset = object_infos.shadow_terminator_normal_offset;
    geometry_offset = object_infos.shadow_terminator_geometry_offset;
  }

  /* Direct light. */
  ClosureLightStack stack;
  stack.cl[0] = closure_light_new(cl, V);
  stack.cl[1] = closure_light_new(cl_reflect, V);
  light_eval_reflection(stack, P, Ng, V, vPz, receiver_light_set, normal_offset, geometry_offset);

  float3 radiance_front = stack.cl[0].light_shadowed;
  float3 radiance_reflect = stack.cl[1].light_shadowed;

  stack.cl[0] = closure_light_new(cl_transmit, V, thickness);
  stack.cl[1] = closure_light_new(cl_refract, V, thickness);
  light_eval_transmission(
      stack, P, Ng, V, vPz, thickness, receiver_light_set, normal_offset, geometry_offset);

  float3 radiance_back = stack.cl[0].light_shadowed;
  float3 radiance_refract = stack.cl[1].light_shadowed;

  /* Indirect light. */
  SphericalHarmonicL1<float4> sh = lightprobe_volume_sample(P, V, Ng);
  LightProbeSample samp = lightprobe_load(gl_FragCoord.xy, P, Ng, V);

  radiance_front += sh.evaluate_lambert(Ng).rgb;
  radiance_back += sh.evaluate_lambert(-Ng).rgb;
  radiance_reflect += lightprobe_eval(samp, cl_reflect, P, V, thickness);
  radiance_refract += lightprobe_eval(samp, cl_refract, P, V, thickness);

  /* Note: planar probes use transmittance and not alpha for transparency. */
  frag_out.radiance = float4(0.0f);
  frag_out.radiance.xyz += radiance_reflect * cl_reflect.color;
  frag_out.radiance.xyz += radiance_refract * cl_refract.color;
  frag_out.radiance.xyz += radiance_front * albedo_front;
  frag_out.radiance.xyz += radiance_back * albedo_back;
}

/** \} */

PipelineGraphic light_single(fullscreen_vert,
                             light_eval_frag,
                             LightEval{
                                 .light_closure_eval_count = 1,
                             });
PipelineGraphic light_double(fullscreen_vert,
                             light_eval_frag,
                             LightEval{
                                 .light_closure_eval_count = 2,
                             });
PipelineGraphic light_triple(fullscreen_vert,
                             light_eval_frag,
                             LightEval{
                                 .light_closure_eval_count = 3,
                             });
PipelineGraphic sphere_eval(fullscreen_vert,
                            sphere_eval_frag,
                            SphereProbeEval{
                                .light_closure_eval_count = 1,
                            });
PipelineGraphic planar_eval(fullscreen_vert,
                            planar_eval_frag,
                            PlanarProbeEval{
                                .legacy_sphere_probe_enable = true,
                                .light_closure_eval_count = 2,
                            });

}  // namespace eevee::deferred
