/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_common_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(draw_view)

#include "draw_view_lib.glsl"
#include "eevee_lightprobe_plane.bsl.hh"
#include "eevee_lightprobe_sphere.bsl.hh"
#include "eevee_lightprobe_volume.bsl.hh"
#include "eevee_reverse_z_lib.bsl.hh"
#include "gpu_shader_math_matrix_transform_lib.glsl"

namespace eevee::lightprobe {

namespace planar::display {

struct VertOut {
  [[flat]] int display_index;
  [[flat]] int probe_index;
};

struct FragOut {
  [[frag_color(0)]] float4 color;
};

struct Resources {
  [[legacy_info]] ShaderCreateInfo draw_view;

  [[push_constant]] int4 world_coord_packed;

  [[storage(0, read)]] PlanarProbeDisplayData (&display_data_buf)[];
};

[[vertex]] [[clip_control]]
void vert_main([[resource_table]] const Resources &srt,
               [[vertex_id]] const int vert_id,
               [[position]] float4 &out_position,
               [[out]] VertOut &v_out)
{
  /* Constant array moved inside function scope.
   * Minimizes local register allocation in MSL. */
  constexpr float2 pos[6] = {float2(-1.0f, -1.0f),
                             float2(1.0f, -1.0f),
                             float2(-1.0f, 1.0f),

                             float2(1.0f, -1.0f),
                             float2(1.0f, 1.0f),
                             float2(-1.0f, 1.0f)};

  float2 lP = pos[vert_id % 6];

  v_out.display_index = vert_id / 6;
  v_out.probe_index = srt.display_data_buf[v_out.display_index].probe_index;

  float4x4 plane_to_world = srt.display_data_buf[v_out.display_index].plane_to_world;

  float3 P = transform_point(plane_to_world, float3(lP, 0.0f));
  out_position = drw_point_world_to_homogenous(P);
  /* Small bias to let the probe draw without Z-fighting. */
  out_position.z -= 0.0001f;
  out_position = reverse_z::transform(out_position);
}

[[fragment]]
void frag_main([[resource_table]] const Resources &srt,
               [[resource_table]] const LightprobeSphereRenderData &spheres,
               [[resource_table]] const LightprobePlaneRenderData &planes,
               [[frag_coord]] const float4 frag_co,
               [[in]] const VertOut &v_out,
               [[out]] FragOut &frag_out)
{
  float4x4 plane_to_world = srt.display_data_buf[v_out.display_index].plane_to_world;

  float2 uv = frag_co.xy / float2(textureSize(planes.planar_radiance_tx, 0).xy);
  /* Render is inverted in Y. */
  float2 planar_uv = float2(uv.x, 1.0f - uv.y);

  float depth = reverse_z::read(
      textureLod(planes.planar_depth_tx, float3(planar_uv, v_out.probe_index), 0.0f).r);
  if (depth == 1.0f) {
    float3 ndc = drw_screen_to_ndc(float3(uv, 0.0f));
    float3 wP = drw_point_ndc_to_world(ndc);
    float3 V = drw_world_incident_vector(wP);
    float3 R = -reflect(V, safe_normalize(float3(plane_to_world[2].xyz)));

    SphereProbeUvArea world_atlas_coord = reinterpret_as_atlas_coord(srt.world_coord_packed);
    frag_out.color = spheres.sample_probe(R, 0.0f, world_atlas_coord);
  }
  else {
    frag_out.color = textureLod(
        planes.planar_radiance_tx, float3(planar_uv, v_out.probe_index), 0.0f);
  }
  frag_out.color.a = 0.0f;
}

}  // namespace planar::display

namespace volume::display {

struct VertOut {
  [[smooth]] float2 lP;
  [[flat]] int3 cell;
};

struct FragOut {
  [[frag_color(0)]] float4 color;
};

struct Resources {
  [[legacy_info]] ShaderCreateInfo draw_view;

  [[push_constant]] float sphere_radius;
  [[push_constant]] int3 grid_resolution;
  [[push_constant]] float4x4 grid_to_world;
  [[push_constant]] float4x4 world_to_grid;
  [[push_constant]] bool display_validity;

  [[sampler(0)]] sampler3D irradiance_a_tx;
  [[sampler(1)]] sampler3D irradiance_b_tx;
  [[sampler(2)]] sampler3D irradiance_c_tx;
  [[sampler(3)]] sampler3D irradiance_d_tx;
  [[sampler(4)]] sampler3D validity_tx;
};

[[vertex]] [[clip_control]]
void vert_main([[resource_table]] const Resources &srt,
               [[vertex_id]] const int vert_id,
               [[position]] float4 &out_position,
               [[out]] VertOut &v_out)
{
  /* Constant array moved inside function scope.
   * Minimizes local register allocation in MSL. */
  constexpr float2 pos[6] = float2_array(float2(-1.0f, -1.0f),
                                         float2(1.0f, -1.0f),
                                         float2(-1.0f, 1.0f),

                                         float2(1.0f, -1.0f),
                                         float2(1.0f, 1.0f),
                                         float2(-1.0f, 1.0f));

  v_out.lP = pos[vert_id % 6];
  int cell_index = vert_id / 6;

  int3 grid_res = srt.grid_resolution;

  v_out.cell = int3(cell_index / (grid_res.z * grid_res.y),
                    (cell_index / grid_res.z) % grid_res.y,
                    cell_index % grid_res.z);

  float3 ws_cell_pos = lightprobe::volume::grid_sample_position(
      srt.grid_to_world, grid_res, v_out.cell);

  float sphere_radius_final = srt.sphere_radius;
  if (srt.display_validity) {
    float validity = texelFetch(srt.validity_tx, v_out.cell, 0).r;
    sphere_radius_final *= mix(1.0f, 0.1f, validity);
  }

  float3 vs_offset = float3(v_out.lP, 0.0f) * sphere_radius_final;
  float3 vP = drw_point_world_to_view(ws_cell_pos) + vs_offset;

  out_position = drw_point_view_to_homogenous(vP);
  /* Small bias to let the icon draw without Z-fighting. */
  out_position.z += 0.0001f;
  out_position = reverse_z::transform(out_position);
}

[[fragment]]
void frag_main([[resource_table]] const Resources &srt,
               [[in]] const VertOut &v_out,
               [[out]] FragOut &frag_out)
{
  float dist_squared = length_squared(v_out.lP);

  /* Discard outside the circle. */
  if (dist_squared > 1.0f) {
    gpu_discard_fragment();
    return;
  }

  SphericalHarmonicL1<float4> sh;
  sh.L0.M0 = texelFetch(srt.irradiance_a_tx, v_out.cell, 0);
  sh.L1.Mn1 = texelFetch(srt.irradiance_b_tx, v_out.cell, 0);
  sh.L1.M0 = texelFetch(srt.irradiance_c_tx, v_out.cell, 0);
  sh.L1.Mp1 = texelFetch(srt.irradiance_d_tx, v_out.cell, 0);
  float validity = texelFetch(srt.validity_tx, v_out.cell, 0).r;

  float3 vN = float3(v_out.lP, sqrt(max(0.0f, 1.0f - dist_squared)));
  float3 N = drw_normal_view_to_world(vN);
  float3 lN = transform_direction(srt.world_to_grid, N);

  float3 irradiance = sh.evaluate_lambert(lN).rgb;

  if (srt.display_validity) {
    frag_out.color = float4(mix(float3(1.0f, 0.0f, 0.0f), float3(0.0f, 1.0f, 0.0f), validity),
                            0.0f);
  }
  else {
    frag_out.color = float4(irradiance, 0.0f);
  }
}

}  // namespace volume::display

namespace sphere::display {

struct VertOut {
  [[smooth]] float3 P;
  [[smooth]] float2 lP;
  [[flat]] int probe_index;
};

struct FragOut {
  [[frag_color(0)]] float4 color;
};

struct Resources {
  [[legacy_info]] ShaderCreateInfo draw_view;
  [[resource_table]] srt_t<LightprobeSphereRenderData> lightprobe_sphere;

  [[storage(0, read)]] SphereProbeDisplayData (&display_data_buf)[];
};

[[vertex]] [[clip_control]]
void vert_main([[resource_table]] const Resources &srt,
               [[resource_table]] const LightprobeSphereRenderData &spheres,
               [[vertex_id]] const int vert_id,
               [[position]] float4 &out_position,
               [[out]] VertOut &v_out)
{
  /* Constant array moved inside function scope.
   * Minimizes local register allocation in MSL. */
  constexpr float2 pos[6] = float2_array(float2(-1.0f, -1.0f),
                                         float2(1.0f, -1.0f),
                                         float2(-1.0f, 1.0f),

                                         float2(1.0f, -1.0f),
                                         float2(1.0f, 1.0f),
                                         float2(-1.0f, 1.0f));

  v_out.lP = pos[vert_id % 6];
  int display_index = vert_id / 6;

  v_out.probe_index = srt.display_data_buf[display_index].probe_index;
  float sphere_radius = srt.display_data_buf[display_index].display_size;

  float3 ws_probe_pos = spheres.lightprobe_sphere_buf[v_out.probe_index].location;

  float3 vs_offset = float3(v_out.lP, 0.0f) * sphere_radius;
  float3 vP = drw_point_world_to_view(ws_probe_pos) + vs_offset;
  v_out.P = drw_point_view_to_world(vP);

  out_position = drw_point_view_to_homogenous(vP);
  /* Small bias to let the icon draw without Z-fighting. */
  out_position.z += 0.0001f;
  out_position = reverse_z::transform(out_position);
}

[[fragment]]
void frag_main([[resource_table]] const Resources & /*srt*/,
               [[resource_table]] const LightprobeSphereRenderData &spheres,
               [[in]] const VertOut &v_out,
               [[out]] FragOut &frag_out)
{
  float dist_squared = length_squared(v_out.lP);

  /* Discard outside the circle. */
  if (dist_squared > 1.0f) {
    gpu_discard_fragment();
    return;
  }

  float3 vN = float3(v_out.lP, sqrt(max(0.0f, 1.0f - dist_squared)));
  float3 N = drw_normal_view_to_world(vN);
  float3 V = drw_world_incident_vector(v_out.P);
  float3 L = reflect(-V, N);

  frag_out.color = spheres.sample_probe(
      L, 0, spheres.lightprobe_sphere_buf[v_out.probe_index].atlas_coord);
  frag_out.color.a = 0.0f;
}

}  // namespace sphere::display

}  // namespace eevee::lightprobe

PipelineGraphic eevee_display_lightprobe_planar(eevee::lightprobe::planar::display::vert_main,
                                                eevee::lightprobe::planar::display::frag_main);
PipelineGraphic eevee_display_lightprobe_sphere(eevee::lightprobe::sphere::display::vert_main,
                                                eevee::lightprobe::sphere::display::frag_main);
PipelineGraphic eevee_display_lightprobe_volume(eevee::lightprobe::volume::display::vert_main,
                                                eevee::lightprobe::volume::display::frag_main);
