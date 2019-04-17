/*
 * Copyright 2011-2015 Blender Foundation
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

#include "blender/blender_texture.h"

CCL_NAMESPACE_BEGIN

namespace {

/* Point density helpers. */

void density_texture_space_invert(float3 &loc, float3 &size)
{
  if (size.x != 0.0f)
    size.x = 0.5f / size.x;
  if (size.y != 0.0f)
    size.y = 0.5f / size.y;
  if (size.z != 0.0f)
    size.z = 0.5f / size.z;

  loc = loc * size - make_float3(0.5f, 0.5f, 0.5f);
}

} /* namespace */

void point_density_texture_space(BL::Depsgraph &b_depsgraph,
                                 BL::ShaderNodeTexPointDensity &b_point_density_node,
                                 float3 &loc,
                                 float3 &size)
{
  BL::Object b_ob(b_point_density_node.object());
  if (!b_ob) {
    loc = make_float3(0.0f, 0.0f, 0.0f);
    size = make_float3(0.0f, 0.0f, 0.0f);
    return;
  }
  float3 min, max;
  b_point_density_node.calc_point_density_minmax(b_depsgraph, &min[0], &max[0]);
  loc = (min + max) * 0.5f;
  size = (max - min) * 0.5f;
  density_texture_space_invert(loc, size);
}

CCL_NAMESPACE_END
