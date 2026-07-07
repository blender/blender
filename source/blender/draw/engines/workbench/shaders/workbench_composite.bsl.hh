/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Deferred pipeline resolve pass.
 */

#pragma once
#pragma create_info

#include "draw_view_lib.glsl"
#include "gpu_shader_fullscreen_lib.glsl"
#include "workbench_cavity.bsl.hh"
#include "workbench_common.bsl.hh"
#include "workbench_curvature.bsl.hh"
#include "workbench_defines.hh"
#include "workbench_matcap.bsl.hh"
#include "workbench_world_light.bsl.hh"

/* TODO(fclem): Move to workbench. */
#define WORKBENCH_LIGHTING_STUDIO 0
#define WORKBENCH_LIGHTING_MATCAP 1
#define WORKBENCH_LIGHTING_FLAT 2

namespace workbench::resolve {

struct Resources {
  [[compilation_constant]] int lighting_mode; /* WORKBENCH_LIGHTING_* */
  [[compilation_constant]] bool use_cavity;
  [[compilation_constant]] bool use_curvature;
  [[compilation_constant]] bool use_shadow;

  [[legacy_info]] ShaderCreateInfo draw_view;
  [[sampler(3)]] sampler2DDepth depth_tx;
  [[sampler(4)]] sampler2D normal_tx;
  [[sampler(5)]] sampler2D material_tx;

  [[sampler(6), condition(use_curvature)]] usampler2D object_id_tx;

  [[sampler(8), condition(use_shadow)]] usampler2D stencil_tx;

  [[sampler(WB_MATCAP_SLOT), condition(lighting_mode == 1 /* WORKBENCH_LIGHTING_MATCAP */)]]
  sampler2DArray matcap_tx;

  [[resource_table]] srt_t<workbench::World> world;

  [[resource_table, condition(use_cavity)]] srt_t<workbench::Cavity> cavity;
};

[[vertex]] void vert([[vertex_id]] const int &vert_id, [[position]] float4 &out_pos)
{
  fullscreen_vertex(vert_id, out_pos);
}

struct FragOut {
  [[frag_color(0)]] float4 color;
};

[[fragment]] void frag([[frag_coord]] const float4 &frag_coord,
                       [[resource_table]] Resources &srt,
                       [[out]] FragOut &frag_out)
{
  float2 uv = frag_coord.xy / float2(textureSize(srt.depth_tx, 0).xy);

  float depth = texture(srt.depth_tx, uv).r;
  if (depth == 1.0f) {
    /* Skip the background. */
    gpu_discard_fragment();
    return;
  }

  /* Normal and Incident vector are in view-space. Lighting is evaluated in view-space. */
  float3 P = drw_point_screen_to_view(float3(uv, 0.5f));
  float3 V = drw_view_incident_vector(P);
  float3 N = workbench::normal_decode(texture(srt.normal_tx, uv));
  float4 mat_data = texture(srt.material_tx, uv);

  float3 base_color = mat_data.rgb;
  float4 color = float4(1.0f);

  if (srt.lighting_mode == WORKBENCH_LIGHTING_MATCAP) [[static_branch]] {
    /* When using matcaps, mat_data.a is the back-face sign. */
    N = (mat_data.a > 0.0f) ? N : -N;
    color.rgb = workbench::get_matcap_lighting(srt.world, srt.matcap_tx, base_color, N, V);
  }
  else if (srt.lighting_mode == WORKBENCH_LIGHTING_STUDIO) [[static_branch]] {
    float roughness = 0.0f, metallic = 0.0f;
    workbench::float_pair_decode(mat_data.a, roughness, metallic);
    color.rgb = workbench::get_world_lighting(srt.world, base_color, roughness, metallic, N, V);
  }
  else if (srt.lighting_mode == WORKBENCH_LIGHTING_FLAT) [[static_branch]] {
    color.rgb = base_color;
  }

  float cavity = 0.0f, edges = 0.0f, curvature = 0.0f;
  if (srt.use_cavity) [[static_branch]] {
    workbench::cavity_compute(
        srt.cavity, srt.world, srt.depth_tx, srt.normal_tx, uv, cavity, edges);
  }

  if (srt.use_curvature) [[static_branch]] {
    workbench::curvature_compute(srt.world, srt.object_id_tx, srt.normal_tx, uv, curvature);
  }
  color.rgb *= clamp((1.0f - cavity) * (1.0f + edges) * (1.0f + curvature), 0.0f, 4.0f);

  if (srt.use_shadow) [[static_branch]] {
    bool shadow = texture(srt.stencil_tx, uv).r != 0;
    color.rgb *= workbench::get_shadow(srt.world, N, shadow);
  }

  frag_out.color = color;
}

/* clang-format off */
PipelineGraphic opaque_studio_cavity_curvature_shadow(         vert, frag, Resources{.lighting_mode = 0 /* WORKBENCH_LIGHTING_STUDIO */, .use_cavity = true,  .use_curvature = true,  .use_shadow = true });
PipelineGraphic opaque_studio_cavity_curvature_no_shadow(      vert, frag, Resources{.lighting_mode = 0 /* WORKBENCH_LIGHTING_STUDIO */, .use_cavity = true,  .use_curvature = true,  .use_shadow = false});
PipelineGraphic opaque_studio_cavity_no_curvature_shadow(      vert, frag, Resources{.lighting_mode = 0 /* WORKBENCH_LIGHTING_STUDIO */, .use_cavity = true,  .use_curvature = false, .use_shadow = true });
PipelineGraphic opaque_studio_cavity_no_curvature_no_shadow(   vert, frag, Resources{.lighting_mode = 0 /* WORKBENCH_LIGHTING_STUDIO */, .use_cavity = true,  .use_curvature = false, .use_shadow = false});
PipelineGraphic opaque_studio_no_cavity_curvature_shadow(      vert, frag, Resources{.lighting_mode = 0 /* WORKBENCH_LIGHTING_STUDIO */, .use_cavity = false, .use_curvature = true,  .use_shadow = true });
PipelineGraphic opaque_studio_no_cavity_curvature_no_shadow(   vert, frag, Resources{.lighting_mode = 0 /* WORKBENCH_LIGHTING_STUDIO */, .use_cavity = false, .use_curvature = true,  .use_shadow = false});
PipelineGraphic opaque_studio_no_cavity_no_curvature_shadow(   vert, frag, Resources{.lighting_mode = 0 /* WORKBENCH_LIGHTING_STUDIO */, .use_cavity = false, .use_curvature = false, .use_shadow = true });
PipelineGraphic opaque_studio_no_cavity_no_curvature_no_shadow(vert, frag, Resources{.lighting_mode = 0 /* WORKBENCH_LIGHTING_STUDIO */, .use_cavity = false, .use_curvature = false, .use_shadow = false});
PipelineGraphic opaque_matcap_cavity_curvature_shadow(         vert, frag, Resources{.lighting_mode = 1 /* WORKBENCH_LIGHTING_MATCAP */, .use_cavity = true,  .use_curvature = true,  .use_shadow = true });
PipelineGraphic opaque_matcap_cavity_curvature_no_shadow(      vert, frag, Resources{.lighting_mode = 1 /* WORKBENCH_LIGHTING_MATCAP */, .use_cavity = true,  .use_curvature = true,  .use_shadow = false});
PipelineGraphic opaque_matcap_cavity_no_curvature_shadow(      vert, frag, Resources{.lighting_mode = 1 /* WORKBENCH_LIGHTING_MATCAP */, .use_cavity = true,  .use_curvature = false, .use_shadow = true });
PipelineGraphic opaque_matcap_cavity_no_curvature_no_shadow(   vert, frag, Resources{.lighting_mode = 1 /* WORKBENCH_LIGHTING_MATCAP */, .use_cavity = true,  .use_curvature = false, .use_shadow = false});
PipelineGraphic opaque_matcap_no_cavity_curvature_shadow(      vert, frag, Resources{.lighting_mode = 1 /* WORKBENCH_LIGHTING_MATCAP */, .use_cavity = false, .use_curvature = true,  .use_shadow = true });
PipelineGraphic opaque_matcap_no_cavity_curvature_no_shadow(   vert, frag, Resources{.lighting_mode = 1 /* WORKBENCH_LIGHTING_MATCAP */, .use_cavity = false, .use_curvature = true,  .use_shadow = false});
PipelineGraphic opaque_matcap_no_cavity_no_curvature_shadow(   vert, frag, Resources{.lighting_mode = 1 /* WORKBENCH_LIGHTING_MATCAP */, .use_cavity = false, .use_curvature = false, .use_shadow = true });
PipelineGraphic opaque_matcap_no_cavity_no_curvature_no_shadow(vert, frag, Resources{.lighting_mode = 1 /* WORKBENCH_LIGHTING_MATCAP */, .use_cavity = false, .use_curvature = false, .use_shadow = false});
PipelineGraphic opaque_flat_cavity_curvature_shadow(           vert, frag, Resources{.lighting_mode = 2 /* WORKBENCH_LIGHTING_FLAT */,   .use_cavity = true,  .use_curvature = true,  .use_shadow = true });
PipelineGraphic opaque_flat_cavity_curvature_no_shadow(        vert, frag, Resources{.lighting_mode = 2 /* WORKBENCH_LIGHTING_FLAT */,   .use_cavity = true,  .use_curvature = true,  .use_shadow = false});
PipelineGraphic opaque_flat_cavity_no_curvature_shadow(        vert, frag, Resources{.lighting_mode = 2 /* WORKBENCH_LIGHTING_FLAT */,   .use_cavity = true,  .use_curvature = false, .use_shadow = true });
PipelineGraphic opaque_flat_cavity_no_curvature_no_shadow(     vert, frag, Resources{.lighting_mode = 2 /* WORKBENCH_LIGHTING_FLAT */,   .use_cavity = true,  .use_curvature = false, .use_shadow = false});
PipelineGraphic opaque_flat_no_cavity_curvature_shadow(        vert, frag, Resources{.lighting_mode = 2 /* WORKBENCH_LIGHTING_FLAT */,   .use_cavity = false, .use_curvature = true,  .use_shadow = true });
PipelineGraphic opaque_flat_no_cavity_curvature_no_shadow(     vert, frag, Resources{.lighting_mode = 2 /* WORKBENCH_LIGHTING_FLAT */,   .use_cavity = false, .use_curvature = true,  .use_shadow = false});
PipelineGraphic opaque_flat_no_cavity_no_curvature_shadow(     vert, frag, Resources{.lighting_mode = 2 /* WORKBENCH_LIGHTING_FLAT */,   .use_cavity = false, .use_curvature = false, .use_shadow = true });
PipelineGraphic opaque_flat_no_cavity_no_curvature_no_shadow(  vert, frag, Resources{.lighting_mode = 2 /* WORKBENCH_LIGHTING_FLAT */,   .use_cavity = false, .use_curvature = false, .use_shadow = false});
/* clang-format on */

}  // namespace workbench::resolve
