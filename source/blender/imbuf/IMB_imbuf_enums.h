/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \file
 * \ingroup imbuf
 */

/* WARNING: Keep explicit value assignments here,
 * this file is included in areas where not all format defines are set
 * (e.g. intern/dds only get WITH_DDS, even if TIFF, HDR etc are also defined).
 * See #46524. */

/** #ImBuf.ftype flag, main image types. */
enum eImbFileType {
  IMB_FTYPE_NONE = 0,
  IMB_FTYPE_PNG = 1,
  IMB_FTYPE_TGA = 2,
  IMB_FTYPE_JPG = 3,
  IMB_FTYPE_BMP = 4,
  IMB_FTYPE_OPENEXR = 5,
  IMB_FTYPE_IMAGIC = 6,
  IMB_FTYPE_PSD = 7,
#ifdef WITH_OPENJPEG
  IMB_FTYPE_JP2 = 8,
#endif
  IMB_FTYPE_RADHDR = 9,
  IMB_FTYPE_TIF = 10,
#ifdef WITH_CINEON
  IMB_FTYPE_CINEON = 11,
  IMB_FTYPE_DPX = 12,
#endif

  IMB_FTYPE_DDS = 13,
#ifdef WITH_WEBP
  IMB_FTYPE_WEBP = 14,
#endif
};

/**
 * Timecode files contain timestamps (PTS, DTS) and packet seek position. These values are obtained
 * by decoding each frame in movie stream. Timecode types define how these map to frame index in
 * Blender. This is used when seeking in movie stream.
 * Note, that meaning of terms timecode and record run here has little connection to their actual
 * meaning.
 */
typedef enum IMB_Timecode_Type {
  /** Don't use time-code files at all. Use FFmpeg API to seek to PTS calculated on the fly. */
  IMB_TC_NONE = 0,
  /**
   * TC entries (and therefore frames in movie stream) are mapped to frame index, such that
   * timestamp in Blender matches timestamp in the movie stream. This assumes, that time starts at
   * 0 in both cases.
   *
   * Simplified formula is `frame_index = movie_stream_timestamp * FPS`.
   */
  IMB_TC_RECORD_RUN = 1,
  /**
   * Each TC entry (and therefore frame in movie stream) is mapped to new frame index in Blender.
   *
   * For example: FFmpeg may say, that a frame should be displayed for 0.5 seconds, but this option
   * ignores that and only diplays it in one particular frame index in Blender.
   */
  IMB_TC_RECORD_RUN_NO_GAPS = 8,
  IMB_TC_NUM_TYPES = 2,
} IMB_Timecode_Type;

typedef enum IMB_Proxy_Size {
  IMB_PROXY_NONE = 0,
  IMB_PROXY_25 = 1,
  IMB_PROXY_50 = 2,
  IMB_PROXY_75 = 4,
  IMB_PROXY_100 = 8,
  IMB_PROXY_MAX_SLOT = 4,
} IMB_Proxy_Size;
ENUM_OPERATORS(IMB_Proxy_Size, IMB_PROXY_100);

#ifdef __cplusplus
}
#endif
