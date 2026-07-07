/* SPDX-FileCopyrightText: 2017-2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

namespace grid {

/* Given a packed float2 and grid configuration, extract axis data into the proper dimensions. */
float3 unpack_xy_to_xyz(float2 xy, uint grid_flag)
{
  if (flag_test(grid_flag, PLANE_XZ)) {
    return float3(xy.x, 0.0f, xy.y);
  }
  else if (flag_test(grid_flag, PLANE_YZ)) {
    return float3(0.0f, xy);
  }
  else { /* GRID_SIMA, PLANE_XY, ... */
    return float3(xy, 0.0f);
  }
}

/* Given a packed float2 and grid configuration, extract a specific axis. */
float unpack_xy_to_axis(float2 xy, uint grid_flag, uint axis)
{
  float3 xyz = unpack_xy_to_xyz(xy, grid_flag);
  return xyz[axis];
}

/* True rue if components of `v` fall within `epsilon` of 0. */
bool is_zero(float2 v, float epsilon)
{
  return all(lessThanEqual(abs(v), float2(epsilon)));
}

}  // namespace grid
