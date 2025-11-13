/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** -- Renderer Options -- */
/* Number of frames over which rolling averages are taken. */
#define MTL_FRAME_AVERAGE_COUNT 15
#define MTL_MAX_DRAWABLES 3
#define MTL_FORCE_WAIT_IDLE 0

/* Number of frames for which we retain in-flight resources such as scratch buffers.
 * Set as number of GPU frames in flight, plus an additional value for extra possible CPU frame. */
#define MTL_NUM_SAFE_FRAMES (MTL_MAX_DRAWABLES + 1)

/* Display debug information about missing attributes and incorrect vertex formats. */
#define MTL_DEBUG_SHADER_ATTRIBUTES 0
