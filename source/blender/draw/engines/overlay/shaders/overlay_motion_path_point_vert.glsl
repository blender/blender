/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_extra_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_motion_path_point)

#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  int pt_size = mpath_point_settings.x;
  int frameCurrent = mpath_point_settings.y;
  int cacheStart = mpath_point_settings.z;
  int stepSize = mpath_point_settings.w;

  gl_Position = drw_view().winmat *
                (drw_view().viewmat * (camera_space_matrix * float4(pos, 1.0f)));
  gl_PointSize = float(pt_size + 2);

  int frame = gl_VertexID + cacheStart;
  bool use_custom_color = custom_color_pre.x >= 0.0f;
  final_color = (use_custom_color) ? float4(custom_color_pre, 1.0f) : theme.colors.vert;

  /* Bias to reduce z fighting with the path */
  gl_Position.z -= 1e-4f;

  if (gl_VertexID % stepSize == 0) {
    gl_PointSize = float(pt_size) + 4;
  }

  /* Draw special dot where the current frame is. */
  if (frame == frameCurrent) {
    gl_PointSize = float(pt_size + 8);
    final_color = theme.colors.current_frame;
    /* Bias more to get these on top of keyframes */
    gl_Position.z -= 1e-4f;
  }
  else if (frame < frameCurrent) {
    if (use_custom_color) {
      final_color = float4(custom_color_pre, 1.0f);
    }
  }
  else {
    /* frame > frameCurrent */
    if (use_custom_color) {
      final_color = float4(custom_color_post, 1.0f);
    }
  }

  if (show_key_frames) {
    /* Overrides the color to highlight points that are keyframes. */
    if ((uint(flag) & MOTIONPATH_VERT_KEY) != 0u) {
      gl_PointSize = float(pt_size + 5);
      final_color = theme.colors.vert_select;
      /* Bias more to get these on top of regular points */
      gl_Position.z -= 1e-4f;
    }
  }

  gl_PointSize *= theme.sizes.pixel;

  view_clipping_distances(pos);
}
