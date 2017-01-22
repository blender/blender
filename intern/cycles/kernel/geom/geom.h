/*
 * Copyright 2011-2013 Blender Foundation
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

#include "geom_attribute.h"
#include "geom_object.h"
#ifdef __PATCH_EVAL__
#  include "geom_patch.h"
#endif
#include "geom_triangle.h"
#include "geom_subd_triangle.h"
#include "geom_triangle_intersect.h"
#include "geom_motion_triangle.h"
#include "geom_motion_triangle_intersect.h"
#include "geom_motion_triangle_shader.h"
#include "geom_motion_curve.h"
#include "geom_curve.h"
#include "geom_volume.h"
#include "geom_primitive.h"

