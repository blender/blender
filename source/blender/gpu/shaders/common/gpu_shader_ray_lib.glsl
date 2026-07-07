/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

/* General purpose 3D ray. */
struct Ray {
  packed_float3 direction;
  float max_time;
  packed_float3 origin;
};
