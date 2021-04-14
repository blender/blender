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
/** \name MovieClip Struct
 * \{ */

#define _DNA_DEFAULT_MovieClipProxy \
  { \
    .build_size_flag = IMB_PROXY_25, \
    .build_tc_flag = IMB_TC_RECORD_RUN | IMB_TC_FREE_RUN | \
                     IMB_TC_INTERPOLATED_REC_DATE_FREE_RUN | IMB_TC_RECORD_RUN_NO_GAPS, \
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

/** \} */

/* clang-format on */
