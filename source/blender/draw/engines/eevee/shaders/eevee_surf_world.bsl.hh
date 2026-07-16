/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Background used to shade the world.
 *
 * Outputs shading parameter per pixel using a set of randomized BSDFs.
 */
#pragma once

#include "infos/eevee_geom_infos.hh"
#include "infos/eevee_nodetree_infos.hh"

#include "eevee_attributes_world_lib.glsl"
#include "eevee_colorspace_lib.bsl.hh"
#include "eevee_lightprobe.bsl.hh"
#include "eevee_nodetree_frag_lib.glsl"
#include "eevee_pipeline.bsl.hh"
#include "eevee_sampling_lib.bsl.hh"
#include "eevee_surf_common.bsl.hh"

float4 closure_to_rgba_world(Closure /*cl*/)
{
  float3 transmittance = g_transmittance;
  closure_weights_reset(0.0f);
  return float4(0.0f, 0.0f, 0.0f, saturate(1.0f - average(transmittance)));
}

namespace eevee {

struct SurfWorld {
  [[legacy_info]] ShaderCreateInfo eevee_geom_iface_info;

  [[push_constant]] float world_opacity_fade;
  [[push_constant]] float world_background_blur;
  [[push_constant]] int4 world_coord_packed;
};

struct SurfWorldFragOut {
  [[frag_color(0)]] float4 background;
};

[[fragment]] [[early_fragment_tests]]
void surf_world([[resource_table]] PipelineConstants & /*pipe*/,
                [[resource_table]] SurfWorld &srt,
                [[resource_table]] const LightprobeRenderData &lightprobes,
                [[resource_table]] RenderPassOutput &render_passes,
                [[resource_table]] const Uniform &uni,
                [[resource_table]] const UtilityTexture & /*util_tx*/,
                [[resource_table]] const draw::View &views,
                [[frag_coord]] const float4 frag_co,
                [[out]] SurfWorldFragOut &frag_out,
                [[front_facing]] const bool front_face)
{
  FRAGMENT_SHADER_CREATE_INFO(eevee_geom_iface_info);

  const ViewMatrices view = views.get(0);
  init_globals(uni, view, front_face);
  /* View position is passed to keep accuracy. */
  g_data.N = view.normal_view_to_world(view.view_incident_vector(interp.P));
  g_data.Ng = g_data.N;
  g_data.P = -g_data.N;
  attrib_load(WorldPoint{g_data.P});

  nodetree_surface(0.0f);

  g_holdout = saturate(g_holdout);

  frag_out.background.rgb = colorspace::safe_color(g_emission) * (1.0f - g_holdout);
  frag_out.background.a = saturate(average(g_transmittance)) * g_holdout;

  if (g_data.ray_type == RAY_TYPE_CAMERA && srt.world_background_blur != 0.0f) {
    [[resource_table]] const LightprobeVolumeRenderData &lp_volumes = lightprobes.volumes;
    [[resource_table]] const LightprobeSphereRenderData &lp_spheres = lightprobes.spheres;

    float base_lod = lightprobe::sphere::roughness_to_lod(srt.world_background_blur);
    float lod = max(1.0f, base_lod);
    float mix_factor = min(1.0f, base_lod);
    SphereProbeUvArea world_atlas_coord = reinterpret_as_atlas_coord(srt.world_coord_packed);
    float4 probe_color = lp_spheres.sample_probe(-g_data.N, lod, world_atlas_coord);
    frag_out.background.rgb = mix(frag_out.background.rgb, probe_color.rgb, mix_factor);

    SphericalHarmonicL1<float4> volume_irradiance = lp_volumes.world();
    float3 radiance_sh = volume_irradiance.evaluate_lambert(-g_data.N).rgb;
    float radiance_mix_factor = lightprobe::sphere::roughness_to_mix_fac(
        srt.world_background_blur);
    frag_out.background.rgb = mix(frag_out.background.rgb, radiance_sh, radiance_mix_factor);
  }

  /* Output environment pass. */
  float4 environment = frag_out.background;
  environment.a = 1.0f - environment.a;
  environment.rgb *= environment.a;
  render_passes.store_color(
      int2(frag_co.xy), uni.uniform_buf.render_pass.environment_id, environment);

  frag_out.background = mix(
      float4(0.0f, 0.0f, 0.0f, 1.0f), frag_out.background, srt.world_opacity_fade);
}
}  // namespace eevee
