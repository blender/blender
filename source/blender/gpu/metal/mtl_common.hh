/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __MTL_COMMON
#define __MTL_COMMON

// -- Renderer Options --
#define MTL_MAX_DRAWABLES 3
#define MTL_MAX_SET_BYTES_SIZE 4096
#define MTL_FORCE_WAIT_IDLE 0
#define MTL_MAX_COMMAND_BUFFERS 64

/* Number of frames for which we retain in-flight resources such as scratch buffers.
 * Set as number of GPU frames in flight, plus an additional value for extra possible CPU frame. */
#define MTL_NUM_SAFE_FRAMES (MTL_MAX_DRAWABLES + 1)

#endif
