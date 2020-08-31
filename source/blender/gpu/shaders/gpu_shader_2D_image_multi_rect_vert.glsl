/**
 * Simple shader that just draw multiple icons at the specified locations
 * does not need any vertex input (producing less call to immBegin/End)
 */

/* Same as ICON_DRAW_CACHE_SIZE */
#define MAX_CALLS 16

uniform vec4 calls_data[MAX_CALLS * 3];

out vec2 texCoord_interp;
flat out vec4 finalColor;

void main()
{
  vec4 pos = calls_data[gl_InstanceID * 3];
  vec4 tex = calls_data[gl_InstanceID * 3 + 1];
  finalColor = calls_data[gl_InstanceID * 3 + 2];

  if (gl_VertexID == 0) {
    pos.xy = pos.xz;
    tex.xy = tex.xz;
  }
  else if (gl_VertexID == 1) {
    pos.xy = pos.xw;
    tex.xy = tex.xw;
  }
  else if (gl_VertexID == 2) {
    pos.xy = pos.yw;
    tex.xy = tex.yw;
  }
  else {
    pos.xy = pos.yz;
    tex.xy = tex.yz;
  }

  gl_Position = vec4(pos.xy, 0.0f, 1.0f);
  texCoord_interp = tex.xy;
}
