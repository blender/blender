/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * Rotation Types
 *
 * They give more semantic information and allow overloaded functions based on rotation type.
 * They also prevent implicit cast from rotation to vector types.
 */

#include "BLI_math_angle_types.hh"
#include "BLI_math_axis_angle_types.hh"
#include "BLI_math_basis_types.hh"
#include "BLI_math_euler_types.hh"
#include "BLI_math_quaternion_types.hh"
