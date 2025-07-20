/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup collada
 */

#pragma once

using Vector = float[3];
using Quat = float[4];
using Color = float[4];
using Matrix = float[4][4];
using DMatrix = double[4][4];

enum BC_global_forward_axis {
  BC_GLOBAL_FORWARD_X = 0,
  BC_GLOBAL_FORWARD_Y = 1,
  BC_GLOBAL_FORWARD_Z = 2,
  BC_GLOBAL_FORWARD_MINUS_X = 3,
  BC_GLOBAL_FORWARD_MINUS_Y = 4,
  BC_GLOBAL_FORWARD_MINUS_Z = 5
};

enum BC_global_up_axis {
  BC_GLOBAL_UP_X = 0,
  BC_GLOBAL_UP_Y = 1,
  BC_GLOBAL_UP_Z = 2,
  BC_GLOBAL_UP_MINUS_X = 3,
  BC_GLOBAL_UP_MINUS_Y = 4,
  BC_GLOBAL_UP_MINUS_Z = 5
};
