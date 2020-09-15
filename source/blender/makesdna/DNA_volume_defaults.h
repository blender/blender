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

/* Struct members on own line. */
/* clang-format off */

/* -------------------------------------------------------------------- */
/** \name Volume Struct
 * \{ */

#define _DNA_DEFAULT_VolumeDisplay \
  { \
    .density = 1.0f, \
    .wireframe_type = VOLUME_WIREFRAME_BOXES, \
    .wireframe_detail = VOLUME_WIREFRAME_COARSE, \
    .slice_depth = 0.5f, \
  }

#define _DNA_DEFAULT_VolumeRender \
  { \
    .space = VOLUME_SPACE_OBJECT, \
    .step_size = 0.0f, \
    .clipping = 0.001f, \
  }

#define _DNA_DEFAULT_Volume \
  { \
    .filepath[0] = '\0', \
    .frame_start = 1, \
    .frame_offset = 0, \
    .frame_duration = 0, \
    .display = _DNA_DEFAULT_VolumeDisplay, \
    .render = _DNA_DEFAULT_VolumeRender, \
  }

/** \} */

/* clang-format on */
