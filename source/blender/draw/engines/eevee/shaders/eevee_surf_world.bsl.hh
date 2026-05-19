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

FRAGMENT_SHADER_CREATE_INFO(eevee_nodetree)
FRAGMENT_SHADER_CREATE_INFO(eevee_geom_world)

#include "draw_view_lib.glsl"
#include "eevee_attributes_world_lib.glsl"
#include "eevee_colorspace_lib.bsl.hh"
#include "eevee_lightprobe_sphere_lib.glsl"
#include "eevee_lightprobe_volume_eval_lib.glsl"
#include "eevee_nodetree_frag_lib.glsl"
#include "eevee_sampling_lib.glsl"
#include "eevee_surf_lib.glsl"

float4 closure_to_rgba_world(Closure /*cl*/)
{
  return float4(0.0f);
}

namespace eevee {

struct SurfWorld {
  [[legacy_info]] ShaderCreateInfo eevee_global_ubo;
  [[legacy_info]] ShaderCreateInfo eevee_lightprobe_sphere_data;
  [[legacy_info]] ShaderCreateInfo eevee_volume_probe_data;
  [[legacy_info]] ShaderCreateInfo eevee_sampling_data;
  [[legacy_info]] ShaderCreateInfo eevee_utility_texture;

  /* Optionally added depending on the material. */
  // [[legacy_info]] ShaderCreateInfo eevee_render_pass_out;
  // [[legacy_info]] ShaderCreateInfo eevee_cryptomatte_out;

  [[push_constant]] float world_opacity_fade;
  [[push_constant]] float world_background_blur;
  [[push_constant]] int4 world_coord_packed;
};

struct SurfWorldFragOut {
  [[frag_color(0)]] float4 background;
};

[[fragment]] [[early_fragment_tests]]
void surf_world([[resource_table]] SurfWorld &srt,
                [[out]] SurfWorldFragOut &frag_out,
                [[front_facing]] const bool front_face)
{
  init_globals(front_face);
  /* View position is passed to keep accuracy. */
  g_data.N = drw_normal_view_to_world(drw_view_incident_vector(interp.P));
  g_data.Ng = g_data.N;
  g_data.P = -g_data.N;
  attrib_load(WorldPoint{0});

  nodetree_surface(0.0f);

  g_holdout = saturate(g_holdout);

  frag_out.background.rgb = colorspace::safe_color(g_emission) * (1.0f - g_holdout);
  frag_out.background.a = saturate(average(g_transmittance)) * g_holdout;

  if (g_data.ray_type == RAY_TYPE_CAMERA && srt.world_background_blur != 0.0f) {
    float base_lod = sphere_probe_roughness_to_lod(srt.world_background_blur);
    float lod = max(1.0f, base_lod);
    float mix_factor = min(1.0f, base_lod);
    SphereProbeUvArea world_atlas_coord = reinterpret_as_atlas_coord(srt.world_coord_packed);
    float4 probe_color = lightprobe_spheres_sample(-g_data.N, lod, world_atlas_coord);
    frag_out.background.rgb = mix(frag_out.background.rgb, probe_color.rgb, mix_factor);

    SphericalHarmonicL1<float4> volume_irradiance = lightprobe_volume_sample(
        g_data.P, float3(0.0f), g_data.Ng);
    float3 radiance_sh = volume_irradiance.evaluate_lambert(-g_data.N).rgb;
    float radiance_mix_factor = sphere_probe_roughness_to_mix_fac(srt.world_background_blur);
    frag_out.background.rgb = mix(frag_out.background.rgb, radiance_sh, radiance_mix_factor);
  }

  /* Output environment pass. */
#ifdef MAT_RENDER_PASS_SUPPORT
  float4 environment = frag_out.background;
  environment.a = 1.0f - environment.a;
  environment.rgb *= environment.a;
  output_renderpass_color(uniform_buf.render_pass.environment_id, environment);
#endif

  frag_out.background = mix(
      float4(0.0f, 0.0f, 0.0f, 1.0f), frag_out.background, srt.world_opacity_fade);
}
}  // namespace eevee
