
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(common_utiltex_lib.glsl)
#pragma BLENDER_REQUIRE(lights_lib.glsl)

uniform sampler2D depthBuffer;

out vec4 fragColor;

void main()
{
  if (laNumLight == 0) {
    /* Early exit: No lights in scene */
    fragColor.r = 1.0;
    return;
  }

  ivec2 texel = ivec2(gl_FragCoord.xy);
  float depth = texelFetch(depthBuffer, texel, 0).r;
  if (depth == 1.0f) {
    /* Early exit background does not receive shadows */
    fragColor.r = 1.0;
    return;
  }

  vec2 texel_size = 1.0 / vec2(textureSize(depthBuffer, 0)).xy;
  vec2 uvs = saturate(gl_FragCoord.xy * texel_size);
  vec4 rand = texelfetch_noise_tex(texel);

  float accum_light = 0.0;
  float tracing_depth = depth;
  /* Constant bias (due to depth buffer precision) */
  /* Magic numbers for 24bits of precision.
   * From http://terathon.com/gdc07_lengyel.pdf (slide 26) */
  tracing_depth -= mix(2.4e-7, 4.8e-7, depth);
  /* Convert to view Z. */
  tracing_depth = get_view_z_from_depth(tracing_depth);

  vec3 viewPosition = get_view_space_from_depth(uvs, depth);
  vec3 worldPosition = transform_point(ViewMatrixInverse, viewPosition);

  vec3 true_normal = normalize(cross(dFdx(viewPosition), dFdy(viewPosition)));

  for (int i = 0; i < MAX_LIGHT && i < laNumLight; i++) {
    LightData ld = lights_data[i];

    vec4 l_vector; /* Non-Normalized Light Vector with length in last component. */
    l_vector.xyz = ld.l_position - worldPosition;
    l_vector.w = length(l_vector.xyz);

    float l_vis = light_shadowing(
        ld, worldPosition, viewPosition, tracing_depth, true_normal, rand.x, true, 1.0);

    accum_light += l_vis;
  }

  fragColor.r = accum_light / float(laNumLight);
}
