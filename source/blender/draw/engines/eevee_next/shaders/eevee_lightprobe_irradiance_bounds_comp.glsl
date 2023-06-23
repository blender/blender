
/**
 * Surface Capture: Output surface parameters to diverse storage.
 *
 * The resources expected to be defined are:
 * - capture_info_buf
 */

#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(common_intersect_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_shadow_tilemap_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_iter_lib.glsl)

void main()
{
  uint index = gl_GlobalInvocationID.x;
  if (index >= resource_len) {
    return;
  }

  ObjectBounds bounds = bounds_buf[index];
  /* Bounds are not correct as culling is disabled for these. */
  if (bounds._inner_sphere_radius <= 0.0) {
    return;
  }

  IsectBox box = isect_data_setup(bounds.bounding_corners[0].xyz,
                                  bounds.bounding_corners[1].xyz,
                                  bounds.bounding_corners[2].xyz,
                                  bounds.bounding_corners[3].xyz);

  vec3 local_min = vec3(FLT_MAX);
  vec3 local_max = vec3(-FLT_MAX);
  for (int i = 0; i < 8; i++) {
    local_min = min(local_min, box.corners[i].xyz);
    local_max = max(local_max, box.corners[i].xyz);
  }

  atomicMin(capture_info_buf.scene_bound_x_min, floatBitsToOrderedInt(local_min.x));
  atomicMax(capture_info_buf.scene_bound_x_max, floatBitsToOrderedInt(local_max.x));

  atomicMin(capture_info_buf.scene_bound_y_min, floatBitsToOrderedInt(local_min.y));
  atomicMax(capture_info_buf.scene_bound_y_max, floatBitsToOrderedInt(local_max.y));

  atomicMin(capture_info_buf.scene_bound_z_min, floatBitsToOrderedInt(local_min.z));
  atomicMax(capture_info_buf.scene_bound_z_max, floatBitsToOrderedInt(local_max.z));
}
