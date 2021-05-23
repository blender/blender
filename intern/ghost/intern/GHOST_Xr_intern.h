/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup GHOST
 */

#pragma once

#include <memory>
#include <vector>

#include "GHOST_Xr_openxr_includes.h"

#define CHECK_XR(call, error_msg) \
  { \
    XrResult _res = call; \
    if (XR_FAILED(_res)) { \
      throw GHOST_XrException(error_msg, _res); \
    } \
  } \
  (void)0

/**
 * Variation of CHECK_XR() that doesn't throw, but asserts for success. Especially useful for
 * destructors, which shouldn't throw.
 */
#define CHECK_XR_ASSERT(call) \
  { \
    XrResult _res = call; \
    assert(_res == XR_SUCCESS); \
    (void)_res; \
  } \
  (void)0

inline void copy_ghost_pose_to_openxr_pose(const GHOST_XrPose &ghost_pose, XrPosef &r_oxr_pose)
{
  /* Set and convert to OpenXR coordinate space. */
  r_oxr_pose.position.x = ghost_pose.position[0];
  r_oxr_pose.position.y = ghost_pose.position[1];
  r_oxr_pose.position.z = ghost_pose.position[2];
  r_oxr_pose.orientation.w = ghost_pose.orientation_quat[0];
  r_oxr_pose.orientation.x = ghost_pose.orientation_quat[1];
  r_oxr_pose.orientation.y = ghost_pose.orientation_quat[2];
  r_oxr_pose.orientation.z = ghost_pose.orientation_quat[3];
}

inline void copy_openxr_pose_to_ghost_pose(const XrPosef &oxr_pose, GHOST_XrPose &r_ghost_pose)
{
  /* Set and convert to Blender coordinate space. */
  r_ghost_pose.position[0] = oxr_pose.position.x;
  r_ghost_pose.position[1] = oxr_pose.position.y;
  r_ghost_pose.position[2] = oxr_pose.position.z;
  r_ghost_pose.orientation_quat[0] = oxr_pose.orientation.w;
  r_ghost_pose.orientation_quat[1] = oxr_pose.orientation.x;
  r_ghost_pose.orientation_quat[2] = oxr_pose.orientation.y;
  r_ghost_pose.orientation_quat[3] = oxr_pose.orientation.z;
}
