/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

ccl_device float3
svm_mapping(NodeMappingType type, float3 vector, float3 location, float3 rotation, float3 scale)
{
  Transform rotationTransform = euler_to_transform(rotation);
  switch (type) {
    case NODE_MAPPING_TYPE_POINT:
      return transform_direction(&rotationTransform, (vector * scale)) + location;
    case NODE_MAPPING_TYPE_TEXTURE:
      return safe_divide(transform_direction_transposed(&rotationTransform, (vector - location)),
                         scale);
    case NODE_MAPPING_TYPE_VECTOR:
      return transform_direction(&rotationTransform, (vector * scale));
    case NODE_MAPPING_TYPE_NORMAL:
      return safe_normalize(transform_direction(&rotationTransform, safe_divide(vector, scale)));
    default:
      return make_float3(0.0f, 0.0f, 0.0f);
  }
}

CCL_NAMESPACE_END
