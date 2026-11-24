/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_model.bsl.hh"
#include "draw_view.bsl.hh"
#include "overlay_common.bsl.hh"
#include "select_lib.bsl.hh"

namespace overlay::lightprobe {

struct Resources {
  [[sampler(0)]] sampler2DDepth depth_buffer;

  [[push_constant]] float4x4 grid_model_matrix;
  [[push_constant]] bool is_transform;

  struct Param {
    float4x4 model_mat;
    eObjectInfoFlag ob_flag;
    int3 grid_resolution;
  };

  Param unpack_parameters() const
  {
    float4x4 model_mat = grid_model_matrix;
    model_mat[0][3] = 0.0f;
    model_mat[1][3] = 0.0f;
    model_mat[2][3] = 0.0f;
    model_mat[3][3] = 1.0f;

    /* Grid points are only displayed when selecting. */
    const eObjectInfoFlag ob_flag = grid_model_matrix[3].w == 1.0f ?
                                        eObjectInfoFlag(OBJECT_SELECTED | OBJECT_ACTIVE) :
                                        eObjectInfoFlag(OBJECT_SELECTED);

    const int3 grid_resolution = int3(
        float3(grid_model_matrix[0].w, grid_model_matrix[1].w, grid_model_matrix[2].w));

    return {
        .model_mat = model_mat,
        .ob_flag = ob_flag,
        .grid_resolution = grid_resolution,
    };
  }
};

[[vertex]]
void grid_points_vert([[resource_table]] const Resources &srt,
                      [[resource_table]] const Uniform &uni,
                      [[resource_table]] const Clipping &clip,
                      [[resource_table]] const draw::View &views,
                      [[resource_table, condition(selectable)]] const draw::Select &sel,
                      [[resource_table,
                        condition(selectable)]] const draw::ResourceCustomID &resources_with_id,
                      [[out, condition(selectable)]] draw::SelectOut &sel_out,
                      [[instance_index]] const int inst_index,
                      [[vertex_id]] const int vert_id,
                      [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
                      [[out]] VertOut &v_out,
                      [[point_size]] float &out_pt_size,
                      [[position]] float4 &out_pos)
{
  if (sel.consts.selectable) [[static_branch]] {
    sel_out.select_id = resources_with_id.get_custom_id(inst_index);
  }

  ViewMatrices view = views.get(0);

  auto [model_mat, ob_flag, grid_resolution] = srt.unpack_parameters();
  const UniformData &theme = uni.uniform_buf;

  float3 ls_cell_location;
  /* Keep in sync with update_irradiance_probe */
  ls_cell_location.z = float(vert_id % grid_resolution.z);
  ls_cell_location.y = float((vert_id / grid_resolution.z) % grid_resolution.y);
  ls_cell_location.x = float(vert_id / (grid_resolution.z * grid_resolution.y));

  ls_cell_location += 1.0f;
  ls_cell_location /= float3(grid_resolution + 1);
  ls_cell_location = ls_cell_location * 2.0f - 1.0f;

  float3 ws_cell_location = (model_mat * float4(ls_cell_location, 1.0f)).xyz;
  out_pos = view.point_world_to_homogenous(ws_cell_location);
  out_pt_size = theme.sizes.vert * 2.0f;

  v_out.final_color.rgb = uni.wire_color_get(ob_flag, true, srt.is_transform).rim;
  v_out.final_color.a = 1.0f;

  /* Shade occluded points differently. */
  float3 ndc_P = view.perspective_divide(out_pos);
  float3 screen_P = view.ndc_to_screen(ndc_P);
  float z_depth = texture(srt.depth_buffer, screen_P.xy).r;
  float z_delta = screen_P.z - z_depth;
  if (z_delta > 0.0f) {
    float fac = 1.0f - z_delta * 5000.0f;
    /* Smooth blend to avoid flickering. */
    v_out.final_color = mix(theme.colors.background, v_out.final_color, clamp(fac, 0.2f, 1.0f));
  }

  if (clip.constants.use_clipping) [[static_branch]] {
    clip.set_clipping_distances(ws_cell_location,
                                clip_distances[0],
                                clip_distances[1],
                                clip_distances[2],
                                clip_distances[3],
                                clip_distances[4],
                                clip_distances[5]);
  }
}

}  // namespace overlay::lightprobe

/* clang-format off */
PipelineGraphic overlay_extra_grid(                   overlay::lightprobe::grid_points_vert, overlay::point_frag, overlay::ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = false});
PipelineGraphic overlay_extra_grid_clipped(           overlay::lightprobe::grid_points_vert, overlay::point_frag, overlay::ClippingConstant{.use_clipping = true},  draw::SelectConstant{.selectable = false});
PipelineGraphic overlay_extra_grid_selectable(        overlay::lightprobe::grid_points_vert, overlay::point_frag, overlay::ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = true});
PipelineGraphic overlay_extra_grid_selectable_clipped(overlay::lightprobe::grid_points_vert, overlay::point_frag, overlay::ClippingConstant{.use_clipping = true},  draw::SelectConstant{.selectable = true});
/* clang-format on */
