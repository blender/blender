
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_geom_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_ambient_occlusion_lib.glsl)

/* Similar to https://atyuwen.github.io/posts/normal-reconstruction/.
 * This samples the depth buffer 4 time for each direction to get the most correct
 * implicit normal reconstruction out of the depth buffer. */
vec3 view_position_derivative_from_depth(
    sampler2D depth_tx, ivec2 extent, vec2 uv, ivec2 offset, vec3 vP, float depth_center)
{
  vec4 H;
  H.x = texelFetch(depth_tx, ivec2(uv * vec2(extent)) - offset * 2, 0).r;
  H.y = texelFetch(depth_tx, ivec2(uv * vec2(extent)) - offset, 0).r;
  H.z = texelFetch(depth_tx, ivec2(uv * vec2(extent)) + offset, 0).r;
  H.w = texelFetch(depth_tx, ivec2(uv * vec2(extent)) + offset * 2, 0).r;

  vec2 uv_offset = vec2(offset) / vec2(extent);
  vec2 uv1 = uv - uv_offset * 2.0;
  vec2 uv2 = uv - uv_offset;
  vec2 uv3 = uv + uv_offset;
  vec2 uv4 = uv + uv_offset * 2.0;

  /* Fix issue with depth precision. Take even larger diff. */
  vec4 diff = abs(vec4(depth_center, H.yzw) - H.x);
  if (max_v4(diff) < 2.4e-7 && all(lessThan(diff.xyz, diff.www))) {
    return 0.25 * (get_view_space_from_depth(uv3, H.w) - get_view_space_from_depth(uv1, H.x));
  }
  /* Simplified (H.xw + 2.0 * (H.yz - H.xw)) - depth_center */
  vec2 deltas = abs((2.0 * H.yz - H.xw) - depth_center);
  if (deltas.x < deltas.y) {
    return vP - get_view_space_from_depth(uv2, H.y);
  }
  else {
    return get_view_space_from_depth(uv3, H.z) - vP;
  }
}

/* TODO(Miguel Pozo): This should be in common_view_lib,
 * but moving it there results in dependency hell. */
bool reconstruct_view_position_and_normal_from_depth(
    sampler2D depth_tx, ivec2 extent, vec2 uv, out vec3 vP, out vec3 vNg)
{
  float depth_center = texelFetch(depth_tx, ivec2(uv * vec2(extent)), 0).r;

  vP = get_view_space_from_depth(uv, depth_center);

  vec3 dPdx = view_position_derivative_from_depth(
      depth_tx, extent, uv, ivec2(1, 0), vP, depth_center);
  vec3 dPdy = view_position_derivative_from_depth(
      depth_tx, extent, uv, ivec2(0, 1), vP, depth_center);

  vNg = safe_normalize(cross(dPdx, dPdy));

  /* Background case. */
  return depth_center != 1.0;
}

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  ivec2 extent = imageSize(in_normal_img).xy;
  if (any(greaterThanEqual(texel, extent))) {
    return;
  }

  vec2 uv = (vec2(texel) + vec2(0.5)) / vec2(extent);
  vec3 vP, vNg;
  if (!reconstruct_view_position_and_normal_from_depth(hiz_tx, extent, uv, vP, vNg)) {
    /* Do not trace for background */
    imageStore(out_ao_img, ivec3(texel, out_ao_img_layer_index), vec4(0.0));
    return;
  }

  vec3 P = transform_point(ViewMatrixInverse, vP);
  vec3 V = cameraVec(P);
  vec3 Ng = transform_direction(ViewMatrixInverse, vNg);
  vec3 N = imageLoad(in_normal_img, ivec3(texel, in_normal_img_layer_index)).xyz;

  OcclusionData data = ambient_occlusion_search(vP, hiz_tx, texel, ao_buf.distance, 0.0, 8.0);

  float visibility;
  float visibility_error_out;
  vec3 bent_normal_out;
  ambient_occlusion_eval(
      data, texel, V, N, Ng, 0.0, visibility, visibility_error_out, bent_normal_out);
  /* Scale by user factor */
  visibility = saturate(visibility);

  imageStore(out_ao_img, ivec3(texel, out_ao_img_layer_index), vec4(visibility));
}
