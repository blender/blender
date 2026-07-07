/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_extra_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_extra_spot_cone)

#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"
#include "overlay_common_lib.glsl"
#include "select_lib.glsl"

void main()
{
  select_id_set(in_select_buf[gl_InstanceID]);

  /* Loading the matrix first before doing the manipulation fixes an issue
   * with the Metal compiler on older Intel macs (see #130867). */
  float4x4 inst_obmat = data_buf[gl_InstanceID].object_to_world;
  float4x4 input_mat = inst_obmat;

  /* Extract data packed inside the unused float4x4 members. */
  float4 inst_data = float4(input_mat[0][3], input_mat[1][3], input_mat[2][3], input_mat[3][3]);
  float4 color = data_buf[gl_InstanceID].color_;
  float inst_color_data = color.a;
  float4x4 obmat = input_mat;
  obmat[0][3] = obmat[1][3] = obmat[2][3] = 0.0f;
  obmat[3][3] = 1.0f;

  final_color = color;
  if (color.a < 0.0f) {
    final_color.a = 1.0f;
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
  float3 vpos = pos;
  float3 vofs = float3(0.0f);
  /* Lights */
  if (flag_test(vclass, VCLASS_LIGHT_AREA_SHAPE)) {
    /* HACK: use alpha color for spots to pass the area_size. */
    if (inst_color_data < 0.0f) {
      lamp_area_size = float2(-inst_color_data);
    }
    vpos.xy *= lamp_area_size;
  }
  else if (flag_test(vclass, VCLASS_LIGHT_SPOT_SHAPE)) {
    lamp_spot_sine = sqrt(1.0f - lamp_spot_cosine * lamp_spot_cosine);
    lamp_spot_sine *= (flag_test(vclass, VCLASS_LIGHT_SPOT_BLEND)) ? lamp_spot_blend : 1.0f;
    vpos = float3(pos.xy * lamp_spot_sine, -lamp_spot_cosine);
  }
  else if (flag_test(vclass, VCLASS_LIGHT_DIST)) {
    /* Meh nasty mess. Select one of the 6 axes to display on. (see light_distance_z_get()) */
    int dist_axis = int(pos.z);
    float dist = pos.z - floor(pos.z) - 0.5f;
    float inv = sign(dist);
    dist = (abs(dist) > 0.15f) ? lamp_clip_end : lamp_clip_sta;
    vofs[dist_axis] = inv * dist / length(obmat[dist_axis].xyz);
    vpos.z = 0.0f;
    if (lamp_clip_end < 0.0f) {
      vpos = vofs = float3(0.0f);
    }
  }
  /* Camera */
  else if (flag_test(vclass, VCLASS_CAMERA_FRAME)) {
    if (flag_test(vclass, VCLASS_CAMERA_VOLUME)) {
      vpos.z = mix(color.b, color.a, pos.z);
    }
    else if (camera_dist > 0.0f) {
      vpos.z = -abs(camera_dist);
    }
    else {
      vpos.z *= -abs(camera_dist);
    }
    vpos.xy = (camera_center + camera_corner * vpos.xy) * abs(vpos.z);
  }
  else if (flag_test(vclass, VCLASS_CAMERA_DIST)) {
    vofs.xy = float2(0.0f);
    vofs.z = -mix(camera_dist_sta, camera_dist_end, pos.z);
    vpos.z = 0.0f;
    /* Distance line endpoints color */
    if (any(notEqual(pos.xy, float2(0.0f)))) {
      /* Override color. */
      switch (int(camera_distance_color)) {
        case 0: /* Mist */
          final_color = float4(0.5f, 0.5f, 0.5f, 1.0f);
          break;
        case 1: /* Mist Active */
          final_color = float4(1.0f, 1.0f, 1.0f, 1.0f);
          break;
        case 2: /* Clip */
          final_color = float4(0.5f, 0.5f, 0.25f, 1.0f);
          break;
        case 3: /* Clip Active */
          final_color = float4(1.0f, 1.0f, 0.5f, 1.0f);
          break;
      }
    }
    /* Focus cross */
    if (pos.z == 2.0f) {
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
  else if (flag_test(vclass, VCLASS_EMPTY_SCALED)) {
    /* This is a bit silly but we avoid scaling the object matrix on CPU (saving a float4x4 mul) */
    vpos *= empty_scale;
  }
  else if (flag_test(vclass, VCLASS_EMPTY_SIZE)) {
    /* This is a bit silly but we avoid scaling the object matrix on CPU (saving a float4x4 mul) */
    vpos *= empty_size;
  }
  else if (flag_test(vclass, VCLASS_EMPTY_AXES)) {
    float axis = vpos.z;
    vofs[int(axis)] = (1.0f + fract(axis)) * empty_scale;
    /* Scale uniformly by axis length */
    vpos *= length(obmat[int(axis)].xyz) * empty_scale;

    float3 axis_color = float3(0.0f);
    axis_color[int(axis)] = 1.0f;
    final_color.rgb = mix(axis_color + fract(axis), color.rgb, color.a);
    final_color.a = 1.0f;
  }

  /* Not exclusive with previous flags. */
  if (flag_test(vclass, VCLASS_CAMERA_VOLUME)) {
    /* Unpack final color. */
    int color_class = int(floor(color.r));
    float color_intensity = fract(color.r);
    switch (color_class) {
      case 0: /* No eye (convergence plane). */
        final_color = float4(1.0f, 1.0f, 1.0f, 1.0f);
        break;
      case 1: /* Left eye. */
        final_color = float4(0.0f, 1.0f, 1.0f, 1.0f);
        break;
      case 2: /* Right eye. */
        final_color = float4(1.0f, 0.0f, 0.0f, 1.0f);
        break;
    }
    final_color *= float4(float3(color_intensity), color.g);
  }

  float3 world_pos;
  if (flag_test(vclass, VCLASS_SCREENSPACE)) {
    /* Relative to DPI scaling. Have constant screen size. */
    float3 screen_pos = drw_view().viewinv[0].xyz * vpos.x + drw_view().viewinv[1].xyz * vpos.y;
    float3 p = (obmat * float4(vofs, 1.0f)).xyz;
    float screen_size = mul_project_m4_v3_zfac(uniform_buf.pixel_fac, p) * theme.sizes.pixel;
    world_pos = p + screen_pos * screen_size;
  }
  else if (flag_test(vclass, VCLASS_SCREENALIGNED)) {
    /* World sized, camera facing geometry. */
    float3 screen_pos = drw_view().viewinv[0].xyz * vpos.x + drw_view().viewinv[1].xyz * vpos.y;
    world_pos = (obmat * float4(vofs, 1.0f)).xyz + screen_pos;
  }
  else {
    world_pos = (obmat * float4(vofs + vpos, 1.0f)).xyz;
  }

  if (flag_test(vclass, VCLASS_LIGHT_SPOT_CONE)) {
    /* Compute point on the cone before and after this one. */
    float2 perp = float2(pos.y, -pos.x);
    constexpr float incr_angle = 2.0f * 3.1415f / 32.0f;
    const float2 slope = float2(cos(incr_angle), sin(incr_angle));
    float3 p0 = float3((pos.xy * slope.x + perp * slope.y) * lamp_spot_sine, -lamp_spot_cosine);
    float3 p1 = float3((pos.xy * slope.x - perp * slope.y) * lamp_spot_sine, -lamp_spot_cosine);
    p0 = (obmat * float4(p0, 1.0f)).xyz;
    p1 = (obmat * float4(p1, 1.0f)).xyz;
    /* Compute normals of each side. */
    float3 edge = obmat[3].xyz - world_pos;
    float3 n0 = normalize(cross(edge, p0 - world_pos));
    float3 n1 = normalize(cross(edge, world_pos - p1));
    bool persp = (drw_view().winmat[3][3] == 0.0f);
    float3 V = (persp) ? normalize(drw_view().viewinv[3].xyz - world_pos) :
                         drw_view().viewinv[2].xyz;
    /* Discard non-silhouette edges. */
    bool facing0 = dot(n0, V) > 0.0f;
    bool facing1 = dot(n1, V) > 0.0f;
    if (facing0 == facing1) {
      /* Hide line by making it cover 0 pixels. */
      world_pos = obmat[3].xyz;
    }
  }

  gl_Position = drw_point_world_to_homogenous(world_pos);

  /* Convert to screen position [0..sizeVp]. */
  edge_pos = edge_start = ((gl_Position.xy / gl_Position.w) * 0.5f + 0.5f) *
                          uniform_buf.size_viewport;

#if defined(SELECT_ENABLE)
  /* HACK: to avoid losing sub-pixel object in selections, we add a bit of randomness to the
   * wire to at least create one fragment that will pass the occlusion query. */
  /* TODO(fclem): Limit this workaround to selection. It's not very noticeable but still... */
  gl_Position.xy += uniform_buf.size_viewport_inv * gl_Position.w *
                    ((gl_VertexID % 2 == 0) ? -1.0f : 1.0f);
#endif

  view_clipping_distances(world_pos);
}
