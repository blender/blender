/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_defs.h"
#include "DNA_vec_types.h"

/* general defines for kernel functions */
#define CM_RESOL 32
#define CM_TABLE 256
#define CM_TABLEDIV (1.0f / 256.0f)

#define CM_TOT 4

#define GPU_SKY_WIDTH 512
#define GPU_SKY_HEIGHT 256

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
  /** Temporary tag for point deletion. */
  CUMA_REMOVE = (1 << 3),
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
  short default_handle_type;
  char _pad[6];
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
  CUMA_USE_WRAPPING = (1 << 5),
} eCurveMappingFlags;

/** #CurveMapping.preset */
typedef enum eCurveMappingPreset {
  CURVE_PRESET_LINE = 0,
  CURVE_PRESET_SHARP = 1,
  CURVE_PRESET_SMOOTH = 2,
  CURVE_PRESET_MAX = 3,
  CURVE_PRESET_MID8 = 4,
  CURVE_PRESET_ROUND = 5,
  CURVE_PRESET_ROOT = 6,
  CURVE_PRESET_GAUSS = 7,
  CURVE_PRESET_BELL = 8,
  CURVE_PRESET_CONSTANT_MEDIAN = 9,
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

/* Multiplier to map YUV U,V range (+-0.436, +-0.615) to +-0.5 on both axes. */
#define SCOPES_VEC_U_SCALE float(0.5f / 0.436f)
#define SCOPES_VEC_V_SCALE float(0.5f / 0.615f)

typedef struct Scopes {
  int ok;
  int sample_full;
  int sample_lines;
  int wavefrm_mode;
  int vecscope_mode;
  int wavefrm_height;
  int vecscope_height;
  int waveform_tot;
  float accuracy;
  float wavefrm_alpha;
  float wavefrm_yfac;
  float vecscope_alpha;
  float minmax[3][2];
  struct Histogram hist;
  float *waveform_1;
  float *waveform_2;
  float *waveform_3;
  float *vecscope;
  float *vecscope_rgb;
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

/** #Scopes.vecscope_mode */
enum {
  SCOPES_VECSCOPE_RGB = 0,
  SCOPES_VECSCOPE_LUMA = 1,
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
  /** White balance parameters. */
  float temperature;
  float tint;
  /** Pre-display RGB curves transform. */
  struct CurveMapping *curve_mapping;
  void *_pad2;
} ColorManagedViewSettings;

typedef struct ColorManagedDisplaySettings {
  char display_device[64];
  char emulation;
  char _pad[7];
} ColorManagedDisplaySettings;

typedef struct ColorManagedColorspaceSettings {
  char name[/*MAX_COLORSPACE_NAME*/ 64];
} ColorManagedColorspaceSettings;

/** #ColorManagedDisplaySettings.emulation */
enum {
  COLORMANAGE_DISPLAY_EMULATION_AUTO = 0,
  COLORMANAGE_DISPLAY_EMULATION_OFF = 1,
};

/** #ColorManagedViewSettings.flag */
enum {
  COLORMANAGE_VIEW_USE_CURVES = (1 << 0),
  COLORMANAGE_VIEW_USE_DEPRECATED = (1 << 1),
  COLORMANAGE_VIEW_USE_WHITE_BALANCE = (1 << 2),
  /* Only work as pure view transform and look, no other settings.
   * Not user editable, but fixed depending on where settings are stored. */
  COLORMANAGE_VIEW_ONLY_VIEW_LOOK = (1 << 3)
};
