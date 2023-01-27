
#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

#define pointSize mpathPointSettings.x
#define frameCurrent mpathPointSettings.y
#define cacheStart mpathPointSettings.z
#define stepSize mpathPointSettings.w

void main()
{
  gl_Position = drw_view.winmat * (drw_view.viewmat * vec4(pos, 1.0));
  gl_PointSize = float(pointSize + 2);

  int frame = gl_VertexID + cacheStart;
  bool use_custom_color = customColor.x >= 0.0;
  finalColor = (use_custom_color) ? vec4(customColor, 1.0) : vec4(1.0);

  /* Bias to reduce z fighting with the path */
  gl_Position.z -= 1e-4;

  if (gl_VertexID % stepSize == 0) {
    gl_PointSize = float(pointSize) + 4;
  }

  if (showKeyFrames) {
    if ((flag & MOTIONPATH_VERT_KEY) != 0u) {
      gl_PointSize = float(pointSize + 5);
      finalColor = colorVertexSelect;
      /* Bias more to get these on top of regular points */
      gl_Position.z -= 1e-4;
    }
    /* Draw big green dot where the current frame is.
     * NOTE: this is only done when keyframes are shown, since this adds similar types of clutter
     */
    if (frame == frameCurrent) {
      gl_PointSize = float(pointSize + 8);
      finalColor = colorCurrentFrame;
      /* Bias more to get these on top of keyframes */
      gl_Position.z -= 1e-4;
    }
  }

  gl_PointSize *= sizePixel;

  view_clipping_distances(pos);
}
