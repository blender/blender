/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

/* clang-format off */

/* -------------------------------------------------------------------- */
/** \name MovieClip Struct
 * \{ */

#define _DNA_DEFAULT_MovieClipProxy \
  { \
    .build_size_flag = IMB_PROXY_25, \
    .build_tc_flag = IMB_TC_RECORD_RUN | IMB_TC_RECORD_RUN_NO_GAPS, \
    .quality = 50, \
  }


#define _DNA_DEFAULT_MovieClip \
  { \
    .aspx = 1.0f, \
    .aspy = 1.0f, \
    .proxy = _DNA_DEFAULT_MovieClipProxy, \
    .start_frame = 1, \
    .frame_offset = 0, \
  }

#define _DNA_DEFAULT_MovieClipUser \
  { \
    .framenr = 1, \
    .render_size = MCLIP_PROXY_RENDER_SIZE_FULL, \
    .render_flag = 0, \
  }

#define _DNA_DEFAULT_MovieClipScopes \
  { \
    .ok = 0, \
    .use_track_mask = 0, \
    .track_preview_height = 120, \
    .frame_width = 0, \
    .frame_height = 0, \
    .undist_marker = _DNA_DEFAULT_MovieTrackingMarker, \
    .track_pos = {0, 0}, \
    .track_disabled = 0, \
    .track_locked = 0, \
    .scene_framenr = 0, \
    .slide_scale = {0.0f, 0.0f}, \
  }

/* initialize as all zeros. */
#define _DNA_DEFAULT_MovieTrackingMarker \
  { \
    .pos = {0.0f, 0.0f}, \
  }

/** \} */

/* clang-format on */
