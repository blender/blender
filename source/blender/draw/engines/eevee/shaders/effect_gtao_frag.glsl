
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_geom_lib.glsl)
#pragma BLENDER_REQUIRE(common_utiltex_lib.glsl)
#pragma BLENDER_REQUIRE(ambient_occlusion_lib.glsl)

/**
 * This shader only compute maximum horizon angles for each directions.
 * The final integration is done at the resolve stage with the shading normal.
 */

out vec4 FragColor;

uniform sampler2D normalBuffer;
#ifdef LAYERED_DEPTH
uniform sampler2DArray depthBufferLayered;
uniform int layer;
#  define gtao_depthBuffer depthBufferLayered
#  define gtao_textureLod(a, b, c) textureLod(a, vec3(b, layer), c)

#else
uniform sampler2D depthBuffer;
#  define gtao_depthBuffer depthBuffer
#  define gtao_textureLod(a, b, c) textureLod(a, b, c)

#endif

/* Similar to https://atyuwen.github.io/posts/normal-reconstruction/.
 * This samples the depth buffer 4 time for each direction to get the most correct
 * implicit normal reconstruction out of the depth buffer. */
vec3 view_position_derivative_from_depth(vec2 uvs, vec2 ofs, vec3 vP, float depth_center)
{
  vec2 uv1 = uvs - ofs * 2.0;
  vec2 uv2 = uvs - ofs;
  vec2 uv3 = uvs + ofs;
  vec2 uv4 = uvs + ofs * 2.0;
  vec4 H;
  H.x = gtao_textureLod(gtao_depthBuffer, uv1, 0.0).r;
  H.y = gtao_textureLod(gtao_depthBuffer, uv2, 0.0).r;
  H.z = gtao_textureLod(gtao_depthBuffer, uv3, 0.0).r;
  H.w = gtao_textureLod(gtao_depthBuffer, uv4, 0.0).r;
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

/* TODO(fclem) port to a common place for other effects to use. */
bool reconstruct_view_position_and_normal_from_depth(vec2 texel, out vec3 vP, out vec3 vNg)
{
  vec2 texel_size = 1.0 / vec2(textureSize(gtao_depthBuffer, 0).xy);
  vec2 uvs = gl_FragCoord.xy * texel_size;
  float depth_center = gtao_textureLod(gtao_depthBuffer, uvs, 0.0).r;

  /* Background case. */
  if (depth_center == 1.0) {
    return false;
  }

  vP = get_view_space_from_depth(uvs, depth_center);

  vec3 dPdx = view_position_derivative_from_depth(uvs, texel_size * vec2(1, 0), vP, depth_center);
  vec3 dPdy = view_position_derivative_from_depth(uvs, texel_size * vec2(0, 1), vP, depth_center);

  vNg = safe_normalize(cross(dPdx, dPdy));

  return true;
}

#ifdef DEBUG_AO

in vec4 uvcoordsvar;

void main()
{
  vec3 vP, vNg;

  if (!reconstruct_view_position_and_normal_from_depth(gl_FragCoord.xy, vP, vNg)) {
    /* Handle Background case. Prevent artifact due to uncleared Horizon Render Target. */
    FragColor = vec4(0.0);
  }
  else {
    vec3 P = transform_point(ViewMatrixInverse, vP);
    vec3 V = cameraVec(P);
    vec3 vV = viewCameraVec(vP);
    vec3 vN = normal_decode(texture(normalBuffer, uvcoordsvar.xy).rg, vV);
    vec3 N = transform_direction(ViewMatrixInverse, vN);
    vec3 Ng = transform_direction(ViewMatrixInverse, vNg);

    OcclusionData data = occlusion_load(vP, 1.0);

    if (min_v4(abs(data.horizons)) != M_PI) {
      FragColor = vec4(diffuse_occlusion(data, V, N, Ng));
    }
    else {
      FragColor = vec4(1.0);
    }
  }
}

#else

void main()
{
  vec2 uvs = gl_FragCoord.xy / vec2(textureSize(gtao_depthBuffer, 0).xy);
  float depth = gtao_textureLod(gtao_depthBuffer, uvs, 0.0).r;
  vec3 vP = get_view_space_from_depth(uvs, depth);

  OcclusionData data = NO_OCCLUSION_DATA;
  /* Do not trace for background */
  if (depth != 1.0) {
    data = occlusion_search(vP, maxzBuffer, aoDistance, 0.0, 8.0);
  }

  FragColor = pack_occlusion_data(data);
}
#endif
