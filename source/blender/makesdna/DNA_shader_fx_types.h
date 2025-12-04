/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_defs.h"
#include "DNA_listBase.h"

enum ShaderFxMode {
  eShaderFxMode_Realtime = (1 << 0),
  eShaderFxMode_Render = (1 << 1),
  eShaderFxMode_Editmode = (1 << 2),
#ifdef DNA_DEPRECATED_ALLOW
  eShaderFxMode_Expanded_DEPRECATED = (1 << 3),
#endif
};

enum ShaderFxFlag {
  /* This fx has been inserted in local override, and hence can be fully edited. */
  eShaderFxFlag_OverrideLibrary_Local = (1 << 0),
};

enum eBlurShaderFx_Flag {
  FX_BLUR_DOF_MODE = (1 << 0),
};

enum ColorizeShaderFxModes {
  eShaderFxColorizeMode_GrayScale = 0,
  eShaderFxColorizeMode_Sepia = 1,
  eShaderFxColorizeMode_Duotone = 2,
  eShaderFxColorizeMode_Custom = 3,
  eShaderFxColorizeMode_Transparent = 4,
};

enum eFlipShaderFx_Flag {
  FX_FLIP_HORIZONTAL = (1 << 0),
  FX_FLIP_VERTICAL = (1 << 1),
};

enum GlowShaderFxModes {
  eShaderFxGlowMode_Luminance = 0,
  eShaderFxGlowMode_Color = 1,
};

enum eGlowShaderFx_Flag {
  FX_GLOW_USE_ALPHA = (1 << 0),
};

enum ePixelShaderFx_Flag {
  FX_PIXEL_FILTER_NEAREST = (1 << 0),
};

enum RimShaderFxModes {
  eShaderFxRimMode_Normal = 0,
  eShaderFxRimMode_Overlay = 1,
  eShaderFxRimMode_Add = 2,
  eShaderFxRimMode_Subtract = 3,
  eShaderFxRimMode_Multiply = 4,
  eShaderFxRimMode_Divide = 5,
};

enum eShadowShaderFx_Flag {
  FX_SHADOW_USE_OBJECT = (1 << 0),
  FX_SHADOW_USE_WAVE = (1 << 1),
};

enum eSwirlShaderFx_Flag {
  FX_SWIRL_MAKE_TRANSPARENT = (1 << 0),
};

struct DRWShadingGroup;

/* WARNING ALERT! TYPEDEF VALUES ARE WRITTEN IN FILES! SO DO NOT CHANGE!
 * (ONLY ADD NEW ITEMS AT THE END)
 */

enum ShaderFxType {
  eShaderFxType_None = 0,
  eShaderFxType_Blur = 1,
  eShaderFxType_Flip = 2,
  eShaderFxType_Light_deprecated = 3, /* DEPRECATED (replaced by scene lights) */
  eShaderFxType_Pixel = 4,
  eShaderFxType_Swirl = 5,
  eShaderFxType_Wave = 6,
  eShaderFxType_Rim = 7,
  eShaderFxType_Colorize = 8,
  eShaderFxType_Shadow = 9,
  eShaderFxType_Glow = 10,
  /* Keep last. */
  NUM_SHADER_FX_TYPES,
};

typedef struct ShaderFxData {
  struct ShaderFxData *next, *prev;

  int type, mode;
  char _pad0[4];
  short flag;
  /* An "expand" bit for each of the constraint's (sub)panels (uiPanelDataExpansion). */
  short ui_expand_flag;
  char name[/*MAX_NAME*/ 64];

  char *error;
} ShaderFxData;

/** Runtime temp data. */
typedef struct ShaderFxData_Runtime {
  float loc[3];
  char _pad[4];
  struct DRWShadingGroup *fx_sh;
  struct DRWShadingGroup *fx_sh_b;
  struct DRWShadingGroup *fx_sh_c;
} ShaderFxData_Runtime;

typedef struct BlurShaderFxData {
  ShaderFxData shaderfx;
  float radius[2];
  /** Flags. */
  int flag;
  /** Number of samples. */
  int samples;
  /** Rotation of blur effect. */
  float rotation;
  char _pad[4];

  ShaderFxData_Runtime runtime;
} BlurShaderFxData;

typedef struct ColorizeShaderFxData {
  ShaderFxData shaderfx;
  int mode;
  float low_color[4];
  float high_color[4];
  float factor;
  /** Flags. */
  int flag;
  char _pad[4];

  ShaderFxData_Runtime runtime;
} ColorizeShaderFxData;

typedef struct FlipShaderFxData {
  ShaderFxData shaderfx;
  /** Flags. */
  int flag;
  /** Internal, not visible in rna. */
  int flipmode;
  ShaderFxData_Runtime runtime;
} FlipShaderFxData;

typedef struct GlowShaderFxData {
  ShaderFxData shaderfx;
  float glow_color[4];
  float select_color[3];
  float threshold;
  /** Flags. */
  int flag;
  int mode;
  float blur[2];
  int samples;
  /** Rotation of effect. */
  float rotation;
  /** Blend modes. */
  int blend_mode;
  char _pad[4];

  ShaderFxData_Runtime runtime;
} GlowShaderFxData;

typedef struct PixelShaderFxData {
  ShaderFxData shaderfx;
  /** Last element used for shader only. */
  int size[3];
  /** Flags. */
  int flag;
  float rgba[4];
  ShaderFxData_Runtime runtime;
} PixelShaderFxData;

typedef struct RimShaderFxData {
  ShaderFxData shaderfx;
  int offset[2];
  /** Flags. */
  int flag;
  float rim_rgb[3];
  float mask_rgb[3];
  int mode;
  int blur[2];
  int samples;
  char _pad[4];
  ShaderFxData_Runtime runtime;
} RimShaderFxData;

typedef struct ShadowShaderFxData {
  ShaderFxData shaderfx;
  struct Object *object;
  int offset[2];
  /** Flags. */
  int flag;
  float shadow_rgba[4];
  float amplitude;
  float period;
  float phase;
  int orientation;
  float scale[2];
  float rotation;
  int blur[2];
  int samples;
  char _pad[4];
  ShaderFxData_Runtime runtime;
} ShadowShaderFxData;

typedef struct SwirlShaderFxData {
  ShaderFxData shaderfx;
  struct Object *object;
  /** Flags. */
  int flag;
  int radius;
  float angle;
  /** Not visible in rna. */
  int transparent;
  ShaderFxData_Runtime runtime;
} SwirlShaderFxData;

typedef struct WaveShaderFxData {
  ShaderFxData shaderfx;
  float amplitude;
  float period;
  float phase;
  int orientation;
  /** Flags. */
  int flag;
  char _pad[4];
  ShaderFxData_Runtime runtime;
} WaveShaderFxData;
