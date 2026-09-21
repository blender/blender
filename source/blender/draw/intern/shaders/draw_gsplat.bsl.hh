/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * Compute shader code for gsplat implementation. For draw interfaces, packing and
 * unpacking code, refer to `draw_gsplat_lib.bsl.hh`.
 *
 * References:
 *
 *  [3dgs2023]  Bernhard Kebl, Georgios Kopanas et al.
 *              3D Gaussian Splatting for Real-Time Radiance Field Rendering
 *              ACM Transactions on Graphics (SIGGRAPH 2023)
 *              https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/
 */

#pragma once

#include "draw_gsplat_lib.bsl.hh"
#include "draw_model.bsl.hh"
#include "draw_shader_shared.hh"
#include "draw_view.bsl.hh"

#include "gpu_shader_math_matrix_transform.bsl.hh"
#include "gpu_shader_math_spherical_harmonics.bsl.hh"
#include "gpu_shader_math_vector_reduce.bsl.hh"

namespace draw::gsplat {

namespace detail {

float3 scene_linear_from_bt709(float3 rgb_color)
{
  float3 c1 = rgb_color * (1.0f / 12.92f);
  float3 c2 = pow((rgb_color + 0.055f) * (1.0f / 1.055f), float3(2.4f));
  return mix(c1, c2, step(float3(0.04045f), rgb_color));
}

/** Helper to unpack 3x8b quantized sh coefficients to a float3 representation. */
float3 unpack_sh(uint3 data, ScalingRange range)
{
  return range.scale_from_unit(float3(data) / float3(255.0f));
}

/**
 * Test if a splat's projection is partially or entirely inside, or partially or
 * entirely enclosing NDC space.
 */
bool is_inside_viewport(AxisTuple axes, float3 hP, float2 viewport_size)
{
  float2 corner = 2.0f * max(abs(axes.axis_0), abs(axes.axis_1)) * 2.0f / viewport_size;
  float2 bbox_bl = hP.xy + corner;
  float2 bbox_ur = hP.xy - corner;

  constexpr float ndc_bounds = 1.0f;
  bool clip_bl = any(equal(max(bbox_bl, float2(-ndc_bounds)), float2(-ndc_bounds)));
  bool clip_ur = any(equal(min(bbox_ur, float2(ndc_bounds)), float2(ndc_bounds)));

  return !(clip_bl || clip_ur);
}

}  // namespace detail

/* Shader can skip ellipses (e.g. for point primitives) or radiance parts (e.g. for overlay). */
struct Pipeline {
  [[compilation_constant]] bool do_ellipses;
  [[compilation_constant]] bool do_radiance;
};

struct Resources {
  [[uniform(DRW_OBJ_DATA_INFO_UBO_SLOT)]] const GSplatInfos &infos;

  [[storage(0, read)]] const uint4 (&shape_data)[];
  [[storage(1, read)]] const uint4 (&radiance_data)[];
  [[storage(2, write)]] uint2 (&ellipses_comp)[];
  [[storage(3, write)]] float2 (&radiance_comp)[];

  [[push_constant]] const float2 viewport_size;
  [[push_constant]] const int resource_id;
  [[push_constant]] const int num_points;
  /* [[push_constant]] const uint inst_id; */
  /* [[push_constant]] const uint base_inst; */

  Gaussian get(uint gs_id) const
  {
    const uint4 data = shape_data[gs_id];
    return Gaussian::unpack(gs_id, data, infos.splat_mean_range, infos.splat_scale_range);
  }
};

/* Return value for `eval_splat()` below.
 * Can indicate that a splat should not be drawn. */
struct SplatData {
  AxisTuple axes;
  float3 N;
  float opacity;

  static SplatData culled()
  {
    return {.opacity = 0.0f};
  }

  bool is_culled() const
  {
    return opacity == 0.0f;
  }

  uint2 pack() const
  {
    return uint2(axes.pack(), packSnorm2x16(detail::pack_normal_octahedral(N)));
  }
};

/**
 * Evaluate projected gsplat data, perform early culling of simple features, and return
 * the required data for later geometry draws.
 */
SplatData eval_splat(Gaussian gs,
                     ObjectMatrices obj,
                     ViewMatrices view,
                     [[resource_table]] const Resources &srt)
{
  /* Is the splat behind the viewer? */
  float3 wP = obj.point_object_to_world(gs.mean);
  float3 hP = view.point_world_to_ndc(wP);
  if (view.is_perspective() && hP.z < 0.0f) {
    return SplatData::culled();
  }

  /* Discard splat if alpha undercuts 1/255. */
  if (gs.opacity < (1.0f / 255.0f)) {
    return SplatData::culled();
  }

  /* Discard splat if scale is invalid along any axis. */
  if (any(equal(gs.scale, float3(0.0f))) || any(isnan(gs.scale))) {
    return SplatData::culled();
  }

  float4x4 viewmat = view.viewmat * obj.model;
  float3x3 covmat = get_covmat(gs.rotation, gs.scale);

  /* Project Gaussian to screen-space, recovering 2D axes for a splat billboard. */
  float3 covmat_proj = project_covmat(
      gs.mean, viewmat, view.winmat, covmat, srt.viewport_size, view.is_perspective());
  AxisTuple axes = covmat_to_axes(covmat_proj);

  /* Discard splat if it is not partially/entirely inside/enclosing the viewport. */
  if (!detail::is_inside_viewport(axes, hP, srt.viewport_size)) {
    return SplatData::culled();
  }

  /* Compute and pack normal. Normal is oriented towards view, never back-facing. */
  float3 lN = get_planar_normal(gs.rotation, gs.scale);
  float3 wN = obj.normal_object_to_world(lN);
  float3 wV = view.world_incident_vector(wP);
  if (dot(wN, wV) < 0.0f) {
    wN = -wN;
  }

  return SplatData{.axes = axes, .N = wN, .opacity = gs.opacity};
}

/**
 * Evaluate radiance (spherical harmonics degrees 0-3). Templated s.a. to only
 * evaluate required degrees in different draws.
 *
 * \note Keep in sync with `pack_radiance` in `draw_cache_impl_gsplat.cc`.
 */
template<uint Degrees>
float3 eval_radiance(Gaussian gs,
                     ObjectMatrices obj,
                     ViewMatrices view,
                     [[resource_table]] const Resources &srt)
{
  /* Object-space incident view vector. */
  float3 wP = obj.point_object_to_world(gs.mean);
  float3 wV = view.world_incident_vector(wP);
  float3 lV = -normalize(transform_direction(obj.model_inverse, wV));

  /* Unpack spherical harmonic degrees 0, 1, 2, 3.
   *
   * The fun part! These require 3*16 coefficients, which are packed over
   * uint4s (i.e. 48by) using 8b per coefficient. Layout is as follows:
   *
   *  pack_0 : [xyzx][yzxy][zxyz][xyzx]
   *  pack_1 : [yzxy][zxyz][xyzx][yzxy]
   *  pack_2 : [zxyz][xyzx][yzxy][zxyz]
   *
   * The functions `detail::slice_tuple_*()` read disjointed color sets
   * across the uint4, unpacking the 6 sets of 3 bytes left to right. */
  const ScalingRange radiance_base_range = srt.infos.radiance_base_range;
  const ScalingRange radiance_sh_range = srt.infos.radiance_sh_range;
  const uint4 sh0 = srt.radiance_data[3 * gs.id];
  const uint4 sh1 = srt.radiance_data[3 * gs.id + 1];
  const uint4 sh2 = srt.radiance_data[3 * gs.id + 2];

  float3 radiance = float3(0.0f);
  if (Degrees > 0) {
    spherical_harmonics::BandL0<float3> L0 = {
        .M0 = detail::unpack_sh(detail::slice_tuple_xyzn(sh0.x), radiance_base_range)};
    radiance += L0.evaluate(lV);
  }
  if (Degrees > 1) {
    spherical_harmonics::BandL1<float3> L1 = {
        .Mn1 = detail::unpack_sh(detail::slice_tuple_xnyz(sh0.x, sh0.y), radiance_sh_range),
        .M0 = detail::unpack_sh(detail::slice_tuple_xynz(sh0.y, sh0.z), radiance_sh_range),
        .Mp1 = detail::unpack_sh(detail::slice_tuple_nxyz(sh0.z), radiance_sh_range)};
    radiance += L1.evaluate(lV);
  }
  if (Degrees > 2) {
    spherical_harmonics::BandL2<float3> L2 = {
        .Mn2 = detail::unpack_sh(detail::slice_tuple_xyzn(sh0.w), radiance_sh_range),
        .Mn1 = detail::unpack_sh(detail::slice_tuple_xnyz(sh0.w, sh1.x), radiance_sh_range),
        .M0 = detail::unpack_sh(detail::slice_tuple_xynz(sh1.x, sh1.y), radiance_sh_range),
        .Mp1 = detail::unpack_sh(detail::slice_tuple_nxyz(sh1.y), radiance_sh_range),
        .Mp2 = detail::unpack_sh(detail::slice_tuple_xyzn(sh1.z), radiance_sh_range)};
    radiance += L2.evaluate(lV);
  }
  if (Degrees > 3) {
    spherical_harmonics::BandL3<float3> L3 = {
        .Mn3 = detail::unpack_sh(detail::slice_tuple_xnyz(sh1.z, sh1.w), radiance_sh_range),
        .Mn2 = detail::unpack_sh(detail::slice_tuple_xynz(sh1.w, sh2.x), radiance_sh_range),
        .Mn1 = detail::unpack_sh(detail::slice_tuple_nxyz(sh2.x), radiance_sh_range),
        .M0 = detail::unpack_sh(detail::slice_tuple_xyzn(sh2.y), radiance_sh_range),
        .Mp1 = detail::unpack_sh(detail::slice_tuple_xnyz(sh2.y, sh2.z), radiance_sh_range),
        .Mp2 = detail::unpack_sh(detail::slice_tuple_xynz(sh2.z, sh2.w), radiance_sh_range),
        .Mp3 = detail::unpack_sh(detail::slice_tuple_nxyz(sh2.w), radiance_sh_range)};
    radiance += L3.evaluate(lV);
  }

  /* Resolve radiance as sh + 0.5, matching the implementation of [3dgs2023]. Note that our
   * conversion to linear rgb is erroneous; splat data is fitted for sRGB data. */
  float3 resolved_radiance = detail::scene_linear_from_bt709(radiance + 0.5f);
  return resolved_radiance;
}
template float3 eval_radiance<1>(Gaussian,
                                 ObjectMatrices,
                                 ViewMatrices,
                                 [[resource_table]] const Resources &);
template float3 eval_radiance<4>(Gaussian,
                                 ObjectMatrices,
                                 ViewMatrices,
                                 [[resource_table]] const Resources &);

[[compute]] [[local_size(DRW_GSPLAT_GROUP_SIZE)]] void compute_entry(
    [[resource_table]] Resources &srt,
    [[resource_table]] const Pipeline &pipeline,
    [[resource_table]] const draw::View &views,
    [[resource_table]] const draw::Model &models,
    [[global_invocation_id]] const uint3 global_id)
{
  if (global_id.x >= srt.num_points) {
    return;
  }

  /* TODO(not_mark): This does not account for instancing. */
  const ObjectMatrices obj = models.get(srt.resource_id);
  const ViewMatrices view = views.get(0);
  const Gaussian gs = srt.get(global_id.x);

  /* Splat computation and output. */
  SplatData splat = SplatData::culled();
  if (pipeline.do_ellipses) [[static_branch]] {
    splat = eval_splat(gs, obj, view, srt);
    srt.ellipses_comp[global_id.x] = splat.is_culled() ? uint2(0) : splat.pack();
  }

  /* Base/full radiance computation and output. */
  if (pipeline.do_radiance) [[static_branch]] {
    float4 radiance = float4(0.0f, 0.0f, 0.0f, 1.0f);

    /* Only compute radiance if necessary. */
    if (pipeline.do_ellipses) [[static_branch]] {
      if (!splat.is_culled()) {
        radiance.rgb = eval_radiance<4>(gs, obj, view, srt);
      }
    }
    else {
      radiance.rgb = eval_radiance<4>(gs, obj, view, srt);
    }

    /* Radiance is packed to fp16. */
    srt.radiance_comp[global_id.x] = uintBitsToFloat(
        uint2(packHalf2x16(radiance.xy), packHalf2x16(radiance.zw)));
  }
}

/* clang-format off */

PipelineCompute compute_ellipses(compute_entry, Pipeline{.do_ellipses = true, .do_radiance = false});
PipelineCompute compute_radiance(compute_entry, Pipeline{.do_ellipses = false, .do_radiance = true});
PipelineCompute compute_ellipses_radiance(compute_entry, Pipeline{.do_ellipses = true, .do_radiance = true});

/* clang-format on */

}  // namespace draw::gsplat
