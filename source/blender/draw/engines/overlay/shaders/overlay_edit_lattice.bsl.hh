/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "draw_model.bsl.hh"
#include "draw_shader_shared.hh"
#include "draw_view.bsl.hh"
#include "gpu_shader_utildefines.bsl.hh"
#include "overlay_common.bsl.hh"
#include "overlay_shader_shared.hh"

namespace overlay::edit_lattice {

struct Resources {
  [[resource_table]] Uniform uni;
  [[resource_table]] Clipping clip;
  [[resource_table]] draw::View views;
  [[resource_table]] draw::Model models;
  [[resource_table]] draw::Resource resources;

  float4 vertex_color(EditVertEdgeFlag flag) const
  {
    const UniformData &theme = uni.uniform_buf;
    if (flag_test(flag, VERT_SELECTED)) {
      return theme.colors.vert_select;
    }
    if (flag_test(flag, VERT_ACTIVE)) {
      return theme.colors.edit_mesh_active;
    }
    return theme.colors.vert;
  }
};

struct VertEditData {
  [[attribute(0)]] float3 pos;
  [[attribute(1)]] uint data;
};
struct VertWeight {
  [[attribute(0)]] float3 pos;
  [[attribute(1)]] float weight;
};

[[vertex]]
void wire_vert([[resource_table]] const Resources &srt,
               [[resource_table]] const Uniform &uni,
               [[resource_table]] const WeightMap &weight_map,
               [[instance_index]] const int inst_index,
               [[in]] const VertWeight &v_weight,
               [[out]] VertOut &v_out,
               [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
               [[position]] float4 &out_pos)
{
  uint id = srt.resources.get(inst_index).resource_id<1>();
  ObjectMatrices model = srt.models.get(id);
  ViewMatrices view = srt.views.get(0);

  float3 ws_P = model.point_object_to_world(v_weight.pos);
  out_pos = view.point_world_to_homogenous(ws_P);

  v_out.final_color = float4(weight_map.weight_to_rgb(v_weight.weight), 1.0f);
  v_out.edge_start = v_out.edge_pos = ((out_pos.xy / out_pos.w) * 0.5f + 0.5f) *
                                      uni.uniform_buf.size_viewport;

  if (srt.clip.constants.use_clipping) [[static_branch]] {
    srt.clip.set_clipping_distances(ws_P,
                                    clip_distances[0],
                                    clip_distances[1],
                                    clip_distances[2],
                                    clip_distances[3],
                                    clip_distances[4],
                                    clip_distances[5]);
  }
}

[[vertex]]
void point_vert([[resource_table]] const Resources &srt,
                [[resource_table]] const Uniform &uni,
                [[instance_index]] const int inst_index,
                [[in]] const VertEditData &v_edit,
                [[out]] VertOut &v_out,
                [[point_size]] float &out_point_size,
                [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
                [[position]] float4 &out_pos)
{
  uint id = srt.resources.get(inst_index).resource_id<1>();
  ObjectMatrices model = srt.models.get(id);
  ViewMatrices view = srt.views.get(0);

  float3 ws_P = model.point_object_to_world(v_edit.pos);
  out_pos = view.point_world_to_homogenous(ws_P);
  /* Small offset in Z */
  out_pos.z -= 3e-4f;

  v_out.final_color = srt.vertex_color(EditVertEdgeFlag(v_edit.data));
  v_out.edge_start = v_out.edge_pos = float2(0.0); /* Not an edge shader. */

  out_point_size = uni.uniform_buf.sizes.vert * 2.0f;

  if (srt.clip.constants.use_clipping) [[static_branch]] {
    srt.clip.set_clipping_distances(ws_P,
                                    clip_distances[0],
                                    clip_distances[1],
                                    clip_distances[2],
                                    clip_distances[3],
                                    clip_distances[4],
                                    clip_distances[5]);
  }
}

PipelineGraphic point(point_vert,
                      point_frag,
                      ClippingConstant{.use_clipping = false},
                      draw::SelectConstant{.selectable = false});
PipelineGraphic point_clipped(point_vert,
                              point_frag,
                              ClippingConstant{.use_clipping = true},
                              draw::SelectConstant{.selectable = false});

PipelineGraphic wire(wire_vert,
                     wire_frag,
                     ClippingConstant{.use_clipping = false},
                     draw::SelectConstant{.selectable = false});
PipelineGraphic wire_clipped(wire_vert,
                             wire_frag,
                             ClippingConstant{.use_clipping = true},
                             draw::SelectConstant{.selectable = false});

}  // namespace overlay::edit_lattice
