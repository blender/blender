/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Draws a bunch of helper geometry which are supposed to be easily batched together.
 * Mostly lines and points.
 */

#pragma once

#include "draw_model.bsl.hh"
#include "draw_view.bsl.hh"
#include "gpu_shader_utildefines.bsl.hh"
#include "overlay_common.bsl.hh"
#include "select_lib.bsl.hh"

namespace overlay::extra {

struct VertIn {
  [[attribute(0)]] float3 pos;
  [[attribute(1)]] int vclass;
};

struct Resources {
  [[storage(0, read)]] ExtraInstanceData (&data_buf)[];
};

/**
 * Specialized vertex shader for many extra types.
 * The input geometry is supposed to have a vertex class specified.
 * Each class have different space / transformation rules.
 *
 * The goal is to be able to reduce draw calls and batch extras using instancing.
 */
[[vertex]]
void vert_main([[resource_table]] const Resources &srt,
               [[resource_table]] const Clipping &clip,
               [[resource_table]] const draw::View &views,
               [[resource_table]] const Uniform &uni,
               [[resource_table, condition(selectable)]] const draw::Select &sel,
               [[out, condition(selectable)]] draw::SelectOut &sel_out,
               [[instance_index]] const int inst_index,
               [[vertex_id]] const int vert_id,
               [[in]] const VertIn &v_in,
               [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
               [[out]] VertOut &v_out,
               [[position]] float4 &out_pos)
{
  if (sel.consts.selectable) [[static_branch]] {
    sel_out.select_id = sel.in_select_buf[inst_index];
  }

  ExtraInstanceData data = srt.data_buf[inst_index];
  float4x4 input_mat = data.object_to_world;
  float4 color = data.color_;
  /* Extract data packed inside the unused float4x4 members. */
  float4 inst_data = float4(input_mat[0][3], input_mat[1][3], input_mat[2][3], input_mat[3][3]);
  float inst_color_data = color.a;

  float4x4 obmat = input_mat;
  obmat[0][3] = 0.0f;
  obmat[1][3] = 0.0f;
  obmat[2][3] = 0.0f;
  obmat[3][3] = 1.0f;

  const ViewMatrices view = views.get(0);

  v_out.final_color = color;
  if (color.a < 0.0f) {
    v_out.final_color.a = 1.0f;
  }

  float2 lamp_area_size = inst_data.xy;
  float lamp_clip_sta = inst_data.z;
  float lamp_clip_end = inst_data.w;

  float lamp_spot_cosine = inst_data.x;
  float lamp_spot_blend = inst_data.y;

  float2 camera_corner = inst_data.xy;
  float2 camera_center = inst_data.zw;
  float camera_dist = color.a;
  float camera_dist_sta = inst_data.z;
  float camera_dist_end = inst_data.w;
  float camera_distance_color = inst_data.x;

  float3 empty_size = inst_data.xyz;
  float empty_scale = inst_data.w;

  float lamp_spot_sine;
  float3 vpos = v_in.pos;
  float3 vofs = float3(0.0f);
  /* Lights */
  if (flag_test(v_in.vclass, VCLASS_LIGHT_AREA_SHAPE)) {
    /* HACK: use alpha color for spots to pass the area_size. */
    if (inst_color_data < 0.0f) {
      lamp_area_size = float2(-inst_color_data);
    }
    vpos.xy *= lamp_area_size;
  }
  else if (flag_test(v_in.vclass, VCLASS_LIGHT_SPOT_SHAPE)) {
    lamp_spot_sine = sqrt(1.0f - lamp_spot_cosine * lamp_spot_cosine);
    lamp_spot_sine *= (flag_test(v_in.vclass, VCLASS_LIGHT_SPOT_BLEND)) ? lamp_spot_blend : 1.0f;
    vpos = float3(v_in.pos.xy * lamp_spot_sine, -lamp_spot_cosine);
  }
  else if (flag_test(v_in.vclass, VCLASS_LIGHT_DIST)) {
    /* Meh nasty mess. Select one of the 6 axes to display on. (see light_distance_z_get()) */
    int dist_axis = int(v_in.pos.z);
    float dist = v_in.pos.z - floor(v_in.pos.z) - 0.5f;
    float inv = sign(dist);
    dist = (abs(dist) > 0.15f) ? lamp_clip_end : lamp_clip_sta;
    vofs[dist_axis] = inv * dist / length(obmat[dist_axis].xyz);
    vpos.z = 0.0f;
    if (lamp_clip_end < 0.0f) {
      vpos = vofs = float3(0.0f);
    }
  }
  /* Camera */
  else if (flag_test(v_in.vclass, VCLASS_CAMERA_FRAME)) {
    if (flag_test(v_in.vclass, VCLASS_CAMERA_VOLUME)) {
      vpos.z = mix(color.b, color.a, v_in.pos.z);
    }
    else if (camera_dist > 0.0f) {
      vpos.z = -abs(camera_dist);
    }
    else {
      vpos.z *= -abs(camera_dist);
    }
    vpos.xy = (camera_center + camera_corner * vpos.xy) * abs(vpos.z);
  }
  else if (flag_test(v_in.vclass, VCLASS_CAMERA_DIST)) {
    vofs.xy = float2(0.0f);
    vofs.z = -mix(camera_dist_sta, camera_dist_end, v_in.pos.z);
    vpos.z = 0.0f;
    /* Distance line endpoints color */
    if (any(notEqual(v_in.pos.xy, float2(0.0f)))) {
      /* Override color. */
      switch (int(camera_distance_color)) {
        case 0: /* Mist */
          v_out.final_color = float4(0.5f, 0.5f, 0.5f, 1.0f);
          break;
        case 1: /* Mist Active */
          v_out.final_color = float4(1.0f, 1.0f, 1.0f, 1.0f);
          break;
        case 2: /* Clip */
          v_out.final_color = float4(0.5f, 0.5f, 0.25f, 1.0f);
          break;
        case 3: /* Clip Active */
          v_out.final_color = float4(1.0f, 1.0f, 0.5f, 1.0f);
          break;
      }
    }
    /* Focus cross */
    if (v_in.pos.z == 2.0f) {
      vofs.z = 0.0f;
      if (camera_dist < 0.0f) {
        vpos.z = -abs(camera_dist);
      }
      else {
        /* Disabled */
        vpos = float3(0.0f);
      }
    }
  }
  /* Empties */
  else if (flag_test(v_in.vclass, VCLASS_EMPTY_SCALED)) {
    /* This is a bit silly but we avoid scaling the object matrix on CPU (saving a float4x4 mul) */
    vpos *= empty_scale;
  }
  else if (flag_test(v_in.vclass, VCLASS_EMPTY_SIZE)) {
    /* This is a bit silly but we avoid scaling the object matrix on CPU (saving a float4x4 mul) */
    vpos *= empty_size;
  }
  else if (flag_test(v_in.vclass, VCLASS_EMPTY_AXES)) {
    float axis = vpos.z;
    vofs[int(axis)] = (1.0f + fract(axis)) * empty_scale;
    /* Scale uniformly by axis length */
    vpos *= length(obmat[int(axis)].xyz) * empty_scale;

    float3 axis_color = float3(0.0f);
    axis_color[int(axis)] = 1.0f;
    v_out.final_color.rgb = mix(axis_color + fract(axis), color.rgb, color.a);
    v_out.final_color.a = 1.0f;
  }

  /* Not exclusive with previous flags. */
  if (flag_test(v_in.vclass, VCLASS_CAMERA_VOLUME)) {
    /* Unpack final color. */
    int color_class = int(floor(color.r));
    float color_intensity = fract(color.r);
    switch (color_class) {
      case 0: /* No eye (convergence plane). */
        v_out.final_color = float4(1.0f, 1.0f, 1.0f, 1.0f);
        break;
      case 1: /* Left eye. */
        v_out.final_color = float4(0.0f, 1.0f, 1.0f, 1.0f);
        break;
      case 2: /* Right eye. */
        v_out.final_color = float4(1.0f, 0.0f, 0.0f, 1.0f);
        break;
    }
    v_out.final_color *= float4(float3(color_intensity), color.g);
  }

  float3 world_pos;
  if (flag_test(v_in.vclass, VCLASS_SCREENSPACE)) {
    /* Relative to DPI scaling. Have constant screen size. */
    float3 screen_pos = view.viewinv[0].xyz * vpos.x + view.viewinv[1].xyz * vpos.y;
    float3 p = (obmat * float4(vofs, 1.0f)).xyz;
    float screen_size = mul_project_m4_v3_zfac(view, uni.uniform_buf.pixel_fac, p) *
                        uni.uniform_buf.sizes.pixel;
    world_pos = p + screen_pos * screen_size;
  }
  else if (flag_test(v_in.vclass, VCLASS_SCREENALIGNED)) {
    /* World sized, camera facing geometry. */
    float3 screen_pos = view.viewinv[0].xyz * vpos.x + view.viewinv[1].xyz * vpos.y;
    world_pos = (obmat * float4(vofs, 1.0f)).xyz + screen_pos;
  }
  else {
    world_pos = (obmat * float4(vofs + vpos, 1.0f)).xyz;
  }

  if (flag_test(v_in.vclass, VCLASS_LIGHT_SPOT_CONE)) {
    /* Compute point on the cone before and after this one. */
    float2 perp = float2(v_in.pos.y, -v_in.pos.x);
    constexpr float incr_angle = 2.0f * 3.1415f / 32.0f;
    const float2 slope = float2(cos(incr_angle), sin(incr_angle));
    float3 p0 = float3((v_in.pos.xy * slope.x + perp * slope.y) * lamp_spot_sine,
                       -lamp_spot_cosine);
    float3 p1 = float3((v_in.pos.xy * slope.x - perp * slope.y) * lamp_spot_sine,
                       -lamp_spot_cosine);
    p0 = (obmat * float4(p0, 1.0f)).xyz;
    p1 = (obmat * float4(p1, 1.0f)).xyz;
    /* Compute normals of each side. */
    float3 edge = obmat[3].xyz - world_pos;
    float3 n0 = normalize(cross(edge, p0 - world_pos));
    float3 n1 = normalize(cross(edge, world_pos - p1));
    bool persp = (view.winmat[3][3] == 0.0f);
    float3 V = (persp) ? normalize(view.viewinv[3].xyz - world_pos) : view.viewinv[2].xyz;
    /* Discard non-silhouette edges. */
    bool facing0 = dot(n0, V) > 0.0f;
    bool facing1 = dot(n1, V) > 0.0f;
    if (facing0 == facing1) {
      /* Hide line by making it cover 0 pixels. */
      world_pos = obmat[3].xyz;
    }
  }

  out_pos = view.point_world_to_homogenous(world_pos);

  v_out.edge_start = v_out.edge_pos = ((out_pos.xy / out_pos.w) * 0.5f + 0.5f) *
                                      uni.uniform_buf.size_viewport;

  if (sel.consts.selectable) [[static_branch]] {
    /* HACK: to avoid losing sub-pixel object in selections, we add a bit of randomness to the
     * wire to at least create one fragment that will pass the occlusion query. */
    /* TODO(fclem): Limit this workaround to selection. It's not very noticeable but still... */
    out_pos.xy += uni.uniform_buf.size_viewport_inv * out_pos.w *
                  ((vert_id % 2 == 0) ? -1.0f : 1.0f);
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

struct GroundLines {
  [[storage(0, read)]] float4 (&data_buf)[];
};

/**
 * Draw a line between a given position and its projection onto the Z=0 plane.
 *
 * TODO(fclem): To be merged with the regular extra shader.
 */
[[vertex]]
void vert_ground_line([[resource_table]] const GroundLines &srt,
                      [[resource_table]] const Clipping &clip,
                      [[resource_table]] const draw::View &views,
                      [[resource_table]] const Uniform &uni,
                      [[resource_table, condition(selectable)]] const draw::Select &sel,
                      [[out, condition(selectable)]] draw::SelectOut &sel_out,
                      [[instance_index]] const int inst_index,
                      [[in]] const VertIn &v_in,
                      [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
                      [[out]] VertOut &v_out,
                      [[position]] float4 &out_pos)
{
  if (sel.consts.selectable) [[static_branch]] {
    sel_out.select_id = sel.in_select_buf[inst_index];
  }

  v_out.final_color = uni.uniform_buf.colors.light;

  const ViewMatrices view = views.get(0);

  /* Relative to DPI scaling. Have constant screen size. */
  float3 screen_pos = view.viewinv[0].xyz * v_in.pos.x + view.viewinv[1].xyz * v_in.pos.y;
  float3 inst_pos = srt.data_buf[inst_index].xyz;
  float3 p = inst_pos;
  p.z *= (v_in.pos.z == 0.0f) ? 0.0f : 1.0f;
  float screen_size = mul_project_m4_v3_zfac(view, uni.uniform_buf.pixel_fac, p) *
                      uni.uniform_buf.sizes.pixel;
  float3 world_pos = p + screen_pos * screen_size;

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

struct ExtraWire {
  [[storage(0, read)]] VertexData (&data_buf)[];
  [[push_constant]] int colorid;
};

/**
 * Extra loose wire batch.
 * Used for relation line drawing, tracking path etc...
 */
[[vertex]] void wire_main(
    [[resource_table]] const ExtraWire &srt,
    [[resource_table]] const Clipping &clip,
    [[resource_table]] const draw::Model &models,
    [[resource_table]] const draw::View &views,
    [[resource_table]] const Uniform &uni,
    [[resource_table, condition(selectable)]] const draw::Select &sel,
    [[resource_table, condition(!selectable)]] const draw::Resource &resources,
    [[resource_table, condition(selectable)]] const draw::ResourceCustomID &resources_with_id,
    [[out, condition(selectable)]] draw::SelectOut &sel_out,
    [[instance_index]] const int inst_index,
    [[vertex_id]] const int vert_id,
    [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
    [[out]] VertOut &v_out,
    [[position]] float4 &out_pos)
{
  if (sel.consts.selectable) [[static_branch]] {
    sel_out.select_id = sel.in_select_buf[inst_index];
  }

  uint resource_id;
  if (sel.consts.selectable) [[static_branch]] {
    resource_id = resources_with_id.get(inst_index).resource_id<1>();
  }
  else {
    resource_id = resources.get(inst_index).resource_id<1>();
  }

  const UniformData &theme = uni.uniform_buf;
  ObjectMatrices obj = models.get(resource_id);
  ViewMatrices view = views.get(0);

  VertexData v_in = srt.data_buf[vert_id];

  float3 world_pos = obj.point_object_to_world(v_in.pos_.xyz);
  out_pos = view.point_world_to_homogenous(world_pos);

  if (sel.consts.selectable) [[static_branch]] {
    /* HACK: to avoid losing sub-pixel object in selections, we add a bit of randomness to the
     * wire to at least create one fragment that will pass the occlusion query. */
    out_pos.xy += uni.uniform_buf.size_viewport_inv * out_pos.w *
                  ((vert_id % 2 == 0) ? -1.0f : 1.0f);
  }

  v_out.edge_start = v_out.edge_pos = ((out_pos.xy / out_pos.w) * 0.5f + 0.5f) *
                                      uni.uniform_buf.size_viewport;

  if (srt.colorid != 0) {
    /* TH_CAMERA_PATH is the only color code at the moment.
     * Checking `colorid != 0` to avoid having to sync its value with the GLSL code. */
    v_out.final_color = theme.colors.camera_path;
    v_out.final_color.a = 0.0f; /* No Stipple */
  }
  else {
    v_out.final_color = v_in.color_;
    v_out.final_color.a = 1.0f; /* Stipple */
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

/**
 * Extra loose wire batch.
 * Used for Hook modifier center drawing.
 */
[[vertex]] void loose_point_vert(
    [[resource_table]] const ExtraWire &srt,
    [[resource_table]] const Clipping &clip,
    [[resource_table]] const draw::Model &models,
    [[resource_table]] const draw::View &views,
    [[resource_table]] const Uniform &uni,
    [[resource_table, condition(selectable)]] const draw::Select &sel,
    [[resource_table, condition(!selectable)]] const draw::Resource &resources,
    [[resource_table, condition(selectable)]] const draw::ResourceCustomID &resources_with_id,
    [[out, condition(selectable)]] draw::SelectOut &sel_out,
    [[instance_index]] const int inst_index,
    [[vertex_id]] const int vert_id,
    [[point_size]] float &out_pt_size,
    [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
    [[out]] VertOut &v_out,
    [[position]] float4 &out_pos)
{
  if (sel.consts.selectable) [[static_branch]] {
    sel_out.select_id = sel.in_select_buf[inst_index];
  }

  uint resource_id;
  if (sel.consts.selectable) [[static_branch]] {
    resource_id = resources_with_id.get(inst_index).resource_id<1>();
  }
  else {
    resource_id = resources.get(inst_index).resource_id<1>();
  }

  const UniformData &theme = uni.uniform_buf;
  ObjectMatrices obj = models.get(resource_id);
  ViewMatrices view = views.get(0);

  VertexData v_in = srt.data_buf[vert_id];

  v_out.final_color = v_in.color_;
  v_out.edge_start = v_out.edge_pos = float2(0.0f); /* Unused, but needs to be written. */

  float3 world_pos = (obj.model * float4(v_in.pos_.xyz, 1.0f)).xyz;
  out_pos = view.point_world_to_homogenous(world_pos);

  out_pt_size = theme.sizes.vert * 2.0f;

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

[[fragment]] void loose_point_frag([[resource_table, condition(selectable)]] draw::Select &sel,
                                   [[resource_table]] const Uniform &uni,
                                   [[frag_coord]] const float4 frag_co,
                                   [[in]] const VertOut &v_out,
                                   [[out]] FragOut &frag_out,
                                   [[in, condition(selectable)]] const draw::SelectOut &sel_out,
                                   [[point_coord]] const float2 point_co)
{
  const UniformData &theme = uni.uniform_buf;

  float2 centered = abs(point_co - float2(0.5f));
  float dist = max(centered.x, centered.y);

  float fac = square(dist) * 4.0f;
  /* Non linear blend. */
  float4 col1 = sqrt(theme.colors.edit_mesh_middle);
  float4 col2 = sqrt(v_out.final_color);
  frag_out.color = mix(col1, col2, 0.45f + fac * 0.65f);
  frag_out.color *= frag_out.color;

  frag_out.line_output = float4(0.0f);

  /* Make the effect more like a fresnel by offsetting
   * the depth and creating mini-spheres.
   * Disabled as it has performance impact. */
  // out_depth = frag_co.z + 1e-6f * fac;

  if (sel.consts.selectable) [[static_branch]] {
    sel.select_id_output(sel_out.select_id, frag_co);
  }
}

/* Extra points. Usually used for object center.  */
[[vertex]] void point_outline_main(
    [[resource_table]] const ExtraWire &srt,
    [[resource_table]] const Clipping &clip,
    [[resource_table]] const draw::Model &models,
    [[resource_table]] const draw::View &views,
    [[resource_table]] const Uniform &uni,
    [[resource_table, condition(selectable)]] const draw::Select &sel,
    [[resource_table, condition(!selectable)]] const draw::Resource &resources,
    [[resource_table, condition(selectable)]] const draw::ResourceCustomID &resources_with_id,
    [[out, condition(selectable)]] draw::SelectOut &sel_out,
    [[instance_index]] const int inst_index,
    [[vertex_id]] const int vert_id,
    [[point_size]] float &out_pt_size,
    [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
    [[out]] VertOutPointOutline &v_out,
    [[position]] float4 &out_pos)
{
  if (sel.consts.selectable) [[static_branch]] {
    sel_out.select_id = sel.in_select_buf[vert_id];
  }

  uint resource_id;
  if (sel.consts.selectable) [[static_branch]] {
    resource_id = resources_with_id.get(inst_index).resource_id<1>();
  }
  else {
    resource_id = resources.get(inst_index).resource_id<1>();
  }

  const UniformData &theme = uni.uniform_buf;
  ObjectMatrices obj = models.get(resource_id);
  ViewMatrices view = views.get(0);

  VertexData v_in = srt.data_buf[vert_id];

  float3 world_pos = obj.point_object_to_world(v_in.pos_.xyz);
  out_pos = view.point_world_to_homogenous(world_pos);

  out_pt_size = theme.sizes.object_center;
  float radius = 0.5f * theme.sizes.object_center;
  float outline_width = theme.sizes.pixel;
  v_out.radii[0] = radius;
  v_out.radii[1] = radius - 1.0f;
  v_out.radii[2] = radius - outline_width;
  v_out.radii[3] = radius - outline_width - 1.0f;
  v_out.radii /= theme.sizes.object_center;

  v_out.inner_color = v_in.color_;
  v_out.outline_color = theme.colors.outline;

  if (sel.consts.selectable) [[static_branch]] {
    /* Selection frame-buffer can be very small.
     * Make sure to only rasterize one pixel to avoid making the selection radius very big. */
    out_pt_size = 1.0f;
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

[[vertex]] void lattice_main(
    [[resource_table]] const Uniform &uni,
    [[resource_table]] const Clipping &clip,
    [[resource_table]] const draw::Model &models,
    [[resource_table]] const draw::View &views,
    [[resource_table, condition(selectable)]] const draw::Select &sel,
    [[resource_table, condition(!selectable)]] const draw::Resource &resources,
    [[resource_table, condition(selectable)]] const draw::ResourceCustomID &resources_with_id,
    [[out, condition(selectable)]] draw::SelectOut &sel_out,
    [[instance_index]] const int inst_index,
    [[vertex_id]] const int vert_id,
    [[in]] const VertIn &v_in,
    [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
    [[out]] VertOut &v_out,
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

  ObjectMatrices obj = models.get(resource_id);
  ViewMatrices view = views.get(0);

  float3 world_pos = obj.point_object_to_world(v_in.pos);
  out_pos = view.point_world_to_homogenous(world_pos);

  if (sel.consts.selectable) [[static_branch]] {
    /* HACK: to avoid losing sub-pixel object in selections, we add a bit of randomness to the
     * wire to at least create one fragment that will pass the occlusion query. */
    out_pos.xy += uni.uniform_buf.size_viewport_inv * out_pos.w *
                  ((vert_id % 2 == 0) ? -1.0f : 1.0f);
  }

  v_out.edge_start = v_out.edge_pos = ((out_pos.xy / out_pos.w) * 0.5f + 0.5f) *
                                      uni.uniform_buf.size_viewport;

  /* Extract data packed inside the unused float4x4 members. */
  v_out.final_color = float4(obj.model[0][3], obj.model[1][3], obj.model[2][3], obj.model[3][3]);

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

}  // namespace overlay::extra

/* clang-format off */
PipelineGraphic overlay_extra(                   overlay::extra::vert_main, overlay::wire_frag, overlay::ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = false});
PipelineGraphic overlay_extra_clipped(           overlay::extra::vert_main, overlay::wire_frag, overlay::ClippingConstant{.use_clipping = true},  draw::SelectConstant{.selectable = false});
PipelineGraphic overlay_extra_selectable(        overlay::extra::vert_main, overlay::wire_frag, overlay::ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = true});
PipelineGraphic overlay_extra_selectable_clipped(overlay::extra::vert_main, overlay::wire_frag, overlay::ClippingConstant{.use_clipping = true},  draw::SelectConstant{.selectable = true});

PipelineGraphic overlay_extra_groundline(                   overlay::extra::vert_ground_line, overlay::wire_frag, overlay::ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = false});
PipelineGraphic overlay_extra_groundline_clipped(           overlay::extra::vert_ground_line, overlay::wire_frag, overlay::ClippingConstant{.use_clipping = true},  draw::SelectConstant{.selectable = false});
PipelineGraphic overlay_extra_groundline_selectable(        overlay::extra::vert_ground_line, overlay::wire_frag, overlay::ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = true});
PipelineGraphic overlay_extra_groundline_selectable_clipped(overlay::extra::vert_ground_line, overlay::wire_frag, overlay::ClippingConstant{.use_clipping = true},  draw::SelectConstant{.selectable = true});

/* NOTE: Spot cones are not selectable. */
PipelineGraphic overlay_extra_spot_cone(         overlay::extra::vert_main, overlay::wire_frag_no_aa, overlay::ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = false});
PipelineGraphic overlay_extra_spot_cone_clipped( overlay::extra::vert_main, overlay::wire_frag_no_aa, overlay::ClippingConstant{.use_clipping = true},  draw::SelectConstant{.selectable = false});

PipelineGraphic overlay_extra_wire(                   overlay::extra::wire_main, overlay::wire_frag_stipple, overlay::ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = false});
PipelineGraphic overlay_extra_wire_clipped(           overlay::extra::wire_main, overlay::wire_frag_stipple, overlay::ClippingConstant{.use_clipping = true},  draw::SelectConstant{.selectable = false});
PipelineGraphic overlay_extra_wire_selectable(        overlay::extra::wire_main, overlay::wire_frag_stipple, overlay::ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = true});
PipelineGraphic overlay_extra_wire_selectable_clipped(overlay::extra::wire_main, overlay::wire_frag_stipple, overlay::ClippingConstant{.use_clipping = true},  draw::SelectConstant{.selectable = true});

PipelineGraphic overlay_extra_point(                   overlay::extra::loose_point_vert, overlay::extra::loose_point_frag, overlay::ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = false});
PipelineGraphic overlay_extra_point_clipped(           overlay::extra::loose_point_vert, overlay::extra::loose_point_frag, overlay::ClippingConstant{.use_clipping = true},  draw::SelectConstant{.selectable = false});
PipelineGraphic overlay_extra_point_selectable(        overlay::extra::loose_point_vert, overlay::extra::loose_point_frag, overlay::ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = true});
PipelineGraphic overlay_extra_point_selectable_clipped(overlay::extra::loose_point_vert, overlay::extra::loose_point_frag, overlay::ClippingConstant{.use_clipping = true},  draw::SelectConstant{.selectable = true});

PipelineGraphic overlay_extra_point_outline(                   overlay::extra::point_outline_main, overlay::point_with_outline_frag, overlay::ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = false});
PipelineGraphic overlay_extra_point_outline_clipped(           overlay::extra::point_outline_main, overlay::point_with_outline_frag, overlay::ClippingConstant{.use_clipping = true},  draw::SelectConstant{.selectable = false});
PipelineGraphic overlay_extra_point_outline_selectable(        overlay::extra::point_outline_main, overlay::point_with_outline_frag, overlay::ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = true});
PipelineGraphic overlay_extra_point_outline_selectable_clipped(overlay::extra::point_outline_main, overlay::point_with_outline_frag, overlay::ClippingConstant{.use_clipping = true},  draw::SelectConstant{.selectable = true});

PipelineGraphic overlay_lattice(                    overlay::extra::lattice_main, overlay::wire_frag, overlay::ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = false});
PipelineGraphic overlay_lattice_clipped(            overlay::extra::lattice_main, overlay::wire_frag, overlay::ClippingConstant{.use_clipping = true},  draw::SelectConstant{.selectable = false});
PipelineGraphic overlay_lattice_selectable(         overlay::extra::lattice_main, overlay::wire_frag, overlay::ClippingConstant{.use_clipping = false}, draw::SelectConstant{.selectable = true});
PipelineGraphic overlay_lattice_selectable_clipped (overlay::extra::lattice_main, overlay::wire_frag, overlay::ClippingConstant{.use_clipping = true},  draw::SelectConstant{.selectable = true});
/* clang-format on */
