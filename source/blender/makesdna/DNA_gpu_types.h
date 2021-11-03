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
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup DNA
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Keep for 'Camera' versioning. */
/** Properties for DOF effect. */
typedef struct GPUDOFSettings {
  /** Focal distance for depth of field. */
  float focus_distance;
  float fstop;
  float focal_length;
  float sensor;
  float rotation;
  float ratio;
  int num_blades;
  int high_quality;
} GPUDOFSettings;

#ifdef __cplusplus
}
#endif
