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
/** \name SpaceClip Struct
 * \{ */

#define _DNA_DEFAULT_MaskSpaceInfo \
  { \
    .draw_flag = 0, \
    .draw_type = MASK_DT_OUTLINE, \
    .overlay_mode = MASK_OVERLAY_ALPHACHANNEL, \
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
  }

/** \} */

/* clang-format on */
