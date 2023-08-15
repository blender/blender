
/**
 * Bilateral filtering of denoised raytraced radiance.
 *
 * Dispatched at fullres using a tile list.
 *
 * Input: Temporaly Stabilized Radiance, Stabilized Variance
 * Ouput: Denoised radiance
 *
 * Following "Stochastic All The Things: Raytracing in Hybrid Real-Time Rendering"
 * by Tomasz Stachowiak
 * https://www.ea.com/seed/news/seed-dd18-presentation-slides-raytracing
 */

#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)

float bilateral_depth_weight(vec3 center_N, vec3 center_P, vec3 sample_P)
{
  vec4 center_plane_eq = vec4(center_N, -dot(center_N, center_P));
  /* Only compare distance to the center plane formed by the normal. */
  float depth_delta = dot(center_plane_eq, vec4(sample_P, 1.0));
  /* TODO(fclem): Scene parameter. This is dependent on scene scale. */
  const float scale = 10000.0;
  float weight = exp2(-scale * sqr(depth_delta));
  return weight;
}

float bilateral_spatial_weight(float sigma, vec2 offset_from_center)
{
  /* From https://github.com/tranvansang/bilateral-filter/blob/master/fshader.frag */
  float fac = -1.0 / square_f(sigma);
  /* Take two standard deviation. */
  fac *= 2.0;
  float weight = exp2(fac * length_squared(offset_from_center));
  return weight;
}

float bilateral_normal_weight(vec3 center_N, vec3 sample_N)
{
  float facing_ratio = dot(center_N, sample_N);
  float weight = saturate(pow8f(facing_ratio));
  return weight;
}

/* In order to remove some more fireflies, "tonemap" the color samples during the accumulation. */
vec3 to_accumulation_space(vec3 color)
{
  return color / (1.0 + dot(color, vec3(1.0)));
}
vec3 from_accumulation_space(vec3 color)
{
  return color / (1.0 - dot(color, vec3(1.0)));
}

void gbuffer_load_closure_data(sampler2DArray gbuffer_closure_tx,
                               ivec2 texel,
                               out ClosureDiffuse closure)
{
  vec4 data_in = texelFetch(gbuffer_closure_tx, ivec3(texel, 1), 0);

  closure.N = gbuffer_normal_unpack(data_in.xy);
}

void gbuffer_load_closure_data(sampler2DArray gbuffer_closure_tx,
                               ivec2 texel,
                               out ClosureRefraction closure)
{
  vec4 data_in = texelFetch(gbuffer_closure_tx, ivec3(texel, 1), 0);

  closure.N = gbuffer_normal_unpack(data_in.xy);
  if (gbuffer_is_refraction(data_in)) {
    closure.roughness = data_in.z;
    closure.ior = gbuffer_ior_unpack(data_in.w);
  }
  else {
    closure.roughness = 1.0;
    closure.ior = 1.1;
  }
}

void gbuffer_load_closure_data(sampler2DArray gbuffer_closure_tx,
                               ivec2 texel,
                               out ClosureReflection closure)
{
  vec4 data_in = texelFetch(gbuffer_closure_tx, ivec3(texel, 0), 0);

  closure.N = gbuffer_normal_unpack(data_in.xy);
  closure.roughness = data_in.z;
}

void main()
{
  const uint tile_size = RAYTRACE_GROUP_SIZE;
  uvec2 tile_coord = unpackUvec2x16(tiles_coord_buf[gl_WorkGroupID.x]);
  ivec2 texel_fullres = ivec2(gl_LocalInvocationID.xy + tile_coord * tile_size);
  vec2 center_uv = vec2(texel_fullres) * raytrace_buf.full_resolution_inv;

  float center_depth = texelFetch(hiz_tx, texel_fullres, 0).r;
  vec3 center_P = get_world_space_from_depth(center_uv, center_depth);

#if defined(RAYTRACE_DIFFUSE)
  ClosureDiffuse sample_closure, center_closure;
#elif defined(RAYTRACE_REFRACT)
  ClosureRefraction sample_closure, center_closure;
#elif defined(RAYTRACE_REFLECT)
  ClosureReflection sample_closure, center_closure;
#else
#  error
#endif
  gbuffer_load_closure_data(gbuffer_closure_tx, texel_fullres, center_closure);

  float roughness = center_closure.roughness;

  float variance = imageLoad(in_variance_img, texel_fullres).r;
  vec3 in_radiance = imageLoad(in_radiance_img, texel_fullres).rgb;

  bool is_background = (center_depth == 0.0);
  bool is_smooth = (roughness < 0.05);
  bool is_low_variance = (variance < 0.05);
  bool is_high_variance = (variance > 0.5);

  /* Width of the box filter in pixels. */
  float filter_size_factor = saturate(roughness * 8.0);
  float filter_size = mix(0.0, 9.0, filter_size_factor);
  uint sample_count = uint(mix(1.0, 10.0, filter_size_factor) * (is_high_variance ? 1.5 : 1.0));

  if (is_smooth || is_background || is_low_variance) {
    /* Early out cases. */
    imageStore(out_radiance_img, texel_fullres, vec4(in_radiance, 0.0));
    return;
  }

  vec2 noise = interlieved_gradient_noise(vec2(texel_fullres) + 0.5, vec2(3, 5), vec2(0.0));
  noise += sampling_rng_2D_get(SAMPLING_RAYTRACE_W);

  vec3 accum_radiance = to_accumulation_space(in_radiance);
  float accum_weight = 1.0;
  /* We want to resize the blur depending on the roughness and keep the amount of sample low.
   * So we do a random sampling around the center point. */
  for (uint i = 0u; i < sample_count; i++) {
    /* Essentially a box radius overtime. */
    vec2 offset_f = (fract(hammersley_2d(i, sample_count) + noise) - 0.5) * filter_size;
    ivec2 offset = ivec2(floor(offset_f + 0.5));

    ivec2 sample_texel = texel_fullres + offset;
    ivec2 sample_tile = sample_texel / RAYTRACE_GROUP_SIZE;
    /* Make sure the sample has been processed and do not contain garbage data. */
    bool unprocessed_tile = imageLoad(tile_mask_img, sample_tile).r == 0;
    if (unprocessed_tile) {
      continue;
    }

    float sample_depth = texelFetch(hiz_tx, sample_texel, 0).r;
    vec2 sample_uv = vec2(sample_texel) * raytrace_buf.full_resolution_inv;
    vec3 sample_P = get_world_space_from_depth(sample_uv, sample_depth);

    /* Background case. */
    if (sample_depth == 0.0) {
      continue;
    }

    gbuffer_load_closure_data(gbuffer_closure_tx, sample_texel, sample_closure);

    float depth_weight = bilateral_depth_weight(center_closure.N, center_P, sample_P);
    float spatial_weight = bilateral_spatial_weight(filter_size, vec2(offset));
    float normal_weight = bilateral_normal_weight(center_closure.N, sample_closure.N);

    float weight = depth_weight * spatial_weight * normal_weight;

    vec3 radiance = imageLoad(in_radiance_img, sample_texel).rgb;
    /* Do not gather unprocessed pixels. */
    if (all(equal(in_radiance, FLT_11_11_10_MAX))) {
      continue;
    }
    accum_radiance += to_accumulation_space(radiance) * weight;
    accum_weight += weight;
  }

  vec3 out_radiance = accum_radiance * safe_rcp(accum_weight);
  out_radiance = from_accumulation_space(out_radiance);

  imageStore(out_radiance_img, texel_fullres, vec4(out_radiance, 0.0));
}
