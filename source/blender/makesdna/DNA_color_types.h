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
 *
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_defs.h"
#include "DNA_vec_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* general defines for kernel functions */
#define CM_RESOL 32
#define CM_TABLE 256
#define CM_TABLEDIV (1.0f / 256.0f)

#define CM_TOT 4

typedef struct CurveMapPoint {
  float x, y;
  /** Shorty for result lookup. */
  short flag, shorty;
} CurveMapPoint;

/** #CurveMapPoint.flag */
enum {
  CUMA_SELECT = (1 << 0),
  CUMA_HANDLE_VECTOR = (1 << 1),
  CUMA_HANDLE_AUTO_ANIM = (1 << 2),
};

typedef struct CurveMap {
  short totpoint;
  short flag DNA_DEPRECATED;

  /** Quick multiply value for reading table. */
  float range;
  /** The x-axis range for the table. */
  float mintable, maxtable;
  /** For extrapolated curves, the direction vector. */
  float ext_in[2], ext_out[2];
  /** Actual curve. */
  CurveMapPoint *curve;
  /** Display and evaluate table. */
  CurveMapPoint *table;

  /** For RGB curves, pre-multiplied table. */
  CurveMapPoint *premultable;
  /** For RGB curves, pre-multiplied extrapolation vector. */
  float premul_ext_in[2];
  float premul_ext_out[2];
} CurveMap;

typedef struct CurveMapping {
  /** Cur; for buttons, to show active curve. */
  int flag, cur;
  int preset;
  int changed_timestamp;

  /** Current rect, clip rect (is default rect too). */
  rctf curr, clipr;

  /** Max 4 builtin curves per mapping struct now. */
  CurveMap cm[4];
  /** Black/white point (black[0] abused for current frame). */
  float black[3], white[3];
  /** Black/white point multiply value, for speed. */
  float bwmul[3];

  /** Sample values, if flag set it draws line and intersection. */
  float sample[3];

  short tone;
  char _pad[6];
} CurveMapping;

/** #CurveMapping.flag */
typedef enum eCurveMappingFlags {
  CUMA_DO_CLIP = (1 << 0),
  CUMA_PREMULLED = (1 << 1),
  CUMA_DRAW_CFRA = (1 << 2),
  CUMA_DRAW_SAMPLE = (1 << 3),

  /** The curve is extended by extrapolation. When not set the curve is extended horizontally. */
  CUMA_EXTEND_EXTRAPOLATE = (1 << 4),
} eCurveMappingFlags;

/** #CurveMapping.preset */
typedef enum eCurveMappingPreset {
  CURVE_PRESET_LINE = 0,
  CURVE_PRESET_SHARP = 1,
  CURVE_PRESET_SMOOTH = 2,
  CURVE_PRESET_MAX = 3,
  CURVE_PRESET_MID9 = 4,
  CURVE_PRESET_ROUND = 5,
  CURVE_PRESET_ROOT = 6,
  CURVE_PRESET_GAUSS = 7,
  CURVE_PRESET_BELL = 8,
} eCurveMappingPreset;

/** #CurveMapping.tone */
typedef enum eCurveMappingTone {
  CURVE_TONE_STANDARD = 0,
  CURVE_TONE_FILMLIKE = 2,
} eCurveMappingTone;

/** #Histogram.mode */
enum {
  HISTO_MODE_LUMA = 0,
  HISTO_MODE_RGB = 1,
  HISTO_MODE_R = 2,
  HISTO_MODE_G = 3,
  HISTO_MODE_B = 4,
  HISTO_MODE_ALPHA = 5,
};

enum {
  HISTO_FLAG_LINE = (1 << 0),
  HISTO_FLAG_SAMPLELINE = (1 << 1),
};

typedef struct Histogram {
  int channels;
  int x_resolution;
  float data_luma[256];
  float data_r[256];
  float data_g[256];
  float data_b[256];
  float data_a[256];
  float xmax, ymax;
  short mode;
  short flag;
  int height;

  /** Sample line only (image coords: source -> destination). */
  float co[2][2];
} Histogram;

typedef struct Scopes {
  int ok;
  int sample_full;
  int sample_lines;
  float accuracy;
  int wavefrm_mode;
  float wavefrm_alpha;
  float wavefrm_yfac;
  int wavefrm_height;
  float vecscope_alpha;
  int vecscope_height;
  float minmax[3][2];
  struct Histogram hist;
  float *waveform_1;
  float *waveform_2;
  float *waveform_3;
  float *vecscope;
  int waveform_tot;
  char _pad[4];
} Scopes;

/** #Scopes.wavefrm_mode */
enum {
  SCOPES_WAVEFRM_LUMA = 0,
  SCOPES_WAVEFRM_RGB_PARADE = 1,
  SCOPES_WAVEFRM_YCC_601 = 2,
  SCOPES_WAVEFRM_YCC_709 = 3,
  SCOPES_WAVEFRM_YCC_JPEG = 4,
  SCOPES_WAVEFRM_RGB = 5,
};

typedef struct ColorManagedViewSettings {
  int flag;
  char _pad[4];
  /** Look which is being applied when displaying buffer on the screen
   * (prior to view transform). */
  char look[64];
  /** View transform which is being applied when displaying buffer on the screen. */
  char view_transform[64];
  /** F-stop exposure. */
  float exposure;
  /** Post-display gamma transform. */
  float gamma;
  /** Pre-display RGB curves transform. */
  struct CurveMapping *curve_mapping;
  void *_pad2;
} ColorManagedViewSettings;

typedef struct ColorManagedDisplaySettings {
  char display_device[64];
} ColorManagedDisplaySettings;

typedef struct ColorManagedColorspaceSettings {
  /** MAX_COLORSPACE_NAME. */
  char name[64];
} ColorManagedColorspaceSettings;

/** #ColorManagedViewSettings.flag */
enum {
  COLORMANAGE_VIEW_USE_CURVES = (1 << 0),
};

#ifdef __cplusplus
}
#endif
