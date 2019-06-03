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
  /* Rendering 2 triangle per icon. */
  int i = gl_VertexID / 6;
  int v = gl_VertexID % 6;

  vec4 pos = calls_data[i * 3];
  vec4 tex = calls_data[i * 3 + 1];
  finalColor = calls_data[i * 3 + 2];

  /* TODO Remove this */
  if (v == 2) {
    v = 4;
  }
  else if (v == 3) {
    v = 0;
  }
  else if (v == 5) {
    v = 2;
  }

  if (v == 0) {
    pos.xy = pos.xw;
    tex.xy = tex.xw;
  }
  else if (v == 1) {
    pos.xy = pos.xz;
    tex.xy = tex.xz;
  }
  else if (v == 2) {
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
