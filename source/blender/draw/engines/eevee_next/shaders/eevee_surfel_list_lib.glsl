
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_matrix_lib.glsl)

/**
 * Return the corresponding list index in the `list_start_buf` for a given world position.
 * It will clamp any coordinate outside valid bounds to nearest list.
 * Also return the surfel sorting value as `r_ray_distance`.
 */
int surfel_list_index_get(ivec2 ray_grid_size, vec3 P, out float r_ray_distance)
{
  vec4 hP = point_world_to_ndc(P);
  r_ray_distance = -hP.z;
  vec2 ssP = hP.xy * 0.5 + 0.5;
  ivec2 ray_coord_on_grid = ivec2(ssP * vec2(ray_grid_size));
  ray_coord_on_grid = clamp(ray_coord_on_grid, ivec2(0), ray_grid_size - 1);

  int list_index = ray_coord_on_grid.y * ray_grid_size.x + ray_coord_on_grid.x;
  return list_index;
}

/**
 * Return the corresponding cluster index in the `cluster_list_tx` for a given world position.
 * It will clamp any coordinate outside valid bounds to nearest cluster.
 */
ivec3 surfel_cluster_index_get(ivec3 cluster_grid_size,
                               float4x4 irradiance_grid_world_to_local,
                               vec3 P)
{
  vec3 lP = transform_point(irradiance_grid_world_to_local, P) * 0.5 + 0.5;
  ivec3 cluster_index = ivec3(lP * vec3(cluster_grid_size));
  cluster_index = clamp(cluster_index, ivec3(0), cluster_grid_size - 1);
  return cluster_index;
}
