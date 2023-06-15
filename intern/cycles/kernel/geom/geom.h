/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

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
