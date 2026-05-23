/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_common_infos.hh"

SHADER_LIBRARY_CREATE_INFO(eevee_global_ubo)

#include "gpu_shader_math_base_lib.glsl"

namespace eevee::cryptomatte {

#define CRYPTOMATTE_LEVELS_MAX 16

struct Resources {
  [[legacy_info]] ShaderCreateInfo eevee_global_ubo;

  [[push_constant]] const int cryptomatte_layer_len;
  [[push_constant]] const int cryptomatte_samples_per_layer;

  [[image(0, read_write, SFLOAT_32_32_32_32)]] image2DArray cryptomatte_img;

  void load_samples(int2 texel, int layer, float2 (&samples)[CRYPTOMATTE_LEVELS_MAX])
  {
    int pass_len = divide_ceil(cryptomatte_samples_per_layer, 2);
    int layer_id = layer * pass_len;

    /* Read all samples from the cryptomatte layer. */
    for (int p = 0; p < pass_len; p++) {
      float4 pass_sample = imageLoadFast(cryptomatte_img, int3(texel, p + layer_id));
      samples[p * 2] = pass_sample.xy;
      samples[p * 2 + 1] = pass_sample.zw;
    }
    for (int i = pass_len * 2; i < CRYPTOMATTE_LEVELS_MAX; i++) {
      samples[i] = float2(0.0f);
    }
  }

  void sort_samples(float2 (&samples)[CRYPTOMATTE_LEVELS_MAX])
  {
    /* Sort samples. Lame implementation, can be replaced with a more efficient algorithm. */
    for (int i = 0; i < cryptomatte_samples_per_layer - 1 && samples[i].y != 0.0f; i++) {
      int highest_index = i;
      float highest_weight = samples[i].y;
      for (int j = i + 1; j < cryptomatte_samples_per_layer && samples[j].y != 0.0f; j++) {
        if (samples[j].y > highest_weight) {
          highest_index = j;
          highest_weight = samples[j].y;
        }
      };

      if (highest_index != i) {
        float2 tmp = samples[i];
        samples[i] = samples[highest_index];
        samples[highest_index] = tmp;
      }
    }
  }

  void store_samples(int2 texel, int layer, float2 samples[CRYPTOMATTE_LEVELS_MAX])
  {
    int pass_len = divide_ceil(cryptomatte_samples_per_layer, 2);
    int layer_id = layer * pass_len;

    /* Store samples back to the cryptomatte layer. */
    for (int p = 0; p < pass_len; p++) {
      float4 pass_sample;
      pass_sample.xy = samples[p * 2];
      pass_sample.zw = samples[p * 2 + 1];
      imageStoreFast(cryptomatte_img, int3(texel, p + layer_id), pass_sample);
    }
    /* Ensure stores are visible to later reads. */
    imageFence(cryptomatte_img);
  }
};

[[compute, local_size(FILM_GROUP_SIZE, FILM_GROUP_SIZE)]]
void sort_samples([[resource_table]] Resources &srt,
                  [[global_invocation_id]] const uint3 global_id,
                  [[local_invocation_id]] const uint3 local_id,
                  [[local_invocation_index]] const uint local_index)
{
  int2 texel = int2(global_id.xy);

  if (any(greaterThanEqual(texel, uniform_buf.film.extent))) {
    return;
  }

  for (int layer = 0; layer < srt.cryptomatte_layer_len; layer++) {
    float2 samples[CRYPTOMATTE_LEVELS_MAX];
    srt.load_samples(texel, layer, samples);
    srt.sort_samples(samples);
    srt.store_samples(texel, layer, samples);
  }
}

}  // namespace eevee::cryptomatte

PipelineCompute eevee_film_cryptomatte_post(eevee::cryptomatte::sort_samples);
