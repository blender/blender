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

#pragma once

// clang-format off
#include "kernel/geom/attribute.h"
#include "kernel/geom/object.h"
#ifdef __PATCH_EVAL__
#  include "kernel/geom/patch.h"
#endif
#include "kernel/geom/triangle.h"
#include "kernel/geom/subd_triangle.h"
#include "kernel/geom/triangle_intersect.h"
#include "kernel/geom/motion_triangle.h"
#include "kernel/geom/motion_triangle_intersect.h"
#include "kernel/geom/motion_triangle_shader.h"
#include "kernel/geom/motion_curve.h"
#include "kernel/geom/motion_point.h"
#include "kernel/geom/point.h"
#include "kernel/geom/point_intersect.h"
#include "kernel/geom/curve.h"
#include "kernel/geom/curve_intersect.h"
#include "kernel/geom/volume.h"
#include "kernel/geom/primitive.h"
#include "kernel/geom/shader_data.h"
// clang-format on
