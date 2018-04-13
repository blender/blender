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

#ifndef __BLENDER_TEXTURE_H__
#define __BLENDER_TEXTURE_H__

#include <stdlib.h>
#include "blender/blender_sync.h"

CCL_NAMESPACE_BEGIN

void point_density_texture_space(BL::Depsgraph& b_depsgraph,
                                 BL::ShaderNodeTexPointDensity& b_point_density_node,
                                 float3& loc,
                                 float3& size);

CCL_NAMESPACE_END

#endif  /* __BLENDER_TEXTURE_H__ */
