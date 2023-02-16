
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  /* Needed only because of wireframe slider.
   * If we could get rid of it would be nice because of performance drain of discard. */
  if (edgeStart.r == -1.0) {
    discard;
    return;
  }

#ifndef SELECT_EDGES
  lineOutput = pack_line_data(gl_FragCoord.xy, edgeStart, edgePos);
  fragColor = finalColor;

#  ifdef CUSTOM_DEPTH_BIAS
  vec2 dir = lineOutput.xy * 2.0 - 1.0;
  bool dir_horiz = abs(dir.x) > abs(dir.y);

  vec2 uv = gl_FragCoord.xy * sizeViewportInv;
  float depth_occluder = texture(depthTex, uv).r;
  float depth_min = depth_occluder;
  vec2 texel_uv_size = sizeViewportInv;

  if (dir_horiz) {
    depth_min = min(depth_min, texture(depthTex, uv + vec2(-texel_uv_size.x, 0.0)).r);
    depth_min = min(depth_min, texture(depthTex, uv + vec2(texel_uv_size.x, 0.0)).r);
  }
  else {
    depth_min = min(depth_min, texture(depthTex, uv + vec2(0, -texel_uv_size.y)).r);
    depth_min = min(depth_min, texture(depthTex, uv + vec2(0, texel_uv_size.y)).r);
  }

  float delta = abs(depth_occluder - depth_min);

  if (gl_FragCoord.z < (depth_occluder + delta) && gl_FragCoord.z > depth_occluder) {
    gl_FragDepth = depth_occluder;
  }
  else {
    gl_FragDepth = gl_FragCoord.z;
  }
#  endif
#endif
}
