/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_defines.hh"
#include "gpu_shader_compat.hh"

/**
 * Gather Filter pass: Filter the gather pass result to reduce noise.
 *
 * This is a simple 3x3 median filter to avoid dilating highlights with a 3x3 max filter even if
 * cheaper.
 */
namespace eevee::dof::filter_pass {

struct FilterSample {
  float4 color;
  float weight;
};

struct Resources {
  [[sampler(0)]] sampler2D color_tx;
  [[sampler(1)]] sampler2D weight_tx;

  [[image(0, write, SFLOAT_16_16_16_16)]] image2D out_color_img;
  [[image(1, write, SFLOAT_16)]] image2D out_weight_img;

  /* -------------------------------------------------------------------- */
  /** \name Pixel cache.
   * \{ */

  [[shared]] float4 color_cache[DOF_FILTER_GROUP_SIZE + 2][DOF_FILTER_GROUP_SIZE + 2];
  [[shared]] float weight_cache[DOF_FILTER_GROUP_SIZE + 2][DOF_FILTER_GROUP_SIZE + 2];

  void init(uint3 global_id, uint3 local_id)
  {
    /**
     * Load enough values into LDS to perform the filter.
     *
     * ┌──────────────────────────────┐
     * │                              │  < Border texels that needs to be loaded.
     * │    x  x  x  x  x  x  x  x    │  ─┐
     * │    x  x  x  x  x  x  x  x    │   │
     * │    x  x  x  x  x  x  x  x    │   │
     * │    x  x  x  x  x  x  x  x    │   │ Thread Group Size 8x8.
     * │ L  L  L  L  L  x  x  x  x    │   │
     * │ L  L  L  L  L  x  x  x  x    │   │
     * │ L  L  L  L  L  x  x  x  x    │   │
     * │ L  L  L  L  L  x  x  x  x    │  ─┘
     * │ L  L  L  L  L                │  < Border texels that needs to be loaded.
     * └──────────────────────────────┘
     *   └───────────┘
     *    Load using 5x5 threads.
     */

    constexpr uint cache_size = uint(DOF_FILTER_GROUP_SIZE + 2);

    int2 texel = int2(global_id.xy) - 1;
    if (all(lessThan(local_id.xy, uint2(cache_size / 2u)))) {
      for (int y = 0; y < 2; y++) {
        for (int x = 0; x < 2; x++) {
          int2 offset = int2(x, y) * int2(cache_size / 2u);
          int2 cache_texel = int2(local_id.xy) + offset;
          int2 load_texel = clamp(texel + offset, int2(0), textureSize(color_tx, 0) - 1);

          color_cache[cache_texel.y][cache_texel.x] = texelFetch(color_tx, load_texel, 0);
          weight_cache[cache_texel.y][cache_texel.x] = texelFetch(weight_tx, load_texel, 0).r;
        }
      }
    }
    barrier();
  }

  FilterSample fetch_sample(int x, int y) const
  {
    return {color_cache[y][x], weight_cache[y][x]};
  }

  /** \} */
};

/* -------------------------------------------------------------------- */
/** \name Median filter
 * From:
 * Implementing Median Filters in XC4000E FPGAs
 * JOHN L. SMITH, Univision Technologies Inc., Billerica, MA
 * http://users.utcluj.ro/~baruch/resources/Image/xl23_16.pdf
 * Figure 1
 * \{ */

FilterSample filter_min(FilterSample a, FilterSample b)
{
  return {min(a.color, b.color), min(a.weight, b.weight)};
}

FilterSample filter_max(FilterSample a, FilterSample b)
{
  return {max(a.color, b.color), max(a.weight, b.weight)};
}

FilterSample filter_min(FilterSample a, FilterSample b, FilterSample c)
{
  return {min(a.color, min(c.color, b.color)), min(a.weight, min(c.weight, b.weight))};
}

FilterSample filter_max(FilterSample a, FilterSample b, FilterSample c)
{
  return {max(a.color, max(c.color, b.color)), max(a.weight, max(c.weight, b.weight))};
}

FilterSample filter_median(FilterSample s1, FilterSample s2, FilterSample s3)
{
  /* From diagram, with nodes numbered from top to bottom. */
  FilterSample l1 = filter_min(s2, s3);
  FilterSample h1 = filter_max(s2, s3);
  FilterSample h2 = filter_max(s1, l1);
  FilterSample l3 = filter_min(h2, h1);
  return l3;
}

struct FilterLmhResult {
  FilterSample low;
  FilterSample median;
  FilterSample high;
};

FilterLmhResult filter_lmh(FilterSample s1, FilterSample s2, FilterSample s3)
{
  /* From diagram, with nodes numbered from top to bottom. */
  FilterSample h1 = filter_max(s2, s3);
  FilterSample l1 = filter_min(s2, s3);

  FilterSample h2 = filter_max(s1, l1);
  FilterSample l2 = filter_min(s1, l1);

  FilterSample h3 = filter_max(h2, h1);
  FilterSample l3 = filter_min(h2, h1);

  FilterLmhResult result;
  result.low = l2;
  result.median = l3;
  result.high = h3;

  return result;
}

/** \} */

[[compute, local_size(DOF_FILTER_GROUP_SIZE, DOF_FILTER_GROUP_SIZE)]]
void comp_main([[resource_table]] Resources &srt,
               [[global_invocation_id]] const uint3 global_id,
               [[local_invocation_id]] const uint3 local_id)
{
  /**
   * NOTE: We can **NOT** optimize by discarding some tiles as the result is sampled using bilinear
   * filtering in the resolve pass. Not outputting to a tile means that border texels have
   * undefined value and tile border will be noticeable in the final image.
   */

  srt.init(global_id, local_id);

  int2 texel = int2(local_id.xy);

  FilterLmhResult rows[3];
  for (int y = 0; y < 3; y++) [[unroll]] {
    rows[y] = filter_lmh(srt.fetch_sample(texel[0] + 0, texel[1] + y),
                         srt.fetch_sample(texel[0] + 1, texel[1] + y),
                         srt.fetch_sample(texel[0] + 2, texel[1] + y));
  }
  /* Left nodes. */
  FilterSample high = filter_max(rows[0].low, rows[1].low, rows[2].low);
  /* Right nodes. */
  FilterSample low = filter_min(rows[0].high, rows[1].high, rows[2].high);
  /* Center nodes. */
  FilterSample median = filter_median(rows[0].median, rows[1].median, rows[2].median);
  /* Last bottom nodes. */
  median = filter_median(low, median, high);

  int2 out_texel = int2(global_id.xy);
  imageStore(srt.out_color_img, out_texel, median.color);
  imageStore(srt.out_weight_img, out_texel, float4(median.weight));
}

}  // namespace eevee::dof::filter_pass

PipelineCompute eevee_depth_of_field_filter(eevee::dof::filter_pass::comp_main);
