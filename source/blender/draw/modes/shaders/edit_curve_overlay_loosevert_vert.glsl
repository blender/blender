/* Draw Curve Vertices */

uniform vec2 viewportSize;

in vec3 pos;
in int data;

out vec4 finalColor;

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  if ((data & VERT_SELECTED) != 0) {
    if ((data & VERT_ACTIVE) != 0) {
      finalColor = colorEditMeshActive;
    }
    else {
      finalColor = colorVertexSelect;
    }
  }
  else {
    finalColor = colorVertex;
  }

  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);
  gl_PointSize = sizeVertex * 2.0;
#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
