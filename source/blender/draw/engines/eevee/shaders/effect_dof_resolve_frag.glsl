/* SPDX-FileCopyrightText: 2021-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Recombine Pass: Load separate convolution layer and composite with self slight defocus
 * convolution and in-focus fields.
 *
 * The half-resolution gather methods are fast but lack precision for small CoC areas. To fix this
 * we do a brute-force gather to have a smooth transition between in-focus and defocus regions.
 */

#pragma BLENDER_REQUIRE(common_utiltex_lib.glsl)
#pragma BLENDER_REQUIRE(effect_dof_lib.glsl)

void dof_slight_focus_gather(float radius, out vec4 out_color, out float out_weight)
{
  /* offset coord to avoid correlation with sampling pattern. */
  vec4 noise = texelfetch_noise_tex(gl_FragCoord.xy + 7.0);

  DofGatherData fg_accum = GATHER_DATA_INIT;
  DofGatherData bg_accum = GATHER_DATA_INIT;

  int i_radius = clamp(int(radius + 0.5), 0, int(layer_threshold));
  const int resolve_ring_density = DOF_SLIGHT_FOCUS_DENSITY;
  ivec2 texel = ivec2(gl_FragCoord.xy);

  bool first_ring = true;

  for (int ring = i_radius; ring > 0; ring--) {
    DofGatherData fg_ring = GATHER_DATA_INIT;
    DofGatherData bg_ring = GATHER_DATA_INIT;

    int ring_distance = ring;
    int ring_sample_count = resolve_ring_density * ring_distance;
    for (int sample_id = 0; sample_id < ring_sample_count; sample_id++) {
      int s = sample_id * (4 / resolve_ring_density) +
              int(noise.y * float((4 - resolve_ring_density) * ring_distance));

      ivec2 offset = dof_square_ring_sample_offset(ring_distance, s);
      float ring_dist = length(vec2(offset));

      DofGatherData pair_data[2];
      for (int i = 0; i < 2; i++) {
        ivec2 sample_offset = ((i == 0) ? offset : -offset);
        ivec2 sample_texel = texel + sample_offset;
        /* OPTI: could precompute the factor. */
        vec2 sample_uv = (vec2(sample_texel) + 0.5) / vec2(textureSize(fullResDepthBuffer, 0));
        float depth = textureLod(fullResDepthBuffer, sample_uv, 0.0).r;
        pair_data[i].color = safe_color(textureLod(fullResColorBuffer, sample_uv, 0.0));
        pair_data[i].coc = dof_coc_from_zdepth(depth);
        pair_data[i].dist = ring_dist;
#ifdef DOF_BOKEH_TEXTURE
        /* Contains sub-pixel distance to bokeh shape. */
        pair_data[i].dist = texelFetch(bokehLut, sample_offset + DOF_MAX_SLIGHT_FOCUS_RADIUS, 0).r;
#endif
        pair_data[i].coc = clamp(pair_data[i].coc, -bokehMaxSize, bokehMaxSize);
      }

      float bordering_radius = ring_dist + 0.5;
      const float isect_mul = 1.0;
      dof_gather_accumulate_sample_pair(
          pair_data, bordering_radius, isect_mul, first_ring, false, false, bg_ring, bg_accum);

#ifdef DOF_BOKEH_TEXTURE
      /* Swap distances in order to flip bokeh shape for foreground. */
      float tmp = pair_data[0].dist;
      pair_data[0].dist = pair_data[1].dist;
      pair_data[1].dist = tmp;
#endif
      dof_gather_accumulate_sample_pair(
          pair_data, bordering_radius, isect_mul, first_ring, false, true, fg_ring, fg_accum);
    }

    dof_gather_accumulate_sample_ring(
        bg_ring, ring_sample_count * 2, first_ring, false, false, bg_accum);
    dof_gather_accumulate_sample_ring(
        fg_ring, ring_sample_count * 2, first_ring, false, true, fg_accum);

    first_ring = false;
  }

  /* Center sample. */
  vec2 sample_uv = uvcoordsvar.xy;
  float depth = textureLod(fullResDepthBuffer, sample_uv, 0.0).r;
  DofGatherData center_data;
  center_data.color = safe_color(textureLod(fullResColorBuffer, sample_uv, 0.0));
  center_data.coc = dof_coc_from_zdepth(depth);
  center_data.coc = clamp(center_data.coc, -bokehMaxSize, bokehMaxSize);
  center_data.dist = 0.0;

  /* Slide 38. */
  float bordering_radius = 0.5;

  dof_gather_accumulate_center_sample(
      center_data, bordering_radius, i_radius, false, true, fg_accum);
  dof_gather_accumulate_center_sample(
      center_data, bordering_radius, i_radius, false, false, bg_accum);

  vec4 bg_col, fg_col;
  float bg_weight, fg_weight;
  vec2 unused_occlusion;

  int total_sample_count = dof_gather_total_sample_count(i_radius + 1, resolve_ring_density);
  dof_gather_accumulate_resolve(total_sample_count, bg_accum, bg_col, bg_weight, unused_occlusion);
  dof_gather_accumulate_resolve(total_sample_count, fg_accum, fg_col, fg_weight, unused_occlusion);

  /* Fix weighting issues on perfectly focus > slight focus transitioning areas. */
  if (abs(center_data.coc) < 0.5) {
    bg_col = center_data.color;
    bg_weight = 1.0;
  }

  /* Alpha Over */
  float alpha = 1.0 - fg_weight;
  out_weight = bg_weight * alpha + fg_weight;
  out_color = bg_col * bg_weight * alpha + fg_col * fg_weight;
}

void dof_resolve_load_layer(sampler2D color_tex,
                            sampler2D weight_tex,
                            out vec4 out_color,
                            out float out_weight)
{
  vec2 pixel_co = gl_FragCoord.xy / 2.0;
  vec2 uv = pixel_co / vec2(textureSize(color_tex, 0).xy);
  out_color = textureLod(color_tex, uv, 0.0);
  out_weight = textureLod(weight_tex, uv, 0.0).r;
}

void main(void)
{
  ivec2 tile_co = ivec2(gl_FragCoord.xy / float(DOF_TILE_DIVISOR));
  CocTile coc_tile = dof_coc_tile_load(fgTileBuffer, bgTileBuffer, tile_co);
  CocTilePrediction prediction = dof_coc_tile_prediction_get(coc_tile);

  fragColor = vec4(0.0);
  float weight = 0.0;

  vec4 layer_color;
  float layer_weight;

  if (!no_holefill_pass && prediction.do_holefill) {
    dof_resolve_load_layer(holefillColorBuffer, holefillWeightBuffer, layer_color, layer_weight);
    fragColor = layer_color * safe_rcp(layer_weight);
    weight = float(layer_weight > 0.0);
  }

  if (!no_background_pass && prediction.do_background) {
    dof_resolve_load_layer(bgColorBuffer, bgWeightBuffer, layer_color, layer_weight);
    /* Always prefer background to holefill pass. */
    layer_color *= safe_rcp(layer_weight);
    layer_weight = float(layer_weight > 0.0);
    /* Composite background. */
    fragColor = fragColor * (1.0 - layer_weight) + layer_color;
    weight = weight * (1.0 - layer_weight) + layer_weight;
    /* Fill holes with the composited background. */
    fragColor *= safe_rcp(weight);
    weight = float(weight > 0.0);
  }

  if (!no_slight_focus_pass && prediction.do_slight_focus) {
    dof_slight_focus_gather(coc_tile.fg_slight_focus_max_coc, layer_color, layer_weight);
    /* Composite slight defocus. */
    fragColor = fragColor * (1.0 - layer_weight) + layer_color;
    weight = weight * (1.0 - layer_weight) + layer_weight;
  }

  if (!no_focus_pass && prediction.do_focus) {
    layer_color = safe_color(textureLod(fullResColorBuffer, uvcoordsvar.xy, 0.0));
    layer_weight = 1.0;
    /* Composite in focus. */
    fragColor = fragColor * (1.0 - layer_weight) + layer_color;
    weight = weight * (1.0 - layer_weight) + layer_weight;
  }

  if (!no_foreground_pass && prediction.do_foreground) {
    dof_resolve_load_layer(fgColorBuffer, fgWeightBuffer, layer_color, layer_weight);
    /* Composite foreground. */
    fragColor = fragColor * (1.0 - layer_weight) + layer_color;
  }

  /* Fix float precision issue in alpha compositing. */
  if (fragColor.a > 0.99) {
    fragColor.a = 1.0;
  }

#if 0 /* Debug */
  if (coc_tile.fg_slight_focus_max_coc >= 0.5) {
    fragColor.rgb *= vec3(1.0, 0.1, 0.1);
  }
#endif
}
