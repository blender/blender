/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_view_clipping_lib.glsl"
#include "common_view_lib.glsl"

#define frameCurrent mpathLineSettings.x
#define frameStart mpathLineSettings.y
#define frameEnd mpathLineSettings.z
#define cacheStart mpathLineSettings.w

/* project to screen space */
vec2 proj(vec4 pos)
{
  return (0.5 * (pos.xy / pos.w) + 0.5) * sizeViewport.xy;
}

void main()
{
  gl_Position = drw_view.winmat * (drw_view.viewmat * (camera_space_matrix * vec4(pos, 1.0)));

  interp_flat.ss_pos = proj(gl_Position);

  int frame = gl_VertexID + cacheStart;

  vec3 blend_base = (abs(frame - frameCurrent) == 0) ?
                        colorCurrentFrame.rgb :
                        colorBackground.rgb; /* "bleed" CFRAME color to ease color blending */
  bool use_custom_color = customColorPre.x >= 0.0;

  if (frame < frameCurrent) {
    if (use_custom_color) {
      interp.color.rgb = customColorPre;
    }
    else {
      interp.color.rgb = colorBeforeFrame.rgb;
    }
  }
  else if (frame > frameCurrent) {
    if (use_custom_color) {
      interp.color.rgb = customColorPost;
    }
    else {
      interp.color.rgb = colorAfterFrame.rgb;
    }
  }
  else {
    /* Current Frame. */
    if (use_custom_color) {
      interp.color.rgb = colorCurrentFrame.rgb;
    }
    else {
      interp.color.rgb = blend_base;
    }
  }

  interp.color.a = 1.0;

  view_clipping_distances(pos);
}
