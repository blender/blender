/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "blender/texture.h"

CCL_NAMESPACE_BEGIN

namespace {

/* Point density helpers. */

void density_texture_space_invert(float3 &loc, float3 &size)
{
  if (size.x != 0.0f) {
    size.x = 0.5f / size.x;
  }
  if (size.y != 0.0f) {
    size.y = 0.5f / size.y;
  }
  if (size.z != 0.0f) {
    size.z = 0.5f / size.z;
  }

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
    loc = zero_float3();
    size = zero_float3();
    return;
  }
  float3 min, max;
  b_point_density_node.calc_point_density_minmax(b_depsgraph, &min[0], &max[0]);
  loc = (min + max) * 0.5f;
  size = (max - min) * 0.5f;
  density_texture_space_invert(loc, size);
}

CCL_NAMESPACE_END
