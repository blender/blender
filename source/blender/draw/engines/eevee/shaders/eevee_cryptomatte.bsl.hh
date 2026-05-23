/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** Storing/merging and sorting cryptomatte samples. */

#pragma once

#include "eevee_film_shared.hh"

namespace eevee::cryptomatte {

float4 false_color(float hash)
{
  uint m3hash = floatBitsToUint(hash);
  return float4(hash,
                float(m3hash << 8) / float(0xFFFFFFFFu),
                float(m3hash << 16) / float(0xFFFFFFFFu),
                1.0f);
}

}  // namespace eevee::cryptomatte

namespace eevee {

struct Cryptomatte {
  [[image(7, read_write, SFLOAT_32_32_32_32)]] image2DArray cryptomatte_img;

  void clear_samples(FilmSample dst)
  {
    int layer_len = imageSize(cryptomatte_img).z;
    for (int i = 0; i < layer_len; i++) {
      imageStoreFast(cryptomatte_img, int3(dst.texel, i), float4(0.0f));
      /* Ensure stores are visible to later reads. */
      imageFence(cryptomatte_img);
    }
  }

  void store_film_sample(FilmSample dst,
                         int cryptomatte_layer_id,
                         int cryptomatte_samples_len,
                         float2 crypto_sample,
                         float4 &out_color)
  {
    if (crypto_sample.y == 0.0f) {
      return;
    }

    for (int i = 0; i < cryptomatte_samples_len / 2; i++) {
      int3 img_co = int3(dst.texel, cryptomatte_layer_id + i);
      float4 sample_pair = imageLoad(cryptomatte_img, img_co);
      if (can_merge_sample(sample_pair.xy, crypto_sample)) {
        sample_pair.xy = merge_sample(dst, sample_pair.xy, crypto_sample);
        /* In viewport only one layer is active. */
        /* TODO(jbakker):  we are displaying the first sample, but we should display the highest
         * weighted one. */
        if (cryptomatte_layer_id + i == 0) {
          out_color = cryptomatte::false_color(sample_pair.x);
        }
      }
      else if (can_merge_sample(sample_pair.zw, crypto_sample)) {
        sample_pair.zw = merge_sample(dst, sample_pair.zw, crypto_sample);
      }
      else if (i == ((cryptomatte_samples_len / 2) - 1)) {
        /* TODO(jbakker): New hash detected, but there is no space left to store it. Currently we
         * will ignore this sample, but ideally we could replace a sample with a lowest weight. */
        continue;
      }
      else {
        continue;
      }
      imageStoreFast(cryptomatte_img, img_co, sample_pair);
      break;
    }
    /* Ensure stores are visible to later reads. */
    imageFence(cryptomatte_img);
  }

 private:
  bool can_merge_sample(float2 dst, float2 src)
  {
    if (all(equal(dst, float2(0.0f, 0.0f)))) {
      return true;
    }
    if (dst.x == src.x) {
      return true;
    }
    return false;
  }

  float2 merge_sample(FilmSample film_sample, float2 dst, float2 src)
  {
    return float2(src.x, (dst.y * film_sample.weight + src.y) * film_sample.weight_sum_inv);
  }
};

}  // namespace eevee
