/* Draw Lattice Vertices */

uniform vec2 viewportSize;

in vec3 pos;
in int data;

/* these are the same for all vertices
 * and does not need interpolation */
flat out int vertFlag;
flat out int clipCase;

/* See fragment shader */
noperspective out vec4 eData1;
flat out vec4 eData2;

/* project to screen space */
vec2 proj(vec4 pos)
{
  return (0.5 * (pos.xy / pos.w) + 0.5) * viewportSize;
}

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  clipCase = 0;

  vec3 world_pos = point_object_to_world(pos);
  vec4 pPos = point_world_to_ndc(world_pos);

  /* only vertex position 0 is used */
  eData1 = eData2 = vec4(1e10);
  eData2.zw = proj(pPos);

  vertFlag = data;

  gl_PointSize = sizeVertex * 2.0;
  gl_Position = pPos;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
