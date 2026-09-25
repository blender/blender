/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_intersect.bsl.hh"
#include "draw_model.bsl.hh"
#include "draw_view.bsl.hh"
#include "draw_viewlayer.bsl.hh"
#include "draw_volume.bsl.hh"
#include "eevee_bxdf_lut_lib.bsl.hh"
#include "eevee_camera_lib.bsl.hh"
#include "eevee_fast_gi.bsl.hh"
#include "eevee_hiz.bsl.hh"
#include "eevee_light_data.bsl.hh"
#include "eevee_light_eval.bsl.hh"
#include "eevee_light_iter.bsl.hh"
#include "eevee_lightprobe.bsl.hh"
#include "eevee_lightprobe_plane.bsl.hh"
#include "eevee_nodetree.bsl.hh"
#include "eevee_nodetree_closures_lib.bsl.hh"
#include "eevee_pipeline.bsl.hh"
#include "eevee_ray_trace_screen_lib.bsl.hh"
#include "eevee_renderpass.bsl.hh"
#include "eevee_sampling_lib.bsl.hh"
#include "eevee_shadow.bsl.hh"
#include "eevee_uniform.bsl.hh"
#include "eevee_utility_tx.bsl.hh"
#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_math_base.bsl.hh"
#include "gpu_shader_utildefines.bsl.hh"

#ifdef GLSL_CPP_STUBS
#  define CLOSURE_BIN_COUNT 3
#endif

/* Maximum number of picked closure. */
#ifndef CLOSURE_BIN_COUNT
/* WORKAROUND: Because of stupid create infos relying on define extraction (which bypass the
 * conditional). This definition would create a redefinition warning from the one defined above. */
#  undef CLOSURE_BIN_COUNT
#  define CLOSURE_BIN_COUNT 1
#endif

namespace eevee {

/* Resource for the raycast node implementation using screen space raytracing. */
struct Raycast {
  [[sampler(PREPASS_NORMAL_TEX_SLOT)]] sampler2D prepass_normal_tx;
  [[sampler(RAYCAST_DEPTH_TEX_SLOT)]] sampler2D raycast_depth_tx;
  [[sampler(OBJECT_ID_TEX_SLOT)]] usampler2D object_id_tx;
};

}

struct LightAccumulation {
  packed_float3 diffuse_light;
  packed_float3 diffuse_color;
  packed_float3 glossy_light;
  packed_float3 glossy_color;
  packed_float3 transmission_light;
  packed_float3 transmission_color;
  int light_accumulation_count;
};

struct ShadingData {
  /** Fragment coordinate. */
  float4 frag_co;
  /** World position. */
  packed_float3 P;
  /** Surface Normal. Normalized, overridden by bump displacement. */
  packed_float3 N;
  /** Raw interpolated normal (non-normalized) data. */
  packed_float3 Ni;
  /** Geometric Normal. */
  packed_float3 Ng;
  /** Curve Tangent Space. */
  packed_float3 curve_T, curve_B, curve_N;
  /** Barycentric coordinates. */
  packed_float2 barycentric_coords;
  packed_float3 barycentric_dists;
  /** Hair thickness in world space. */
  float hair_diameter;
  /** Index of the strand for per strand effects. */
  int hair_strand_id;
  /** Pointcloud infos. */
  packed_float3 point_position;
  float point_radius;
  int point_id;
  /** Ray properties (approximation). */
  float ray_depth;
  float ray_length;
  uchar ray_type;
  /** Is hair. */
  bool is_strand;
  /** Fragment front_facing value or true for other stages. */
  bool front_facing;

  LightAccumulation light_accum;

  packed_float3 emission;
  packed_float3 transmittance;
  float holdout;

  packed_float3 volume_scattering;
  float volume_anisotropy;
  packed_float3 volume_absorption;

  /* Global thickness set after nodetree_thickness evaluation. Otherwise set to 0.0f. */
  Thickness thickness;

  uint resource_id_raw;

  /* Sampled closure parameters. */
  Reservoir<ClosureUndetermined> closure_bins[CLOSURE_BIN_COUNT];

  bool closure_reflection_bin;

  Reservoir<ClosureUndetermined> closure_get(uchar i) const
  {
    switch (i) {
      case 0:
        return closure_bins[0];
#if CLOSURE_BIN_COUNT > 1
      case 1:
        return closure_bins[1];
#endif
#if CLOSURE_BIN_COUNT > 2
      case 2:
        return closure_bins[2];
#endif
    }
    /* Unreachable. */
    assert(false);
    return closure_bins[0];
  }

  ClosureUndetermined closure_get_resolved(uchar i, float additional_weight) const
  {
    Reservoir<ClosureUndetermined> r = closure_get(i);
    ClosureUndetermined cl = r.data;
    cl.color *= r.get_final_weight() * additional_weight;
    return cl;
  }
};

struct KernelGlobals {
  [[resource_table]] NodeTreeRes nt;

  [[resource_table]] draw::View view;
  [[resource_table]] draw::Model model;
  [[resource_table]] draw::Infos infos;
  [[resource_table]] draw::Attributes ob_attr;
  [[resource_table]] draw::ViewLayerAttributes vl_attr;

  [[resource_table]] eevee::PipelineConstants pipe;
  [[resource_table]] eevee::Uniform uniforms;
  [[resource_table]] eevee::Sampling sampling;

  [[resource_table]] [[condition(is_volume_pipe)]] draw::Volume volume;

  [[resource_table]] [[condition(!is_occupancy_pipe)]] UtilityTexture util_tx;

  [[resource_table]] [[condition(use_ambient_occlusion)]] eevee::HiZ hiz;

  [[resource_table]] [[condition(use_aov_output)]] eevee::RenderPassOutput renderpass_out;

  [[resource_table]] [[condition(use_raycast)]] eevee::Raycast raycast;
  [[resource_table]] [[condition(use_raycast)]] draw::ViewCulling view_culling;

  [[resource_table]] [[condition(use_forward_lighting)]] eevee::LightEvalIterator light_eval;
  [[resource_table]] [[condition(use_forward_lighting)]] eevee::LightprobeRenderData lightprobes;
  [[resource_table]] [[condition(use_forward_lighting)]]
  eevee::LightprobePlaneRenderData lightprobe_planes;

  [[resource_table]] [[condition(use_forward_lighting && use_transparency)]]
  eevee::PreviousLayerHiZ previous_layer_hiz;
  [[resource_table]] [[condition(use_forward_lighting && use_transparency)]]
  eevee::PreviousLayerRadiance previous_layer_radiance;

  [[resource_table]] [[condition(use_lighting_nodes)]] eevee::LightRenderData lrd;
  [[resource_table]] [[condition(use_lighting_nodes)]] eevee::ShadowRenderData srd;

  uint resource_id_get(const ShadingData &sd)
  {
    draw::ID id{sd.resource_id_raw};
    if (pipe.is_shadow_pipe) {
      return id.resource_id<64>();
    }
    return id.resource_id<1>();
  }

  uint view_id_get(const ShadingData &sd)
  {
    draw::ID id{sd.resource_id_raw};
    if (pipe.is_shadow_pipe) {
      return id.view_id<64>();
    }
    return id.view_id<1>();
  }

  ObjectMatrices object_matrices_get(const ShadingData &sd)
  {
    return model.get(resource_id_get(sd));
  }

  ObjectInfos object_infos_get(const ShadingData &sd)
  {
    return infos.get(resource_id_get(sd));
  }

  ViewMatrices view_matrices_get(const ShadingData &sd)
  {
    return view.get(view_id_get(sd));
  }

  ObjectMatrices light_matrices_get(int light_index)
  {
    return model.get(lrd.light_buf[light_index].resource_id);
  }
};

void closure_weights_reset(KernelGlobals &kg, ShadingData &sd, float closure_rand)
{
  sd.closure_bins[0].reset(closure_rand);
#if CLOSURE_BIN_COUNT > 1
  sd.closure_bins[1].reset(closure_rand);
#endif
#if CLOSURE_BIN_COUNT > 2
  sd.closure_bins[2].reset(closure_rand);
#endif

  sd.volume_scattering = float3(0.0f);
  sd.volume_anisotropy = 0.0f;
  sd.volume_absorption = float3(0.0f);

  sd.emission = float3(0.0f);
  sd.transmittance = float3(0.0f);
  sd.volume_scattering = float3(0.0f);
  sd.volume_absorption = float3(0.0f);
  sd.holdout = 0.0f;
  sd.closure_reflection_bin = true;

#if defined(GPU_FRAGMENT_SHADER) || defined(GLSL_CPP_STUBS)
  if (kg.pipe.use_lighting_nodes) [[static_branch]] {
    sd.light_accum = LightAccumulation{};
  }
#endif
}

template<typename T> void closure_base_copy(ClosureUndetermined &cl, T in_cl)
{
  cl.color = in_cl.color;
  cl.N = in_cl.N;
  cl.type = closure_type_get(in_cl);
}

template void closure_base_copy<ClosureDiffuse>(ClosureUndetermined &, ClosureDiffuse);
template void closure_base_copy<ClosureSubsurface>(ClosureUndetermined &, ClosureSubsurface);
template void closure_base_copy<ClosureTranslucent>(ClosureUndetermined &, ClosureTranslucent);
template void closure_base_copy<ClosureReflection>(ClosureUndetermined &, ClosureReflection);
template void closure_base_copy<ClosureRefraction>(ClosureUndetermined &, ClosureRefraction);
template void closure_base_copy<ClosureThinRefraction>(ClosureUndetermined &,
                                                       ClosureThinRefraction);

/* Single BSDFs. */
Closure closure_eval(ShadingData &sd, ClosureDiffuse diffuse)
{
  ClosureUndetermined cl;
  closure_base_copy(cl, diffuse);
#if (CLOSURE_BIN_COUNT > 1) && defined(MAT_TRANSLUCENT) && !defined(MAT_CLEARCOAT)
  /* Use second slot so we can have diffuse + translucent without noise. */
  closure_select(sd.closure_bins[1], cl);
#else
  /* Either is single closure or use same bin as transmission bin. */
  closure_select(sd.closure_bins[0], cl);
#endif
  return Closure(0);
}

Closure closure_eval(ShadingData &sd, ClosureSubsurface diffuse)
{
  ClosureUndetermined cl;
  closure_base_copy(cl, diffuse);
  cl.data.rgb = diffuse.sss_radius;
  /* Transmission Closures are always in first bin. */
  closure_select(sd.closure_bins[0], cl);
  return Closure(0);
}

Closure closure_eval(ShadingData &sd, ClosureTranslucent translucent)
{
  ClosureUndetermined cl;
  closure_base_copy(cl, translucent);
  /* Transmission Closures are always in first bin. */
  closure_select(sd.closure_bins[0], cl);
  return Closure(0);
}

Closure closure_eval(ShadingData &sd, ClosureReflection reflection)
{
  ClosureUndetermined cl;
  closure_base_copy(cl, reflection);
  cl.data.r = reflection.roughness;

#ifdef MAT_CLEARCOAT
/* Alternate between two bins on a per closure basis.
 * Allow clearcoat layer without noise.
 * Choosing the bin with the least weight can choose a
 * different bin for the same closure and
 * produce issue with ray-tracing denoiser.
 * Always start with the second bin, this one doesn't
 * overlap with other closure. */
#  if CLOSURE_BIN_COUNT == 2
  /* Multiple reflection closures. */
  if (sd.closure_reflection_bin) {
    closure_select(sd.closure_bins[1], cl);
  }
  else {
    closure_select(sd.closure_bins[0], cl);
  }
#  elif CLOSURE_BIN_COUNT == 3
  /* Multiple reflection closures and one other closure. */
  if (sd.closure_reflection_bin) {
    closure_select(sd.closure_bins[2], cl);
  }
  else {
    closure_select(sd.closure_bins[1], cl);
  }
#  else
#    error Clearcoat should always have at least 2 bins
#  endif
  sd.closure_reflection_bin = !sd.closure_reflection_bin;
#else
#  if CLOSURE_BIN_COUNT == 1
  /* Only one reflection closure is present in the whole tree. */
  closure_select(sd.closure_bins[0], cl);
#  elif CLOSURE_BIN_COUNT == 2
  /* Only one reflection and one other closure. */
  closure_select(sd.closure_bins[1], cl);
#  elif CLOSURE_BIN_COUNT == 3
  /* Only one reflection and two other closures. */
  closure_select(sd.closure_bins[2], cl);
#  endif
#endif

#undef CHOOSE_MIN_WEIGHT_CLOSURE_BIN

  return Closure(0);
}

Closure closure_eval(ShadingData &sd, ClosureRefraction refraction)
{
  ClosureUndetermined cl;
  closure_base_copy(cl, refraction);
  cl.data.r = refraction.roughness;
  cl.data.g = refraction.ior;
  /* Transmission Closures are always in first bin. */
  closure_select(sd.closure_bins[0], cl);
  return Closure(0);
}

Closure closure_eval(ShadingData &sd, ClosureThinRefraction refraction)
{
  ClosureUndetermined cl;
  closure_base_copy(cl, refraction);
  cl.data.r = refraction.roughness;
  /* Transmission Closures are always in first bin. */
  closure_select(sd.closure_bins[0], cl);
  return Closure(0);
}

Closure closure_eval(ShadingData &sd, ClosureEmission emission)
{
  sd.emission += emission.emission;
  return Closure(0);
}

Closure closure_eval(ShadingData &sd, ClosureTransparency transparency)
{
  sd.transmittance += transparency.transmittance;
  sd.holdout += transparency.holdout;
  return Closure(0);
}

Closure closure_eval(ShadingData &sd, ClosureVolumeScatter volume_scatter)
{
  sd.volume_scattering += volume_scatter.scattering;
  sd.volume_anisotropy += volume_scatter.anisotropy;
  return Closure(0);
}

Closure closure_eval(ShadingData &sd, ClosureVolumeAbsorption volume_absorption)
{
  sd.volume_absorption += volume_absorption.absorption;
  return Closure(0);
}

Closure closure_eval(ShadingData & /*sd*/, ClosureHair /*hair*/)
{
  /* TODO */
  return Closure(0);
}

/* Glass BSDF. */
Closure closure_eval(ShadingData &sd, ClosureReflection reflection, ClosureRefraction refraction)
{
  closure_eval(sd, reflection);
  closure_eval(sd, refraction);
  return Closure(0);
}

/* Dielectric BSDF. */
Closure closure_eval(ShadingData &sd, ClosureDiffuse diffuse, ClosureReflection reflection)
{
  closure_eval(sd, diffuse);
  closure_eval(sd, reflection);
  return Closure(0);
}

/* Coat BSDF. */
Closure closure_eval(ShadingData &sd, ClosureReflection reflection, ClosureReflection coat)
{
  closure_eval(sd, reflection);
  closure_eval(sd, coat);
  return Closure(0);
}

/* Volume BSDF. */
Closure closure_eval(ShadingData &sd,
                     ClosureVolumeScatter volume_scatter,
                     ClosureVolumeAbsorption volume_absorption,
                     ClosureEmission emission)
{
  closure_eval(sd, volume_scatter);
  closure_eval(sd, volume_absorption);
  closure_eval(sd, emission);
  return Closure(0);
}

/* Specular BSDF. */
Closure closure_eval(ShadingData &sd,
                     ClosureDiffuse diffuse,
                     ClosureReflection reflection,
                     ClosureReflection coat)
{
  closure_eval(sd, diffuse);
  closure_eval(sd, reflection);
  closure_eval(sd, coat);
  return Closure(0);
}

/* Principled BSDF. */
Closure closure_eval(ShadingData &sd,
                     ClosureDiffuse diffuse,
                     ClosureReflection reflection,
                     ClosureReflection coat,
                     ClosureRefraction refraction)
{
  closure_eval(sd, diffuse);
  closure_eval(sd, reflection);
  closure_eval(sd, coat);
  closure_eval(sd, refraction);
  return Closure(0);
}

/* NOP since we are sampling closures. */
Closure closure_add(Closure /*cl1*/, Closure /*cl2*/)
{
  return Closure(0);
}
Closure closure_mix(Closure /*cl1*/, Closure /*cl2*/, float /*fac*/)
{
  return Closure(0);
}

float ambient_occlusion_eval(const KernelGlobals &kg,
                             const ShadingData &sd,
                             float3 normal,
                             float max_distance,
                             const float inverted,
                             const float sample_count)
{
#if defined(GPU_FRAGMENT_SHADER) || defined(GLSL_CPP_STUBS)
  if (kg.pipe.use_ambient_occlusion) [[static_branch]] {
    if (!kg.pipe.is_occupancy_pipe) [[static_branch]] {
      const eevee::Sampling &samp = kg.sampling;
      const UtilityTexture &util_tx = kg.util_tx;
      const eevee::HiZ &hiz = kg.hiz;
      const eevee::Uniform &uni = kg.uniforms;
      const draw::View &views = kg.view;

      const ViewMatrices view = views.get(0);

      float3 vP = view.point_world_to_view(sd.P);
      float3 vN = view.normal_world_to_view(normal);

      float4 noise = util_tx.fetch(sd.frag_co.xy, UTIL_BLUE_NOISE_LAYER);
      noise = fract(noise + samp.rng_3D_get(SAMPLING_AO_U).xyzx);

      float result = eevee::fast_gi::eval<float>(uni,
                                                 view,
                                                 uni.raytrace_buf.fast_gi_thickness,
                                                 hiz.hiz_tx,
                                                 hiz.hiz_tx /* Dummy. */,
                                                 hiz.hiz_tx /* Dummy. */,
                                                 vP,
                                                 vN,
                                                 noise,
                                                 uni.uniform_buf.ao.pixel_size,
                                                 max_distance,
                                                 uni.uniform_buf.ao.angle_bias,
                                                 2,
                                                 int(sample_count / 2.0f),
                                                 inverted != 0.0f,
                                                 true);

      return saturate(result);
    }
  }
#endif
  return 1.0f;
}

void raycast_eval(KernelGlobals &kg,
                  const ShadingData &sd,
                  float3 position,
                  float3 direction,
                  float max_distance,
                  bool self_only,
                  bool &is_hit,
                  bool &self_hit,
                  float &hit_distance,
                  float3 &hit_position,
                  float3 &hit_normal)
{

  is_hit = false;
  self_hit = false;
  hit_distance = max_distance;
  hit_position = float3(0.0f);
  hit_normal = float3(0.0f);

  direction = normalize(direction);

  if (kg.pipe.use_raycast) [[static_branch]] {
    if (!kg.uniforms.pipeline_buf.can_raycast) {
      /* We can't ray-cast on pre-pass for ray-cast visible objects.
       * We use a UBO property to avoid compiling more shader variants. */
      return;
    }

    float3 ws_start = position;
    float3 ws_end = position + direction * max_distance;
    if (!clip_ray(ws_start,
                  ws_end,
                  direction,
                  max_distance,
                  kg.view_culling.get(0).frustum_planes.planes))
    {
      return;
    }

    {
      const eevee::Raycast &raycast = kg.raycast;
      const ViewMatrices view = kg.view.get(0);

      {
        /* Offset the start to prevent wrong intersection due to depth precision. */
        float3 vs_start = view.point_world_to_view(ws_start);
        float start_depth = view.depth_view_to_screen(vs_start.z);
        float offset_depth = uintBitsToFloat(floatBitsToUint(start_depth) + 2);
        float offset_delta = abs(view.depth_screen_to_view(offset_depth) - vs_start.z);
        ws_start += direction * offset_delta;
      }

      float noise_offset = kg.sampling.rng_1D_get(SAMPLING_RAYTRACE_W);
      float jitter = interleaved_gradient_noise(sd.frag_co.xy, 1.0f, noise_offset);

      float2 hit_uv = float2(0.0f);
      uint self_id = kg.resource_id_get(sd) & uint(0xFFFF);

      float result = raytrace_screen_2(view,
                                       view.point_world_to_view(ws_start),
                                       view.point_world_to_view(ws_end),
                                       view.normal_world_to_view(direction),
                                       raycast.raycast_depth_tx,
                                       kg.uniforms.raytrace_buf,
                                       64,
                                       jitter,
                                       raycast.object_id_tx,
                                       self_only ? self_id : 0u,
                                       hit_uv);
      if (result >= 0.0f) {
        is_hit = true;
        hit_position = ws_start + direction * result;
        hit_distance = distance(position, hit_position);
        hit_normal = normalize(texture(raycast.prepass_normal_tx, hit_uv).xyz * 2.0f - 1.0f);
        int2 hit_texel = int2(hit_uv * float2(textureSize(raycast.object_id_tx, 0)));
        uint hit_id = texelFetch(raycast.object_id_tx, hit_texel, 0).x;
        self_hit = self_only || (hit_id == self_id);
      }
    }
  }
}

float3 nodetree_displacement(KernelGlobals &kg, ShadingData &sd);
float4 closure_to_rgba(KernelGlobals &kg, ShadingData &sd, Closure cl);

/**
 * Used for packing.
 * This is the reflection coefficient also denoted r.
 * https://en.wikipedia.org/wiki/Fresnel_equations#Complex_amplitude_reflection_and_transmission_coefficients
 */
float f0_from_ior(float eta)
{
  return (eta - 1.0f) / (eta + 1.0f);
}

/**
 * Simplified form of F_eta(eta, 1.0).
 * This is the power reflection coefficient also denoted R.
 * https://en.wikipedia.org/wiki/Fresnel_equations#Complex_amplitude_reflection_and_transmission_coefficients
 */
float F0_from_ior(float eta)
{
  return square(f0_from_ior(eta));
}
float F0_from_f0(float f0)
{
  return square(f0);
}

/**
 * Return the fresnel color from a precomputed LUT value.
 */
template<typename T> float3 F_brdf_single_scatter(float3 f0, float3 f90, T lut)
{
  return f0 * lut.scale + f90 * lut.bias;
}
template float3 F_brdf_single_scatter<eevee::lut::GGXBrdfData>(float3,
                                                               float3,
                                                               eevee::lut::GGXBrdfData);
template float3 F_brdf_single_scatter<eevee::lut::GGXBsdfData>(float3,
                                                               float3,
                                                               eevee::lut::GGXBsdfData);

/* Multi-scattering brdf approximation from
 * "A Multiple-Scattering Microfacet Model for Real-Time Image-based Lighting"
 * https://jcgt.org/published/0008/01/03/paper.pdf by Carmelo J. Fdez-Agüera. */
template<typename T> float3 F_brdf_multi_scatter(float3 f0, float3 f90, T lut)
{
  float3 FssEss = F_brdf_single_scatter(f0, f90, lut);

  float Ess = lut.scale + lut.bias;
  float Ems = 1.0f - Ess;
  float3 Favg = f0 + (f90 - f0) / 21.0f;

  /* The original paper uses `FssEss * radiance + Fms*Ems * irradiance`, but
   * "A Journey Through Implementing Multi-scattering BRDFs and Area Lights" by Steve McAuley
   * suggests to use `FssEss * radiance + Fms*Ems * radiance` which results in comparable quality.
   * We handle `radiance` outside of this function, so the result simplifies to:
   * `FssEss + Fms*Ems = FssEss * (1 + Ems*Favg / (1 - Ems*Favg)) = FssEss / (1 - Ems*Favg)`.
   * This is a simple albedo scaling very similar to the approach used by Cycles:
   * "Practical multiple scattering compensation for microfacet model". */
  return FssEss / (1.0f - Ems * Favg);
}
template float3 F_brdf_multi_scatter<eevee::lut::GGXBrdfData>(float3,
                                                              float3,
                                                              eevee::lut::GGXBrdfData);
template float3 F_brdf_multi_scatter<eevee::lut::GGXBsdfData>(float3,
                                                              float3,
                                                              eevee::lut::GGXBsdfData);

void brdf_f82_tint_lut(KernelGlobals &kg,
                       float3 F0,
                       float3 F82,
                       float cos_theta,
                       float roughness,
                       bool do_multiscatter,
                       float3 &reflectance)
{
  if (!kg.pipe.is_occupancy_pipe) [[static_branch]] {
    eevee::lut::GGXBrdfData lut = eevee::lut::GGXBrdfData::sample_utility_tx(
        kg.util_tx, cos_theta, roughness);

    reflectance = do_multiscatter ? F_brdf_multi_scatter(F0, float3(1.0f), lut) :
                                    F_brdf_single_scatter(F0, float3(1.0f), lut);

    /* Precompute the F82 term factor for the Fresnel model.
     * In the classic F82 model, the F82 input directly determines the value of the Fresnel
     * model at ~82 degrees, similar to F0 and F90.
     * With F82-Tint, on the other hand, the value at 82 degrees is the value of the classic
     * Schlick model multiplied by the tint input. Therefore, the factor follows by setting
     * `F82Tint(cosI) = FSchlick(cosI) - b*cosI*(1-cosI)^6` and `F82Tint(acos(1/7)) =
     * FSchlick(acos(1/7)) * f82_tint` and solving for `b`. */
    constexpr float f = 6.0f / 7.0f;
    constexpr float f5 = (f * f) * (f * f) * f;
    constexpr float f6 = (f * f) * (f * f) * (f * f);
    float3 F_schlick = mix(F0, float3(1.0f), f5);
    float3 b = F_schlick * (7.0f / f6) * (1.0f - F82);
    reflectance -= b * lut.metal_bias;
  }
}

/* Computes the reflectance and transmittance based on the tint (`f0`, `f90`, `transmission_tint`)
 * and the BSDF LUT. */
void bsdf_lut(KernelGlobals &kg,
              float3 F0,
              float3 F90,
              float3 transmission_tint,
              float cos_theta,
              float roughness,
              float ior,
              bool do_multiscatter,
              float3 &reflectance,
              float3 &transmittance)
{

  if (ior == 1.0f) {
    reflectance = float3(0.0f);
    transmittance = transmission_tint;
    return;
  }

  /* TODO(not_mark): strip namespaces on BSL port. */
  eevee::lut::GGXBsdfData lut = {};

  const float f0 = f0_from_ior(ior);

  if (!kg.pipe.is_occupancy_pipe) [[static_branch]] {
    if (ior > 1.0f) {
      /* Gradually increase `f90` from 0 to 1 when IOR is in the range of [1.0f, 1.33f], to avoid
       * harsh transition at `IOR == 1`. */
      if (all(equal(F90, float3(1.0f)))) {
        F90 = float3(saturate(2.33f / 0.33f * f0));
      }

      eevee::lut::GGXBrdfData brdf_lut = eevee::lut::GGXBrdfData::sample_utility_tx(
          kg.util_tx, cos_theta, roughness);
      eevee::lut::GGXBtdfGt1Data btdf_lut = eevee::lut::GGXBtdfGt1Data::sample_utility_tx(
          kg.util_tx, cos_theta, roughness, f0);

      lut.scale = brdf_lut.scale;
      lut.bias = brdf_lut.bias;
      lut.transmission_factor = btdf_lut.transmission_factor;
    }
    else {
      lut = eevee::lut::GGXBsdfData::sample_utility_tx(kg.util_tx, cos_theta, roughness, ior);
    }
  }

  reflectance = F_brdf_single_scatter(F0, F90, lut);
  transmittance = (float3(1.0f) - F0) * lut.transmission_factor * transmission_tint;

  if (do_multiscatter) {
    const float real_F0 = F0_from_f0(f0);
    const float Ess = real_F0 * lut.scale + lut.bias + (1.0f - real_F0) * lut.transmission_factor;
    const float Ems = 1.0f - Ess;
    /* Assume that the transmissive tint makes up most of the overall color if it's not zero. */
    const float3 Favg = all(equal(transmission_tint, float3(0.0f))) ? F0 + (F90 - F0) / 21.0f :
                                                                      transmission_tint;

    float3 scale = 1.0f / (1.0f - Ems * Favg);
    reflectance *= scale;
    transmittance *= scale;
  }
}

/* Computes the reflectance and transmittance based on the BSDF LUT. */
float2 bsdf_lut(
    KernelGlobals &kg, float cos_theta, float roughness, float ior, bool do_multiscatter)
{
  float F0 = F0_from_ior(ior);
  float3 color = float3(1.0f);
  float3 reflectance, transmittance;
  bsdf_lut(kg,
           float3(F0),
           color,
           color,
           cos_theta,
           roughness,
           ior,
           do_multiscatter,
           reflectance,
           transmittance);
  return float2(reflectance.r, transmittance.r);
}

float3 brdf_lut(KernelGlobals &kg,
                float3 F0,
                float3 F90,
                float cos_theta,
                float roughness,
                bool do_multiscatter)
{
  if (!kg.pipe.is_occupancy_pipe) [[static_branch]] {
    eevee::lut::GGXBrdfData lut = eevee::lut::GGXBrdfData::sample_utility_tx(
        kg.util_tx, cos_theta, roughness);

    return do_multiscatter ? F_brdf_multi_scatter(F0, F90, lut) :
                             F_brdf_single_scatter(F0, F90, lut);
  }
  return float3(0.0f);
}

/* -------------------------------------------------------------------- */
/** \name Mixed render resolution
 *
 * Callbacks image texture sampling.
 *
 * \{ */

float texture_lod_bias_get(KernelGlobals &kg)
{
  const eevee::Uniform &uni = kg.uniforms;
  return uni.uniform_buf.film.texture_lod_bias;
}

/**
 * Scale hardware derivatives depending on render resolution.
 * This is because the distance between pixels increases as we lower the resolution. The hardware
 * uses neighboring pixels to compute derivatives and thus the value increases as we lower the
 * resolution. So we compensate by scaling them back to the expected amplitude at full resolution.
 */
float derivative_scale_get(KernelGlobals &kg)
{
  const eevee::Uniform &uni = kg.uniforms;
  return 1.0f / float(uni.uniform_buf.film.scaling_factor);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Fragment Displacement
 *
 * Displacement happening in the fragment shader.
 * Can be used in conjunction with a per vertex displacement.
 *
 * \{ */

#ifdef MAT_DISPLACEMENT_BUMP
/* Return new shading normal. */
float3 displacement_bump([[maybe_unused]] KernelGlobals &kg, const ShadingData &sd)
{
#  if !defined(MAT_GEOM_CURVES)
  /* This is the filter width for automatic displacement + bump mapping, which is fixed.
   * NOTE: keep the same as default bump node filter width. */
  constexpr float bump_filter_width = 0.1f;

  float2 dHd;
  dF_branch(dot(nodetree_displacement(kg, sd), sd.N + dF_impl(sd.N)), bump_filter_width, dHd);

  float3 dPdx = gpu_dfdx(sd.P) * derivative_scale_get(kg);
  float3 dPdy = gpu_dfdy(sd.P) * derivative_scale_get(kg);

  /* Get surface tangents from normal. */
  float3 Rx = cross(dPdy, sd.N);
  float3 Ry = cross(sd.N, dPdx);

  /* Compute surface gradient and determinant. */
  float det = dot(dPdx, Rx);

  float3 surfgrad = dHd.x * Rx + dHd.y * Ry;

  float facing = FrontFacing ? 1.0f : -1.0f;
  return normalize(bump_filter_width * abs(det) * sd.N - facing * sign(det) * surfgrad);
#  else
  return sd.N;
#  endif
}
#endif

void fragment_displacement([[maybe_unused]] KernelGlobals &kg, [[maybe_unused]] ShadingData &sd)
{
#ifdef MAT_DISPLACEMENT_BUMP
  sd.N = displacement_bump(kg, sd);
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Coordinate implementations
 *
 * Callbacks for the texture coordinate node.
 *
 * \{ */

struct Coordinates {
  float3 camera;
  float3 screen;
  float3 reflect;
  float3 incoming;
};

Coordinates coordinate_impl(KernelGlobals &kg, const ShadingData &sd, float3 P, float3 N)
{
  const ViewMatrices view = kg.view_matrices_get(sd);

  Coordinates coords = {};

  if (false /* Probe. */) {
    /* Unsupported. It would make the probe camera-dependent. */
    coords.camera = P;
    coords.screen.xy = float2(0.5f);
  }
  else {
    const eevee::Uniform &uni = kg.uniforms;
    if (kg.pipe.is_world) [[static_branch]] {
      coords.camera = view.normal_world_to_view(P);
      coords.screen.xy = view.point_world_to_screen(P + view.position()).xy;
      coords.screen.xy = coords.screen.xy * uni.uniform_buf.camera.uv_scale +
                         uni.uniform_buf.camera.uv_bias;
    }
    else {
      const CameraData cam = uni.uniform_buf.camera;
      if (is_panoramic(cam.type)) {
        /* Panoramic camera render through per-face sub-views.
         * Can't use `view` here because that's only one sub-view, not the camera. */
        coords.camera = transform_point(cam.viewmat, P);
        float3 dir = P - cam.viewinv[3].xyz;
        coords.screen.xy = fract(eevee::camera::uv_from_world(cam, dir) + cam.uv_bias);
      }
      else {
        const ViewMatrices view = kg.view_matrices_get(sd);
        coords.camera = view.point_world_to_view(P);
        coords.screen.xy = view.point_world_to_screen(P).xy;
        coords.screen.xy = coords.screen.xy * cam.uv_scale + cam.uv_bias;
      }
    }
  }
  coords.camera.z = -coords.camera.z;

  if (kg.pipe.is_world) [[static_branch]] {
    coords.reflect = N;
    coords.incoming = -P;
  }
  else {
    coords.reflect = -reflect(view.world_incident_vector(P), N);
    coords.incoming = view.world_incident_vector(P);
  }
  return coords;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume Attribute post
 *
 * TODO(@fclem): These implementation details should concern the DRWContext and not be a fix on
 * the engine side. But as of now, the engines are responsible for loading the attributes.
 *
 * \{ */

/* Point clouds and curves and splats are not compatible with volume grids.
 * They will fall back to their own attributes loading. */

float attr_load_temperature_post(KernelGlobals &kg, float attr)
{
  if (kg.pipe.is_volume_pipe) [[static_branch]] {
    const draw::Volume &vol = kg.volume;
    /* Bring the value into standard range without having to modify the grid values */
    attr = (attr > 0.01f) ?
               (attr * vol.drw_volume.temperature_mul + vol.drw_volume.temperature_bias) :
               0.0f;
  }
  return attr;
}
float4 attr_load_color_post(KernelGlobals &kg, float4 attr)
{
  if (kg.pipe.is_volume_pipe) [[static_branch]] {
    const draw::Volume &vol = kg.volume;
    /* Density is premultiplied for interpolation, divide it out here. */
    attr.rgb *= safe_rcp(attr.a);
    attr.rgb *= vol.drw_volume.color_mul.rgb;
    attr.a = 1.0f;
  }
  return attr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GSplat Attributes
 *
 * GSplats override the radiance attribute, unpacking packed data from a float2. Additionally,
 * it applies its own alpha transparency on top of existing transmittance.
 *
 * \{ */

#if defined(MAT_GEOM_GSPLAT) && !defined(MAT_VOLUME)
#  define GSPLAT_ATTRIBUTES_LOAD_POST
#endif

float4 attr_load_radiance_post(KernelGlobals & /*kg*/, float4 attr)
{
#ifdef GSPLAT_ATTRIBUTES_LOAD_POST
  /* Radiance is packed as 2xfp16, unpack it from this representation. */
  uint2 data = floatBitsToUint(attr.xy);
  return float4(unpackHalf2x16(data.x), unpackHalf2x16(data.y));
#endif
  return attr;
}

#undef GSPLAT_ATTRIBUTES_LOAD_POST

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniform Attributes
 *
 * TODO(@fclem): These implementation details should concern the DRWContext and not be a fix on
 * the engine side. But as of now, the engines are responsible for loading the attributes.
 *
 * \{ */

float4 attr_load_uniform(KernelGlobals &kg,
                         const ShadingData &sd,
                         float4 /*attr*/,
                         const uint attr_hash)
{
  const draw::Attributes &ob_attributes = kg.ob_attr;
  return ob_attributes.get(kg.object_infos_get(sd), attr_hash);
}

float4 node_attribute_layer_impl(KernelGlobals &kg, const uint attr_hash)
{
  const draw::ViewLayerAttributes &vl_attributes = kg.vl_attr;
  return vl_attributes.get(attr_hash);
}

void scene_time_uniforms(KernelGlobals &kg, float &seconds, float &frame)
{
  const eevee::Uniform &uni = kg.uniforms;

  seconds = uni.uniform_buf.scene.time;
  frame = uni.uniform_buf.scene.frame;
}

void light_iter_init(KernelGlobals &kg,
                     const ShadingData &sd,
                     eevee::light::VisibleLightIterator &iter)
{
  if (kg.pipe.use_lighting_nodes) [[static_branch]] {
#if defined(GPU_FRAGMENT_SHADER) || defined(GLSL_CPP_STUBS)
    const ViewMatrices view = kg.view_matrices_get(sd);
    const float3 forward = view.forward();
    const float linear_view_z = dot(forward, sd.P) - dot(forward, view.position());
    iter.init(
        kg.lrd, sd.frag_co.xy, linear_view_z, receiver_light_set_get(kg.object_infos_get(sd)));
#endif
  }
}

bool light_iter_next(KernelGlobals &kg, eevee::light::VisibleLightIterator &iter)
{
  if (kg.pipe.use_lighting_nodes) [[static_branch]] {
#if defined(GPU_FRAGMENT_SHADER) || defined(GLSL_CPP_STUBS)
    return iter.next(kg.lrd);
#endif
  }
  return false;
}

bool light_iter_should_skip(KernelGlobals &kg, eevee::light::VisibleLightIterator &iter, float3 P)
{
  if (kg.pipe.use_lighting_nodes) [[static_branch]] {
#if defined(GPU_FRAGMENT_SHADER) || defined(GLSL_CPP_STUBS)
    return iter.should_skip(kg.lrd, P);
#endif
  }
  return true;
}

#if (defined(GPU_FRAGMENT_SHADER) && defined(SRT_CONSTANT_use_lighting_nodes)) || \
    defined(GLSL_CPP_STUBS)

#  define LIGHT_ITER_BEGIN(light_index) \
    { \
      eevee_light_VisibleLightIterator iter; \
      light_iter_init(kg, sd, iter); \
      while (light_iter_next(kg, iter)) { \
        if (light_iter_should_skip(kg, iter, sd.P)) { \
          continue; \
        } \
        light_index = iter.index;

#else

/* Avoid double definition warning (GPU_SHADER_CREATE_INFO shenanigans).  */
#  ifdef LIGHT_ITER_BEGIN
#    undef LIGHT_ITER_BEGIN
#  endif

#  define LIGHT_ITER_BEGIN(light_index) \
    { \
      if (false) {

#endif

#define LIGHT_ITER_END() \
  } \
  }

void node_light_info_impl(
    KernelGlobals &kg, const int light_index, float4 &color, float &power, float3 &position)
{
  if (kg.pipe.use_lighting_nodes) [[static_branch]] {
    const eevee::LightRenderData &lrd = kg.lrd;
    const draw::Model &models = kg.model;

    LightData light = lrd.light_buf[light_index];

    color = float4(light.color, 1.0f);
    /* Pre-divide power by PI,
     * so users get a closer match to physically correct energy by default. */
    power = light.point_power * M_1_PI;
    /* Retrieve the data from the draw manager matrix, since Sun lights object_to_world doesn't
     * store the actual position. */
    position = models.get(light.resource_id).model[3].xyz;
  }
}

void node_light_evaluation_common_impl(KernelGlobals &kg,
                                       int light_index,
                                       float3 position,
                                       float3 &direction,
                                       float &distance,
                                       float &mask)
{
  if (kg.pipe.use_lighting_nodes) [[static_branch]] {
    const eevee::LightRenderData &lrd = kg.lrd;

    LightData light = lrd.light_buf[light_index];
    const bool is_directional = (light.type == LIGHT_SUN) || (light.type == LIGHT_SUN_ORTHO);
    LightVector lv = LightVector::get(light, is_directional, position);

    direction = lv.L;
    distance = lv.dist;
    mask = light_attenuation_surface(light, is_directional, lv);
  }
}

template<bool use_diffuse>
void node_light_evaluation_impl(KernelGlobals &kg,
                                const ShadingData &sd,
                                int light_index,
                                float3 position,
                                float3 normal,
                                float roughness,
                                float &factor)
{
  if (kg.pipe.use_lighting_nodes) [[static_branch]] {
    const eevee::LightRenderData &lrd = kg.lrd;
    UtilityTexture &util_tx = kg.util_tx;

    LightData light = lrd.light_buf[light_index];
    const bool is_directional = (light.type == LIGHT_SUN) || (light.type == LIGHT_SUN_ORTHO);
    LightVector lv = LightVector::get(light, is_directional, position);
    LightShape light_shape_vertices = LightShape::get(light, lv);

    const ViewMatrices view = kg.view_matrices_get(sd);
    const float3 V = view.world_incident_vector(position);

    /* TODO(not_mark): remove, and update tests as this causes precision change. */
    /* Load LTC matrix and rotate into orthonormal basis around N. */
    eevee::LTCData ltc_data;
    if constexpr (use_diffuse) {
      ltc_data = eevee::LTCData::identity(normal, V);
    }
    else {
      ltc_data = eevee::LTCData::sample_ltc_lut(util_tx, normal, V, dot(normal, V), roughness);
    }
    float3x3 T = from_incident_vector(normal, V);
    ltc_data.Minv = ltc_data.Minv * transpose(T);
    factor = eevee::ltc::evaluate(util_tx.utility_tx, light, light_shape_vertices, lv, ltc_data);

    factor *= light.shape_power;
    /* Remove the base power (exposed in Light Info).
     * The goal is to get a somewhat normalized factor. */
    factor /= (light.point_power * M_1_PI);
  }
}

template void node_light_evaluation_impl<true>(
    KernelGlobals &, const ShadingData &, int, float3, float3, float, float &);

template void node_light_evaluation_impl<false>(
    KernelGlobals &, const ShadingData &, int, float3, float3, float, float &);

void node_shadow_raycast_impl(KernelGlobals &kg,
                              const ShadingData &sd,
                              const int light_index,
                              float3 position,
                              float softness,
                              float4 &color)
{
  if (kg.pipe.use_lighting_nodes) [[static_branch]] {
    color = float4(1.0f);

#if defined(GPU_FRAGMENT_SHADER) || defined(GLSL_CPP_STUBS)
    LightData light = kg.lrd.light_buf[light_index];

    if (light.tilemap_index == LIGHT_NO_SHADOW) {
      return;
    }

    int ray_count = kg.uniforms.uniform_buf.shadow.ray_count;
    int ray_step_count = kg.uniforms.uniform_buf.shadow.step_count;

    ObjectInfos object_infos = kg.object_infos_get(sd);

    float shadow = eevee::shadow_eval(kg.srd,
                                      light,
                                      (light.type == LIGHT_SUN) || (light.type == LIGHT_SUN_ORTHO),
                                      false,
                                      false,
                                      sd.frag_co.xy,
                                      sd.thickness,
                                      position,
                                      sd.Ng,
                                      sd.N,
                                      object_infos.shadow_terminator_normal_offset,
                                      object_infos.shadow_terminator_geometry_offset,
                                      softness,
                                      ray_count,
                                      ray_step_count);

    color.rgb = float3(shadow);
#endif
  }
}

float4 node_attribute_light_impl([[resource_table]] KernelGlobals &kg,
                                 const int light_index,
                                 uint attr_hash)
{
  if (kg.pipe.use_lighting_nodes) [[static_branch]] {
    LightData light = kg.lrd.light_buf[light_index];
    return kg.ob_attr.get(kg.infos.get(light.resource_id), attr_hash);
  }
  return float4(0.0f);
}

bool node_attribute_light_is_sun_impl(KernelGlobals &kg, const int light_index)
{
  if (kg.pipe.use_lighting_nodes) [[static_branch]] {
    LightData light = kg.lrd.light_buf[light_index];
    return is_sun_light(light.type);
  }
  return false;
}

bool node_attribute_light_is_point_impl(KernelGlobals &kg, const int light_index)
{
  if (kg.pipe.use_lighting_nodes) [[static_branch]] {
    LightData light = kg.lrd.light_buf[light_index];
    return is_point_light(light.type) && !is_spot_light(light.type);
  }
  return false;
}

bool node_attribute_light_is_spot_impl(KernelGlobals &kg, const int light_index)
{
  if (kg.pipe.use_lighting_nodes) [[static_branch]] {
    LightData light = kg.lrd.light_buf[light_index];
    return is_spot_light(light.type);
  }
  return false;
}

bool node_attribute_light_is_area_impl(KernelGlobals &kg, const int light_index)
{
  if (kg.pipe.use_lighting_nodes) [[static_branch]] {
    LightData light = kg.lrd.light_buf[light_index];
    return is_area_light(light.type);
  }
  return false;
}

float node_attribute_light_cutoff_distance_impl(KernelGlobals &kg, const int light_index)
{
  if (kg.pipe.use_lighting_nodes) [[static_branch]] {
    LightData light = kg.lrd.light_buf[light_index];
    if (is_sun_light(light.type)) {
      return 1e20f;
    }
    return inversesqrt(light.local.local.influence_radius_invsqr_surface);
  }
  return 1.0f;
}

void node_light_accumulation_impl(KernelGlobals &kg,
                                  ShadingData &sd,
                                  const int light_index,
                                  float3 diffuse_light,
                                  float3 diffuse_color,
                                  float3 glossy_light,
                                  float3 glossy_color,
                                  float3 transmission_light,
                                  float3 transmission_color,
                                  float weight,
                                  Closure &result)
{
#if defined(GPU_FRAGMENT_SHADER) || defined(GLSL_CPP_STUBS)
  if (kg.pipe.use_lighting_nodes) [[static_branch]] {
    LightData light = kg.lrd.light_buf[light_index];
    float3 diffuse_color_weight = diffuse_color * weight;
    float3 glossy_color_weight = glossy_color * weight;
    float3 transmission_color_weight = transmission_color * weight;
    sd.light_accum.diffuse_light += diffuse_color_weight * diffuse_light *
                                    light.power_factor[LIGHT_DIFFUSE];
    sd.light_accum.glossy_light += glossy_color_weight * glossy_light *
                                   light.power_factor[LIGHT_SPECULAR];
    sd.light_accum.transmission_light += transmission_color_weight * transmission_light *
                                         light.power_factor[LIGHT_TRANSMISSION];
    sd.light_accum.diffuse_color += diffuse_color_weight;
    sd.light_accum.glossy_color += glossy_color_weight;
    sd.light_accum.transmission_color += transmission_color_weight;
    sd.light_accum.light_accumulation_count++;
  }
#endif

  result = Closure(0);
}

/** \} */

void output_aov(KernelGlobals &kg,
                int2 texel,
                float4 color,
                float value,
                uint hash,
                float holdout,
                eObjectInfoFlag ob_flag)
{
#if defined(GPU_FRAGMENT_SHADER) || defined(GLSL_CPP_STUBS)
  if (kg.pipe.use_aov_output) [[static_branch]] {
    const RenderBuffersInfoData &render_pass = kg.uniforms.uniform_buf.render_pass;

    uint total_len = render_pass.aovs.color_len + render_pass.aovs.value_len;

    /* Search hashes in uint4 packs with 4 comparisons to find the index of a matching AOV hash. */
    uint hash_index;
    for (hash_index = 0u; hash_index < AOV_MAX && hash_index < total_len; hash_index += 4u) {
      bool4 cmp_mask = equal(render_pass.aovs.hash[hash_index >> 2u], uint4(hash));
      if (any(cmp_mask)) {
        /* Left-reduce `cmp_mask` to find the index of the matching AOV hash. */
        hash_index += (cmp_mask[0] ? 0u : (cmp_mask[1] ? 1u : (cmp_mask[2] ? 2u : 3u)));
        break;
      }
    }

    /* If a candidate was found by hash, output to texture array layer. */
    if (hash_index < total_len) {
      /* Object holdout. */
      if (flag_test(ob_flag, OBJECT_HOLDOUT)) {
        holdout = 1.0f;
      }
      holdout = saturate(holdout);

      /* Value hashes are stored after color hashes, so the index tells us the AOV type. */
      bool is_value = hash_index >= uint(render_pass.aovs.color_len);
      uint aov_index = hash_index - (is_value ? render_pass.aovs.color_len : 0u);

      /* Apply holdout to relevant AOV type. */
      float4 out_aov = is_value ? float4(value) : color;
      out_aov *= 1.0f - holdout;

      eevee::RenderPassOutput &rp_out = kg.renderpass_out;
      if (is_value) {
        rp_out.store_value(texel, int(render_pass.value_len + aov_index), out_aov.r);
      }
      else {
        rp_out.store_color(texel, int(render_pass.color_len + aov_index), out_aov);
      }
    }
  }
#endif
}
