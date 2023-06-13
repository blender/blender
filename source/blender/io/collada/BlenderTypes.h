/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup collada
 */

#pragma once

typedef float(Vector)[3];
typedef float(Quat)[4];
typedef float(Color)[4];
typedef float(Matrix)[4][4];
typedef double(DMatrix)[4][4];

typedef enum BC_global_forward_axis {
  BC_GLOBAL_FORWARD_X = 0,
  BC_GLOBAL_FORWARD_Y = 1,
  BC_GLOBAL_FORWARD_Z = 2,
  BC_GLOBAL_FORWARD_MINUS_X = 3,
  BC_GLOBAL_FORWARD_MINUS_Y = 4,
  BC_GLOBAL_FORWARD_MINUS_Z = 5
} BC_global_forward_axis;

typedef enum BC_global_up_axis {
  BC_GLOBAL_UP_X = 0,
  BC_GLOBAL_UP_Y = 1,
  BC_GLOBAL_UP_Z = 2,
  BC_GLOBAL_UP_MINUS_X = 3,
  BC_GLOBAL_UP_MINUS_Y = 4,
  BC_GLOBAL_UP_MINUS_Z = 5
} BC_global_up_axis;
