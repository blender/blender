
uniform ivec4 mpathPointSettings;
uniform bool showKeyFrames = true;
uniform vec3 customColor;

#define pointSize mpathPointSettings.x
#define frameCurrent mpathPointSettings.y
#define cacheStart mpathPointSettings.z
#define stepSize mpathPointSettings.w

in vec3 pos;
in int flag;

#define MOTIONPATH_VERT_SEL (1 << 0)
#define MOTIONPATH_VERT_KEY (1 << 1)

out vec4 finalColor;

void main()
{
  gl_Position = ViewProjectionMatrix * vec4(pos, 1.0);
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
    if ((flag & MOTIONPATH_VERT_KEY) != 0) {
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

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(pos);
#endif
}
