
uniform ivec4 mpathLineSettings;
uniform bool selected;
uniform vec3 customColor;

#define frameCurrent mpathLineSettings.x
#define frameStart mpathLineSettings.y
#define frameEnd mpathLineSettings.z
#define cacheStart mpathLineSettings.w

in vec3 pos;

out vec2 ssPos;
out vec4 finalColor_geom;

/* project to screen space */
vec2 proj(vec4 pos)
{
  return (0.5 * (pos.xy / pos.w) + 0.5) * sizeViewport.xy;
}

#define SET_INTENSITY(A, B, C, min, max) \
  (((1.0 - (float(C - B) / float(C - A))) * (max - min)) + min)

void main()
{
  gl_Position = ViewProjectionMatrix * vec4(pos, 1.0);

  ssPos = proj(gl_Position);

  int frame = gl_VertexID + cacheStart;

  float intensity; /* how faint */

  vec3 blend_base = (abs(frame - frameCurrent) == 0) ?
                        colorCurrentFrame.rgb :
                        colorBackground.rgb; /* "bleed" cframe color to ease color blending */
  bool use_custom_color = customColor.x >= 0.0;
  /* TODO: We might want something more consistent with custom color and standard colors. */
  if (frame < frameCurrent) {
    if (use_custom_color) {
      /* Custom color: previous frames color is darker than current frame */
      finalColor_geom.rgb = customColor * 0.25;
    }
    else {
      /* black - before frameCurrent */
      if (selected) {
        intensity = SET_INTENSITY(frameStart, frame, frameCurrent, 0.25, 0.75);
      }
      else {
        intensity = SET_INTENSITY(frameStart, frame, frameCurrent, 0.68, 0.92);
      }
      finalColor_geom.rgb = mix(colorWire.rgb, blend_base, intensity);
    }
  }
  else if (frame > frameCurrent) {
    if (use_custom_color) {
      /* Custom color: next frames color is equal to user selected color */
      finalColor_geom.rgb = customColor;
    }
    else {
      /* blue - after frameCurrent */
      if (selected) {
        intensity = SET_INTENSITY(frameCurrent, frame, frameEnd, 0.25, 0.75);
      }
      else {
        intensity = SET_INTENSITY(frameCurrent, frame, frameEnd, 0.68, 0.92);
      }

      finalColor_geom.rgb = mix(colorBonePose.rgb, blend_base, intensity);
    }
  }
  else {
    if (use_custom_color) {
      /* Custom color: current frame color is slightly darker than user selected color */
      finalColor_geom.rgb = customColor * 0.5;
    }
    else {
      /* green - on frameCurrent */
      if (selected) {
        intensity = 0.92f;
      }
      else {
        intensity = 0.75f;
      }
      finalColor_geom.rgb = mix(colorBackground.rgb, blend_base, intensity);
    }
  }

  finalColor_geom.a = 1.0;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(pos);
#endif
}
