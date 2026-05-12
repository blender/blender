/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_defs.h"
#include "DNA_vec_types.h"

#include "BLI_enum_flags.hh"

namespace blender {

/* general defines for kernel functions */
#define CM_RESOL 32
#define CM_TABLE 256
#define CM_TABLEDIV (1.0f / 256.0f)

#define CM_TOT 4

#define GPU_SKY_WIDTH 512
#define GPU_SKY_HEIGHT 256

/** #CurveMapPoint.flag */
enum eCurveMapPoint_Flag : short {
  CUMA_SELECT = (1 << 0),
  CUMA_HANDLE_VECTOR = (1 << 1),
  CUMA_HANDLE_AUTO_ANIM = (1 << 2),
  /** Temporary tag for point deletion. */
  CUMA_REMOVE = (1 << 3),
  /** Active point in selection. */
  CUMA_ACTIVE = (1 << 4),
};
ENUM_OPERATORS(eCurveMapPoint_Flag)

/** #CurveMapping.flag */
enum eCurveMappingFlags : int {
  CUMA_DO_CLIP = (1 << 0),
  CUMA_PREMULLED = (1 << 1),
  CUMA_DRAW_CFRA = (1 << 2),
  CUMA_DRAW_SAMPLE = (1 << 3),

  /** The curve is extended by extrapolation. When not set the curve is extended horizontally. */
  CUMA_EXTEND_EXTRAPOLATE = (1 << 4),
  CUMA_USE_WRAPPING = (1 << 5),
};
ENUM_OPERATORS(eCurveMappingFlags)

/** #CurveMapping.preset */
enum eCurveMappingPreset : int {
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
};

/** #CurveMapping.tone */
enum eCurveMappingTone : short {
  CURVE_TONE_STANDARD = 0,
  CURVE_TONE_FILMLIKE = 2,
};

/** #Histogram.mode */
enum eHistogram_Mode : short {
  HISTO_MODE_LUMA = 0,
  HISTO_MODE_RGB = 1,
  HISTO_MODE_R = 2,
  HISTO_MODE_G = 3,
  HISTO_MODE_B = 4,
  HISTO_MODE_ALPHA = 5,
};

/** #Histogram.flag */
enum eHistogram_Flag : short {
  HISTO_FLAG_LINE = (1 << 0),
  HISTO_FLAG_SAMPLELINE = (1 << 1),
};
ENUM_OPERATORS(eHistogram_Flag)

/** #Scopes.wavefrm_mode */
enum eScopes_WaveformMode : int {
  SCOPES_WAVEFRM_LUMA = 0,
  SCOPES_WAVEFRM_RGB_PARADE = 1,
  SCOPES_WAVEFRM_YCC_601 = 2,
  SCOPES_WAVEFRM_YCC_709 = 3,
  SCOPES_WAVEFRM_YCC_JPEG = 4,
  SCOPES_WAVEFRM_RGB = 5,
};

/** #Scopes.vecscope_mode */
enum eScopes_VecscopeMode : int {
  SCOPES_VECSCOPE_RGB = 0,
  SCOPES_VECSCOPE_LUMA = 1,
};

/** #ColorManagedDisplaySettings.emulation */
enum eColorManageDisplay_Emulation : char {
  COLORMANAGE_DISPLAY_EMULATION_AUTO = 0,
  COLORMANAGE_DISPLAY_EMULATION_OFF = 1,
};

/** #ColorManagedViewSettings.flag */
enum eColorManageView_Flag : int {
  COLORMANAGE_VIEW_USE_CURVES = (1 << 0),
  COLORMANAGE_VIEW_USE_DEPRECATED = (1 << 1),
  COLORMANAGE_VIEW_USE_WHITE_BALANCE = (1 << 2),
  /* Only work as pure view transform and look, no other settings.
   * Not user editable, but fixed depending on where settings are stored. */
  COLORMANAGE_VIEW_ONLY_VIEW_LOOK = (1 << 3)
};
ENUM_OPERATORS(eColorManageView_Flag)

struct CurveMapPoint {
  float x = 0, y = 0;
  /** Shorty for result lookup. */
  eCurveMapPoint_Flag flag = {};
  short shorty = 0;
};

struct CurveMap {
  DNA_DEFINE_CXX_METHODS(CurveMap)

  short totpoint = 0;
  DNA_DEPRECATED short flag = 0;

  /** Quick multiply value for reading table. */
  float range = 0;
  /** The x-axis range for the table. */
  float mintable = 0, maxtable = 0;
  /** For extrapolated curves, the direction vector. */
  float ext_in[2] = {}, ext_out[2] = {};
  /** Actual curve. */
  CurveMapPoint *curve = nullptr;
  /** Display and evaluate table. */
  CurveMapPoint *table = nullptr;

  /** For RGB curves, pre-multiplied table. */
  CurveMapPoint *premultable = nullptr;
  /** For RGB curves, pre-multiplied extrapolation vector. */
  float premul_ext_in[2] = {};
  float premul_ext_out[2] = {};
  eCurveMapPoint_Flag default_handle_type = {};
  char _pad[6] = {};
};

struct CurveMapping {
  DNA_DEFINE_CXX_METHODS(CurveMapping)

  /** Cur; for buttons, to show active curve. */
  eCurveMappingFlags flag = {};
  int cur = 0;
  eCurveMappingPreset preset = CURVE_PRESET_LINE;
  int changed_timestamp = 0;

  /** Current rect, clip rect (is default rect too). */
  rctf curr = {}, clipr = {};

  /** Max 4 builtin curves per mapping struct now. */
  CurveMap cm[4];
  /** Black/white point (black[0] abused for current frame). */
  float black[3] = {}, white[3] = {};
  /** Black/white point multiply value, for speed. */
  float bwmul[3] = {};

  /** Sample values, if flag set it draws line and intersection. */
  float sample[3] = {};

  eCurveMappingTone tone = CURVE_TONE_STANDARD;
  char _pad[6] = {};
};

struct Histogram {
  int channels = 0;
  int x_resolution = 0;
  float data_luma[256] = {};
  float data_r[256] = {};
  float data_g[256] = {};
  float data_b[256] = {};
  float data_a[256] = {};
  float xmax = 0, ymax = 0;
  eHistogram_Mode mode = {};
  eHistogram_Flag flag = {};
  int height = 0;

  /** Sample line only (image coords: source -> destination). */
  float co[2][2] = {};
};

struct Scopes {
  int ok = 0;
  int sample_full = 0;
  int sample_lines = 0;
  eScopes_WaveformMode wavefrm_mode = {};
  eScopes_VecscopeMode vecscope_mode = {};
  int wavefrm_height = 0;
  int vecscope_height = 0;
  int waveform_tot = 0;
  float accuracy = 0;
  float wavefrm_alpha = 0;
  float wavefrm_yfac = 0;
  float vecscope_alpha = 0;
  float minmax[3][2] = {};
  struct Histogram hist;
  float *waveform_1 = nullptr;
  float *waveform_2 = nullptr;
  float *waveform_3 = nullptr;
  float *vecscope = nullptr;
  float *vecscope_rgb = nullptr;
};

struct ColorManagedViewSettings {
  eColorManageView_Flag flag = {};
  char _pad[4] = {};
  /** Look which is being applied when displaying buffer on the screen
   * (prior to view transform). */
  char look[64] = "";
  /** View transform which is being applied when displaying buffer on the screen. */
  char view_transform[64] = "";
  /** F-stop exposure. */
  float exposure = 0;
  /** Post-display gamma transform. */
  float gamma = 0;
  /** White balance parameters. */
  float temperature = 0;
  float tint = 0;
  /** Pre-display RGB curves transform. */
  struct CurveMapping *curve_mapping = nullptr;
  void *_pad2 = nullptr;
};

struct ColorManagedDisplaySettings {
  char display_device[64] = "";
  eColorManageDisplay_Emulation emulation = {};
  char _pad[7] = {};
};

struct ColorManagedColorspaceSettings {
  char name[/*MAX_COLORSPACE_NAME*/ 64] = "";
};

}  // namespace blender
