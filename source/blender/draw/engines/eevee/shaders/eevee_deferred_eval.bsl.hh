/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Compute light objects lighting contribution using captured Gbuffer data.
 */

#pragma once

#include "draw_object_infos_infos.hh"
#include "draw_view_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(draw_view)
FRAGMENT_SHADER_CREATE_INFO(draw_object_infos)

#include "draw_view_lib.glsl"
#include "eevee_closure.bsl.hh"
#include "eevee_gbuffer_read.bsl.hh"
#include "eevee_hiz.bsl.hh"
#include "eevee_light_eval.bsl.hh"
#include "eevee_lightprobe.bsl.hh"
#include "eevee_renderpass.bsl.hh"
#include "eevee_subsurface_lib.bsl.hh"
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

/**
 * Clear AOVs for secondary deferred layers.
 * The first opaque layer will always have AOV buffers cleared.
 * However the subsequent layers (e.g. refraction) have to clear the result of the first layer for
 * all the pixels they touch. Doing it inside the material shader proved to be a bottleneck for
 * shader compilation. To avoid the overhead, we draw a full-screen triangle that will clear the
 * AOVs for the pixel affected by the next layer using stencil test after the pre-pass.
 */
[[fragment, early_fragment_tests]]
void aov_clear_frag([[resource_table]] RenderPassOutput &render_passes,
                    [[resource_table]] Uniform &uni,
                    [[frag_coord]] const float4 frag_co)
{
  render_passes.clear_aovs(uni, int2(frag_co.xy));
}

PipelineGraphic aov_clear(fullscreen_vert, aov_clear_frag);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deferred Pipeline
 * \{ */

struct LightEval {
  [[legacy_info]] ShaderCreateInfo draw_object_infos;
  [[legacy_info]] ShaderCreateInfo draw_view;

  [[specialization_constant(true)]] bool use_split_indirect;
  [[specialization_constant(true)]] bool use_lightprobe_eval;
  [[specialization_constant(false)]] bool use_transmission;
  [[specialization_constant(-1)]] int render_pass_shadow_id;
  [[specialization_constant(1)]] int shadow_ray_count;
  [[specialization_constant(6)]] int shadow_ray_step_count;

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
                     [[resource_table]] LightEvalIterator &lights,
                     [[resource_table]] const Uniform &uni,
                     [[resource_table]] const LightprobeRenderData &lightprobes,
                     [[resource_table]] const HiZ &hiz,
                     [[resource_table]] const UtilityTexture &util_tx,
                     [[resource_table]] const gbuffer::Reader &reader,
                     [[resource_table]] RenderPassOutput &render_passes,
                     [[frag_coord]] const float4 frag_co,
                     [[in]] const VertOut v_out)
{
  [[resource_table]] LightEvalData &lrt = lights.inner;

  const int2 texel = int2(frag_co.xy);

  /* Bias the shading point position because of depth buffer precision.
   * Constant is taken from https://www.terathon.com/gdc07_lengyel.pdf. */
  constexpr float bias = 2.4e-7f;

  const float depth = texelFetch(hiz.hiz_tx, texel, 0).r - bias;
  const gbuffer::Layers gbuf = reader.read_layers(texel);
  const Thickness thickness = reader.read_thickness(gbuf.header, texel);
  const uchar closure_count = gbuf.header.closure_len();

  const float3 P = drw_point_screen_to_world(float3(v_out.screen_uv, depth));
  const float3 Ng = gbuf.header.geometry_normal(gbuf.surface_N());
  const float3 V = drw_world_incident_vector(P);
  const float vPz = dot(drw_view_forward(), P) - dot(drw_view_forward(), drw_view_position());

  light::EvalCtx<false> ctx;
  /* Unroll light stack array assignments to avoid non-constant indexing. */
  for (uint i = 0u; i < 3; i++) [[unroll]] {
    if (lrt.light_closure_eval_count_reflect > i) [[static_branch]] {
      ctx.stack.cl[i] = closure_light_new(util_tx, gbuf.layer[i], V);
    }
  }

  ctx.P = P;
  ctx.Ng = Ng;
  ctx.V = V;
  ctx.texel = frag_co.xy;
  ctx.thickness = thickness;
  ctx.receiver_light_set = 0;
  ctx.terminator_normal_offset = 0.0f;
  ctx.terminator_geometry_offset = 0.0f;
  if (gbuf.header.use_object_id()) {
    uint object_id = reader.read_object_id(texel);
    ObjectInfos object_infos = drw_infos[object_id];
    ctx.receiver_light_set = receiver_light_set_get(object_infos);
    ctx.terminator_normal_offset = object_infos.shadow_terminator_normal_offset;
    ctx.terminator_geometry_offset = object_infos.shadow_terminator_geometry_offset;
  }

  /* TODO(fclem): If transmission (no SSS) is present, we could reduce LIGHT_CLOSURE_EVAL_COUNT
   * by 1 for this evaluation and skip evaluating the transmission closure twice. */
  lights.eval_reflection(ctx, vPz);

  if (srt.use_transmission) {
    light::EvalCtx<true> ctx_tr = light::init_from_reflect_ctx(ctx);

    ClosureUndetermined cl_transmit = gbuf.layer[0];
    ctx_tr.stack.cl[0] = closure_light_new(util_tx, cl_transmit, V, thickness);

    /* NOTE: Only evaluates `stack.cl[0]`. */
    lights.eval_transmission(ctx_tr, vPz);

    if (cl_transmit.type == CLOSURE_BSSRDF_BURLEY_ID) {
      /* Apply transmission profile onto transmitted light and sum with reflected light. */
      float3 sss_profile = subsurface_transmission(
          util_tx, to_closure_subsurface(cl_transmit).sss_radius, thickness.value());
      ctx.stack.cl[0].light_shadowed += ctx_tr.stack.cl[0].light_shadowed * sss_profile;
      ctx.stack.cl[0].light_unshadowed += ctx_tr.stack.cl[0].light_unshadowed * sss_profile;
    }
    else {
      ctx.stack.cl[0].light_shadowed = ctx_tr.stack.cl[0].light_shadowed;
      ctx.stack.cl[0].light_unshadowed = ctx_tr.stack.cl[0].light_unshadowed;
    }
  }

  if (srt.render_pass_shadow_id != -1) {
    float3 radiance_shadowed = float3(0);
    float3 radiance_unshadowed = float3(0);
    for (uint i = 0u; i < 3; i++) [[unroll]] {
      if (lrt.light_closure_eval_count_reflect > i) [[static_branch]] {
        if (i < closure_count) {
          radiance_shadowed += ctx.stack.cl[i].light_shadowed;
          radiance_unshadowed += ctx.stack.cl[i].light_unshadowed;
        }
      }
    }
    float3 shadows = radiance_shadowed * safe_rcp(radiance_unshadowed);
    render_passes.store_value(texel, srt.render_pass_shadow_id, average(shadows));
  }

  if (srt.use_lightprobe_eval) {
    LightProbeSample samp = lightprobes.load(frag_co.xy, P, Ng, V);

    float clamp_indirect = uni.uniform_buf.clamp.surface_indirect;
    samp.volume_irradiance = spherical_harmonics::clamp_energy(samp.volume_irradiance,
                                                               clamp_indirect);

    uint3 bin_indices = gbuf.header.bin_index_per_layer();

    for (uint i = 0u; i < 3; i++) [[unroll]] {
      if (lrt.light_closure_eval_count_reflect > i) [[static_branch]] {
        if (i < closure_count) {
          float3 indirect_light = lightprobes.eval(samp, gbuf.layer[i], P, V, thickness);
          float3 direct_light = ctx.stack.cl[i].light_shadowed;
          if (srt.use_split_indirect) {
            srt.write_radiance_indirect(bin_indices[i], texel, indirect_light);
            srt.write_radiance_direct(bin_indices[i], texel, direct_light);
          }
          else {
            srt.write_radiance_direct(bin_indices[i], texel, direct_light + indirect_light);
          }
        }
      }
    }
  }
  else {
    uint3 bin_indices = gbuf.header.bin_index_per_layer();

    for (uint i = 0u; i < 3; i++) [[unroll]] {
      if (lrt.light_closure_eval_count_reflect > i) [[static_branch]] {
        if (i < closure_count) {
          float3 direct_light = ctx.stack.cl[i].light_shadowed;
          srt.write_radiance_direct(bin_indices[i], texel, direct_light);
        }
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Probe Capture Pipeline
 * \{ */

struct SphereProbeEval {
  [[legacy_info]] ShaderCreateInfo draw_view;
  [[legacy_info]] ShaderCreateInfo draw_object_infos;
};

/* Sphere probe evaluate everything as diffuse since they can only rely on volume light-probes
 * being available. */
[[fragment, early_fragment_tests]]
void sphere_eval_frag([[resource_table]] SphereProbeEval & /*srt*/,
                      [[resource_table]] LightEvalIterator &lights,
                      [[resource_table]] const Sampling &sampling,
                      [[resource_table]] const LightprobeVolumeRenderData &lightprobes,
                      [[resource_table]] const HiZ &hiz,
                      [[resource_table]] const UtilityTexture &util_tx,
                      [[resource_table]] const gbuffer::Reader &reader,
                      [[frag_coord]] const float4 frag_co,
                      [[in]] const VertOut v_out,
                      [[out]] FragOut &frag_out)
{
  int2 texel = int2(frag_co.xy);

  float depth = texelFetch(hiz.hiz_tx, texel, 0).r;

  const gbuffer::Layers gbuf = reader.read_layers(texel);

  if (gbuf.has_no_closure()) {
    frag_out.radiance = float4(0.0f);
    return;
  }

  const uchar closure_count = gbuf.header.closure_len();
  const Thickness thickness = reader.read_thickness(gbuf.header, texel);

  float3 albedo_front = float3(0.0f);
  float3 albedo_back = float3(0.0f);

  /* Unroll needed for gbuf.layer access. */
  for (int i = 0; i < 3 /* GBUFFER_LAYER_MAX */; i++) [[unroll]] {
    if (i < closure_count) {
      ClosureUndetermined cl = gbuf.layer[i];
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
        case CLOSURE_BSDF_THIN_GLASS_TRANSMISSION_ID:
          albedo_back += cl.color;
          break;
        case CLOSURE_NONE_ID:
          /* TODO(fclem): Assert. */
          break;
      }
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

  light::EvalCtx<false> ctx;
  ctx.P = P;
  ctx.Ng = Ng;
  ctx.V = V;
  ctx.texel = frag_co.xy;
  ctx.thickness = thickness;
  ctx.receiver_light_set = 0;
  ctx.terminator_normal_offset = 0.0f;
  ctx.terminator_geometry_offset = 0.0f;
  if (gbuf.header.use_object_id()) {
    uint object_id = reader.read_object_id(texel);
    ObjectInfos object_infos = drw_infos[object_id];
    ctx.receiver_light_set = receiver_light_set_get(object_infos);
    ctx.terminator_normal_offset = object_infos.shadow_terminator_normal_offset;
    ctx.terminator_geometry_offset = object_infos.shadow_terminator_geometry_offset;
  }

  /* Direct light. */
  ctx.stack.cl[0] = closure_light_new(util_tx, cl, V);
  lights.eval_reflection(ctx, vPz);

  float3 radiance_front = ctx.stack.cl[0].light_shadowed;

  light::EvalCtx<true> ctx_tr = light::init_from_reflect_ctx(ctx);

  ctx_tr.stack.cl[0] = closure_light_new(util_tx, cl_transmit, V, thickness);
  lights.eval_transmission(ctx_tr, vPz);

  float3 radiance_back = ctx_tr.stack.cl[0].light_shadowed;

  /* Indirect light. */
  /* Can only load irradiance to avoid dependency loop with the reflection probe. */
  SphericalHarmonicL1<float4> sh = lightprobes.sample_probe(sampling, P, V, Ng);

  radiance_front += sh.evaluate_lambert(Ng).rgb;
  /* TODO(fclem): Correct transmission eval. */
  radiance_back += sh.evaluate_lambert(-Ng).rgb;

  frag_out.radiance = float4(radiance_front * albedo_front + radiance_back * albedo_back, 0.0f);
}

struct PlanarProbeEval {
  /* TODO(fclem): Workaround waiting for this lib to be ported to BSL.  */
  [[compilation_constant]] bool legacy_sphere_probe_enable;

  [[legacy_info]] ShaderCreateInfo draw_view;
  [[legacy_info]] ShaderCreateInfo draw_object_infos;
};

[[fragment, early_fragment_tests]]
void planar_eval_frag([[resource_table]] PlanarProbeEval & /*srt*/,
                      [[resource_table]] LightEvalIterator &lights,
                      [[resource_table]] const LightprobeRenderData &lightprobes,
                      [[resource_table]] const Sampling &sampling,
                      [[resource_table]] const HiZ &hiz,
                      [[resource_table]] const UtilityTexture &util_tx,
                      [[resource_table]] const gbuffer::Reader &reader,
                      [[frag_coord]] const float4 frag_co,
                      [[in]] const VertOut v_out,
                      [[out]] FragOut &frag_out)
{
  int2 texel = int2(frag_co.xy);

  float depth = texelFetch(hiz.hiz_tx, texel, 0).r;

  const gbuffer::Layers gbuf = reader.read_layers(texel);
  const uchar closure_count = gbuf.header.closure_len();
  const Thickness thickness = reader.read_thickness(gbuf.header, texel);

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

  /* Unroll needed for gbuf.layer access. */
  for (int i = 0; i < 3; i++) [[unroll]] {
    if (i < closure_count) {
      ClosureUndetermined cl = gbuf.layer[i];
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
        case CLOSURE_BSDF_THIN_GLASS_TRANSMISSION_ID: {
          cl_refract.color += cl.color;
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

  light::EvalCtx<false> ctx;
  ctx.P = P;
  ctx.Ng = Ng;
  ctx.V = V;
  ctx.texel = frag_co.xy;
  ctx.thickness = thickness;
  ctx.receiver_light_set = 0;
  ctx.terminator_normal_offset = 0.0f;
  ctx.terminator_geometry_offset = 0.0f;
  if (gbuf.header.use_object_id()) {
    uint object_id = reader.read_object_id(texel);
    ObjectInfos object_infos = drw_infos[object_id];
    ctx.receiver_light_set = receiver_light_set_get(object_infos);
    ctx.terminator_normal_offset = object_infos.shadow_terminator_normal_offset;
    ctx.terminator_geometry_offset = object_infos.shadow_terminator_geometry_offset;
  }

  /* Direct light. */
  ctx.stack.cl[0] = closure_light_new(util_tx, cl, V);
  ctx.stack.cl[1] = closure_light_new(util_tx, cl_reflect, V);
  lights.eval_reflection(ctx, vPz);

  float3 radiance_front = ctx.stack.cl[0].light_shadowed;
  float3 radiance_reflect = ctx.stack.cl[1].light_shadowed;

  light::EvalCtx<true> ctx_tr = light::init_from_reflect_ctx(ctx);
  ctx_tr.stack.cl[0] = closure_light_new(util_tx, cl_transmit, V, thickness);
  ctx_tr.stack.cl[1] = closure_light_new(util_tx, cl_refract, V, thickness);
  lights.eval_transmission(ctx_tr, vPz);

  float3 radiance_back = ctx_tr.stack.cl[0].light_shadowed;
  float3 radiance_refract = ctx_tr.stack.cl[1].light_shadowed;

  /* Indirect light. */
  [[resource_table]] const LightprobeVolumeRenderData &lp_volumes = lightprobes.volumes;
  SphericalHarmonicL1<float4> sh = lp_volumes.sample_probe(sampling, P, V, Ng);
  LightProbeSample samp = lightprobes.load(frag_co.xy, P, Ng, V);

  radiance_front += sh.evaluate_lambert(Ng).rgb;
  radiance_back += sh.evaluate_lambert(-Ng).rgb;
  radiance_reflect += lightprobes.eval(samp, cl_reflect, P, V, thickness);
  radiance_refract += lightprobes.eval(samp, cl_refract, P, V, thickness);

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
                             LightEvalData{
                                 .light_closure_eval_count_reflect = 1,
                                 .light_closure_eval_count_transmit = 1,
                             },
                             ShadowRenderData{
                                 .shadow_random = true,
                             });
PipelineGraphic light_double(fullscreen_vert,
                             light_eval_frag,
                             LightEvalData{
                                 .light_closure_eval_count_reflect = 2,
                                 .light_closure_eval_count_transmit = 1,
                             },
                             ShadowRenderData{
                                 .shadow_random = true,
                             });
PipelineGraphic light_triple(fullscreen_vert,
                             light_eval_frag,
                             LightEvalData{
                                 .light_closure_eval_count_reflect = 3,
                                 .light_closure_eval_count_transmit = 1,
                             },
                             ShadowRenderData{
                                 .shadow_random = true,
                             });
PipelineGraphic sphere_eval(fullscreen_vert,
                            sphere_eval_frag,
                            LightEvalData{
                                .light_closure_eval_count_reflect = 1,
                                .light_closure_eval_count_transmit = 1,
                            },
                            ShadowRenderData{
                                .shadow_random = true,
                            });
PipelineGraphic planar_eval(fullscreen_vert,
                            planar_eval_frag,
                            PlanarProbeEval{
                                .legacy_sphere_probe_enable = true,
                            },
                            LightEvalData{
                                .light_closure_eval_count_reflect = 2,
                                .light_closure_eval_count_transmit = 2,
                            },
                            ShadowRenderData{
                                .shadow_random = true,
                            });

}  // namespace eevee::deferred
