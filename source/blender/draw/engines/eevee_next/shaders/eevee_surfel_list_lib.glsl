
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

/**
 * Return the coresponding list index in the `list_start_buf` for a given world position.
 * It will clamp any coordinate outside valid bounds to nearest list.
 * Also return the surfel sorting value as `r_ray_distance`.
 */
int surfel_list_index_get(vec3 P, out float r_ray_distance)
{
  vec4 hP = point_world_to_ndc(P);
  r_ray_distance = -hP.z;
  vec2 ssP = hP.xy * 0.5 + 0.5;
  ivec2 ray_coord_on_grid = ivec2(ssP * vec2(list_info_buf.ray_grid_size));
  ray_coord_on_grid = clamp(ray_coord_on_grid, ivec2(0), list_info_buf.ray_grid_size - 1);

  int list_index = ray_coord_on_grid.y * list_info_buf.ray_grid_size.x + ray_coord_on_grid.x;
  return list_index;
}
