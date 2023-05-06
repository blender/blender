
/**
 * Gather pass: Convolve foreground and background parts in separate passes.
 *
 * Using the min&max CoC tile buffer, we select the best appropriate method to blur the scene
 * color. A fast gather path is taken if there is not many CoC variation inside the tile.
 *
 * We sample using an octaweb sampling pattern. We randomize the kernel center and each ring
 * rotation to ensure maximum coverage.
 */

#pragma BLENDER_REQUIRE(common_utiltex_lib.glsl)
#pragma BLENDER_REQUIRE(effect_dof_lib.glsl)

#ifdef DOF_HOLEFILL_PASS
/* Dirty global variable that isn't used. So it should get optimized out. */
vec2 outOcclusion;
#endif

#ifdef DOF_FOREGROUND_PASS
#  define is_foreground true
#else /* DOF_BACKGROUND_PASS */
#  define is_foreground false
#endif

const float unit_ring_radius = 1.0 / float(gather_ring_count);
const float unit_sample_radius = 1.0 / float(gather_ring_count + 0.5);
const float large_kernel_radius = 0.5 + float(gather_ring_count);
const float smaller_kernel_radius = 0.5 + float(gather_ring_count - gather_density_change_ring);
/* NOTE(@fclem): the bias is reducing issues with density change visible transition. */
const float radius_downscale_factor = smaller_kernel_radius / large_kernel_radius;
const int change_density_at_ring = (gather_ring_count - gather_density_change_ring + 1);
const float coc_radius_error = 2.0;

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
                     vec4 noise,
                     out vec2 center_co,
                     out float lod,
                     out float intersection_multiplier)
{
  /* Jitter center half a ring to reduce undersampling. */
  vec2 jitter_ofs = 0.499 * noise.zw * sqrt(noise.x);
#ifdef DOF_BOKEH_TEXTURE
  jitter_ofs *= bokehAnisotropy;
#endif
  center_co = gl_FragCoord.xy + jitter_ofs * base_radius * unit_sample_radius;

  /* TODO(@fclem): Seems like the default lod selection is too big. Bias to avoid blocky moving
   * out of focus shapes. */
  const float lod_bias = -2.0;
  lod = max(floor(log2(base_radius * unit_sample_radius) + 0.5) + lod_bias, 0.0);

  if (no_gather_mipmaps) {
    lod = 0.0;
  }
  /* (Slide 64). */
  intersection_multiplier = pow(0.5, lod);
}

void dof_gather_accumulator(float base_radius,
                            float min_intersectable_radius,
                            const bool do_fast_gather,
                            const bool do_density_change)
{
  vec4 noise = no_gather_random ? vec4(0.0, 0.0, 0.0, 1.0) : texelfetch_noise_tex(gl_FragCoord.xy);

  if (!do_fast_gather) {
    /* Jitter the radius to reduce noticeable density changes. */
    base_radius += noise.x * unit_ring_radius * base_radius;
  }
  else {
    /* Jittering the radius more than we need means we are going to feather the bokeh shape half
     * a ring. So we need to compensate for fast gather that does not check CoC intersection. */
    base_radius += (0.5 - noise.x) * 1.5 * unit_ring_radius * base_radius;
  }
  /* TODO(@fclem): another seed? For now Cranly-Partterson rotation with golden ratio. */
  noise.x = fract(noise.x + 0.61803398875);

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
#ifdef DOF_BOKEH_TEXTURE
        /* Scaling to 0.25 for speed. Improves texture cache hit. */
        offset_co = texture(bokehLut, offset_co * 0.25 + 0.5).rg;
        offset_co *= bokehAnisotropy;
#endif
        vec2 sample_co = center_co + offset_co * ring_radius;
        vec2 sample_uv = sample_co * gatherOutputTexelSize * gatherInputUvCorrection;
        if (do_fast_gather) {
          pair_data[i].color = dof_load_gather_color(colorBufferBilinear, sample_uv, lod);
        }
        else {
          pair_data[i].color = dof_load_gather_color(colorBuffer, sample_uv, lod);
        }
        pair_data[i].coc = dof_load_gather_coc(cocBuffer, sample_uv, lod);
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

#ifdef DOF_FOREGROUND_PASS /* Reduce issue with closer foreground over distant foreground. */
    /* TODO(@fclem): This seems to not be completely correct as the issue remains. */
    float ring_area = (sqr(float(ring) + 0.5 + coc_radius_error) -
                       sqr(float(ring) - 0.5 + coc_radius_error)) *
                      sqr(base_radius * unit_sample_radius);
    dof_gather_ammend_weight(ring_data, ring_area);
#endif

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
#ifndef DOF_FOREGROUND_PASS /* Samples are already weighted per ring in foreground pass. */
        dof_gather_ammend_weight(accum_data, outer_rings_weight);
#endif
        /* Re-init kernel position & sampling parameters. */
        dof_gather_init(base_radius, noise, center_co, lod, isect_mul);
        density_change++;
      }
    }
  }

  {
    /* Center sample. */
    vec2 sample_uv = center_co * gatherOutputTexelSize * gatherInputUvCorrection;
    DofGatherData center_data;
    if (do_fast_gather) {
      center_data.color = dof_load_gather_color(colorBufferBilinear, sample_uv, lod);
    }
    else {
      center_data.color = dof_load_gather_color(colorBuffer, sample_uv, lod);
    }
    center_data.coc = dof_load_gather_coc(cocBuffer, sample_uv, lod);
    center_data.dist = 0.0;

    /* Slide 38. */
    float bordering_radius = (0.5 + coc_radius_error) * base_radius * unit_sample_radius;

    dof_gather_accumulate_center_sample(
        center_data, bordering_radius, do_fast_gather, is_foreground, accum_data);
  }

  int total_sample_count = dof_gather_total_sample_count_with_density_change(
      gather_ring_count, gather_ring_density, density_change);
  dof_gather_accumulate_resolve(total_sample_count, accum_data, outColor, outWeight, outOcclusion);

#if defined(DOF_DEBUG_GATHER_PERF)
  if (density_change > 0) {
    float fac = saturate(float(density_change) / float(10.0));
    outColor.rgb = avg(outColor.rgb) * neon_gradient(fac);
  }
  if (do_fast_gather) {
    outColor.rgb = avg(outColor.rgb) * vec3(0.0, 1.0, 0.0);
  }
#elif defined(DOF_DEBUG_SCATTER_PERF)
  outColor.rgb = avg(outColor.rgb) * vec3(0.0, 1.0, 0.0);
#endif

  /* Output premultiplied color so we can use bilinear sampler in resolve pass. */
  outColor *= outWeight;
}

void main()
{
  ivec2 tile_co = ivec2(gl_FragCoord.xy / float(DOF_TILE_DIVISOR / 2));
  CocTile coc_tile = dof_coc_tile_load(cocTilesFgBuffer, cocTilesBgBuffer, tile_co);
  CocTilePrediction prediction = dof_coc_tile_prediction_get(coc_tile);

#if defined(DOF_FOREGROUND_PASS)
  float base_radius = -coc_tile.fg_min_coc;
  float min_radius = -coc_tile.fg_max_coc;
  float min_intersectable_radius = -coc_tile.fg_max_intersectable_coc;
  bool can_early_out = !prediction.do_foreground;

#elif defined(DOF_HOLEFILL_PASS)
  float base_radius = -coc_tile.fg_min_coc;
  float min_radius = -coc_tile.fg_max_coc;
  float min_intersectable_radius = DOF_TILE_LARGE_COC;
  bool can_early_out = !prediction.do_holefill;

#else /* DOF_BACKGROUND_PASS */
  float base_radius = coc_tile.bg_max_coc;
  float min_radius = coc_tile.bg_min_coc;
  float min_intersectable_radius = coc_tile.bg_min_intersectable_coc;
  bool can_early_out = !prediction.do_background;
#endif

  bool do_fast_gather = dof_do_fast_gather(base_radius, min_radius, is_foreground);

  /* Gather at half resolution. Divide CoC by 2. */
  base_radius *= 0.5;
  min_intersectable_radius *= 0.5;

  bool do_density_change = dof_do_density_change(base_radius, min_intersectable_radius);

  if (can_early_out) {
    /* Early out. */
    outColor = vec4(0.0);
    outWeight = 0.0;
    outOcclusion = vec2(0.0, 0.0);
  }
  else if (do_fast_gather) {
    dof_gather_accumulator(base_radius, min_intersectable_radius, true, false);
  }
  else if (do_density_change) {
    dof_gather_accumulator(base_radius, min_intersectable_radius, false, true);
  }
  else {
    dof_gather_accumulator(base_radius, min_intersectable_radius, false, false);
  }
}
