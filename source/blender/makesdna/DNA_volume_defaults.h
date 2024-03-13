/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

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
    .precision = VOLUME_PRECISION_HALF, \
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
    .velocity_scale = 1.0f, \
}

/** \} */

/* clang-format on */
