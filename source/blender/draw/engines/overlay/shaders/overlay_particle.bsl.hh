/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "draw_model.bsl.hh"
#include "draw_view.bsl.hh"
#include "gpu_shader_math_constants.bsl.hh"
#include "gpu_shader_math_matrix_transform.bsl.hh"
#include "overlay_common.bsl.hh"
#include "select_lib.bsl.hh"

namespace overlay::particle {

/* Called 'point' instead of 'dot' to avoid name collision with builtin function. */
namespace point {

struct VertIn {
  [[attribute(0)]] float3 part_pos;
  [[attribute(1)]] float4 part_rot;
  [[attribute(2)]] float part_val;
};

struct VertOut {
  [[flat]] float4 final_color;
};

struct Resources {
  [[sampler(0)]] sampler1D weight_tx;
  /* Draw-size packed in alpha. */
  [[push_constant]] float4 ucolor;
};

[[vertex]]
void vert_main([[resource_table]] const Resources &srt,
               [[resource_table]] const Uniform &uni,
               [[resource_table, condition(selectable)]] draw::ResourceCustomID &resources_with_id,
               [[resource_table, condition(selectable)]] const draw::Select &sel,
               [[resource_table]] const draw::View &views,
               [[resource_table]] const Clipping &clip,
               [[instance_index]] const int inst_index,
               [[point_size]] float &out_point_size,
               [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
               [[in]] const VertIn &v_in,
               [[out]] VertOut &v_out,
               [[out, condition(selectable)]] draw::SelectOut &sel_out,
               [[position]] float4 &out_pos)
{
  if (sel.consts.selectable) [[static_branch]] {
    sel_out.select_id = resources_with_id.get_custom_id(inst_index);
  }

  const ViewMatrices view = views.get(0);

  /* Draw-size packed in alpha. */
  float draw_size = srt.ucolor.a;

  float3 world_pos = v_in.part_pos;

  out_pos = view.point_world_to_homogenous(world_pos);
  /* World sized points. */
  out_point_size = draw_size * view.winmat[1][1] * uni.uniform_buf.size_viewport.y / out_pos.w;

  /* Coloring */
  if (v_in.part_val < 0.0f) {
    v_out.final_color = float4(srt.ucolor.rgb, 1.0f);
  }
  else {
    v_out.final_color = float4(texture(srt.weight_tx, v_in.part_val).rgb, 1.0f);
  }

  if (clip.constants.use_clipping) [[static_branch]] {
    clip.set_clipping_distances(world_pos,
                                clip_distances[0],
                                clip_distances[1],
                                clip_distances[2],
                                clip_distances[3],
                                clip_distances[4],
                                clip_distances[5]);
  }
}

[[fragment]]
void frag_main([[resource_table, condition(selectable)]] draw::Select &sel,
               [[in]] const VertOut &v_out,
               [[in, condition(selectable)]] const draw::SelectOut &sel_out,
               [[out]] FragOut &frag_out,
               [[frag_coord]] const float4 frag_co,
               [[point_coord]] const float2 point_co)
{
  float2 uv = point_co - float2(0.5f);
  float dist = length(uv);

  if (dist > 0.5f) {
    gpu_discard_fragment();
    return;
  }
  /* Nice sphere falloff. */
  float intensity = sqrt(1.0f - dist * 2.0f) * 0.5f + 0.5f;
  frag_out.color = v_out.final_color * float4(intensity, intensity, intensity, 1.0f);

  /* The default value of GL_POINT_SPRITE_COORD_ORIGIN is GL_UPPER_LEFT. Need to reverse the Y. */
  uv.y = -uv.y;
  /* Subtract distance to outer edge of the circle. (0.75 is manually tweaked to look better) */
  float2 edge_pos = frag_co.xy - uv * (0.75f / (dist + 1e-9f));
  float2 edge_start = edge_pos + float2(-uv.y, uv.x);

  frag_out.line_output = pack_line_data(frag_co.xy, edge_start, edge_pos);

  if (sel.consts.selectable) [[static_branch]] {
    sel.select_id_output(sel_out.select_id, frag_co);
  }
}

/* clang-format off */
PipelineGraphic dot(                   vert_main, frag_main, ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = false});
PipelineGraphic dot_clipped(           vert_main, frag_main, ClippingConstant{.use_clipping = true }, draw::SelectConstant{.selectable = false});
PipelineGraphic dot_selectable(        vert_main, frag_main, ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = true});
PipelineGraphic dot_selectable_clipped(vert_main, frag_main, ClippingConstant{.use_clipping = true }, draw::SelectConstant{.selectable = true});
/* clang-format on */

}  // namespace point

namespace hair {

struct VertIn {
  [[attribute(0)]] float3 pos;
  [[attribute(1)]] float3 nor;
};

struct Resources {
  [[resource_table]] Uniform uni;

  [[push_constant]] int color_type;
  [[push_constant]] bool is_transform;
  [[push_constant]] bool use_coloring;

  WireColor color_get(ObjectInfos info) const
  {
    if (color_type == V3D_SHADING_OBJECT_COLOR || color_type == V3D_SHADING_RANDOM_COLOR) {
      return uni.wire_object_color_get(info, eV3DShadingColorType(color_type), use_coloring);
    }
    return uni.wire_color_get(info.flag, use_coloring, is_transform);
  }
};

[[vertex]]
void vert_main([[resource_table]] const Resources &srt,
               [[resource_table]] const Uniform &uni,
               [[resource_table, condition(!selectable)]] draw::Resource &resources,
               [[resource_table, condition(selectable)]] draw::ResourceCustomID &resources_with_id,
               [[resource_table, condition(selectable)]] const draw::Select &sel,
               [[resource_table]] const draw::Model &models,
               [[resource_table]] const draw::Infos &infos,
               [[resource_table]] const draw::View &views,
               [[resource_table]] const Clipping &clip,
               [[instance_index]] const int inst_index,
               [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
               [[in]] const VertIn &v_in,
               [[out]] VertOut &v_out,
               [[out, condition(selectable)]] draw::SelectOut &sel_out,
               [[position]] float4 &out_pos)
{
  uint resource_id;
  if (sel.consts.selectable) [[static_branch]] {
    sel_out.select_id = resources_with_id.get_custom_id(inst_index);
    resource_id = resources_with_id.get(inst_index).resource_id<1>();
  }
  else {
    resource_id = resources.get(inst_index).resource_id<1>();
  }

  const ObjectMatrices obj = models.get(resource_id);
  const ObjectInfos info = infos.get(resource_id);
  const ViewMatrices view = views.get(0);

  float3 ws_P = obj.point_object_to_world(v_in.pos);
  float3 ws_N = normalize(obj.normal_object_to_world(-v_in.nor));

  out_pos = view.point_world_to_homogenous(ws_P);

  float facing = saturate(abs(dot(ws_N, view.world_incident_vector(ws_P))));
  WireColor color = srt.color_get(info);
  float3 final_front_col = non_linear_blend_color(color.rim, color.wire, 0.35f);
  v_out.final_color.rgb = non_linear_blend_color(color.rim, final_front_col, facing);
  v_out.final_color.a = 1.0f;

  v_out.edge_start = v_out.edge_pos = ((out_pos.xy / out_pos.w) * 0.5f + 0.5f) *
                                      uni.uniform_buf.size_viewport;

  if (clip.constants.use_clipping) [[static_branch]] {
    clip.set_clipping_distances(ws_P,
                                clip_distances[0],
                                clip_distances[1],
                                clip_distances[2],
                                clip_distances[3],
                                clip_distances[4],
                                clip_distances[5]);
  }
}

}  // namespace hair

/* clang-format off */
PipelineGraphic strand(                   hair::vert_main, wire_frag, ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = false});
PipelineGraphic strand_clipped(           hair::vert_main, wire_frag, ClippingConstant{.use_clipping = true }, draw::SelectConstant{.selectable = false});
PipelineGraphic strand_selectable(        hair::vert_main, wire_frag, ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = true});
PipelineGraphic strand_selectable_clipped(hair::vert_main, wire_frag, ClippingConstant{.use_clipping = true }, draw::SelectConstant{.selectable = true});
/* clang-format on */

namespace wire {

struct Resources {
  [[resource_table]] Uniform uni;

  /* Draw-size packed in alpha. */
  [[push_constant]] float4 ucolor;
  [[push_constant]] int shape_type;

  [[sampler(0)]] sampler1D weight_tx;

  /* Use first attribute to only bind one buffer. */
  [[storage(0, read), frequency(GEOMETRY)]] ParticlePointData (&part_pos)[];
};

float3 rotate(float3 vec, float4 quat)
{
  /* The quaternion representation here stores the w component in the first index. */
  return vec + 2.0f * cross(quat.yzw, cross(quat.yzw, vec) + quat.x * vec);
}

/* Could be move to a shared library. */
float2 circle_position(float angle)
{
  return float2(cos(angle), sin(angle));
}

[[vertex]]
void vert_main([[resource_table]] const Resources &srt,
               [[resource_table]] const Uniform &uni,
               [[resource_table, condition(selectable)]] draw::ResourceCustomID &resources_with_id,
               [[resource_table, condition(selectable)]] const draw::Select &sel,
               [[resource_table]] const draw::View &views,
               [[resource_table]] const Clipping &clip,
               [[instance_index]] const int inst_index,
               [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
               [[vertex_id]] const int vert_id,
               [[out]] VertOut &v_out,
               [[out, condition(selectable)]] draw::SelectOut &sel_out,
               [[position]] float4 &out_pos)
{
  if (sel.consts.selectable) [[static_branch]] {
    sel_out.select_id = resources_with_id.get_custom_id(inst_index);
  }

  const ViewMatrices view = views.get(0);

  int particle_id = vert_id;
  int shape_vert_id = vert_id;

  switch (OVERLAY_ParticleShape(srt.shape_type)) {
    case PART_SHAPE_AXIS:
    case PART_SHAPE_CROSS:
      shape_vert_id = vert_id % 6;
      particle_id = vert_id / 6;
      break;
    case PART_SHAPE_CIRCLE:
      shape_vert_id = vert_id % (PARTICLE_SHAPE_CIRCLE_RESOLUTION * 2);
      particle_id = vert_id / (PARTICLE_SHAPE_CIRCLE_RESOLUTION * 2);
      break;
  }

  ParticlePointData part = srt.part_pos[particle_id];

  uint axis_id = uint(shape_vert_id) >> 1u;
  uint axis_vert = uint(shape_vert_id) & 1u;

#ifdef GPU_PROVOKING_VERTEX_LAST
  /* Metal and Vulkan (without extension) have different provoking vertex convention. */
  axis_vert ^= 1u;
#endif

  float3 shape_pos = float3(0.0f);
  switch (OVERLAY_ParticleShape(srt.shape_type)) {
    case PART_SHAPE_AXIS:
      shape_pos = float3(axis_id == 0, axis_id == 1, axis_id == 2) * 2.0f * float(axis_vert != 0u);
      break;
    case PART_SHAPE_CROSS:
      shape_pos = float3(axis_id == 0, axis_id == 1, axis_id == 2) *
                  (axis_vert == 0u ? 1.0f : -1.0f);
      break;
    case PART_SHAPE_CIRCLE:
      shape_pos.xy = circle_position(M_TAU * float((shape_vert_id + 1) / 2) /
                                     float(PARTICLE_SHAPE_CIRCLE_RESOLUTION));
      break;
  }

  v_out.final_color = float4(1.0f);
  if (srt.shape_type == PART_SHAPE_AXIS) {
    /* Works because of flat interpolation. */
    v_out.final_color.rgb = shape_pos;
  }
  else {
    v_out.final_color.rgb = part.value < 0.0f ? srt.ucolor.rgb :
                                                texture(srt.weight_tx, part.value).rgb;
  }

  /* Draw-size packed in alpha. */
  shape_pos *= srt.ucolor.a;

  float3 world_pos = part.position;
  if (srt.shape_type == PART_SHAPE_CIRCLE) {
    /* World sized, camera facing geometry. */
    world_pos += transform_direction(view.viewinv, shape_pos);
  }
  else {
    world_pos += rotate(shape_pos, part.rotation);
  }
  out_pos = view.point_world_to_homogenous(world_pos);

  v_out.edge_start = v_out.edge_pos = ((out_pos.xy / out_pos.w) * 0.5f + 0.5f) *
                                      uni.uniform_buf.size_viewport;

  if (clip.constants.use_clipping) [[static_branch]] {
    clip.set_clipping_distances(world_pos,
                                clip_distances[0],
                                clip_distances[1],
                                clip_distances[2],
                                clip_distances[3],
                                clip_distances[4],
                                clip_distances[5]);
  }
}

}  // namespace wire

/* clang-format off */
PipelineGraphic shape(                   wire::vert_main, wire_frag, ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = false});
PipelineGraphic shape_clipped(           wire::vert_main, wire_frag, ClippingConstant{.use_clipping = true }, draw::SelectConstant{.selectable = false});
PipelineGraphic shape_selectable(        wire::vert_main, wire_frag, ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = true});
PipelineGraphic shape_selectable_clipped(wire::vert_main, wire_frag, ClippingConstant{.use_clipping = true }, draw::SelectConstant{.selectable = true});
/* clang-format on */

}  // namespace overlay::particle
