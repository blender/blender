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
 * \ingroup DNA
 */

#pragma once

#include "DNA_view3d_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XrSessionSettings {
  /** Shading settings, struct shared with 3D-View so settings are the same. */
  struct View3DShading shading;

  char _pad[7];

  char base_pose_type; /* eXRSessionBasePoseType */
  /** Object to take the location and rotation as base position from. */
  Object *base_pose_object;
  float base_pose_location[3];
  float base_pose_angle;

  /** View3D draw flags (V3D_OFSDRAW_NONE, V3D_OFSDRAW_SHOW_ANNOTATION, ...). */
  char draw_flags;
  char _pad2[3];

  /** Clipping distance. */
  float clip_start, clip_end;

  int flag;
} XrSessionSettings;

typedef enum eXrSessionFlag {
  XR_SESSION_USE_POSITION_TRACKING = (1 << 0),
} eXrSessionFlag;

typedef enum eXRSessionBasePoseType {
  XR_BASE_POSE_SCENE_CAMERA = 0,
  XR_BASE_POSE_OBJECT = 1,
  XR_BASE_POSE_CUSTOM = 2,
} eXRSessionBasePoseType;

/** XR action type. Enum values match those in GHOST_XrActionType enum for consistency. */
typedef enum eXrActionType {
  XR_BOOLEAN_INPUT = 1,
  XR_FLOAT_INPUT = 2,
  XR_VECTOR2F_INPUT = 3,
  XR_POSE_INPUT = 4,
  XR_VIBRATION_OUTPUT = 100,
} eXrActionType;

typedef enum eXrOpFlag {
  XR_OP_PRESS = 0,
  XR_OP_RELEASE = 1,
  XR_OP_MODAL = 2,
} eXrOpFlag;

#ifdef __cplusplus
}
#endif
