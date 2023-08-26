/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Reduce copy pass: filter fireflies and split color between scatter and gather input.
 *
 * NOTE: The texture can end up being too big because of the mipmap padding. We correct for
 * that during the convolution phase.
 *
 * Inputs:
 * - Output of setup pass (halfres) and reduce downsample pass (quarter res).
 * Outputs:
 * - Halfres padded to avoid mipmap misalignment (so possibly not matching input size).
 * - Gather input color (whole mip chain), Scatter rect list, Signed CoC (whole mip chain).
 */

#pragma BLENDER_REQUIRE(eevee_depth_of_field_lib.glsl)

/* NOTE: Do not compare alpha as it is not scattered by the scatter pass. */
float dof_scatter_neighborhood_rejection(vec3 color)
{
  color = min(vec3(dof_buf.scatter_neighbor_max_color), color);

  float validity = 0.0;

  /* Centered in the middle of 4 quarter res texel. */
  vec2 texel_size = 1.0 / vec2(textureSize(downsample_tx, 0).xy);
  vec2 uv = ((vec2(gl_GlobalInvocationID.xy) + 0.5) * 0.5) * texel_size;

  vec3 max_diff = vec3(0.0);
  for (int i = 0; i < 4; i++) {
    vec2 sample_uv = uv + quad_offsets[i] * texel_size;
    vec3 ref = textureLod(downsample_tx, sample_uv, 0.0).rgb;

    ref = min(vec3(dof_buf.scatter_neighbor_max_color), ref);
    float diff = max_v3(max(vec3(0.0), abs(ref - color)));

    const float rejection_threshold = 0.7;
    diff = saturate(diff / rejection_threshold - 1.0);
    validity = max(validity, diff);
  }

  return validity;
}

/* This avoids Bokeh sprite popping in and out at the screen border and
 * drawing Bokeh sprites larger than the screen. */
float dof_scatter_screen_border_rejection(float coc, ivec2 texel)
{
  vec2 screen_size = vec2(imageSize(inout_color_lod0_img));
  vec2 uv = (vec2(texel) + 0.5) / screen_size;
  vec2 screen_pos = uv * screen_size;
  float min_screen_border_distance = min_v2(min(screen_pos, screen_size - screen_pos));
  /* Fullres to halfres CoC. */
  coc *= 0.5;
  /* Allow 10px transition. */
  const float rejection_hardeness = 1.0 / 10.0;
  return saturate((min_screen_border_distance - abs(coc)) * rejection_hardeness + 1.0);
}

float dof_scatter_luminosity_rejection(vec3 color)
{
  const float rejection_hardness = 1.0;
  return saturate(max_v3(color - dof_buf.scatter_color_threshold) * rejection_hardness);
}

float dof_scatter_coc_radius_rejection(float coc)
{
  const float rejection_hardness = 0.3;
  return saturate((abs(coc) - dof_buf.scatter_coc_threshold) * rejection_hardness);
}

float fast_luma(vec3 color)
{
  return (2.0 * color.g) + color.r + color.b;
}

#define cache_size (gl_WorkGroupSize.x)
shared vec4 color_cache[cache_size][cache_size];
shared float coc_cache[cache_size][cache_size];
shared float do_scatter[cache_size][cache_size];

void main()
{
  ivec2 texel = min(ivec2(gl_GlobalInvocationID.xy), imageSize(inout_color_lod0_img) - 1);
  uvec2 texel_local = gl_LocalInvocationID.xy;
  /* Increase readability. */
#define LOCAL_INDEX texel_local.y][texel_local.x
#define LOCAL_OFFSET(x_, y_) texel_local.y + (y_)][texel_local.x + (x_)

  /* Load level 0 into cache. */
  color_cache[LOCAL_INDEX] = imageLoad(inout_color_lod0_img, texel);
  coc_cache[LOCAL_INDEX] = imageLoad(in_coc_lod0_img, texel).r;

  /* Only scatter if luminous enough. */
  do_scatter[LOCAL_INDEX] = dof_scatter_luminosity_rejection(color_cache[LOCAL_INDEX].rgb);
  /* Only scatter if CoC is big enough. */
  do_scatter[LOCAL_INDEX] *= dof_scatter_coc_radius_rejection(coc_cache[LOCAL_INDEX]);
  /* Only scatter if CoC is not too big to avoid performance issues. */
  do_scatter[LOCAL_INDEX] *= dof_scatter_screen_border_rejection(coc_cache[LOCAL_INDEX], texel);
  /* Only scatter if neighborhood is different enough. */
  do_scatter[LOCAL_INDEX] *= dof_scatter_neighborhood_rejection(color_cache[LOCAL_INDEX].rgb);
  /* For debugging. */
  if (no_scatter_pass) {
    do_scatter[LOCAL_INDEX] = 0.0;
  }

  barrier();

  /* Add a scatter sprite for each 2x2 pixel neighborhood passing the threshold. */
  if (all(equal(texel_local & 1u, uvec2(0)))) {
    vec4 do_scatter4;
    /* Follows quad_offsets order. */
    do_scatter4.x = do_scatter[LOCAL_OFFSET(0, 1)];
    do_scatter4.y = do_scatter[LOCAL_OFFSET(1, 1)];
    do_scatter4.z = do_scatter[LOCAL_OFFSET(1, 0)];
    do_scatter4.w = do_scatter[LOCAL_OFFSET(0, 0)];
    if (any(greaterThan(do_scatter4, vec4(0.0)))) {
      /* Apply energy conservation to anamorphic scattered bokeh. */
      do_scatter4 *= max_v2(dof_buf.bokeh_anisotropic_scale_inv);

      /* Circle of Confusion. */
      vec4 coc4;
      coc4.x = coc_cache[LOCAL_OFFSET(0, 1)];
      coc4.y = coc_cache[LOCAL_OFFSET(1, 1)];
      coc4.z = coc_cache[LOCAL_OFFSET(1, 0)];
      coc4.w = coc_cache[LOCAL_OFFSET(0, 0)];
      /* We are scattering at half resolution, so divide CoC by 2. */
      coc4 *= 0.5;
      /* Sprite center position. Center sprite around the 4 texture taps. */
      vec2 offset = vec2(gl_GlobalInvocationID.xy) + 1;
      /* Add 2.5 to max_coc because the max_coc may not be centered on the sprite origin
       * and because we smooth the bokeh shape a bit in the pixel shader. */
      vec2 half_extent = max_v4(abs(coc4)) * dof_buf.bokeh_anisotropic_scale + 2.5;
      /* Issue a sprite for each field if any CoC matches. */
      if (any(lessThan(do_scatter4 * sign(coc4), vec4(0.0)))) {
        /* Same value for all threads. Not an issue if we don't sync access to it. */
        scatter_fg_indirect_buf.vertex_len = 4u;
        /* Issue 1 strip instance per sprite. */
        uint rect_id = atomicAdd(scatter_fg_indirect_buf.instance_len, 1u);
        if (rect_id < dof_buf.scatter_max_rect) {

          vec4 coc4_fg = max(vec4(0.0), -coc4);
          vec4 fg_weights = dof_layer_weight(coc4_fg) * dof_sample_weight(coc4_fg) * do_scatter4;
          /* Filter NaNs. */
          fg_weights = select(fg_weights, vec4(0.0), equal(coc4_fg, vec4(0.0)));

          ScatterRect rect_fg;
          rect_fg.offset = offset;
          /* Negate extent to flip the sprite. Mimics optical phenomenon. */
          rect_fg.half_extent = -half_extent;
          /* NOTE: Since we flipped the quad along (1,-1) line, we need to also swap the (1,1) and
           * (0,0) values so that quad_offsets is in the right order in the vertex shader. */

          /* Circle of Confusion absolute radius in halfres pixels. */
          rect_fg.color_and_coc[0].a = coc4_fg[0];
          rect_fg.color_and_coc[1].a = coc4_fg[3];
          rect_fg.color_and_coc[2].a = coc4_fg[2];
          rect_fg.color_and_coc[3].a = coc4_fg[1];
          /* Apply weights. */
          rect_fg.color_and_coc[0].rgb = color_cache[LOCAL_OFFSET(0, 1)].rgb * fg_weights[0];
          rect_fg.color_and_coc[1].rgb = color_cache[LOCAL_OFFSET(0, 0)].rgb * fg_weights[3];
          rect_fg.color_and_coc[2].rgb = color_cache[LOCAL_OFFSET(1, 0)].rgb * fg_weights[2];
          rect_fg.color_and_coc[3].rgb = color_cache[LOCAL_OFFSET(1, 1)].rgb * fg_weights[1];

          scatter_fg_list_buf[rect_id] = rect_fg;
        }
      }
      if (any(greaterThan(do_scatter4 * sign(coc4), vec4(0.0)))) {
        /* Same value for all threads. Not an issue if we don't sync access to it. */
        scatter_bg_indirect_buf.vertex_len = 4u;
        /* Issue 1 strip instance per sprite. */
        uint rect_id = atomicAdd(scatter_bg_indirect_buf.instance_len, 1u);
        if (rect_id < dof_buf.scatter_max_rect) {
          vec4 coc4_bg = max(vec4(0.0), coc4);
          vec4 bg_weights = dof_layer_weight(coc4_bg) * dof_sample_weight(coc4_bg) * do_scatter4;
          /* Filter NaNs. */
          bg_weights = select(bg_weights, vec4(0.0), equal(coc4_bg, vec4(0.0)));

          ScatterRect rect_bg;
          rect_bg.offset = offset;
          rect_bg.half_extent = half_extent;

          /* Circle of Confusion absolute radius in halfres pixels. */
          rect_bg.color_and_coc[0].a = coc4_bg[0];
          rect_bg.color_and_coc[1].a = coc4_bg[1];
          rect_bg.color_and_coc[2].a = coc4_bg[2];
          rect_bg.color_and_coc[3].a = coc4_bg[3];
          /* Apply weights. */
          rect_bg.color_and_coc[0].rgb = color_cache[LOCAL_OFFSET(0, 1)].rgb * bg_weights[0];
          rect_bg.color_and_coc[1].rgb = color_cache[LOCAL_OFFSET(1, 1)].rgb * bg_weights[1];
          rect_bg.color_and_coc[2].rgb = color_cache[LOCAL_OFFSET(1, 0)].rgb * bg_weights[2];
          rect_bg.color_and_coc[3].rgb = color_cache[LOCAL_OFFSET(0, 0)].rgb * bg_weights[3];

          scatter_bg_list_buf[rect_id] = rect_bg;
        }
      }
    }
  }

  /* Remove scatter color from gather. */
  color_cache[LOCAL_INDEX].rgb *= 1.0 - do_scatter[LOCAL_INDEX];
  imageStore(inout_color_lod0_img, texel, color_cache[LOCAL_INDEX]);

  /* Recursive downsample. */
  for (uint i = 1u; i < DOF_MIP_COUNT; i++) {
    barrier();
    uint mask = ~(~0u << i);
    if (all(equal(gl_LocalInvocationID.xy & mask, uvec2(0)))) {
      uint ofs = 1u << (i - 1u);

      /* TODO(fclem): Could use wave shuffle intrinsics to avoid LDS as suggested by the paper. */
      vec4 coc4;
      coc4.x = coc_cache[LOCAL_OFFSET(0, ofs)];
      coc4.y = coc_cache[LOCAL_OFFSET(ofs, ofs)];
      coc4.z = coc_cache[LOCAL_OFFSET(ofs, 0)];
      coc4.w = coc_cache[LOCAL_OFFSET(0, 0)];

      vec4 colors[4];
      colors[0] = color_cache[LOCAL_OFFSET(0, ofs)];
      colors[1] = color_cache[LOCAL_OFFSET(ofs, ofs)];
      colors[2] = color_cache[LOCAL_OFFSET(ofs, 0)];
      colors[3] = color_cache[LOCAL_OFFSET(0, 0)];

      vec4 weights = dof_bilateral_coc_weights(coc4);
      weights *= dof_bilateral_color_weights(colors);
      /* Normalize so that the sum is 1. */
      weights *= safe_rcp(sum(weights));

      color_cache[LOCAL_INDEX] = weighted_sum_array(colors, weights);
      coc_cache[LOCAL_INDEX] = dot(coc4, weights);

      ivec2 texel = ivec2(gl_GlobalInvocationID.xy >> i);

      if (i == 1) {
        imageStore(out_color_lod1_img, texel, color_cache[LOCAL_INDEX]);
        imageStore(out_coc_lod1_img, texel, vec4(coc_cache[LOCAL_INDEX]));
      }
      else if (i == 2) {
        imageStore(out_color_lod2_img, texel, color_cache[LOCAL_INDEX]);
        imageStore(out_coc_lod2_img, texel, vec4(coc_cache[LOCAL_INDEX]));
      }
      else /* if (i == 3) */ {
        imageStore(out_color_lod3_img, texel, color_cache[LOCAL_INDEX]);
        imageStore(out_coc_lod3_img, texel, vec4(coc_cache[LOCAL_INDEX]));
      }
    }
  }
}
