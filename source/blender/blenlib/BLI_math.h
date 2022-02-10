/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

#pragma once

/** \file
 * \ingroup bli
 *
 * \section mathabbrev Abbreviations
 *
 * - `fl` = float
 * - `db` = double
 * - `v2` = vec2 = vector 2
 * - `v3` = vec3 = vector 3
 * - `v4` = vec4 = vector 4
 * - `vn` = vec4 = vector N dimensions, *passed as an arg, after the vector*.
 * - `qt` = quat = quaternion
 * - `dq` = dquat = dual quaternion
 * - `m2` = mat2 = matrix 2x2
 * - `m3` = mat3 = matrix 3x3
 * - `m4` = mat4 = matrix 4x4
 * - `eul` = euler rotation
 * - `eulO` = euler with order
 * - `plane` = plane 4, (vec3, distance)
 * - `plane3` = plane 3 (same as a `plane` with a zero 4th component)
 *
 * \subsection mathabbrev_all Function Type Abbreviations
 *
 * For non float versions of functions (which typically operate on floats),
 * use single suffix abbreviations.
 *
 * - `_d` = double
 * - `_i` = int
 * - `_u` = unsigned int
 * - `_char` = char
 * - `_uchar` = unsigned char
 *
 * \section mathvarnames Variable Names
 *
 * - f = single value
 * - a, b, c = vectors
 * - r = result vector
 * - A, B, C = matrices
 * - R = result matrix
 */

#include "BLI_math_base.h"
#include "BLI_math_color.h"
#include "BLI_math_geom.h"
#include "BLI_math_interp.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_solvers.h"
#include "BLI_math_statistics.h"
#include "BLI_math_time.h"
#include "BLI_math_vector.h"
