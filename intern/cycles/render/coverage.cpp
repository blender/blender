/*
 * Copyright 2018 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "render/coverage.h"
#include "render/buffers.h"

#include "kernel/kernel_compat_cpu.h"
#include "kernel/kernel_types.h"
#include "kernel/split/kernel_split_data.h"

#include "kernel/kernel_globals.h"
#include "kernel/kernel_id_passes.h"

#include "util/util_map.h"

CCL_NAMESPACE_BEGIN

static bool crypomatte_comp(const pair<float, float> &i, const pair<float, float> j)
{
  return i.first > j.first;
}

void Coverage::finalize()
{
  int pass_offset = 0;
  if (kernel_data.film.cryptomatte_passes & CRYPT_OBJECT) {
    finalize_buffer(coverage_object, pass_offset);
    pass_offset += kernel_data.film.cryptomatte_depth * 4;
  }
  if (kernel_data.film.cryptomatte_passes & CRYPT_MATERIAL) {
    finalize_buffer(coverage_material, pass_offset);
    pass_offset += kernel_data.film.cryptomatte_depth * 4;
  }
  if (kernel_data.film.cryptomatte_passes & CRYPT_ASSET) {
    finalize_buffer(coverage_asset, pass_offset);
  }
}

void Coverage::init_path_trace()
{
  kg->coverage_object = kg->coverage_material = kg->coverage_asset = NULL;

  if (kernel_data.film.cryptomatte_passes & CRYPT_ACCURATE) {
    if (kernel_data.film.cryptomatte_passes & CRYPT_OBJECT) {
      coverage_object.clear();
      coverage_object.resize(tile.w * tile.h);
    }
    if (kernel_data.film.cryptomatte_passes & CRYPT_MATERIAL) {
      coverage_material.clear();
      coverage_material.resize(tile.w * tile.h);
    }
    if (kernel_data.film.cryptomatte_passes & CRYPT_ASSET) {
      coverage_asset.clear();
      coverage_asset.resize(tile.w * tile.h);
    }
  }
}

void Coverage::init_pixel(int x, int y)
{
  if (kernel_data.film.cryptomatte_passes & CRYPT_ACCURATE) {
    const int pixel_index = tile.w * (y - tile.y) + x - tile.x;
    if (kernel_data.film.cryptomatte_passes & CRYPT_OBJECT) {
      kg->coverage_object = &coverage_object[pixel_index];
    }
    if (kernel_data.film.cryptomatte_passes & CRYPT_MATERIAL) {
      kg->coverage_material = &coverage_material[pixel_index];
    }
    if (kernel_data.film.cryptomatte_passes & CRYPT_ASSET) {
      kg->coverage_asset = &coverage_asset[pixel_index];
    }
  }
}

void Coverage::finalize_buffer(vector<CoverageMap> &coverage, const int pass_offset)
{
  if (kernel_data.film.cryptomatte_passes & CRYPT_ACCURATE) {
    flatten_buffer(coverage, pass_offset);
  }
  else {
    sort_buffer(pass_offset);
  }
}

void Coverage::flatten_buffer(vector<CoverageMap> &coverage, const int pass_offset)
{
  /* Sort the coverage map and write it to the output */
  int pixel_index = 0;
  int pass_stride = tile.buffers->params.get_passes_size();
  for (int y = 0; y < tile.h; ++y) {
    for (int x = 0; x < tile.w; ++x) {
      const CoverageMap &pixel = coverage[pixel_index];
      if (!pixel.empty()) {
        /* buffer offset */
        int index = x + y * tile.stride;
        float *buffer = (float *)tile.buffer + index * pass_stride;

        /* sort the cryptomatte pixel */
        vector<pair<float, float>> sorted_pixel;
        for (CoverageMap::const_iterator it = pixel.begin(); it != pixel.end(); ++it) {
          sorted_pixel.push_back(std::make_pair(it->second, it->first));
        }
        sort(sorted_pixel.begin(), sorted_pixel.end(), crypomatte_comp);
        int num_slots = 2 * (kernel_data.film.cryptomatte_depth);
        if (sorted_pixel.size() > num_slots) {
          float leftover = 0.0f;
          for (vector<pair<float, float>>::iterator it = sorted_pixel.begin() + num_slots;
               it != sorted_pixel.end();
               ++it) {
            leftover += it->first;
          }
          sorted_pixel[num_slots - 1].first += leftover;
        }
        int limit = min(num_slots, sorted_pixel.size());
        for (int i = 0; i < limit; ++i) {
          kernel_write_id_slots(buffer + kernel_data.film.pass_cryptomatte + pass_offset,
                                2 * (kernel_data.film.cryptomatte_depth),
                                sorted_pixel[i].second,
                                sorted_pixel[i].first);
        }
      }
      ++pixel_index;
    }
  }
}

void Coverage::sort_buffer(const int pass_offset)
{
  /* Sort the coverage map and write it to the output */
  int pass_stride = tile.buffers->params.get_passes_size();
  for (int y = 0; y < tile.h; ++y) {
    for (int x = 0; x < tile.w; ++x) {
      /* buffer offset */
      int index = x + y * tile.stride;
      float *buffer = (float *)tile.buffer + index * pass_stride;
      kernel_sort_id_slots(buffer + kernel_data.film.pass_cryptomatte + pass_offset,
                           2 * (kernel_data.film.cryptomatte_depth));
    }
  }
}

CCL_NAMESPACE_END
