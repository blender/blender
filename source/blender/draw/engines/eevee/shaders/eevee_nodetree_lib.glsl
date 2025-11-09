/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_common_infos.hh"
#include "infos/eevee_uniform_infos.hh"

SHADER_LIBRARY_CREATE_INFO(eevee_global_ubo)
SHADER_LIBRARY_CREATE_INFO(eevee_utility_texture)

#include "draw_model_lib.glsl"
#include "draw_object_infos_lib.glsl"
#include "draw_view_lib.glsl"
#include "eevee_nodetree_closures_lib.glsl"
#include "eevee_renderpass_lib.glsl"
#include "eevee_utility_tx_lib.glsl"
#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_safe_lib.glsl"
#include "gpu_shader_math_vector_reduce_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

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

Closure closure_eval(ClosureHair hair)
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
Closure closure_add(Closure cl1, Closure cl2)
{
  return Closure(0);
}
Closure closure_mix(Closure cl1, Closure cl2, float fac)
{
  return Closure(0);
}

float ambient_occlusion_eval(float3 normal,
                             float max_distance,
                             const float inverted,
                             const float sample_count)
{
  /* Avoid multi-line pre-processor conditionals.
   * Some drivers don't handle them correctly. */
  // clang-format off
#if defined(GPU_FRAGMENT_SHADER) && defined(MAT_AMBIENT_OCCLUSION) && !defined(MAT_DEPTH) && !defined(MAT_SHADOW)
  // clang-format on
#  if 0 /* TODO(fclem): Finish inverted horizon scan. */
  /* TODO(fclem): Replace eevee_ambient_occlusion_lib by eevee_horizon_scan_eval_lib when this is
   * finished. */
  float3 vP = drw_point_world_to_view(g_data.P);
  float3 vN = drw_normal_world_to_view(normal);

  int2 texel = int2(gl_FragCoord.xy);
  float2 noise;
  noise.x = interleaved_gradient_noise(float2(texel), 3.0f, 0.0f);
  noise.y = utility_tx_fetch(utility_tx, float2(texel), UTIL_BLUE_NOISE_LAYER).r;
  noise = fract(noise + sampling_rng_2D_get(SAMPLING_AO_U));

  ClosureOcclusion occlusion;
  occlusion.N = (inverted != 0.0f) ? -vN : vN;

  HorizonScanContext ctx;
  ctx.occlusion = occlusion;

  horizon_scan_eval(vP,
                    ctx,
                    noise,
                    uniform_buf.ao.pixel_size,
                    max_distance,
                    uniform_buf.ao.thickness_near,
                    uniform_buf.ao.thickness_far,
                    uniform_buf.ao.angle_bias,
                    2,
                    10,
                    inverted != 0.0f,
                    true);

  return saturate(ctx.occlusion_result.r);
#  else
  float3 vP = drw_point_world_to_view(g_data.P);
  int2 texel = int2(gl_FragCoord.xy);
  OcclusionData data = ambient_occlusion_search(
      vP, hiz_tx, texel, max_distance, inverted, sample_count);

  float3 V = drw_world_incident_vector(g_data.P);
  float3 N = normal;
  float3 Ng = g_data.Ng;

  float unused_error, visibility;
  float3 unused;
  ambient_occlusion_eval(data, texel, V, N, Ng, inverted, visibility, unused_error, unused);
  return visibility;
#  endif
#else
  return 1.0f;
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

/* Return the fresnel color from a precomputed LUT value (from brdf_lut). */
float3 F_brdf_single_scatter(float3 f0, float3 f90, float2 lut)
{
  return f0 * lut.x + f90 * lut.y;
}

/* Multi-scattering brdf approximation from
 * "A Multiple-Scattering Microfacet Model for Real-Time Image-based Lighting"
 * https://jcgt.org/published/0008/01/03/paper.pdf by Carmelo J. Fdez-Agüera. */
float3 F_brdf_multi_scatter(float3 f0, float3 f90, float2 lut)
{
  float3 FssEss = F_brdf_single_scatter(f0, f90, lut);

  float Ess = lut.x + lut.y;
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

float2 brdf_lut(float cos_theta, float roughness)
{
  auto &utility_tx = sampler_get(eevee_utility_texture, utility_tx);
  return utility_tx_sample_lut(utility_tx, cos_theta, roughness, UTIL_BSDF_LAYER).rg;
}

void brdf_f82_tint_lut(float3 F0,
                       float3 F82,
                       float cos_theta,
                       float roughness,
                       bool do_multiscatter,
                       out float3 reflectance)
{
  auto &utility_tx = sampler_get(eevee_utility_texture, utility_tx);
  float3 split_sum = utility_tx_sample_lut(utility_tx, cos_theta, roughness, UTIL_BSDF_LAYER).rgb;

  reflectance = do_multiscatter ? F_brdf_multi_scatter(F0, float3(1.0f), split_sum.xy) :
                                  F_brdf_single_scatter(F0, float3(1.0f), split_sum.xy);

  /* Precompute the F82 term factor for the Fresnel model.
   * In the classic F82 model, the F82 input directly determines the value of the Fresnel
   * model at ~82°, similar to F0 and F90.
   * With F82-Tint, on the other hand, the value at 82° is the value of the classic Schlick
   * model multiplied by the tint input.
   * Therefore, the factor follows by setting `F82Tint(cosI) = FSchlick(cosI) - b*cosI*(1-cosI)^6`
   * and `F82Tint(acos(1/7)) = FSchlick(acos(1/7)) * f82_tint` and solving for `b`. */
  constexpr float f = 6.0f / 7.0f;
  constexpr float f5 = (f * f) * (f * f) * f;
  constexpr float f6 = (f * f) * (f * f) * (f * f);
  float3 F_schlick = mix(F0, float3(1.0f), f5);
  float3 b = F_schlick * (7.0f / f6) * (1.0f - F82);
  reflectance -= b * split_sum.z;
}

/* Return texture coordinates to sample BSDF LUT. */
float3 lut_coords_bsdf(float cos_theta, float roughness, float ior)
{
  /* IOR is the sine of the critical angle. */
  float critical_cos = sqrt(1.0f - ior * ior);

  float3 coords;
  coords.x = square(ior);
  coords.y = cos_theta;
  coords.y -= critical_cos;
  coords.y /= (coords.y > 0.0f) ? (1.0f - critical_cos) : critical_cos;
  coords.y = coords.y * 0.5f + 0.5f;
  coords.z = roughness;

  return saturate(coords);
}

/* Return texture coordinates to sample Surface LUT. */
float3 lut_coords_btdf(float cos_theta, float roughness, float f0)
{
  return float3(sqrt(f0), sqrt(1.0f - cos_theta), roughness);
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
              out float3 reflectance,
              out float3 transmittance)
{
  auto &utility_tx = sampler_get(eevee_utility_texture, utility_tx);
  if (ior == 1.0f) {
    reflectance = float3(0.0f);
    transmittance = transmission_tint;
    return;
  }

  float2 split_sum;
  float transmission_factor;

  const float f0 = f0_from_ior(ior);

  if (ior > 1.0f) {
    /* Gradually increase `f90` from 0 to 1 when IOR is in the range of [1.0f, 1.33f], to avoid
     * harsh transition at `IOR == 1`. */
    if (all(equal(F90, float3(1.0f)))) {
      F90 = float3(saturate(2.33f / 0.33f * f0));
    }
    const float3 coords = lut_coords_btdf(cos_theta, roughness, f0);
    const float4 bsdf = utility_tx_sample_bsdf_lut(utility_tx, coords.xy, coords.z);
    split_sum = brdf_lut(cos_theta, roughness);
    transmission_factor = bsdf.a;
  }
  else {
    const float3 coords = lut_coords_bsdf(cos_theta, roughness, ior);
    const float3 bsdf = utility_tx_sample_bsdf_lut(utility_tx, coords.xy, coords.z).rgb;
    split_sum = bsdf.rg;
    transmission_factor = bsdf.b;
  }

  reflectance = F_brdf_single_scatter(F0, F90, split_sum);
  transmittance = (float3(1.0f) - F0) * transmission_factor * transmission_tint;

  if (do_multiscatter) {
    const float real_F0 = F0_from_f0(f0);
    const float Ess = real_F0 * split_sum.x + split_sum.y + (1.0f - real_F0) * transmission_factor;
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

#ifdef GPU_VERTEX_SHADER
#  define closure_to_rgba(a) float4(0.0f)
#endif

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
  g_data.N = g_data.Ni = displacement_bump();
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
#ifdef MAT_GEOM_WORLD
    vP = drw_normal_world_to_view(P);
#else
    vP = drw_point_world_to_view(P);
#endif
  }
  vP.z = -vP.z;
  return vP;
}

float3 coordinate_screen(float3 P)
{
  float3 window = float3(0.0f);
  if (false /* Probe. */) {
    /* Unsupported. It would make the probe camera-dependent. */
    window.xy = float2(0.5f);
  }
  else {
#ifdef MAT_GEOM_WORLD
    window.xy = drw_point_view_to_screen(interp.P).xy;
#else
    /* TODO(fclem): Actual camera transform. */
    window.xy = drw_point_world_to_screen(P).xy;
#endif
    window.xy = window.xy * uniform_buf.camera.uv_scale + uniform_buf.camera.uv_bias;
  }
  return window;
}

float3 coordinate_reflect(float3 P, float3 N)
{
#ifdef MAT_GEOM_WORLD
  return N;
#else
  return -reflect(drw_world_incident_vector(P), N);
#endif
}

float3 coordinate_incoming(float3 P)
{
#ifdef MAT_GEOM_WORLD
  return -P;
#else
  return drw_world_incident_vector(P);
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
  return uniform_buf.film.texture_lod_bias;
}

/**
 * Scale hardware derivatives depending on render resolution.
 * This is because the distance between pixels increases as we lower the resolution. The hardware
 * uses neighboring pixels to compute derivatives and thus the value increases as we lower the
 * resolution. So we compensate by scaling them back to the expected amplitude at full resolution.
 */
float derivative_scale_get()
{
  return 1.0 / float(uniform_buf.film.scaling_factor);
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

float4 attr_load_uniform(float4 attr, const uint attr_hash)
{
  return drw_object_attribute(attr_hash);
}

/** \} */
