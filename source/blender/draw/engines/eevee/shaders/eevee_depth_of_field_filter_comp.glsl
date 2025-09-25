/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Gather Filter pass: Filter the gather pass result to reduce noise.
 *
 * This is a simple 3x3 median filter to avoid dilating highlights with a 3x3 max filter even if
 * cheaper.
 */

#include "infos/eevee_depth_of_field_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_depth_of_field_filter)

struct FilterSample {
  float4 color;
  float weight;

  METAL_CONSTRUCTOR_2(FilterSample, float4, color, float, weight)
};

/* -------------------------------------------------------------------- */
/** \name Pixel cache.
 * \{ */
#define cache_size (gl_WorkGroupSize.x + 2)
shared float4 color_cache[cache_size][cache_size];
shared float weight_cache[cache_size][cache_size];

void cache_init()
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

  int2 texel = int2(gl_GlobalInvocationID.xy) - 1;
  if (all(lessThan(gl_LocalInvocationID.xy, uint2(cache_size / 2u)))) {
    for (int y = 0; y < 2; y++) {
      for (int x = 0; x < 2; x++) {
        int2 offset = int2(x, y) * int2(cache_size / 2u);
        int2 cache_texel = int2(gl_LocalInvocationID.xy) + offset;
        int2 load_texel = clamp(texel + offset, int2(0), textureSize(color_tx, 0) - 1);

        color_cache[cache_texel.y][cache_texel.x] = texelFetch(color_tx, load_texel, 0);
        weight_cache[cache_texel.y][cache_texel.x] = texelFetch(weight_tx, load_texel, 0).r;
      }
    }
  }
  barrier();
}

FilterSample cache_sample(int x, int y)
{
  return FilterSample(color_cache[y][x], weight_cache[y][x]);
}

/** \} */

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
  return FilterSample(min(a.color, b.color), min(a.weight, b.weight));
}

FilterSample filter_max(FilterSample a, FilterSample b)
{
  return FilterSample(max(a.color, b.color), max(a.weight, b.weight));
}

FilterSample filter_min(FilterSample a, FilterSample b, FilterSample c)
{
  return FilterSample(min(a.color, min(c.color, b.color)), min(a.weight, min(c.weight, b.weight)));
}

FilterSample filter_max(FilterSample a, FilterSample b, FilterSample c)
{
  return FilterSample(max(a.color, max(c.color, b.color)), max(a.weight, max(c.weight, b.weight)));
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

void main()
{
  /**
   * NOTE: We can **NOT** optimize by discarding some tiles as the result is sampled using bilinear
   * filtering in the resolve pass. Not outputting to a tile means that border texels have
   * undefined value and tile border will be noticeable in the final image.
   */

  cache_init();

  int2 texel = int2(gl_LocalInvocationID.xy);

  FilterLmhResult rows[3];
  for (int y = 0; y < 3; y++) {
    rows[y] = filter_lmh(cache_sample(texel.x + 0, texel.y + y),
                         cache_sample(texel.x + 1, texel.y + y),
                         cache_sample(texel.x + 2, texel.y + y));
  }
  /* Left nodes. */
  FilterSample high = filter_max(rows[0].low, rows[1].low, rows[2].low);
  /* Right nodes. */
  FilterSample low = filter_min(rows[0].high, rows[1].high, rows[2].high);
  /* Center nodes. */
  FilterSample median = filter_median(rows[0].median, rows[1].median, rows[2].median);
  /* Last bottom nodes. */
  median = filter_median(low, median, high);

  int2 out_texel = int2(gl_GlobalInvocationID.xy);
  imageStore(out_color_img, out_texel, median.color);
  imageStore(out_weight_img, out_texel, float4(median.weight));
}
