/**
 * Simple shader that just draw multiple icons at the specified locations
 * does not need any vertex input (producing less call to immBegin/End)
 */

/* Same as ICON_DRAW_CACHE_SIZE */
#ifndef USE_GPU_SHADER_CREATE_INFO
#  define MAX_CALLS 16

uniform vec4 calls_data[MAX_CALLS * 3];

out vec2 texCoord_interp;
flat out vec4 finalColor;

in vec2 pos;
#endif

void main()
{
  vec4 rect = multi_rect_data.calls_data[gl_InstanceID * 3];
  vec4 tex = multi_rect_data.calls_data[gl_InstanceID * 3 + 1];
  finalColor = multi_rect_data.calls_data[gl_InstanceID * 3 + 2];

  /* Use pos to select the right swizzle (instead of gl_VertexID)
   * in order to workaround an OSX driver bug. */
  if (pos == vec2(0.0, 0.0)) {
    rect.xy = rect.xz;
    tex.xy = tex.xz;
  }
  else if (pos == vec2(0.0, 1.0)) {
    rect.xy = rect.xw;
    tex.xy = tex.xw;
  }
  else if (pos == vec2(1.0, 1.0)) {
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
