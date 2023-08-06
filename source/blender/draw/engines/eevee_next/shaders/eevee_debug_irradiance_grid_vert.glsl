
#pragma BLENDER_REQUIRE(eevee_lightprobe_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  ivec3 grid_resolution = textureSize(debug_data_tx, 0);
  ivec3 grid_sample;
  int sample_id = 0;
  if (debug_mode == DEBUG_IRRADIANCE_CACHE_VALIDITY) {
    /* Points. */
    sample_id = gl_VertexID;
  }
  else if (debug_mode == DEBUG_IRRADIANCE_CACHE_VIRTUAL_OFFSET) {
    /* Lines. */
    sample_id = gl_VertexID / 2;
  }

  grid_sample.x = (sample_id % grid_resolution.x);
  grid_sample.y = (sample_id / grid_resolution.x) % grid_resolution.y;
  grid_sample.z = (sample_id / (grid_resolution.x * grid_resolution.y));

  vec3 P = lightprobe_irradiance_grid_sample_position(grid_mat, grid_resolution, grid_sample);

  vec4 debug_data = texelFetch(debug_data_tx, grid_sample, 0);
  if (debug_mode == DEBUG_IRRADIANCE_CACHE_VALIDITY) {
    interp_color = vec4(1.0 - debug_data.r, debug_data.r, 0.0, 0.0);
    gl_PointSize = 3.0;
    if (debug_data.r > debug_value) {
      /* Only render points that are below threshold. */
      gl_Position = vec4(0.0);
      gl_PointSize = 0.0;
      return;
    }
  }
  else if (debug_mode == DEBUG_IRRADIANCE_CACHE_VIRTUAL_OFFSET) {
    if (is_zero(debug_data.xyz)) {
      /* Only render points that have offset. */
      gl_Position = vec4(0.0);
      gl_PointSize = 0.0;
      return;
    }

    if ((gl_VertexID & 1) == 1) {
      P += debug_data.xyz;
    }
  }

  gl_Position = point_world_to_ndc(P);
  gl_Position.z -= 2.5e-5;
  gl_PointSize = 3.0;
}
