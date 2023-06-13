/**
 * Simple shader that just draw multiple icons at the specified locations
 * does not need any vertex input (producing less call to immBegin/End)
 */

void main()
{
  vec4 rect = multi_icon_data.calls_data[gl_InstanceID * 3];
  vec4 tex = multi_icon_data.calls_data[gl_InstanceID * 3 + 1];
  finalColor = multi_icon_data.calls_data[gl_InstanceID * 3 + 2];

  /* Use pos to select the right swizzle (instead of gl_VertexID)
   * in order to workaround an OSX driver bug. */
  if (all(equal(pos, vec2(0.0, 0.0)))) {
    rect.xy = rect.xz;
    tex.xy = tex.xz;
  }
  else if (all(equal(pos, vec2(0.0, 1.0)))) {
    rect.xy = rect.xw;
    tex.xy = tex.xw;
  }
  else if (all(equal(pos, vec2(1.0, 1.0)))) {
    rect.xy = rect.yw;
    tex.xy = tex.yw;
  }
  else {
    rect.xy = rect.yz;
    tex.xy = tex.yz;
  }

  gl_Position = vec4(rect.xy, 0.0f, 1.0f);
  texCoord_interp = tex.xy;
}
