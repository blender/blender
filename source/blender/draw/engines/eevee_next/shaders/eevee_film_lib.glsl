
/**
 * Film accumulation utils functions.
 **/

#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_camera_lib.glsl)

/* Return scene linear Z depth from the camera or radial depth for panoramic cameras. */
float film_depth_convert_to_scene(float depth)
{
  if (false /* Panoramic */) {
    /* TODO */
    return 1.0;
  }
  return abs(get_view_z_from_depth(depth));
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
  film_sample.texel += texel_film;
  /* Use extend on borders. */
  film_sample.texel = clamp(film_sample.texel, ivec2(0, 0), film_buf.extent - 1);

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

void film_sample_accum(FilmSample samp, int pass_id, sampler2D tex, inout vec4 accum)
{
  if (pass_id == -1) {
    return;
  }
  accum += texelFetch(tex, samp.texel, 0) * samp.weight;
}

void film_sample_accum(FilmSample samp, int pass_id, sampler2D tex, inout float accum)
{
  if (pass_id == -1) {
    return;
  }
  accum += texelFetch(tex, samp.texel, 0).x * samp.weight;
}

void film_sample_accum(FilmSample samp, int pass_id, sampler2DArray tex, inout vec4 accum)
{
  if (pass_id == -1) {
    return;
  }
  accum += texelFetch(tex, ivec3(samp.texel, pass_id), 0) * samp.weight;
}

void film_sample_accum(FilmSample samp, int pass_id, sampler2DArray tex, inout float accum)
{
  if (pass_id == -1) {
    return;
  }
  accum += texelFetch(tex, ivec3(samp.texel, pass_id), 0).x * samp.weight;
}

void film_sample_accum_mist(FilmSample samp, inout float accum)
{
  if (film_buf.mist_id == -1) {
    return;
  }
  float depth = texelFetch(depth_tx, samp.texel, 0).x;
  vec2 uv = (vec2(samp.texel) + 0.5) / textureSize(depth_tx, 0).xy;
  vec3 vP = get_view_space_from_depth(uv, depth);
  bool is_persp = ProjectionMatrix[3][3] == 0.0;
  float mist = (is_persp) ? length(vP) : abs(vP.z);
  /* Remap to 0..1 range. */
  mist = saturate(mist * film_buf.mist_scale + film_buf.mist_bias);
  /* Falloff. */
  mist = pow(mist, film_buf.mist_exponent);
  accum += mist * samp.weight;
}

void film_sample_accum_combined(FilmSample samp, inout vec4 accum)
{
  if (film_buf.combined_id == -1) {
    return;
  }
  vec4 color = texelFetch(combined_tx, samp.texel, 0);
  /* Convert transmittance to opacity. */
  color.a = saturate(1.0 - color.a);
  /* TODO(fclem) Pre-expose. */
  color.rgb = log2(1.0 + color.rgb);

  accum += color * samp.weight;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Load/Store Data
 * \{ */

#define WEIGHT_lAYER_ACCUMULATION 0
#define WEIGHT_lAYER_DISTANCE 1

/* Returns the distance used to store nearest interpolation data. */
float film_distance_load(ivec2 texel)
{
  /* Repeat texture coordinates as the weight can be optimized to a small portion of the film. */
  texel = texel % imageSize(in_weight_img).xy;

  if (film_buf.use_history == false) {
    return 1.0e16;
  }
  return imageLoad(in_weight_img, ivec3(texel, WEIGHT_lAYER_DISTANCE)).x;
}

float film_weight_load(ivec2 texel)
{
  /* Repeat texture coordinates as the weight can be optimized to a small portion of the film. */
  texel = texel % imageSize(in_weight_img).xy;

  if (film_buf.use_history == false) {
    return 0.0;
  }
  return imageLoad(in_weight_img, ivec3(texel, WEIGHT_lAYER_ACCUMULATION)).x;
}

/* Return the motion in pixels. */
void film_motion_load()
{
  // ivec2 texel_sample = film_sample_get(0, texel_film, distance_sample);
  // vec4 vector = texelFetch(vector_tx, texel_sample);

  // vector.xy *= film_buf.extent;
}

/* Returns resolved final color. */
void film_store_combined(FilmSample dst, vec4 color, inout vec4 display)
{
  if (film_buf.combined_id == -1) {
    return;
  }

  /* Could we assume safe color from earlier pass? */
  color = safe_color(color);
  if (false) {
    /* Re-projection using motion vectors. */
    // ivec2 texel_combined = texel_film + film_motion_load(texel_film);
    // float weight_combined = film_weight_load(texel_combined);
  }
#ifdef USE_NEIGHBORHOOD_CLAMPING
  /* Only do that for combined pass as it has a non-negligeable performance impact. */
  // color = clamp_bbox(color, min, max);
#endif

  vec4 dst_color = imageLoad(in_combined_img, dst.texel);

  color = (dst_color * dst.weight + color) * dst.weight_sum_inv;

  /* TODO(fclem) undo Pre-expose. */
  // color.rgb = exp2(color.rgb) - 1.0;

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
  imageStore(out_weight_img, ivec3(texel, WEIGHT_lAYER_DISTANCE), vec4(value));
}

void film_store_weight(ivec2 texel, float value)
{
  imageStore(out_weight_img, ivec3(texel, WEIGHT_lAYER_ACCUMULATION), vec4(value));
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

  if (film_buf.has_data) {
    float film_weight = film_distance_load(texel_film);

    /* Get sample closest to target texel. It is always sample 0. */
    FilmSample film_sample = film_sample_get(0, texel_film);

    if (film_sample.weight < film_weight) {
      float depth = texelFetch(depth_tx, film_sample.texel, 0).x;
      vec4 normal = texelFetch(normal_tx, film_sample.texel, 0);
      vec4 vector = texelFetch(vector_tx, film_sample.texel, 0);

      film_store_depth(texel_film, depth, out_depth);
      film_store_data(texel_film, film_buf.normal_id, normal, out_color);
      film_store_data(texel_film, film_buf.vector_id, vector, out_color);
      film_store_distance(texel_film, film_sample.weight);
    }
    else {
      out_depth = imageLoad(depth_img, texel_film).r;
    }
  }

  if (film_buf.combined_id != -1) {
    vec4 combined_accum = vec4(0.0);

    for (int i = 0; i < film_buf.samples_len; i++) {
      FilmSample src = film_sample_get(i, texel_film);
      film_sample_accum_combined(src, combined_accum);
    }
    film_store_combined(dst, combined_accum, out_color);
  }

  if (film_buf.any_render_pass_1) {
    vec4 diffuse_light_accum = vec4(0.0);
    vec4 specular_light_accum = vec4(0.0);
    vec4 volume_light_accum = vec4(0.0);
    vec4 emission_accum = vec4(0.0);

    for (int i = 0; i < film_buf.samples_len; i++) {
      FilmSample src = film_sample_get(i, texel_film);
      film_sample_accum(src, film_buf.diffuse_light_id, diffuse_light_tx, diffuse_light_accum);
      film_sample_accum(src, film_buf.specular_light_id, specular_light_tx, specular_light_accum);
      film_sample_accum(src, film_buf.volume_light_id, volume_light_tx, volume_light_accum);
      film_sample_accum(src, film_buf.emission_id, emission_tx, emission_accum);
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
      film_sample_accum(src, film_buf.diffuse_color_id, diffuse_color_tx, diffuse_color_accum);
      film_sample_accum(src, film_buf.specular_color_id, specular_color_tx, specular_color_accum);
      film_sample_accum(src, film_buf.environment_id, environment_tx, environment_accum);
      film_sample_accum(src, film_buf.shadow_id, shadow_tx, shadow_accum);
      film_sample_accum(src, film_buf.ambient_occlusion_id, ambient_occlusion_tx, ao_accum);
      film_sample_accum_mist(src, mist_accum);
    }
    film_store_color(dst, film_buf.diffuse_color_id, diffuse_color_accum, out_color);
    film_store_color(dst, film_buf.specular_color_id, specular_color_accum, out_color);
    film_store_color(dst, film_buf.environment_id, environment_accum, out_color);
    film_store_value(dst, film_buf.shadow_id, shadow_accum, out_color);
    film_store_value(dst, film_buf.ambient_occlusion_id, ao_accum, out_color);
    film_store_value(dst, film_buf.mist_id, mist_accum, out_color);
  }

  for (int aov = 0; aov < film_buf.aov_color_len; aov++) {
    vec4 aov_accum = vec4(0.0);

    for (int i = 0; i < film_buf.samples_len; i++) {
      FilmSample src = film_sample_get(i, texel_film);
      film_sample_accum(src, aov, aov_color_tx, aov_accum);
    }
    film_store_color(dst, film_buf.aov_color_id + aov, aov_accum, out_color);
  }

  for (int aov = 0; aov < film_buf.aov_value_len; aov++) {
    float aov_accum = 0.0;

    for (int i = 0; i < film_buf.samples_len; i++) {
      FilmSample src = film_sample_get(i, texel_film);
      film_sample_accum(src, aov, aov_value_tx, aov_accum);
    }
    film_store_value(dst, film_buf.aov_value_id + aov, aov_accum, out_color);
  }
}
