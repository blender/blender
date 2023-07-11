
/**
 * Virtual shadowmapping: Usage tagging
 *
 * Shadow pages are only allocated if they are visible.
 * This pass scan the depth buffer and tag all tiles that are needed for light shadowing as
 * needed.
 */

#pragma BLENDER_REQUIRE(eevee_shadow_tag_usage_lib.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  ivec2 tex_size = textureSize(depth_tx, 0).xy;

  if (!in_range_inclusive(texel, ivec2(0), ivec2(tex_size - 1))) {
    return;
  }

  float depth = texelFetch(depth_tx, texel, 0).r;
  if (depth == 1.0) {
    return;
  }

  vec2 uv = (vec2(texel) + 0.5) / vec2(tex_size);
  vec3 vP = get_view_space_from_depth(uv, depth);
  vec3 P = transform_point(ViewMatrixInverse, vP);
  vec2 pixel = vec2(gl_GlobalInvocationID.xy);

  shadow_tag_usage(vP, P, pixel);
}
