/**
 * Simple shader that just draw one icon at the specified location
 * does not need any vertex input (producing less call to immBegin/End)
 */

#ifndef USE_GPU_SHADER_CREATE_INFO
uniform mat4 ModelViewProjectionMatrix;
uniform vec4 rect_icon;
uniform vec4 rect_geom;

out vec2 texCoord_interp;
#endif

void main()
{
  vec2 uv;
  vec2 co;

#ifdef GPU_METAL
/* Metal API does not support Triangle fan primitive topology.
 * When this shader is called using Triangle-Strip, vertex ID's
 * are in a different order. */
#  define GPU_PRIM_TRI_STRIP
#endif

#ifdef GPU_PRIM_TRI_STRIP
  if (gl_VertexID == 0) {
    co = rect_geom.xw;
    uv = rect_icon.xw;
  }
  else if (gl_VertexID == 1) {
    co = rect_geom.xy;
    uv = rect_icon.xy;
  }
  else if (gl_VertexID == 2) {
    co = rect_geom.zw;
    uv = rect_icon.zw;
  }
  else {
    co = rect_geom.zy;
    uv = rect_icon.zy;
  }
#else
  if (gl_VertexID == 0) {
    co = rect_geom.xy;
    uv = rect_icon.xy;
  }
  else if (gl_VertexID == 1) {
    co = rect_geom.xw;
    uv = rect_icon.xw;
  }
  else if (gl_VertexID == 2) {
    co = rect_geom.zw;
    uv = rect_icon.zw;
  }
  else {
    co = rect_geom.zy;
    uv = rect_icon.zy;
  }
#endif

  gl_Position = ModelViewProjectionMatrix * vec4(co, 0.0f, 1.0f);
  texCoord_interp = uv;
}
