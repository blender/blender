/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

/* clang-format off */

/* -------------------------------------------------------------------- */
/** \name SpaceClip Struct
 * \{ */

#define _DNA_DEFAULT_MaskSpaceInfo \
  { \
    .draw_flag = MASK_DRAWFLAG_SPLINE, \
    .draw_type = MASK_DT_OUTLINE, \
    .overlay_mode = MASK_OVERLAY_ALPHACHANNEL, \
    .blend_factor = 0.7f, \
  }

#define _DNA_DEFAULT_SpaceClipOverlay \
  { \
    .flag = SC_SHOW_OVERLAYS | SC_SHOW_CURSOR, \
  }

#define _DNA_DEFAULT_SpaceClip \
  { \
    .spacetype = SPACE_CLIP, \
    .link_flag = 0, \
    .xof = 0, \
    .yof = 0, \
    .xlockof = 0, \
    .ylockof = 0, \
    .zoom = 1.0f, \
    .user = _DNA_DEFAULT_MovieClipUser, \
    .scopes = _DNA_DEFAULT_MovieClipScopes, \
    .flag = SC_SHOW_MARKER_PATTERN | SC_SHOW_TRACK_PATH | SC_SHOW_GRAPH_TRACKS_MOTION | \
                 SC_SHOW_GRAPH_FRAMES | SC_SHOW_ANNOTATION, \
    .mode = SC_MODE_TRACKING, \
    .view = SC_VIEW_CLIP, \
    .path_length = 20, \
    .loc = {0, 0}, \
    .scale = 0, \
    .angle = 0, \
    .stabmat = _DNA_DEFAULT_UNIT_M4, \
    .unistabmat = _DNA_DEFAULT_UNIT_M4, \
    .postproc_flag = 0, \
    .gpencil_src = SC_GPENCIL_SRC_CLIP, \
    .around = V3D_AROUND_CENTER_MEDIAN, \
    .cursor = {0, 0}, \
    .mask_info = _DNA_DEFAULT_MaskSpaceInfo, \
    .overlay = _DNA_DEFAULT_SpaceClipOverlay, \
  }

/** \} */

/* clang-format on */
