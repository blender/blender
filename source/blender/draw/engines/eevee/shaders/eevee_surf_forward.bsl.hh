/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Forward lighting evaluation: Lighting is evaluated during the geometry rasterization.
 *
 * This is used by alpha blended materials and materials using Shader to RGB nodes.
 */
#pragma once

#include "draw_view.bsl.hh"
#include "eevee_forward_lib.bsl.hh"
#include "eevee_nodetree_frag_lib.bsl.hh"
#include "eevee_reverse_z_lib.bsl.hh"
#include "eevee_sampling_lib.bsl.hh"
#include "eevee_surf_common.bsl.hh"
#include "eevee_volume_lib.bsl.hh"

float4 closure_to_rgba_forward(KernelGlobals &kg, ShadingData &sd, Closure /*cl_unused*/)
{
  [[resource_table]] const eevee::PipelineConstants &pipe = kg.pipe;
  if (!pipe.is_occupancy_pipe) [[static_branch]] {
    [[resource_table]] const draw::View &views = kg.view;
    [[resource_table]] const eevee::Sampling &sampling = kg.sampling;
    [[resource_table]] const UtilityTexture &util_tx = kg.util_tx;
    draw::ID id{sd.resource_id_raw};
    const uint resource_id = id.resource_id<1>();

    const float2 frag_co = sd.frag_co.xy;

    float3 radiance, transmittance;
    eevee::forward_lighting_eval(
        kg, sd, views.get(0), resource_id, sd.thickness, frag_co, radiance, transmittance);

    /* Reset for the next closure tree. */
    float noise = util_tx.fetch(frag_co, UTIL_BLUE_NOISE_LAYER).r;
    float closure_rand = fract(noise + sampling.rng_1D_get(SAMPLING_CLOSURE));
    closure_weights_reset(kg, sd, closure_rand);

    if (pipe.use_forward_lighting && pipe.use_transparency) [[static_branch]] {
      [[resource_table]] eevee::LightprobeRenderData &lightprobes = kg.lightprobes;
      [[resource_table]] eevee::LightprobeSphereRenderData &lp_spheres = lightprobes.spheres;
      [[resource_table]] eevee::PreviousLayerHiZ &prev_hiz = kg.previous_layer_hiz;
      [[resource_table]] eevee::PreviousLayerRadiance &prev_radiance = kg.previous_layer_radiance;

      float3 V = -views.get(0).world_incident_vector(sd.P);
      eevee::LightProbeSample samp = lightprobes.load(frag_co, sd.P, sd.N, V);
      float3 radiance_behind = lp_spheres.spherical_sample_normalized_with_parallax(
          samp, sd.P, V, 0.0);

      int2 texel = int2(frag_co);
      if (texelFetchExtend(prev_hiz.hiz_prev_tx, texel, 0).x != 1.0f) {
        radiance_behind = texelFetch(prev_radiance.previous_layer_radiance_tx, texel, 0).xyz;
      }

      radiance += radiance_behind * saturate(transmittance);
    }

    return float4(radiance, saturate(1.0f - average(transmittance)));
  }
  else {
    return float4(0);
  }
}

namespace eevee {

struct SurfaceForwardFragOut {
  /* Splitting RGB components into different target to overcome the lack of dual source blending
   * with multiple render targets. */
  [[frag_color(0)]] float4 combined_r;
  [[frag_color(1)]] float4 combined_g;
  [[frag_color(2)]] float4 combined_b;
  [[frag_color(3)]] float4 combined_a;
};

/* Early fragment test is needed for render passes support for forward surfaces. */
/* NOTE: This removes the possibility of using gl_FragDepth. */
[[fragment]] [[early_fragment_tests]]
void surf_forward([[resource_table]] KernelGlobals &kg,
                  [[resource_table]] PipelineConstants &pipe,
                  [[resource_table]] LightEvalIterator & /*lights*/,
                  [[resource_table]] LightprobeRenderData & /*lightprobes*/,
                  [[resource_table]] LightprobePlaneRenderData & /*lightprobe_planes*/,
                  [[resource_table]] const draw::View &views,
                  [[resource_table]] const draw::Model & /*models*/,
                  [[resource_table]] const draw::Infos & /*infos*/,
                  [[resource_table]] const UnifiedVolumeData &volumes,
                  [[resource_table]] const Uniform &uni,
                  [[resource_table]] const Sampling &sampling,
                  [[resource_table]] const UtilityTexture &util_tx,
                  [[in]] const VertOutCommon &interp,
                  [[in]] [[condition(is_curves)]] const VertOutCurves &curves_interp,
                  [[in]] [[condition(is_pointcloud)]] const VertOutPointcloud &ptcloud_interp,
                  [[frag_coord]] const float4 frag_co,
                  [[out]] SurfaceForwardFragOut &frag_out,
                  [[front_facing]] const bool front_face)
{
  draw::ID id{interp.resource_id_raw};
  const uint resource_id = id.resource_id<1>();

  const ViewMatrices view = views.get(0);

  ShadingData sd = init_globals(uni, interp, view, front_face, frag_co);
  if (pipe.is_mesh) [[static_branch]] {
    init_globals_mesh(interp, sd);
  }
  else if (pipe.is_curves) [[static_branch]] {
    init_globals_curves(interp, curves_interp, sd, view);
  }
  else if (pipe.is_pointcloud) [[static_branch]] {
    init_globals_pointcloud(ptcloud_interp, sd);
  }

  float noise = util_tx.fetch(frag_co.xy, UTIL_BLUE_NOISE_LAYER).r;
  float closure_rand = fract(noise + sampling.rng_1D_get(SAMPLING_CLOSURE));

  fragment_displacement(kg, sd);

  sd.thickness = Thickness::from(nodetree_thickness(kg, sd),
                                 ThicknessMode(kg.nt.node_tree.thickness_mode));

  nodetree_surface(kg, sd, closure_rand);

  float3 radiance, transmittance;
  eevee::forward_lighting_eval(
      kg, sd, view, resource_id, sd.thickness, frag_co.xy, radiance, transmittance);

  if (pipe.use_lighting_nodes) [[static_branch]] {
    radiance += sd.light_accum.diffuse_light;
    radiance += sd.light_accum.glossy_light;
    radiance += sd.light_accum.transmission_light;
  }

  /* Volumetric resolve and compositing. */
  float2 uvs = frag_co.xy * uni.uniform_buf.volumes.main_view_extent_inv;
  VolumeResolveSample vol = volumes.resolve(float3(uvs, reverse_z::read(frag_co.z)));
  /* Removes the part of the volume scattering that has
   * already been added to the destination pixels by the opaque resolve.
   * Since we do that using the blending pipeline we need to account for material transmittance. */
  vol.scattering -= vol.scattering * sd.transmittance;
  radiance = radiance * vol.transmittance + vol.scattering;

  eObjectInfoFlag ob_flag = kg.object_infos_get(sd).flag;
  if (flag_test(ob_flag, OBJECT_HOLDOUT)) {
    sd.holdout = 1.0f - average(sd.transmittance);
    radiance *= 0.0f;
  }

  sd.holdout = saturate(sd.holdout);

  radiance *= 1.0f - sd.holdout;

  /* There can be 2 frame-buffer layout for forward transparency:
   * - Combined RGB radiance with Monochromatic transmittance.
   * - Channel split RGB radiance & RGB transmittance + Dedicated average alpha with holdout. */
  if (uni.pipeline_buf.use_monochromatic_transmittance) {
    frag_out.combined_r = float4(radiance.rgb, transmittance.r);
  }
  else {
    frag_out.combined_r = float4(radiance.r, 0.0f, 0.0f, transmittance.r);
    frag_out.combined_g = float4(radiance.g, 0.0f, 0.0f, transmittance.g);
    frag_out.combined_b = float4(radiance.b, 0.0f, 0.0f, transmittance.b);
    frag_out.combined_a = float4(sd.holdout, 0.0f, 0.0f, average(transmittance));
  }
}

}  // namespace eevee
