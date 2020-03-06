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

#ifndef __COVERAGE_H__
#define __COVERAGE_H__

#include "util/util_map.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

struct KernelGlobals;
class RenderTile;

typedef unordered_map<float, float> CoverageMap;

class Coverage {
 public:
  Coverage(KernelGlobals *kg_, RenderTile &tile_) : kg(kg_), tile(tile_)
  {
  }
  void init_path_trace();
  void init_pixel(int x, int y);
  void finalize();

 private:
  vector<CoverageMap> coverage_object;
  vector<CoverageMap> coverage_material;
  vector<CoverageMap> coverage_asset;
  KernelGlobals *kg;
  RenderTile &tile;
  void finalize_buffer(vector<CoverageMap> &coverage, const int pass_offset);
  void flatten_buffer(vector<CoverageMap> &coverage, const int pass_offset);
  void sort_buffer(const int pass_offset);
};

CCL_NAMESPACE_END

#endif /* __COVERAGE_H__ */
