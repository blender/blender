/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_model.bsl.hh"
#include "draw_shader_shared.hh"
#include "draw_view.bsl.hh"
#include "gpu_shader_attribute_load.bsl.hh"
#include "gpu_shader_index_load.bsl.hh"
#include "overlay_common.bsl.hh"

namespace overlay::edit_mesh::normals {

enum NormalType : int {
  FACE_NORMAL,
  VERT_NORMAL,
  LOOP_NORMAL,
};

struct Constants {
  [[compilation_constant]] NormalType type;
};

struct Resources {
  [[resource_table]] Constants consts;

  [[resource_table]] Uniform uni;
  [[resource_table]] Clipping clip;
  [[resource_table]] draw::View views;
  [[resource_table]] draw::Model models;
  [[resource_table]] draw::Resource resources;

  [[sampler(0)]] sampler2DDepth depth_tx;

  [[storage(1, read), frequency(GEOMETRY)]] const float (&pos)[];
  [[storage(0, read), frequency(GEOMETRY), condition(type == VERT_NORMAL)]] uint (&vnor)[];
  [[storage(0, read), frequency(GEOMETRY), condition(type == LOOP_NORMAL)]] uint (&lnor)[];
  [[storage(0, read), frequency(GEOMETRY), condition(type == FACE_NORMAL)]] uint (&norAndFlag)[];

  [[push_constant]] int2 gpu_attr_0;
  [[push_constant]] int2 gpu_attr_1;

  [[push_constant]] float normal_size;
  [[push_constant]] float normal_screen_size;
  [[push_constant]] float alpha;
  [[push_constant]] bool is_constant_screen_size_normals;

  [[push_constant]] bool hq_normals;

  bool test_occlusion(float4 hs_pos) const
  {
    float3 ndc = (hs_pos.xyz / hs_pos.w) * 0.5f + 0.5f;
    return (ndc.z - 0.00035f) > texture(depth_tx, ndc.xy).r;
  }

  float3 normal_load(uint vert_i) const
  {
    if (consts.type == FACE_NORMAL) [[static_branch]] {
      if (hq_normals) {
        float4 nor_raw = gpu_attr_load_short4_snorm(norAndFlag, gpu_attr_0, vert_i);
        return nor_raw.xyz;
      }
      float4 nor_raw = gpu_attr_load_uint_1010102_snorm(norAndFlag, gpu_attr_0, vert_i);
      return nor_raw.xyz;
    }
    else if (consts.type == VERT_NORMAL) [[static_branch]] {
      float4 nor_raw = gpu_attr_load_uint_1010102_snorm(vnor, gpu_attr_0, vert_i);
      return nor_raw.xyz;
    }
    else if (consts.type == LOOP_NORMAL) [[static_branch]] {
      if (hq_normals) {
        float4 nor_raw = gpu_attr_load_short4_snorm(lnor, gpu_attr_0, vert_i);
        return nor_raw.xyz;
      }
      float4 nor_raw = gpu_attr_load_uint_1010102_snorm(lnor, gpu_attr_0, vert_i);
      return nor_raw.xyz;
    }
    return float3(0);
  }

  float4 color_get() const
  {
    const UniformData &theme = uni.uniform_buf;
    if (consts.type == FACE_NORMAL) [[static_branch]] {
      return theme.colors.normal;
    }
    else if (consts.type == VERT_NORMAL) [[static_branch]] {
      return theme.colors.vnormal;
    }
    else if (consts.type == LOOP_NORMAL) [[static_branch]] {
      return theme.colors.lnormal;
    }
    return float4(0.0f);
  }

  float normal_display_length(ViewMatrices view, float3 ws_P, float3 ws_N) const
  {
    const UniformData &theme = uni.uniform_buf;
    if (is_constant_screen_size_normals) {
      if (view.is_perspective()) {
        float dist_fac = length(view.position() - ws_P);
        float cos_fac = dot(view.forward(), view.world_incident_vector(ws_P));
        return normal_screen_size * dist_fac * cos_fac * theme.pixel_fac * theme.sizes.pixel;
      }
      float frustrum_fac = mul_project_m4_v3_zfac(view, theme.pixel_fac, ws_N) * theme.sizes.pixel;
      return normal_screen_size * frustrum_fac;
    }
    return normal_size;
  }
};

[[vertex]]
void vert_main([[resource_table]] const Resources &srt,
               [[resource_table]] const IndexLoad &indices,
               [[instance_index]] const int inst_index,
               [[vertex_id]] const int vert_id,
               [[position]] float4 &out_pos,
               [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
               [[out]] VertOut &v_out)
{
  /* Point primitive. */
  constexpr uint input_primitive_vertex_count = 1u;
  /* Line list primitive. */
  constexpr uint output_primitive_vertex_count = 2u;
  constexpr uint output_primitive_count = 1u;
  constexpr uint output_invocation_count = 1u;

  constexpr uint output_vertex_count_per_invocation = output_primitive_count *
                                                      output_primitive_vertex_count;
  constexpr uint output_vertex_count_per_input_primitive = output_vertex_count_per_invocation *
                                                           output_invocation_count;

  uint in_primitive_id = uint(vert_id) / output_vertex_count_per_input_primitive;
  uint in_primitive_first_vertex = in_primitive_id * input_primitive_vertex_count;

  uint vert_i = indices.load(in_primitive_first_vertex);

  float3 ls_P = gpu_attr_load_float3(srt.pos, srt.gpu_attr_1, vert_i);
  float3 ls_N = srt.normal_load(vert_i);
  v_out.final_color = srt.color_get();

  uint id = srt.resources.get(inst_index).resource_id<1>();
  ObjectMatrices model = srt.models.get(id);
  ViewMatrices view = srt.views.get(0);

  float3 ws_N = normalize(model.normal_object_to_world(ls_N));
  float3 ws_P = model.point_object_to_world(ls_P);

  if ((vert_id & 1) == 0) {
    ws_P += ws_N * srt.normal_display_length(view, ws_P, ws_N);
  }

  out_pos = view.point_world_to_homogenous(ws_P);

  /* Convert to screen position [0..sizeVp]. */
  v_out.edge_start = v_out.edge_pos = ((out_pos.xy / out_pos.w) * 0.5f + 0.5f) *
                                      srt.uni.uniform_buf.size_viewport;

  v_out.final_color.a *= srt.test_occlusion(out_pos) ? srt.alpha : 1.0f;

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

/* clang-format off */
PipelineGraphic face(vert_main, wire_frag, ClippingConstant{.use_clipping = false}, Constants{.type = FACE_NORMAL});
PipelineGraphic loop(vert_main, wire_frag, ClippingConstant{.use_clipping = false}, Constants{.type = LOOP_NORMAL});
PipelineGraphic vert(vert_main, wire_frag, ClippingConstant{.use_clipping = false}, Constants{.type = VERT_NORMAL});

PipelineGraphic face_clipped(vert_main, wire_frag, ClippingConstant{.use_clipping = true}, Constants{.type = FACE_NORMAL});
PipelineGraphic loop_clipped(vert_main, wire_frag, ClippingConstant{.use_clipping = true}, Constants{.type = LOOP_NORMAL});
PipelineGraphic vert_clipped(vert_main, wire_frag, ClippingConstant{.use_clipping = true}, Constants{.type = VERT_NORMAL});
/* clang-format on */

}  // namespace overlay::edit_mesh::normals
