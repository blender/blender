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

typedef enum IMB_Timecode_Type {
  /** Don't use time-code files at all. */
  IMB_TC_NONE = 0,
  /**
   * Use images in the order as they are recorded
   * (currently, this is the only one implemented
   * and is a sane default).
   */
  IMB_TC_RECORD_RUN = 1,
  /**
   * Use global timestamp written by recording
   * device (prosumer camcorders e.g. can do that).
   */
  IMB_TC_FREE_RUN = 2,
  /**
   * Interpolate a global timestamp using the
   * record date and time written by recording
   * device (*every* consumer camcorder can do that).
   */
  IMB_TC_INTERPOLATED_REC_DATE_FREE_RUN = 4,
  IMB_TC_RECORD_RUN_NO_GAPS = 8,
  IMB_TC_MAX_SLOT = 4,
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
