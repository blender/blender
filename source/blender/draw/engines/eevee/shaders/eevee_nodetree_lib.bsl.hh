/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_common_infos.hh"
#include "infos/eevee_geom_infos.hh"

#include "draw_intersect_lib.glsl"
#include "draw_model.bsl.hh"
#include "draw_view.bsl.hh"
#include "eevee_bxdf_lut_lib.bsl.hh"
#include "eevee_hiz.bsl.hh"
#include "eevee_nodetree_closures_lib.glsl"
#include "eevee_pipeline.bsl.hh"
#include "eevee_ray_trace_screen_lib.bsl.hh"
#include "eevee_renderpass.bsl.hh"
#include "eevee_sampling_lib.bsl.hh"
#include "eevee_uniform.bsl.hh"
#include "eevee_utility_tx.bsl.hh"
#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_safe_lib.glsl"
#include "gpu_shader_math_vector_reduce_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

uint resource_id_get()
{
  /* clang-format off */ /* Multiline macro mess the shader log line. */
  [[resource_table]] const eevee::PipelineConstants &pipe = resource_table_get(eevee::PipelineConstants);
  /* clang-format on */
  auto &interp_flat = interface_get(eevee_geom_iface_info, interp_flat);
  draw::ID id{interp_flat.resource_id_raw};
  if (pipe.is_shadow_pipe) {
    return id.resource_id<64>();
  }
  return id.resource_id<1>();
}

uint view_id_get()
{
  /* clang-format off */ /* Multiline macro mess the shader log line. */
  [[resource_table]] const eevee::PipelineConstants &pipe = resource_table_get(eevee::PipelineConstants);
  /* clang-format on */
  auto &interp_flat = interface_get(eevee_geom_iface_info, interp_flat);
  draw::ID id{interp_flat.resource_id_raw};
  if (pipe.is_shadow_pipe) {
    return id.view_id<64>();
  }
  return id.view_id<1>();
}

ObjectMatrices object_matrices_get()
{
  [[resource_table]] const draw::Model &models = resource_table_get(draw::Model);
  return models.get(resource_id_get());
}

ObjectInfos object_infos_get()
{
  [[resource_table]] const draw::Infos &infos = resource_table_get(draw::Infos);
  return infos.get(resource_id_get());
}

ViewMatrices view_matrices_get()
{
  [[resource_table]] const draw::View &views = resource_table_get(draw::View);
  return views.get(view_id_get());
}

#define closure_base_copy(cl, in_cl) \
  cl.weight = in_cl.weight; \
  cl.color = in_cl.color; \
  cl.N = in_cl.N; \
  cl.type = closure_type_get(in_cl);

/* Single BSDFs. */
Closure closure_eval(ClosureDiffuse diffuse)
{
  ClosureUndetermined cl;
  closure_base_copy(cl, diffuse);
#if (CLOSURE_BIN_COUNT > 1) && defined(MAT_TRANSLUCENT) && !defined(MAT_CLEARCOAT)
  /* Use second slot so we can have diffuse + translucent without noise. */
  closure_select(g_closure_bins[1], g_closure_rand[1], cl);
#else
  /* Either is single closure or use same bin as transmission bin. */
  closure_select(g_closure_bins[0], g_closure_rand[0], cl);
#endif
  return Closure(0);
}

Closure closure_eval(ClosureSubsurface diffuse)
{
  ClosureUndetermined cl;
  closure_base_copy(cl, diffuse);
  cl.data.rgb = diffuse.sss_radius;
  /* Transmission Closures are always in first bin. */
  closure_select(g_closure_bins[0], g_closure_rand[0], cl);
  return Closure(0);
}

Closure closure_eval(ClosureTranslucent translucent)
{
  ClosureUndetermined cl;
  closure_base_copy(cl, translucent);
  /* Transmission Closures are always in first bin. */
  closure_select(g_closure_bins[0], g_closure_rand[0], cl);
  return Closure(0);
}

/* Alternate between two bins on a per closure basis.
 * Allow clearcoat layer without noise.
 * Choosing the bin with the least weight can choose a
 * different bin for the same closure and
 * produce issue with ray-tracing denoiser.
 * Always start with the second bin, this one doesn't
 * overlap with other closure. */
bool g_closure_reflection_bin = true;
#define CHOOSE_MIN_WEIGHT_CLOSURE_BIN(a, b) \
  if (g_closure_reflection_bin) { \
    closure_select(g_closure_bins[b], g_closure_rand[b], cl); \
  } \
  else { \
    closure_select(g_closure_bins[a], g_closure_rand[a], cl); \
  } \
  g_closure_reflection_bin = !g_closure_reflection_bin;

Closure closure_eval(ClosureReflection reflection)
{
  ClosureUndetermined cl;
  closure_base_copy(cl, reflection);
  cl.data.r = reflection.roughness;

#ifdef MAT_CLEARCOAT
#  if CLOSURE_BIN_COUNT == 2
  /* Multiple reflection closures. */
  CHOOSE_MIN_WEIGHT_CLOSURE_BIN(0, 1);
#  elif CLOSURE_BIN_COUNT == 3
  /* Multiple reflection closures and one other closure. */
  CHOOSE_MIN_WEIGHT_CLOSURE_BIN(1, 2);
#  else
#    error Clearcoat should always have at least 2 bins
#  endif
#else
#  if CLOSURE_BIN_COUNT == 1
  /* Only one reflection closure is present in the whole tree. */
  closure_select(g_closure_bins[0], g_closure_rand[0], cl);
#  elif CLOSURE_BIN_COUNT == 2
  /* Only one reflection and one other closure. */
  closure_select(g_closure_bins[1], g_closure_rand[1], cl);
#  elif CLOSURE_BIN_COUNT == 3
  /* Only one reflection and two other closures. */
  closure_select(g_closure_bins[2], g_closure_rand[2], cl);
#  endif
#endif

#undef CHOOSE_MIN_WEIGHT_CLOSURE_BIN

  return Closure(0);
}

Closure closure_eval(ClosureRefraction refraction)
{
  ClosureUndetermined cl;
  closure_base_copy(cl, refraction);
  cl.data.r = refraction.roughness;
  cl.data.g = refraction.ior;
  /* Transmission Closures are always in first bin. */
  closure_select(g_closure_bins[0], g_closure_rand[0], cl);
  return Closure(0);
}

Closure closure_eval(ClosureThinRefraction refraction)
{
  ClosureUndetermined cl;
  closure_base_copy(cl, refraction);
  cl.data.r = refraction.roughness;
  /* Transmission Closures are always in first bin. */
  closure_select(g_closure_bins[0], g_closure_rand[0], cl);
  return Closure(0);
}

Closure closure_eval(ClosureEmission emission)
{
  g_emission += emission.emission * emission.weight;
  return Closure(0);
}

Closure closure_eval(ClosureTransparency transparency)
{
  g_transmittance += transparency.transmittance * transparency.weight;
  g_holdout += transparency.holdout * transparency.weight;
  return Closure(0);
}

Closure closure_eval(ClosureVolumeScatter volume_scatter)
{
  g_volume_scattering += volume_scatter.scattering * volume_scatter.weight;
  g_volume_anisotropy += volume_scatter.anisotropy * volume_scatter.weight;
  return Closure(0);
}

Closure closure_eval(ClosureVolumeAbsorption volume_absorption)
{
  g_volume_absorption += volume_absorption.absorption * volume_absorption.weight;
  return Closure(0);
}

Closure closure_eval(ClosureHair /*hair*/)
{
  /* TODO */
  return Closure(0);
}

/* Glass BSDF. */
Closure closure_eval(ClosureReflection reflection, ClosureRefraction refraction)
{
  closure_eval(reflection);
  closure_eval(refraction);
  return Closure(0);
}

/* Dielectric BSDF. */
Closure closure_eval(ClosureDiffuse diffuse, ClosureReflection reflection)
{
  closure_eval(diffuse);
  closure_eval(reflection);
  return Closure(0);
}

/* Coat BSDF. */
Closure closure_eval(ClosureReflection reflection, ClosureReflection coat)
{
  closure_eval(reflection);
  closure_eval(coat);
  return Closure(0);
}

/* Volume BSDF. */
Closure closure_eval(ClosureVolumeScatter volume_scatter,
                     ClosureVolumeAbsorption volume_absorption,
                     ClosureEmission emission)
{
  closure_eval(volume_scatter);
  closure_eval(volume_absorption);
  closure_eval(emission);
  return Closure(0);
}

/* Specular BSDF. */
Closure closure_eval(ClosureDiffuse diffuse, ClosureReflection reflection, ClosureReflection coat)
{
  closure_eval(diffuse);
  closure_eval(reflection);
  closure_eval(coat);
  return Closure(0);
}

/* Principled BSDF. */
Closure closure_eval(ClosureDiffuse diffuse,
                     ClosureReflection reflection,
                     ClosureReflection coat,
                     ClosureRefraction refraction)
{
  closure_eval(diffuse);
  closure_eval(reflection);
  closure_eval(coat);
  closure_eval(refraction);
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

float ambient_occlusion_eval([[maybe_unused]] float3 normal,
                             [[maybe_unused]] float max_distance,
                             [[maybe_unused]] const float inverted,
                             [[maybe_unused]] const float sample_count)
{
  FRAGMENT_SHADER_CREATE_INFO(draw_view);

  /* clang-format off */ /* Multiline macros would break line count. */
  [[resource_table]] [[maybe_unused]] const eevee::Sampling &samp = resource_table_get(eevee::Sampling);
  [[resource_table]] [[maybe_unused]] const UtilityTexture &util_tx = resource_table_get(UtilityTexture);
  [[resource_table]] [[maybe_unused]] const eevee::HiZ &hiz = resource_table_get(eevee::HiZ);
  [[resource_table]] [[maybe_unused]] const eevee::Uniform &uni = resource_table_get(eevee::Uniform);
  [[resource_table]] [[maybe_unused]] const draw::View &views = resource_table_get(draw::View);
  /* clang-format on */

  {
#if defined(GPU_FRAGMENT_SHADER) && defined(MAT_AMBIENT_OCCLUSION) && !defined(MAT_DEPTH) && \
    !defined(MAT_SHADOW)

    const ViewMatrices view = views.get(0);

    float3 vP = view.point_world_to_view(g_data.P);
    float3 vN = view.normal_world_to_view(normal);

    int2 texel = int2(gl_FragCoord.xy);
    float4 noise = util_tx.fetch(float2(texel), UTIL_BLUE_NOISE_LAYER);
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
#else
    return 1.0f;
#endif
  }
}

void raycast_eval([[maybe_unused]] float3 position,
                  float3 direction,
                  float max_distance,
                  [[maybe_unused]] bool self_only,
                  bool &is_hit,
                  bool &self_hit,
                  float &hit_distance,
                  float3 &hit_position,
                  float3 &hit_normal)
{
  [[resource_table]] const eevee::Uniform &uni = resource_table_get(eevee::Uniform);

  is_hit = false;
  self_hit = false;
  hit_distance = max_distance;
  hit_position = float3(0.0f);
  hit_normal = float3(0.0f);

  direction = normalize(direction);

#if defined(MAT_RAYCAST)
  if (!uni.pipeline_buf.can_raycast) {
    /* We can't ray-cast on pre-pass for ray-cast visible objects.
     * We use a UBO property to avoid compiling more shader variants. */
    return;
  }

  float3 ws_start = position;
  float3 ws_end = position + direction * max_distance;
  if (!clip_ray(
          ws_start, ws_end, direction, max_distance, drw_view_culling().frustum_planes.planes))
  {
    return;
  }

  {
    FRAGMENT_SHADER_CREATE_INFO(draw_view);

    [[resource_table]] const draw::View &views = resource_table_get(draw::View);
    [[resource_table]] const eevee::Sampling &samp = resource_table_get(eevee::Sampling);
    const auto &raycast_depth_tx = sampler_get(eevee_raycast, raycast_depth_tx);
    const auto &prepass_normal_tx = sampler_get(eevee_raycast, prepass_normal_tx);
    const auto &object_id_tx = sampler_get(eevee_raycast, object_id_tx);

    const ViewMatrices view = views.get(0);

    {
      /* Offset the start to prevent wrong intersection due to depth precision. */
      float3 vs_start = view.point_world_to_view(ws_start);
      float start_depth = view.depth_view_to_screen(vs_start.z);
      float offset_depth = uintBitsToFloat(floatBitsToUint(start_depth) + 2);
      float offset_delta = abs(view.depth_screen_to_view(offset_depth) - vs_start.z);
      ws_start += direction * offset_delta;
    }

    float noise_offset = samp.rng_1D_get(SAMPLING_RAYTRACE_W);
    float jitter = interleaved_gradient_noise(gl_FragCoord.xy, 1.0f, noise_offset);

    float2 hit_uv = float2(0.0f);
    uint self_id = resource_id_get() & uint(0xFFFF);

    float result = raytrace_screen_2(views.get(0),
                                     view.point_world_to_view(ws_start),
                                     view.point_world_to_view(ws_end),
                                     view.normal_world_to_view(direction),
                                     raycast_depth_tx,
                                     uni.raytrace_buf,
                                     64,
                                     jitter,
                                     object_id_tx,
                                     self_only ? self_id : 0,
                                     hit_uv);
    if (result >= 0.0f) {
      is_hit = true;
      hit_position = ws_start + direction * result;
      hit_distance = distance(position, hit_position);
      hit_normal = normalize(texture(prepass_normal_tx, hit_uv).xyz * 2.0f - 1.0f);
      int2 hit_texel = int2(hit_uv * float2(textureSize(object_id_tx, 0)));
      uint hit_id = texelFetch(object_id_tx, hit_texel, 0).x;
      self_hit = self_only || (hit_id == self_id);
    }
  }
#endif
}

#ifndef GPU_METAL
Closure nodetree_surface(float closure_rand);
Closure nodetree_volume();
float3 nodetree_displacement();
float nodetree_thickness();
float4 closure_to_rgba(Closure cl);
#endif

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

void brdf_f82_tint_lut(float3 F0,
                       float3 F82,
                       float cos_theta,
                       float roughness,
                       bool do_multiscatter,
                       float3 &reflectance)
{
  [[resource_table]] const UtilityTexture util_tx = resource_table_get(UtilityTexture);
  eevee::lut::GGXBrdfData lut = eevee::lut::GGXBrdfData::sample_utility_tx(
      util_tx, cos_theta, roughness);

  reflectance = do_multiscatter ? F_brdf_multi_scatter(F0, float3(1.0f), lut) :
                                  F_brdf_single_scatter(F0, float3(1.0f), lut);

  /* Precompute the F82 term factor for the Fresnel model.
   * In the classic F82 model, the F82 input directly determines the value of the Fresnel
   * model at ~82 degrees, similar to F0 and F90.
   * With F82-Tint, on the other hand, the value at 82 degrees is the value of the classic Schlick
   * model multiplied by the tint input.
   * Therefore, the factor follows by setting `F82Tint(cosI) = FSchlick(cosI) - b*cosI*(1-cosI)^6`
   * and `F82Tint(acos(1/7)) = FSchlick(acos(1/7)) * f82_tint` and solving for `b`. */
  constexpr float f = 6.0f / 7.0f;
  constexpr float f5 = (f * f) * (f * f) * f;
  constexpr float f6 = (f * f) * (f * f) * (f * f);
  float3 F_schlick = mix(F0, float3(1.0f), f5);
  float3 b = F_schlick * (7.0f / f6) * (1.0f - F82);
  reflectance -= b * lut.metal_bias;
}

/* Computes the reflectance and transmittance based on the tint (`f0`, `f90`, `transmission_tint`)
 * and the BSDF LUT. */
void bsdf_lut(float3 F0,
              float3 F90,
              float3 transmission_tint,
              float cos_theta,
              float roughness,
              float ior,
              bool do_multiscatter,
              float3 &reflectance,
              float3 &transmittance)
{
  [[resource_table]] const UtilityTexture util_tx = resource_table_get(UtilityTexture);
  if (ior == 1.0f) {
    reflectance = float3(0.0f);
    transmittance = transmission_tint;
    return;
  }

  /* TODO(not_mark): strip namespaces on BSL port. */
  eevee::lut::GGXBsdfData lut;

  const float f0 = f0_from_ior(ior);

  if (ior > 1.0f) {
    /* Gradually increase `f90` from 0 to 1 when IOR is in the range of [1.0f, 1.33f], to avoid
     * harsh transition at `IOR == 1`. */
    if (all(equal(F90, float3(1.0f)))) {
      F90 = float3(saturate(2.33f / 0.33f * f0));
    }

    eevee::lut::GGXBrdfData brdf_lut = eevee::lut::GGXBrdfData::sample_utility_tx(
        util_tx, cos_theta, roughness);
    eevee::lut::GGXBtdfGt1Data btdf_lut = eevee::lut::GGXBtdfGt1Data::sample_utility_tx(
        util_tx, cos_theta, roughness, f0);

    lut.scale = brdf_lut.scale;
    lut.bias = brdf_lut.bias;
    lut.transmission_factor = btdf_lut.transmission_factor;
  }
  else {
    lut = eevee::lut::GGXBsdfData::sample_utility_tx(util_tx, cos_theta, roughness, ior);
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
float2 bsdf_lut(float cos_theta, float roughness, float ior, bool do_multiscatter)
{
  float F0 = F0_from_ior(ior);
  float3 color = float3(1.0f);
  float3 reflectance, transmittance;
  bsdf_lut(float3(F0),
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

/* -------------------------------------------------------------------- */
/** \name Fragment Displacement
 *
 * Displacement happening in the fragment shader.
 * Can be used in conjunction with a per vertex displacement.
 *
 * \{ */

#ifndef GPU_METAL
/* Prototype. */
float derivative_scale_get();
#endif

#ifdef MAT_DISPLACEMENT_BUMP
/* Return new shading normal. */
float3 displacement_bump()
{
#  if !defined(MAT_GEOM_CURVES)
  /* This is the filter width for automatic displacement + bump mapping, which is fixed.
   * NOTE: keep the same as default bump node filter width. */
  constexpr float bump_filter_width = 0.1f;

  float2 dHd;
  dF_branch(dot(nodetree_displacement(), g_data.N + dF_impl(g_data.N)), bump_filter_width, dHd);

  float3 dPdx = gpu_dfdx(g_data.P) * derivative_scale_get();
  float3 dPdy = gpu_dfdy(g_data.P) * derivative_scale_get();

  /* Get surface tangents from normal. */
  float3 Rx = cross(dPdy, g_data.N);
  float3 Ry = cross(g_data.N, dPdx);

  /* Compute surface gradient and determinant. */
  float det = dot(dPdx, Rx);

  float3 surfgrad = dHd.x * Rx + dHd.y * Ry;

  float facing = FrontFacing ? 1.0f : -1.0f;
  return normalize(bump_filter_width * abs(det) * g_data.N - facing * sign(det) * surfgrad);
#  else
  return g_data.N;
#  endif
}
#endif

void fragment_displacement()
{
#ifdef MAT_DISPLACEMENT_BUMP
  g_data.N = displacement_bump();
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Coordinate implementations
 *
 * Callbacks for the texture coordinate node.
 *
 * \{ */

float3 coordinate_camera(float3 P)
{
  float3 vP;
  if (false /* Probe. */) {
    /* Unsupported. It would make the probe camera-dependent. */
    vP = P;
  }
  else {
    const ViewMatrices view = view_matrices_get();
#ifdef MAT_GEOM_WORLD
    vP = view.normal_world_to_view(P);
#else
    vP = view.point_world_to_view(P);
#endif
  }
  vP.z = -vP.z;
  return vP;
}

float3 coordinate_screen(float3 P)
{
  [[resource_table]] const eevee::Uniform &uni = resource_table_get(eevee::Uniform);
  float3 window = float3(0.0f);
  if (false /* Probe. */) {
    /* Unsupported. It would make the probe camera-dependent. */
    window.xy = float2(0.5f);
  }
  else {
    const ViewMatrices view = view_matrices_get();
#ifdef MAT_GEOM_WORLD
    window.xy = view.point_view_to_screen(interp.P).xy;
#else
    /* TODO(fclem): Actual camera transform. */
    window.xy = view.point_world_to_screen(P).xy;
#endif
    window.xy = window.xy * uni.uniform_buf.camera.uv_scale + uni.uniform_buf.camera.uv_bias;
  }
  return window;
}

float3 coordinate_reflect(float3 P, float3 N)
{
#ifdef MAT_GEOM_WORLD
  return N;
#else
  const ViewMatrices view = view_matrices_get();
  return -reflect(view.world_incident_vector(P), N);
#endif
}

float3 coordinate_incoming(float3 P)
{
#ifdef MAT_GEOM_WORLD
  return -P;
#else
  const ViewMatrices view = view_matrices_get();
  return view.world_incident_vector(P);
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mixed render resolution
 *
 * Callbacks image texture sampling.
 *
 * \{ */

float texture_lod_bias_get()
{
  [[resource_table]] const eevee::Uniform &uni = resource_table_get(eevee::Uniform);
  return uni.uniform_buf.film.texture_lod_bias;
}

/**
 * Scale hardware derivatives depending on render resolution.
 * This is because the distance between pixels increases as we lower the resolution. The hardware
 * uses neighboring pixels to compute derivatives and thus the value increases as we lower the
 * resolution. So we compensate by scaling them back to the expected amplitude at full resolution.
 */
float derivative_scale_get()
{
  [[resource_table]] const eevee::Uniform &uni = resource_table_get(eevee::Uniform);
  return 1.0f / float(uni.uniform_buf.film.scaling_factor);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume Attribute post
 *
 * TODO(@fclem): These implementation details should concern the DRWContext and not be a fix on
 * the engine side. But as of now, the engines are responsible for loading the attributes.
 *
 * \{ */

/* Point clouds and curves are not compatible with volume grids.
 * They will fall back to their own attributes loading. */
#if defined(MAT_VOLUME) && !defined(MAT_GEOM_CURVES) && !defined(MAT_GEOM_POINTCLOUD)
#  if defined(VOLUME_INFO_LIB) && !defined(MAT_GEOM_WORLD)
/* We could just check for GRID_ATTRIBUTES but this avoids for header dependency. */
#    define GRID_ATTRIBUTES_LOAD_POST
#  endif
#endif

float attr_load_temperature_post(float attr)
{
#ifdef GRID_ATTRIBUTES_LOAD_POST
  /* Bring the value into standard range without having to modify the grid values */
  attr = (attr > 0.01f) ? (attr * drw_volume.temperature_mul + drw_volume.temperature_bias) : 0.0f;
#endif
  return attr;
}
float4 attr_load_color_post(float4 attr)
{
#ifdef GRID_ATTRIBUTES_LOAD_POST
  /* Density is premultiplied for interpolation, divide it out here. */
  attr.rgb *= safe_rcp(attr.a);
  attr.rgb *= drw_volume.color_mul.rgb;
  attr.a = 1.0f;
#endif
  return attr;
}

#undef GRID_ATTRIBUTES_LOAD_POST

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniform Attributes
 *
 * TODO(@fclem): These implementation details should concern the DRWContext and not be a fix on
 * the engine side. But as of now, the engines are responsible for loading the attributes.
 *
 * \{ */

float4 attr_load_uniform(float4 /*attr*/, const uint attr_hash)
{
  const auto &attrs_buf = buffer_get(draw_object_attributes, drw_attrs);

  ObjectInfos infos = object_infos_get();
  uint index = infos.object_attrs_offset;
  for (uint i = 0; i < infos.object_attrs_len; i++, index++) {
    ObjectAttribute attr = attrs_buf[index];
    if (attr.hash_code == attr_hash) {
      return float4(attr.data_x, attr.data_y, attr.data_z, attr.data_w);
    }
  }
  return float4(0.0f);
}

void scene_time_uniforms(float &seconds, float &frame)
{
  [[resource_table]] const eevee::Uniform &uni = resource_table_get(eevee::Uniform);

  seconds = uni.uniform_buf.scene.time;
  frame = uni.uniform_buf.scene.frame;
}

/** \} */
