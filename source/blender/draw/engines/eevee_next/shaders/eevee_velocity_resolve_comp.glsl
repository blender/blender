
/**
 * Fullscreen pass that compute motion vector for static geometry.
 * Animated geometry has already written correct motion vectors.
 */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_velocity_lib.glsl)

#define is_valid_output(img_) (imageSize(img_).x > 1)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  vec4 motion = imageLoad(velocity_view_img, texel);

  bool pixel_has_valid_motion = (motion.x != VELOCITY_INVALID);
  float depth = texelFetch(depth_tx, texel, 0).r;
  bool is_background = (depth == 1.0f);

  vec2 uv = vec2(texel) * drw_view.viewport_size_inverse;
  vec3 P_next, P_prev, P_curr;

  if (pixel_has_valid_motion) {
    /* Animated geometry. View motion already computed during prepass. Convert only to camera. */
    // P_prev = get_world_space_from_depth(uv + motion.xy, 0.5);
    // P_curr = get_world_space_from_depth(uv, 0.5);
    // P_next = get_world_space_from_depth(uv + motion.zw, 0.5);
    return;
  }
  else if (is_background) {
    /* NOTE: Use viewCameraVec to avoid imprecision if camera is far from origin. */
    vec3 vV = viewCameraVec(get_view_space_from_depth(uv, 1.0));
    vec3 V = transform_direction(ViewMatrixInverse, vV);
    /* Background has no motion under camera translation. Translate view vector with the camera. */
    /* WATCH(fclem): Might create precision issues. */
    P_next = camera_next.viewinv[3].xyz + V;
    P_curr = camera_curr.viewinv[3].xyz + V;
    P_prev = camera_prev.viewinv[3].xyz + V;
  }
  else {
    /* Static geometry. No translation in world space. */
    P_curr = get_world_space_from_depth(uv, depth);
    P_prev = P_curr;
    P_next = P_curr;
  }

  vec4 vel_camera, vel_view;
  velocity_camera(P_prev, P_curr, P_next, vel_camera, vel_view);

  if (in_texture_range(texel, depth_tx)) {
    imageStore(velocity_view_img, texel, vel_view);

    if (is_valid_output(velocity_camera_img)) {
      imageStore(velocity_camera_img, texel, vel_camera);
    }
  }
}
