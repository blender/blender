/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

#define pointSize mpathPointSettings.x
#define frameCurrent mpathPointSettings.y
#define cacheStart mpathPointSettings.z
#define stepSize mpathPointSettings.w

void main()
{
  gl_Position = drw_view.winmat * (drw_view.viewmat * (camera_space_matrix * vec4(pos, 1.0)));
  gl_PointSize = float(pointSize + 2);

  int frame = gl_VertexID + cacheStart;
  bool use_custom_color = customColorPre.x >= 0.0;
  finalColor = (use_custom_color) ? vec4(customColorPre, 1.0) : colorVertex;

  /* Bias to reduce z fighting with the path */
  gl_Position.z -= 1e-4;

  if (gl_VertexID % stepSize == 0) {
    gl_PointSize = float(pointSize) + 4;
  }

  /* Draw special dot where the current frame is. */
  if (frame == frameCurrent) {
    gl_PointSize = float(pointSize + 8);
    finalColor = colorCurrentFrame;
    /* Bias more to get these on top of keyframes */
    gl_Position.z -= 1e-4;
  }
  else if (frame < frameCurrent) {
    if (use_custom_color) {
      finalColor = vec4(customColorPre, 1.0);
    }
  }
  else {
    /* frame > frameCurrent */
    if (use_custom_color) {
      finalColor = vec4(customColorPost, 1.0);
    }
  }

  if (showKeyFrames) {
    /* Overrides the color to highlight points that are keyframes. */
    if ((flag & MOTIONPATH_VERT_KEY) != 0u) {
      gl_PointSize = float(pointSize + 5);
      finalColor = colorVertexSelect;
      /* Bias more to get these on top of regular points */
      gl_Position.z -= 1e-4;
    }
  }

  gl_PointSize *= sizePixel;

  view_clipping_distances(pos);
}
