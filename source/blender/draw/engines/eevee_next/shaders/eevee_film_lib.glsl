
/**
 * Film accumulation utils functions.
 **/

#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_geom_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_camera_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_velocity_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_colorspace_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_cryptomatte_lib.glsl)

/* Return scene linear Z depth from the camera or radial depth for panoramic cameras. */
float film_depth_convert_to_scene(float depth)
{
  if (false /* Panoramic */) {
    /* TODO */
    return 1.0;
  }
  return abs(get_view_z_from_depth(depth));
}

/* Load a texture sample in a specific format. Combined pass needs to use this. */
vec4 film_texelfetch_as_YCoCg_opacity(sampler2D tx, ivec2 texel)
{
  vec4 color = texelFetch(combined_tx, texel, 0);
  /* Convert transmittance to opacity. */
  color.a = saturate(1.0 - color.a);
  /* Transform to YCoCg for accumulation. */
  color.rgb = colorspace_YCoCg_from_scene_linear(color.rgb);
  return color;
}

/* Returns a weight based on Luma to reduce the flickering introduced by high energy pixels. */
float film_luma_weight(float luma)
{
  /* Slide 20 of "High Quality Temporal Supersampling" by Brian Karis at Siggraph 2014. */
  /* To preserve more details in dark areas, we use a bigger bias. */
  return 1.0 / (4.0 + luma * film_buf.exposure_scale);
}

/* -------------------------------------------------------------------- */
/** \name Filter
 * \{ */

FilmSample film_sample_get(int sample_n, ivec2 texel_film)
{
#ifdef PANORAMIC
  /* TODO(fclem): Panoramic projection will be more complex. The samples will have to be retrieve
   * at runtime, maybe by scanning a whole region. Offset and weight will have to be computed by
   * reprojecting the incoming pixel data into film pixel space. */
#else

#  ifdef SCALED_RENDERING
  texel_film /= film_buf.scaling_factor;
#  endif

  FilmSample film_sample = film_buf.samples[sample_n];
  film_sample.texel += texel_film + film_buf.offset;
  /* Use extend on borders. */
  film_sample.texel = clamp(film_sample.texel, ivec2(0, 0), film_buf.render_extent - 1);

  /* TODO(fclem): Panoramic projection will need to compute the sample weight in the shader
   * instead of precomputing it on CPU. */
#  ifdef SCALED_RENDERING
  /* We need to compute the real distance and weight since a sample
   * can be used by many final pixel. */
  vec2 offset = film_buf.subpixel_offset - vec2(texel_film % film_buf.scaling_factor);
  film_sample.weight = film_filter_weight(film_buf.filter_size, len_squared(offset));
#  endif

#endif /* PANORAMIC */

  /* Always return a weight above 0 to avoid blind spots between samples. */
  film_sample.weight = max(film_sample.weight, 1e-6);

  return film_sample;
}

/* Returns the combined weights of all samples affecting this film pixel. */
float film_weight_accumulation(ivec2 texel_film)
{
#if 0 /* TODO(fclem): Reference implementation, also needed for panoramic cameras. */
  float weight = 0.0;
  for (int i = 0; i < film_buf.samples_len; i++) {
    weight += film_sample_get(i, texel_film).weight;
  }
  return weight;
#endif
  return film_buf.samples_weight_total;
}

void film_sample_accum(
    FilmSample samp, int pass_id, int layer, sampler2DArray tex, inout vec4 accum)
{
  if (pass_id < 0 || layer < 0) {
    return;
  }
  accum += texelFetch(tex, ivec3(samp.texel, layer), 0) * samp.weight;
}

void film_sample_accum(
    FilmSample samp, int pass_id, int layer, sampler2DArray tex, inout float accum)
{
  if (pass_id < 0 || layer < 0) {
    return;
  }
  accum += texelFetch(tex, ivec3(samp.texel, layer), 0).x * samp.weight;
}

void film_sample_accum_mist(FilmSample samp, inout float accum)
{
  if (film_buf.mist_id == -1) {
    return;
  }
  float depth = texelFetch(depth_tx, samp.texel, 0).x;
  vec2 uv = (vec2(samp.texel) + 0.5) / vec2(textureSize(depth_tx, 0).xy);
  vec3 vP = get_view_space_from_depth(uv, depth);
  bool is_persp = ProjectionMatrix[3][3] == 0.0;
  float mist = (is_persp) ? length(vP) : abs(vP.z);
  /* Remap to 0..1 range. */
  mist = saturate(mist * film_buf.mist_scale + film_buf.mist_bias);
  /* Falloff. */
  mist = pow(mist, film_buf.mist_exponent);
  accum += mist * samp.weight;
}

void film_sample_accum_combined(FilmSample samp, inout vec4 accum, inout float weight_accum)
{
  if (film_buf.combined_id == -1) {
    return;
  }
  vec4 color = film_texelfetch_as_YCoCg_opacity(combined_tx, samp.texel);

  /* Weight by luma to remove fireflies. */
  float weight = film_luma_weight(color.x) * samp.weight;

  accum += color * weight;
  weight_accum += weight;
}

#ifdef GPU_METAL
void film_sample_cryptomatte_accum(FilmSample samp,
                                   int layer,
                                   sampler2D tex,
                                   thread vec2 *crypto_samples)
#else
void film_sample_cryptomatte_accum(FilmSample samp,
                                   int layer,
                                   sampler2D tex,
                                   inout vec2 crypto_samples[4])
#endif
{
  float hash = texelFetch(tex, samp.texel, 0)[layer];
  /* Find existing entry. */
  for (int i = 0; i < 4; i++) {
    if (crypto_samples[i].x == hash) {
      crypto_samples[i].y += samp.weight;
      return;
    }
  }
  /* Overwrite entry with less weight. */
  for (int i = 0; i < 4; i++) {
    if (crypto_samples[i].y < samp.weight) {
      crypto_samples[i] = vec2(hash, samp.weight);
      return;
    }
  }
}

void film_cryptomatte_layer_accum_and_store(
    FilmSample dst, ivec2 texel_film, int pass_id, int layer_component, inout vec4 out_color)
{
  if (pass_id == -1) {
    return;
  }
  /* x = hash, y = accumulated weight. Only keep track of 4 highest weighted samples. */
  vec2 crypto_samples[4] = vec2[4](vec2(0.0), vec2(0.0), vec2(0.0), vec2(0.0));
  for (int i = 0; i < film_buf.samples_len; i++) {
    FilmSample src = film_sample_get(i, texel_film);
    film_sample_cryptomatte_accum(src, layer_component, cryptomatte_tx, crypto_samples);
  }
  for (int i = 0; i < 4; i++) {
    cryptomatte_store_film_sample(dst, pass_id, crypto_samples[i], out_color);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Load/Store Data
 * \{ */

/* Returns the distance used to store nearest interpolation data. */
float film_distance_load(ivec2 texel)
{
  /* Repeat texture coordinates as the weight can be optimized to a small portion of the film. */
  texel = texel % imageSize(in_weight_img).xy;

  if (!film_buf.use_history || film_buf.use_reprojection) {
    return 1.0e16;
  }
  return imageLoad(in_weight_img, ivec3(texel, FILM_WEIGHT_LAYER_DISTANCE)).x;
}

float film_weight_load(ivec2 texel)
{
  /* Repeat texture coordinates as the weight can be optimized to a small portion of the film. */
  texel = texel % imageSize(in_weight_img).xy;

  if (!film_buf.use_history || film_buf.use_reprojection) {
    return 0.0;
  }
  return imageLoad(in_weight_img, ivec3(texel, FILM_WEIGHT_LAYER_ACCUMULATION)).x;
}

/* Returns motion in pixel space to retrieve the pixel history. */
vec2 film_pixel_history_motion_vector(ivec2 texel_sample)
{
  /**
   * Dilate velocity by using the nearest pixel in a cross pattern.
   * "High Quality Temporal Supersampling" by Brian Karis at Siggraph 2014 (Slide 27)
   */
  const ivec2 corners[4] = ivec2[4](ivec2(-2, -2), ivec2(2, -2), ivec2(-2, 2), ivec2(2, 2));
  float min_depth = texelFetch(depth_tx, texel_sample, 0).x;
  ivec2 nearest_texel = texel_sample;
  for (int i = 0; i < 4; i++) {
    ivec2 texel = clamp(texel_sample + corners[i], ivec2(0), textureSize(depth_tx, 0).xy - 1);
    float depth = texelFetch(depth_tx, texel, 0).x;
    if (min_depth > depth) {
      min_depth = depth;
      nearest_texel = texel;
    }
  }

  vec4 vector = velocity_resolve(vector_tx, nearest_texel, min_depth);

  /* Transform to pixel space. */
  vector.xy *= vec2(film_buf.extent);

  return vector.xy;
}

/* \a t is inter-pixel position. 0 means perfectly on a pixel center.
 * Returns weights in both dimensions.
 * Multiply each dimension weights to get final pixel weights. */
#ifdef GPU_METAL
void film_get_catmull_rom_weights(vec2 t, thread vec2 *weights)
#else
void film_get_catmull_rom_weights(vec2 t, out vec2 weights[4])
#endif
{
  vec2 t2 = t * t;
  vec2 t3 = t2 * t;
  float fc = 0.5; /* Catmull-Rom. */

  vec2 fct = t * fc;
  vec2 fct2 = t2 * fc;
  vec2 fct3 = t3 * fc;
  weights[0] = (fct2 * 2.0 - fct3) - fct;
  weights[1] = (t3 * 2.0 - fct3) + (-t2 * 3.0 + fct2) + 1.0;
  weights[2] = (-t3 * 2.0 + fct3) + (t2 * 3.0 - (2.0 * fct2)) + fct;
  weights[3] = fct3 - fct2;
}

/* Load color using a special filter to avoid losing detail.
 * \a texel is sample position with subpixel accuracy. */
vec4 film_sample_catmull_rom(sampler2D color_tx, vec2 input_texel)
{
  vec2 center_texel;
  vec2 inter_texel = modf(input_texel, center_texel);
  vec2 weights[4];
  film_get_catmull_rom_weights(inter_texel, weights);

#if 0 /* Reference. 16 Taps. */
  vec4 color = vec4(0.0);
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      ivec2 texel = ivec2(center_texel) + ivec2(x, y) - 1;
      texel = clamp(texel, ivec2(0), textureSize(color_tx, 0).xy - 1);
      color += texelFetch(color_tx, texel, 0) * weights[x].x * weights[y].y;
    }
  }
  return color;

#elif 1 /* Optimize version. 5 Bilinear Taps. */
  /**
   * Use optimized version by leveraging bilinear filtering from hardware sampler and by removing
   * corner taps.
   * From "Filmic SMAA" by Jorge Jimenez at Siggraph 2016
   * http://advances.realtimerendering.com/s2016/Filmic%20SMAA%20v7.pptx
   */
  center_texel += 0.5;

  /* Slide 92. */
  vec2 weight_12 = weights[1] + weights[2];
  vec2 uv_12 = (center_texel + weights[2] / weight_12) * film_buf.extent_inv;
  vec2 uv_0 = (center_texel - 1.0) * film_buf.extent_inv;
  vec2 uv_3 = (center_texel + 2.0) * film_buf.extent_inv;

  vec4 color;
  vec4 weight_cross = weight_12.xyyx * vec4(weights[0].yx, weights[3].xy);
  float weight_center = weight_12.x * weight_12.y;

  color = textureLod(color_tx, uv_12, 0.0) * weight_center;
  color += textureLod(color_tx, vec2(uv_12.x, uv_0.y), 0.0) * weight_cross.x;
  color += textureLod(color_tx, vec2(uv_0.x, uv_12.y), 0.0) * weight_cross.y;
  color += textureLod(color_tx, vec2(uv_3.x, uv_12.y), 0.0) * weight_cross.z;
  color += textureLod(color_tx, vec2(uv_12.x, uv_3.y), 0.0) * weight_cross.w;
  /* Re-normalize for the removed corners. */
  return color / (weight_center + sum(weight_cross));

#else /* Nearest interpolation for debugging. 1 Tap. */
  ivec2 texel = ivec2(center_texel) + ivec2(greaterThan(inter_texel, vec2(0.5)));
  texel = clamp(texel, ivec2(0), textureSize(color_tx, 0).xy - 1);
  return texelFetch(color_tx, texel, 0);
#endif
}

/* Return history clipping bounding box in YCoCg color space. */
void film_combined_neighbor_boundbox(ivec2 texel, out vec4 min_c, out vec4 max_c)
{
  /* Plus (+) shape offsets. */
  const ivec2 plus_offsets[5] = ivec2[5](ivec2(0, 0), /* Center */
                                         ivec2(-1, 0),
                                         ivec2(0, -1),
                                         ivec2(1, 0),
                                         ivec2(0, 1));
#if 0
  /**
   * Compute Variance of neighborhood as described in:
   * "An Excursion in Temporal Supersampling" by Marco Salvi at GDC 2016.
   * and:
   * "A Survey of Temporal Antialiasing Techniques" by Yang et al.
   */

  /* First 2 moments. */
  vec4 mu1 = vec4(0), mu2 = vec4(0);
  for (int i = 0; i < 5; i++) {
    vec4 color = film_texelfetch_as_YCoCg_opacity(combined_tx, texel + plus_offsets[i]);
    mu1 += color;
    mu2 += sqr(color);
  }
  mu1 *= (1.0 / 5.0);
  mu2 *= (1.0 / 5.0);

  /* Extent scaling. Range [0.75..1.25].
   * Balance between more flickering (0.75) or more ghosting (1.25). */
  const float gamma = 1.25;
  /* Standard deviation. */
  vec4 sigma = sqrt(abs(mu2 - sqr(mu1)));
  /* eq. 6 in "A Survey of Temporal Antialiasing Techniques". */
  min_c = mu1 - gamma * sigma;
  max_c = mu1 + gamma * sigma;
#else
  /**
   * Simple bounding box calculation in YCoCg as described in:
   * "High Quality Temporal Supersampling" by Brian Karis at Siggraph 2014
   */
  min_c = vec4(1e16);
  max_c = vec4(-1e16);
  for (int i = 0; i < 5; i++) {
    vec4 color = film_texelfetch_as_YCoCg_opacity(combined_tx, texel + plus_offsets[i]);
    min_c = min(min_c, color);
    max_c = max(max_c, color);
  }
  /* (Slide 32) Simple clamp to min/max of 8 neighbors results in 3x3 box artifacts.
   * Round bbox shape by averaging 2 different min/max from 2 different neighborhood. */
  vec4 min_c_3x3 = min_c;
  vec4 max_c_3x3 = max_c;
  const ivec2 corners[4] = ivec2[4](ivec2(-1, -1), ivec2(1, -1), ivec2(-1, 1), ivec2(1, 1));
  for (int i = 0; i < 4; i++) {
    vec4 color = film_texelfetch_as_YCoCg_opacity(combined_tx, texel + corners[i]);
    min_c_3x3 = min(min_c_3x3, color);
    max_c_3x3 = max(max_c_3x3, color);
  }
  min_c = (min_c + min_c_3x3) * 0.5;
  max_c = (max_c + max_c_3x3) * 0.5;
#endif
}

/* 1D equivalent of line_aabb_clipping_dist(). */
float film_aabb_clipping_dist_alpha(float origin, float direction, float aabb_min, float aabb_max)
{
  if (abs(direction) < 1e-5) {
    return 0.0;
  }
  float nearest_plane = (direction > 0.0) ? aabb_min : aabb_max;
  return (nearest_plane - origin) / direction;
}

/* Modulate the history color to avoid ghosting artifact. */
vec4 film_amend_combined_history(
    vec4 min_color, vec4 max_color, vec4 color_history, vec4 src_color, ivec2 src_texel)
{
  /* Clip instead of clamping to avoid color accumulating in the AABB corners. */
  vec4 clip_dir = src_color - color_history;

  float t = line_aabb_clipping_dist(color_history.rgb, clip_dir.rgb, min_color.rgb, max_color.rgb);
  color_history.rgb += clip_dir.rgb * saturate(t);

  /* Clip alpha on its own to avoid interference with other channels. */
  float t_a = film_aabb_clipping_dist_alpha(color_history.a, clip_dir.a, min_color.a, max_color.a);
  color_history.a += clip_dir.a * saturate(t_a);

  return color_history;
}

float film_history_blend_factor(float velocity,
                                vec2 texel,
                                float luma_min,
                                float luma_max,
                                float luma_incoming,
                                float luma_history)
{
  /* 5% of incoming color by default. */
  float blend = 0.05;
  /* Blend less history if the pixel has substantial velocity. */
  blend = mix(blend, 0.20, saturate(velocity * 0.02));
  /**
   * "High Quality Temporal Supersampling" by Brian Karis at Siggraph 2014 (Slide 43)
   * Bias towards history if incoming pixel is near clamping. Reduces flicker.
   */
  float distance_to_luma_clip = min_v2(vec2(luma_history - luma_min, luma_max - luma_history));
  /* Divide by bbox size to get a factor. 2 factor to compensate the line above. */
  distance_to_luma_clip *= 2.0 * safe_rcp(luma_max - luma_min);
  /* Linearly blend when history gets below to 25% of the bbox size. */
  blend *= saturate(distance_to_luma_clip * 4.0 + 0.1);
  /* Discard out of view history. */
  if (any(lessThan(texel, vec2(0))) || any(greaterThanEqual(texel, vec2(film_buf.extent)))) {
    blend = 1.0;
  }
  /* Discard history if invalid. */
  if (film_buf.use_history == false) {
    blend = 1.0;
  }
  return blend;
}

/* Returns resolved final color. */
void film_store_combined(
    FilmSample dst, ivec2 src_texel, vec4 color, float color_weight, inout vec4 display)
{
  if (film_buf.combined_id == -1) {
    return;
  }

  vec4 color_src, color_dst;
  float weight_src, weight_dst;

  /* Undo the weighting to get final spatialy-filtered color. */
  color_src = color / color_weight;

  if (film_buf.use_reprojection) {
    /* Interactive accumulation. Do reprojection and Temporal Anti-Aliasing. */

    /* Reproject by finding where this pixel was in the previous frame. */
    vec2 motion = film_pixel_history_motion_vector(src_texel);
    vec2 history_texel = vec2(dst.texel) + motion;

    float velocity = length(motion);

    /* Load weight if it is not uniform across the whole buffer (i.e: upsampling, panoramic). */
    // dst.weight = film_weight_load(texel_combined);

    color_dst = film_sample_catmull_rom(in_combined_tx, history_texel);
    color_dst.rgb = colorspace_YCoCg_from_scene_linear(color_dst.rgb);

    /* Get local color bounding box of source neighborhood. */
    vec4 min_color, max_color;
    film_combined_neighbor_boundbox(src_texel, min_color, max_color);

    float blend = film_history_blend_factor(
        velocity, history_texel, min_color.x, max_color.x, color_src.x, color_dst.x);

    color_dst = film_amend_combined_history(min_color, max_color, color_dst, color_src, src_texel);

    /* Luma weighted blend to avoid flickering. */
    weight_dst = film_luma_weight(color_dst.x) * (1.0 - blend);
    weight_src = film_luma_weight(color_src.x) * (blend);
  }
  else {
    /* Everything is static. Use render accumulation. */
    color_dst = texelFetch(in_combined_tx, dst.texel, 0);
    color_dst.rgb = colorspace_YCoCg_from_scene_linear(color_dst.rgb);

    /* Luma weighted blend to avoid flickering. */
    weight_dst = film_luma_weight(color_dst.x) * dst.weight;
    weight_src = color_weight;
  }
  /* Weighted blend. */
  color = color_dst * weight_dst + color_src * weight_src;
  color /= weight_src + weight_dst;

  color.rgb = colorspace_scene_linear_from_YCoCg(color.rgb);

  /* Fix alpha not accumulating to 1 because of float imprecision. */
  if (color.a > 0.995) {
    color.a = 1.0;
  }

  /* Filter NaNs. */
  if (any(isnan(color))) {
    color = vec4(0.0, 0.0, 0.0, 1.0);
  }

  if (film_buf.display_id == -1) {
    display = color;
  }
  imageStore(out_combined_img, dst.texel, color);
}

void film_store_color(FilmSample dst, int pass_id, vec4 color, inout vec4 display)
{
  if (pass_id == -1) {
    return;
  }

  vec4 data_film = imageLoad(color_accum_img, ivec3(dst.texel, pass_id));

  color = (data_film * dst.weight + color) * dst.weight_sum_inv;

  /* Filter NaNs. */
  if (any(isnan(color))) {
    color = vec4(0.0, 0.0, 0.0, 1.0);
  }

  if (film_buf.display_id == pass_id) {
    display = color;
  }
  imageStore(color_accum_img, ivec3(dst.texel, pass_id), color);
}

void film_store_value(FilmSample dst, int pass_id, float value, inout vec4 display)
{
  if (pass_id == -1) {
    return;
  }

  float data_film = imageLoad(value_accum_img, ivec3(dst.texel, pass_id)).x;

  value = (data_film * dst.weight + value) * dst.weight_sum_inv;

  /* Filter NaNs. */
  if (isnan(value)) {
    value = 0.0;
  }

  if (film_buf.display_id == pass_id) {
    display = vec4(value, value, value, 1.0);
  }
  imageStore(value_accum_img, ivec3(dst.texel, pass_id), vec4(value));
}

/* Nearest sample variant. Always stores the data. */
void film_store_data(ivec2 texel_film, int pass_id, vec4 data_sample, inout vec4 display)
{
  if (pass_id == -1) {
    return;
  }

  if (film_buf.display_id == pass_id) {
    display = data_sample;
  }
  imageStore(color_accum_img, ivec3(texel_film, pass_id), data_sample);
}

void film_store_depth(ivec2 texel_film, float value, out float out_depth)
{
  if (film_buf.depth_id == -1) {
    return;
  }

  out_depth = film_depth_convert_to_scene(value);

  imageStore(depth_img, texel_film, vec4(out_depth));
}

void film_store_distance(ivec2 texel, float value)
{
  imageStore(out_weight_img, ivec3(texel, FILM_WEIGHT_LAYER_DISTANCE), vec4(value));
}

void film_store_weight(ivec2 texel, float value)
{
  imageStore(out_weight_img, ivec3(texel, FILM_WEIGHT_LAYER_ACCUMULATION), vec4(value));
}

float film_display_depth_ammend(ivec2 texel, float depth)
{
  /* This effectively offsets the depth of the whole 2x2 region to the lowest value of the region
   * twice. One for X and one for Y direction. */
  /* TODO(fclem): This could be improved as it gives flickering result at depth discontinuity.
   * But this is the quickest stable result I could come with for now. */
#ifdef GPU_FRAGMENT_SHADER
  depth += fwidth(depth);
#endif
  /* Small offset to avoid depth test lessEqual failing because of all the conversions loss. */
  depth += 2.4e-7 * 4.0;
  return saturate(depth);
}

/** \} */

/** NOTE: out_depth is scene linear depth from the camera origin. */
void film_process_data(ivec2 texel_film, out vec4 out_color, out float out_depth)
{
  out_color = vec4(0.0);
  out_depth = 0.0;

  float weight_accum = film_weight_accumulation(texel_film);
  float film_weight = film_weight_load(texel_film);
  float weight_sum = film_weight + weight_accum;
  film_store_weight(texel_film, weight_sum);

  FilmSample dst;
  dst.texel = texel_film;
  dst.weight = film_weight;
  dst.weight_sum_inv = 1.0 / weight_sum;

  /* NOTE: We split the accumulations into separate loops to avoid using too much registers and
   * maximize occupancy. */

  if (film_buf.combined_id != -1) {
    /* NOTE: Do weight accumulation again since we use custom weights. */
    float weight_accum = 0.0;
    vec4 combined_accum = vec4(0.0);

    FilmSample src;
    for (int i = film_buf.samples_len - 1; i >= 0; i--) {
      src = film_sample_get(i, texel_film);
      film_sample_accum_combined(src, combined_accum, weight_accum);
    }
    /* NOTE: src.texel is center texel in incoming data buffer. */
    film_store_combined(dst, src.texel, combined_accum, weight_accum, out_color);
  }

  if (film_buf.has_data) {
    float film_distance = film_distance_load(texel_film);

    /* Get sample closest to target texel. It is always sample 0. */
    FilmSample film_sample = film_sample_get(0, texel_film);

    if (film_buf.use_reprojection || film_sample.weight < film_distance) {
      vec4 normal = texelFetch(rp_color_tx, ivec3(film_sample.texel, rp_buf.normal_id), 0);
      float depth = texelFetch(depth_tx, film_sample.texel, 0).x;
      vec4 vector = velocity_resolve(vector_tx, film_sample.texel, depth);
      /* Transform to pixel space. */
      vector *= vec4(vec2(film_buf.render_extent), -vec2(film_buf.render_extent));

      film_store_depth(texel_film, depth, out_depth);
      film_store_data(texel_film, film_buf.normal_id, normal, out_color);
      film_store_data(texel_film, film_buf.vector_id, vector, out_color);
      film_store_distance(texel_film, film_sample.weight);
    }
    else {
      out_depth = imageLoad(depth_img, texel_film).r;
    }
  }

  if (film_buf.any_render_pass_1) {
    vec4 diffuse_light_accum = vec4(0.0);
    vec4 specular_light_accum = vec4(0.0);
    vec4 volume_light_accum = vec4(0.0);
    vec4 emission_accum = vec4(0.0);

    for (int i = 0; i < film_buf.samples_len; i++) {
      FilmSample src = film_sample_get(i, texel_film);
      film_sample_accum(src,
                        film_buf.diffuse_light_id,
                        rp_buf.diffuse_light_id,
                        rp_color_tx,
                        diffuse_light_accum);
      film_sample_accum(src,
                        film_buf.specular_light_id,
                        rp_buf.specular_light_id,
                        rp_color_tx,
                        specular_light_accum);
      film_sample_accum(
          src, film_buf.volume_light_id, rp_buf.volume_light_id, rp_color_tx, volume_light_accum);
      film_sample_accum(
          src, film_buf.emission_id, rp_buf.emission_id, rp_color_tx, emission_accum);
    }
    film_store_color(dst, film_buf.diffuse_light_id, diffuse_light_accum, out_color);
    film_store_color(dst, film_buf.specular_light_id, specular_light_accum, out_color);
    film_store_color(dst, film_buf.volume_light_id, volume_light_accum, out_color);
    film_store_color(dst, film_buf.emission_id, emission_accum, out_color);
  }

  if (film_buf.any_render_pass_2) {
    vec4 diffuse_color_accum = vec4(0.0);
    vec4 specular_color_accum = vec4(0.0);
    vec4 environment_accum = vec4(0.0);
    float mist_accum = 0.0;
    float shadow_accum = 0.0;
    float ao_accum = 0.0;

    for (int i = 0; i < film_buf.samples_len; i++) {
      FilmSample src = film_sample_get(i, texel_film);
      film_sample_accum(src,
                        film_buf.diffuse_color_id,
                        rp_buf.diffuse_color_id,
                        rp_color_tx,
                        diffuse_color_accum);
      film_sample_accum(src,
                        film_buf.specular_color_id,
                        rp_buf.specular_color_id,
                        rp_color_tx,
                        specular_color_accum);
      film_sample_accum(
          src, film_buf.environment_id, rp_buf.environment_id, rp_color_tx, environment_accum);
      film_sample_accum(src, film_buf.shadow_id, rp_buf.shadow_id, rp_value_tx, shadow_accum);
      film_sample_accum(
          src, film_buf.ambient_occlusion_id, rp_buf.ambient_occlusion_id, rp_value_tx, ao_accum);
      film_sample_accum_mist(src, mist_accum);
    }
    film_store_color(dst, film_buf.diffuse_color_id, diffuse_color_accum, out_color);
    film_store_color(dst, film_buf.specular_color_id, specular_color_accum, out_color);
    film_store_color(dst, film_buf.environment_id, environment_accum, out_color);
    film_store_color(dst, film_buf.shadow_id, vec4(vec3(shadow_accum), 1.0), out_color);
    film_store_color(dst, film_buf.ambient_occlusion_id, vec4(vec3(ao_accum), 1.0), out_color);
    film_store_value(dst, film_buf.mist_id, mist_accum, out_color);
  }

  for (int aov = 0; aov < film_buf.aov_color_len; aov++) {
    vec4 aov_accum = vec4(0.0);

    for (int i = 0; i < film_buf.samples_len; i++) {
      FilmSample src = film_sample_get(i, texel_film);
      film_sample_accum(src, 0, rp_buf.color_len + aov, rp_color_tx, aov_accum);
    }
    film_store_color(dst, film_buf.aov_color_id + aov, aov_accum, out_color);
  }

  for (int aov = 0; aov < film_buf.aov_value_len; aov++) {
    float aov_accum = 0.0;

    for (int i = 0; i < film_buf.samples_len; i++) {
      FilmSample src = film_sample_get(i, texel_film);
      film_sample_accum(src, 0, rp_buf.value_len + aov, rp_value_tx, aov_accum);
    }
    film_store_value(dst, film_buf.aov_value_id + aov, aov_accum, out_color);
  }

  if (film_buf.cryptomatte_samples_len != 0) {
    /* Cryptomatte passes cannot be cleared by a weighted store like other passes. */
    if (!film_buf.use_history || film_buf.use_reprojection) {
      cryptomatte_clear_samples(dst);
    }

    film_cryptomatte_layer_accum_and_store(
        dst, texel_film, film_buf.cryptomatte_object_id, 0, out_color);
    film_cryptomatte_layer_accum_and_store(
        dst, texel_film, film_buf.cryptomatte_asset_id, 1, out_color);
    film_cryptomatte_layer_accum_and_store(
        dst, texel_film, film_buf.cryptomatte_material_id, 2, out_color);
  }
}
