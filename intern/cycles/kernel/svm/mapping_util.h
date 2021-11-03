/*
 * Copyright 2011-2014 Blender Foundation
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
      return safe_divide_float3_float3(
          transform_direction_transposed(&rotationTransform, (vector - location)), scale);
    case NODE_MAPPING_TYPE_VECTOR:
      return transform_direction(&rotationTransform, (vector * scale));
    case NODE_MAPPING_TYPE_NORMAL:
      return safe_normalize(
          transform_direction(&rotationTransform, safe_divide_float3_float3(vector, scale)));
    default:
      return make_float3(0.0f, 0.0f, 0.0f);
  }
}

CCL_NAMESPACE_END
