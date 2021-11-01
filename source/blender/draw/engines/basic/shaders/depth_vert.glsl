
#ifdef CONSERVATIVE_RASTER
RESOURCE_ID_VARYING
#endif

#ifndef POINTCLOUD
in vec3 pos;
#endif

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND
#ifdef CONSERVATIVE_RASTER
  PASS_RESOURCE_ID
#endif

#ifdef POINTCLOUD
  vec3 world_pos = pointcloud_get_pos();
#else
  vec3 world_pos = point_object_to_world(pos);
#endif

  gl_Position = point_world_to_ndc(world_pos);

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
