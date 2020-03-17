
uniform vec4 color;

/* ---- Per instance Attrs ---- */
in vec3 pointcloud_pos;
in vec3 pointcloud_radius;

out vec4 finalColor;

void main()
{
  vec3 world_pos = point_object_to_world(pointcloud_pos);

  vec3 world_size = abs(mat3(ModelMatrix) * vec3(pointcloud_radius));
  float world_radius = (world_size.x + world_size.y + world_size.z) / 3.0;

  gl_Position = point_world_to_ndc(world_pos);
  /* World sized points. */
  gl_PointSize = sizePixel * world_radius * ProjectionMatrix[1][1] * sizeViewport.y /
                 gl_Position.w;

  finalColor = color;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
