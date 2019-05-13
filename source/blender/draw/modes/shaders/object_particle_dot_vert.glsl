
uniform float pixel_size;
uniform float size;

in vec3 pos;
in float val;

out vec4 radii;
flat out float finalVal;

void main()
{
  vec3 world_pos = point_object_to_world(pos);

  float view_z = dot(ViewMatrixInverse[2].xyz, world_pos - ViewMatrixInverse[3].xyz);

  bool is_persp = (ProjectionMatrix[3][3] == 0.0);
  float psize = (is_persp) ? (size / (-view_z * pixel_size)) : (size / pixel_size);
  gl_Position = point_world_to_ndc(world_pos);

  gl_PointSize = psize;

  // calculate concentric radii in pixels
  float radius = 0.5 * psize;

  // start at the outside and progress toward the center
  radii[0] = radius;
  radii[1] = radius - 1.0;
  radii[2] = radius - 1.0;
  radii[3] = radius - 2.0;

  // convert to PointCoord units
  radii /= psize;

  finalVal = val;
}
