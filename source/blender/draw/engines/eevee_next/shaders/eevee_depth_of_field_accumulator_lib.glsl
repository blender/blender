/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Depth of Field Gather accumulator.
 * We currently have only 2 which are very similar.
 * One is for the halfres gather passes and the other one for slight in focus regions.
 */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_colorspace_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_depth_of_field_lib.glsl)

/* -------------------------------------------------------------------- */
/** \name Options.
 * \{ */

/* Quality options */
#ifdef DOF_HOLEFILL_PASS
/* No need for very high density for hole_fill. */
const int gather_ring_count = 3;
const int gather_ring_density = 3;
const int gather_max_density_change = 0;
const int gather_density_change_ring = 1;
#else
const int gather_ring_count = DOF_GATHER_RING_COUNT;
const int gather_ring_density = 3;
const int gather_max_density_change = 50; /* Dictates the maximum good quality blur. */
const int gather_density_change_ring = 1;
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Constants.
 * \{ */

const float unit_ring_radius = 1.0 / float(gather_ring_count);
const float unit_sample_radius = 1.0 / float(gather_ring_count + 0.5);
const float large_kernel_radius = 0.5 + float(gather_ring_count);
const float smaller_kernel_radius = 0.5 + float(gather_ring_count - gather_density_change_ring);
/* NOTE(fclem) the bias is reducing issues with density change visible transition. */
const float radius_downscale_factor = smaller_kernel_radius / large_kernel_radius;
const int change_density_at_ring = (gather_ring_count - gather_density_change_ring + 1);
const float coc_radius_error = 2.0;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gather common.
 * \{ */

struct DofGatherData {
  vec4 color;
  float weight;
  float dist; /* TODO remove */
  /* For scatter occlusion. */
  float coc;
  float coc_sqr;
  /* For ring bucket merging. */
  float transparency;

  float layer_opacity;

#ifdef GPU_METAL
  /* Explicit constructors -- To support GLSL syntax. */
  inline DofGatherData() = default;
  inline DofGatherData(vec4 in_color,
                       float in_weight,
                       float in_dist,
                       float in_coc,
                       float in_coc_sqr,
                       float in_transparency,
                       float in_layer_opacity)
      : color(in_color),
        weight(in_weight),
        dist(in_dist),
        coc(in_coc),
        coc_sqr(in_coc_sqr),
        transparency(in_transparency),
        layer_opacity(in_layer_opacity)
  {
  }
#endif
};

#define GATHER_DATA_INIT DofGatherData(vec4(0.0), 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)

/* Intersection with the center of the kernel. */
float dof_intersection_weight(float coc, float distance_from_center, float intersection_multiplier)
{
  if (no_smooth_intersection) {
    return step(0.0, (abs(coc) - distance_from_center));
  }
  else {
    /* (Slide 64). */
    return saturate((abs(coc) - distance_from_center) * intersection_multiplier + 0.5);
  }
}

/* Returns weight of the sample for the outer bucket (containing previous
 * rings). */
float dof_gather_accum_weight(float coc, float bordering_radius, bool first_ring)
{
  /* First ring has nothing to be mixed against. */
  if (first_ring) {
    return 0.0;
  }
  return saturate(coc - bordering_radius);
}

void dof_gather_ammend_weight(inout DofGatherData sample_data, float weight)
{
  sample_data.color *= weight;
  sample_data.coc *= weight;
  sample_data.coc_sqr *= weight;
  sample_data.weight *= weight;
}

void dof_gather_accumulate_sample(DofGatherData sample_data,
                                  float weight,
                                  inout DofGatherData accum_data)
{
  accum_data.color += sample_data.color * weight;
  accum_data.coc += sample_data.coc * weight;
  accum_data.coc_sqr += sample_data.coc * (sample_data.coc * weight);
  accum_data.weight += weight;
}

void dof_gather_accumulate_sample_pair(DofGatherData pair_data[2],
                                       float bordering_radius,
                                       float intersection_multiplier,
                                       bool first_ring,
                                       const bool do_fast_gather,
                                       const bool is_foreground,
                                       inout DofGatherData ring_data,
                                       inout DofGatherData accum_data)
{
  if (do_fast_gather) {
    for (int i = 0; i < 2; i++) {
      dof_gather_accumulate_sample(pair_data[i], 1.0, accum_data);
      accum_data.layer_opacity += 1.0;
    }
    return;
  }

#if 0
  const float mirroring_threshold = -dof_layer_threshold - dof_layer_offset;
  /* TODO(fclem) Promote to parameter? dither with Noise? */
  const float mirroring_min_distance = 15.0;
  if (pair_data[0].coc < mirroring_threshold &&
      (pair_data[1].coc - mirroring_min_distance) > pair_data[0].coc)
  {
    pair_data[1].coc = pair_data[0].coc;
  }
  else if (pair_data[1].coc < mirroring_threshold &&
           (pair_data[0].coc - mirroring_min_distance) > pair_data[1].coc)
  {
    pair_data[0].coc = pair_data[1].coc;
  }
#endif

  for (int i = 0; i < 2; i++) {
    float sample_weight = dof_sample_weight(pair_data[i].coc);
    float layer_weight = dof_layer_weight(pair_data[i].coc, is_foreground);
    float inter_weight = dof_intersection_weight(
        pair_data[i].coc, pair_data[i].dist, intersection_multiplier);
    float weight = inter_weight * layer_weight * sample_weight;

    /* If a CoC is larger than bordering radius we accumulate it to the general accumulator.
     * If not, we accumulate to the ring bucket. This is to have more consistent sample occlusion.
     */
    float accum_weight = dof_gather_accum_weight(pair_data[i].coc, bordering_radius, first_ring);
    dof_gather_accumulate_sample(pair_data[i], weight * accum_weight, accum_data);
    dof_gather_accumulate_sample(pair_data[i], weight * (1.0 - accum_weight), ring_data);

    accum_data.layer_opacity += layer_weight;

    if (is_foreground) {
      ring_data.transparency += 1.0 - inter_weight * layer_weight;
    }
    else {
      float coc = is_foreground ? -pair_data[i].coc : pair_data[i].coc;
      ring_data.transparency += saturate(coc - bordering_radius);
    }
  }
}

void dof_gather_accumulate_sample_ring(DofGatherData ring_data,
                                       int sample_count,
                                       bool first_ring,
                                       const bool do_fast_gather,
                                       /* accum_data occludes the ring_data if true. */
                                       const bool reversed_occlusion,
                                       inout DofGatherData accum_data)
{
  if (do_fast_gather) {
    /* Do nothing as ring_data contains nothing. All samples are already in
     * accum_data. */
    return;
  }

  if (first_ring) {
    /* Layer opacity is directly accumulated into accum_data data. */
    accum_data.color = ring_data.color;
    accum_data.coc = ring_data.coc;
    accum_data.coc_sqr = ring_data.coc_sqr;
    accum_data.weight = ring_data.weight;

    accum_data.transparency = ring_data.transparency / float(sample_count);
    return;
  }

  if (ring_data.weight == 0.0) {
    return;
  }

  float ring_avg_coc = ring_data.coc / ring_data.weight;
  float accum_avg_coc = accum_data.coc / accum_data.weight;

  /* Smooth test to set opacity to see if the ring average coc occludes the
   * accumulation. Test is reversed to be multiplied against opacity. */
  float ring_occlu = saturate(accum_avg_coc - ring_avg_coc);
  /* The bias here is arbitrary. Seems to avoid weird looking foreground in most
   * cases. We might need to make it a parameter or find a relative bias. */
  float accum_occlu = saturate((ring_avg_coc - accum_avg_coc) * 0.1 - 1.0);

  if (is_resolve) {
    ring_occlu = accum_occlu = 0.0;
  }

  if (no_gather_occlusion) {
    ring_occlu = 0.0;
    accum_occlu = 0.0;
  }

  /* (Slide 40) */
  float ring_opacity = saturate(1.0 - ring_data.transparency / float(sample_count));
  float accum_opacity = 1.0 - accum_data.transparency;

  if (reversed_occlusion) {
    /* Accum_data occludes the ring. */
    float alpha = (accum_data.weight == 0.0) ? 0.0 : accum_opacity * accum_occlu;
    float one_minus_alpha = 1.0 - alpha;

    accum_data.color += ring_data.color * one_minus_alpha;
    accum_data.coc += ring_data.coc * one_minus_alpha;
    accum_data.coc_sqr += ring_data.coc_sqr * one_minus_alpha;
    accum_data.weight += ring_data.weight * one_minus_alpha;

    accum_data.transparency *= 1.0 - ring_opacity;
  }
  else {
    /* Ring occludes the accum_data (Same as reference). */
    float alpha = (accum_data.weight == 0.0) ? 1.0 : (ring_opacity * ring_occlu);
    float one_minus_alpha = 1.0 - alpha;

    accum_data.color = accum_data.color * one_minus_alpha + ring_data.color;
    accum_data.coc = accum_data.coc * one_minus_alpha + ring_data.coc;
    accum_data.coc_sqr = accum_data.coc_sqr * one_minus_alpha + ring_data.coc_sqr;
    accum_data.weight = accum_data.weight * one_minus_alpha + ring_data.weight;
  }
}

/* FIXME(fclem) Seems to be wrong since it needs `ringcount + 1` as input for
 * slight-focus gather. */
/* This should be replaced by web_sample_count_get() but doing so is breaking other things. */
int dof_gather_total_sample_count(const int ring_count, const int ring_density)
{
  return (ring_count * ring_count - ring_count) * ring_density + 1;
}

void dof_gather_accumulate_center_sample(DofGatherData center_data,
                                         float bordering_radius,
                                         int i_radius,
                                         const bool do_fast_gather,
                                         const bool is_foreground,
                                         const bool is_resolve,
                                         inout DofGatherData accum_data)
{
  float layer_weight = dof_layer_weight(center_data.coc, is_foreground);
  float sample_weight = dof_sample_weight(center_data.coc);
  float weight = layer_weight * sample_weight;
  float accum_weight = dof_gather_accum_weight(center_data.coc, bordering_radius, false);

  if (do_fast_gather) {
    /* Hope for the compiler to optimize the above. */
    layer_weight = 1.0;
    sample_weight = 1.0;
    accum_weight = 1.0;
    weight = 1.0;
  }

  center_data.transparency = 1.0 - weight;

  dof_gather_accumulate_sample(center_data, weight * accum_weight, accum_data);

  if (!do_fast_gather) {
    if (is_resolve) {
      /* NOTE(fclem): Hack to smooth transition to full in-focus opacity. */
      int total_sample_count = dof_gather_total_sample_count(i_radius + 1,
                                                             DOF_SLIGHT_FOCUS_DENSITY);
      float fac = saturate(1.0 - abs(center_data.coc) / float(dof_layer_threshold));
      accum_data.layer_opacity += float(total_sample_count) * fac * fac;
    }
    accum_data.layer_opacity += layer_weight;

    /* Logic of dof_gather_accumulate_sample(). */
    weight *= (1.0 - accum_weight);
    center_data.coc_sqr = center_data.coc * (center_data.coc * weight);
    center_data.color *= weight;
    center_data.coc *= weight;
    center_data.weight = weight;

    if (is_foreground && !is_resolve) {
      /* Reduce issue with closer foreground over distant foreground. */
      float ring_area = sqr(bordering_radius);
      dof_gather_ammend_weight(center_data, ring_area);
    }

    /* Accumulate center as its own ring. */
    dof_gather_accumulate_sample_ring(
        center_data, 1, false, do_fast_gather, is_foreground, accum_data);
  }
}

int dof_gather_total_sample_count_with_density_change(const int ring_count,
                                                      const int ring_density,
                                                      int density_change)
{
  int sample_count_per_density_change = dof_gather_total_sample_count(ring_count, ring_density) -
                                        dof_gather_total_sample_count(
                                            ring_count - gather_density_change_ring, ring_density);

  return dof_gather_total_sample_count(ring_count, ring_density) +
         sample_count_per_density_change * density_change;
}

void dof_gather_accumulate_resolve(int total_sample_count,
                                   DofGatherData accum_data,
                                   out vec4 out_col,
                                   out float out_weight,
                                   out vec2 out_occlusion)
{
  float weight_inv = safe_rcp(accum_data.weight);
  out_col = accum_data.color * weight_inv;
  out_occlusion = vec2(abs(accum_data.coc), accum_data.coc_sqr) * weight_inv;

  if (is_foreground) {
    out_weight = 1.0 - accum_data.transparency;
  }
  else if (accum_data.weight > 0.0) {
    out_weight = accum_data.layer_opacity / float(total_sample_count);
  }
  else {
    out_weight = 0.0;
  }
  /* Gathering may not accumulate to 1.0 alpha because of float precision. */
  if (out_weight > 0.99) {
    out_weight = 1.0;
  }
  else if (out_weight < 0.01) {
    out_weight = 0.0;
  }
  /* Same thing for alpha channel. */
  if (out_col.a > 0.993) {
    out_col.a = 1.0;
  }
  else if (out_col.a < 0.003) {
    out_col.a = 0.0;
  }
}

float dof_load_gather_coc(sampler2D gather_input_coc_tx, vec2 uv, float lod)
{
  float coc = textureLod(gather_input_coc_tx, uv, lod).r;
  /* We gather at halfres. CoC must be divided by 2 to be compared against radii. */
  return coc * 0.5;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Common Gather accumulator.
 * \{ */

/* Radii needs to be halfres CoC sizes. */
bool dof_do_density_change(float base_radius, float min_intersectable_radius)
{
  /* Reduce artifact for very large blur. */
  min_intersectable_radius *= 0.1;

  bool need_new_density = (base_radius * unit_ring_radius > min_intersectable_radius);
  bool larger_than_min_density = (base_radius * radius_downscale_factor >
                                  float(gather_ring_count));

  return need_new_density && larger_than_min_density;
}

void dof_gather_init(float base_radius,
                     vec2 noise,
                     out vec2 center_co,
                     out float lod,
                     out float intersection_multiplier)
{
  /* Jitter center half a ring to reduce undersampling. */
  vec2 jitter_ofs = 0.499 * sample_disk(noise);
  if (DOF_BOKEH_TEXTURE) {
    jitter_ofs *= dof_buf.bokeh_anisotropic_scale;
  }
  vec2 frag_coord = vec2(gl_GlobalInvocationID.xy) + 0.5;
  center_co = frag_coord + jitter_ofs * base_radius * unit_sample_radius;

  /* TODO(fclem) Seems like the default lod selection is too big. Bias to avoid blocky moving out
   * of focus shapes. */
  const float lod_bias = -2.0;
  lod = max(floor(log2(base_radius * unit_sample_radius) + 0.5) + lod_bias, 0.0);

  if (no_gather_mipmaps) {
    lod = 0.0;
  }
  /* (Slide 64). */
  intersection_multiplier = pow(0.5, lod);
}

void dof_gather_accumulator(sampler2D color_tx,
                            sampler2D color_bilinear_tx,
                            sampler2D coc_tx,
                            sampler2D bkh_lut_tx, /* Renamed because of ugly macro. */
                            float base_radius,
                            float min_intersectable_radius,
                            const bool do_fast_gather,
                            const bool do_density_change,
                            out vec4 out_color,
                            out float out_weight,
                            out vec2 out_occlusion)
{
  vec2 frag_coord = vec2(gl_GlobalInvocationID.xy);
  vec2 noise_offset = sampling_rng_2D_get(SAMPLING_LENS_U);
  vec2 noise = no_gather_random ? vec2(0.0, 0.0) :
                                  vec2(interlieved_gradient_noise(frag_coord, 0, noise_offset.x),
                                       interlieved_gradient_noise(frag_coord, 1, noise_offset.y));

  if (!do_fast_gather) {
    /* Jitter the radius to reduce noticeable density changes. */
    base_radius += noise.x * unit_ring_radius * base_radius;
  }
  else {
    /* Jittering the radius more than we need means we are going to feather the bokeh shape half a
     * ring. So we need to compensate for fast gather that does not check CoC intersection. */
    base_radius += (0.5 - noise.x) * 1.5 * unit_ring_radius * base_radius;
  }
  /* TODO(fclem) another seed? For now Cranly-Partterson rotation with golden ratio. */
  noise.x = fract(noise.x * 6.1803398875);

  float lod, isect_mul;
  vec2 center_co;
  dof_gather_init(base_radius, noise, center_co, lod, isect_mul);

  bool first_ring = true;

  DofGatherData accum_data = GATHER_DATA_INIT;

  int density_change = 0;
  for (int ring = gather_ring_count; ring > 0; ring--) {
    int sample_pair_count = gather_ring_density * ring;

    float step_rot = M_PI / float(sample_pair_count);
    mat2 step_rot_mat = rot2_from_angle(step_rot);

    float angle_offset = noise.y * step_rot;
    vec2 offset = vec2(cos(angle_offset), sin(angle_offset));

    float ring_radius = float(ring) * unit_sample_radius * base_radius;

    /* Slide 38. */
    float bordering_radius = ring_radius +
                             (0.5 + coc_radius_error) * base_radius * unit_sample_radius;
    DofGatherData ring_data = GATHER_DATA_INIT;
    for (int sample_pair = 0; sample_pair < sample_pair_count; sample_pair++) {
      offset = step_rot_mat * offset;

      DofGatherData pair_data[2];
      for (int i = 0; i < 2; i++) {
        vec2 offset_co = ((i == 0) ? offset : -offset);
        if (DOF_BOKEH_TEXTURE) {
          /* Scaling to 0.25 for speed. Improves texture cache hit. */
          offset_co = texture(bkh_lut_tx, offset_co * 0.25 + 0.5).rg;
          offset_co *= (is_foreground) ? -dof_buf.bokeh_anisotropic_scale :
                                         dof_buf.bokeh_anisotropic_scale;
        }
        vec2 sample_co = center_co + offset_co * ring_radius;
        vec2 sample_uv = sample_co * dof_buf.gather_uv_fac;
        if (do_fast_gather) {
          pair_data[i].color = textureLod(color_bilinear_tx, sample_uv, lod);
        }
        else {
          pair_data[i].color = textureLod(color_tx, sample_uv, lod);
        }
        pair_data[i].coc = dof_load_gather_coc(coc_tx, sample_uv, lod);
        pair_data[i].dist = ring_radius;
      }

      dof_gather_accumulate_sample_pair(pair_data,
                                        bordering_radius,
                                        isect_mul,
                                        first_ring,
                                        do_fast_gather,
                                        is_foreground,
                                        ring_data,
                                        accum_data);
    }

    if (is_foreground) {
      /* Reduce issue with closer foreground over distant foreground. */
      /* TODO(fclem) this seems to not be completely correct as the issue remains. */
      float ring_area = (sqr(float(ring) + 0.5 + coc_radius_error) -
                         sqr(float(ring) - 0.5 + coc_radius_error)) *
                        sqr(base_radius * unit_sample_radius);
      dof_gather_ammend_weight(ring_data, ring_area);
    }

    dof_gather_accumulate_sample_ring(
        ring_data, sample_pair_count * 2, first_ring, do_fast_gather, is_foreground, accum_data);

    first_ring = false;

    if (do_density_change && (ring == change_density_at_ring) &&
        (density_change < gather_max_density_change))
    {
      if (dof_do_density_change(base_radius, min_intersectable_radius)) {
        base_radius *= radius_downscale_factor;
        ring += gather_density_change_ring;
        /* We need to account for the density change in the weights (slide 62).
         * For that multiply old kernel data by its area divided by the new kernel area. */
        const float outer_rings_weight = 1.0 / (radius_downscale_factor * radius_downscale_factor);
        /* Samples are already weighted per ring in foreground pass. */
        if (!is_foreground) {
          dof_gather_ammend_weight(accum_data, outer_rings_weight);
        }
        /* Re-init kernel position & sampling parameters. */
        dof_gather_init(base_radius, noise, center_co, lod, isect_mul);
        density_change++;
      }
    }
  }

  {
    /* Center sample. */
    vec2 sample_uv = center_co * dof_buf.gather_uv_fac;
    DofGatherData center_data;
    if (do_fast_gather) {
      center_data.color = textureLod(color_bilinear_tx, sample_uv, lod);
    }
    else {
      center_data.color = textureLod(color_tx, sample_uv, lod);
    }
    center_data.coc = dof_load_gather_coc(coc_tx, sample_uv, lod);
    center_data.dist = 0.0;

    /* Slide 38. */
    float bordering_radius = (0.5 + coc_radius_error) * base_radius * unit_sample_radius;

    dof_gather_accumulate_center_sample(
        center_data, bordering_radius, 0, do_fast_gather, is_foreground, false, accum_data);
  }

  int total_sample_count = dof_gather_total_sample_count_with_density_change(
      gather_ring_count, gather_ring_density, density_change);
  dof_gather_accumulate_resolve(
      total_sample_count, accum_data, out_color, out_weight, out_occlusion);

  if (debug_gather_perf && density_change > 0) {
    float fac = saturate(float(density_change) / float(10.0));
    out_color.rgb = avg(out_color.rgb) * neon_gradient(fac);
  }
  if (debug_gather_perf && do_fast_gather) {
    out_color.rgb = avg(out_color.rgb) * vec3(0.0, 1.0, 0.0);
  }
  if (debug_scatter_perf) {
    out_color.rgb = avg(out_color.rgb) * vec3(0.0, 1.0, 0.0);
  }

  /* Output premultiplied color so we can use bilinear sampler in resolve pass. */
  out_color *= out_weight;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Slight focus accumulator.
 *
 * The full pixel neighborhood is gathered.
 * \{ */

void dof_slight_focus_gather(depth2D depth_tx,
                             sampler2D color_tx,
                             sampler2D bkh_lut_tx, /* Renamed because of ugly macro job. */
                             float radius,
                             out vec4 out_color,
                             out float out_weight,
                             out float out_center_coc)
{
  vec2 frag_coord = vec2(gl_GlobalInvocationID.xy) + 0.5;
  vec2 noise_offset = sampling_rng_2D_get(SAMPLING_LENS_U);
  vec2 noise = no_gather_random ? vec2(0.0) :
                                  vec2(interlieved_gradient_noise(frag_coord, 3, noise_offset.x),
                                       interlieved_gradient_noise(frag_coord, 5, noise_offset.y));

  DofGatherData fg_accum = GATHER_DATA_INIT;
  DofGatherData bg_accum = GATHER_DATA_INIT;

  int i_radius = clamp(int(radius), 0, int(dof_layer_threshold));

  const float sample_count_max = float(DOF_SLIGHT_FOCUS_SAMPLE_MAX);
  /* Scale by search area. */
  float sample_count = sample_count_max * saturate(sqr(radius) / sqr(dof_layer_threshold));

  bool first_ring = true;

  for (float s = 0.0; s < sample_count; s++) {
    vec2 rand2 = fract(hammersley_2d(s, sample_count) + noise);
    vec2 offset = sample_disk(rand2);
    float ring_dist = sqrt(rand2.y);

    DofGatherData pair_data[2];
    for (int i = 0; i < 2; i++) {
      vec2 sample_offset = ((i == 0) ? offset : -offset);
      /* OPTI: could precompute the factor. */
      vec2 sample_uv = (frag_coord + sample_offset) / vec2(textureSize(depth_tx, 0));
      float depth = textureLod(depth_tx, sample_uv, 0.0).r;
      pair_data[i].coc = dof_coc_from_depth(dof_buf, sample_uv, depth);
      pair_data[i].color = safe_color(textureLod(color_tx, sample_uv, 0.0));
      pair_data[i].dist = ring_dist;
      if (DOF_BOKEH_TEXTURE) {
        /* Contains sub-pixel distance to bokeh shape. */
        ivec2 lut_texel = ivec2(round(sample_offset)) + dof_max_slight_focus_radius;
        pair_data[i].dist = texelFetch(bkh_lut_tx, lut_texel, 0).r;
      }
      pair_data[i].coc = clamp(pair_data[i].coc, -dof_buf.coc_abs_max, dof_buf.coc_abs_max);
    }

    float bordering_radius = ring_dist + 0.5;
    const float isect_mul = 1.0;
    DofGatherData bg_ring = GATHER_DATA_INIT;
    dof_gather_accumulate_sample_pair(
        pair_data, bordering_radius, isect_mul, first_ring, false, false, bg_ring, bg_accum);
    /* Treat each sample as a ring. */
    dof_gather_accumulate_sample_ring(bg_ring, 2, first_ring, false, false, bg_accum);

    if (DOF_BOKEH_TEXTURE) {
      /* Swap distances in order to flip bokeh shape for foreground. */
      float tmp = pair_data[0].dist;
      pair_data[0].dist = pair_data[1].dist;
      pair_data[1].dist = tmp;
    }
    DofGatherData fg_ring = GATHER_DATA_INIT;
    dof_gather_accumulate_sample_pair(
        pair_data, bordering_radius, isect_mul, first_ring, false, true, fg_ring, fg_accum);
    /* Treat each sample as a ring. */
    dof_gather_accumulate_sample_ring(fg_ring, 2, first_ring, false, true, fg_accum);

    first_ring = false;
  }

  /* Center sample. */
  vec2 sample_uv = frag_coord / vec2(textureSize(depth_tx, 0));
  DofGatherData center_data;
  center_data.color = safe_color(textureLod(color_tx, sample_uv, 0.0));
  center_data.coc = dof_coc_from_depth(dof_buf, sample_uv, textureLod(depth_tx, sample_uv, 0.0).r);
  center_data.coc = clamp(center_data.coc, -dof_buf.coc_abs_max, dof_buf.coc_abs_max);
  center_data.dist = 0.0;

  out_center_coc = center_data.coc;

  /* Slide 38. */
  float bordering_radius = 0.5;

  dof_gather_accumulate_center_sample(
      center_data, bordering_radius, i_radius, false, true, true, fg_accum);
  dof_gather_accumulate_center_sample(
      center_data, bordering_radius, i_radius, false, false, true, bg_accum);

  vec4 bg_col, fg_col;
  float bg_weight, fg_weight;
  vec2 unused_occlusion;

  int total_sample_count = int(sample_count) * 2 + 1;
  dof_gather_accumulate_resolve(total_sample_count, bg_accum, bg_col, bg_weight, unused_occlusion);
  dof_gather_accumulate_resolve(total_sample_count, fg_accum, fg_col, fg_weight, unused_occlusion);

  /* Fix weighting issues on perfectly focus to slight focus transitioning areas. */
  if (abs(center_data.coc) < 0.5) {
    bg_col = center_data.color;
    bg_weight = 1.0;
  }

  /* Alpha Over */
  float alpha = 1.0 - fg_weight;
  out_weight = bg_weight * alpha + fg_weight;
  out_color = bg_col * bg_weight * alpha + fg_col * fg_weight;
}

/** \} */
