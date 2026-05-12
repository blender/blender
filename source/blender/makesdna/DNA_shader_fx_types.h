/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "BLI_enum_flags.hh"

struct DRWShadingGroup;
namespace blender {

enum ShaderFxMode : int {
  eShaderFxMode_Realtime = (1 << 0),
  eShaderFxMode_Render = (1 << 1),
  eShaderFxMode_Editmode = (1 << 2),
#ifdef DNA_DEPRECATED_ALLOW
  eShaderFxMode_Expanded_DEPRECATED = (1 << 3),
#endif
};
ENUM_OPERATORS(ShaderFxMode)

enum ShaderFxFlag : short {
  /* This fx has been inserted in local override, and hence can be fully edited. */
  eShaderFxFlag_OverrideLibrary_Local = (1 << 0),
};
ENUM_OPERATORS(ShaderFxFlag)

enum eBlurShaderFx_Flag : int {
  FX_BLUR_DOF_MODE = (1 << 0),
};
ENUM_OPERATORS(eBlurShaderFx_Flag)

enum ColorizeShaderFxModes : int {
  eShaderFxColorizeMode_GrayScale = 0,
  eShaderFxColorizeMode_Sepia = 1,
  eShaderFxColorizeMode_Duotone = 2,
  eShaderFxColorizeMode_Custom = 3,
  eShaderFxColorizeMode_Transparent = 4,
};

enum eFlipShaderFx_Flag : int {
  FX_FLIP_HORIZONTAL = (1 << 0),
  FX_FLIP_VERTICAL = (1 << 1),
};
ENUM_OPERATORS(eFlipShaderFx_Flag)

enum GlowShaderFxModes : int {
  eShaderFxGlowMode_Luminance = 0,
  eShaderFxGlowMode_Color = 1,
};

enum eGlowShaderFx_Flag : int {
  FX_GLOW_USE_ALPHA = (1 << 0),
};
ENUM_OPERATORS(eGlowShaderFx_Flag)

enum ePixelShaderFx_Flag : int {
  FX_PIXEL_FILTER_NEAREST = (1 << 0),
};
ENUM_OPERATORS(ePixelShaderFx_Flag)

enum RimShaderFxModes : int {
  eShaderFxRimMode_Normal = 0,
  eShaderFxRimMode_Overlay = 1,
  eShaderFxRimMode_Add = 2,
  eShaderFxRimMode_Subtract = 3,
  eShaderFxRimMode_Multiply = 4,
  eShaderFxRimMode_Divide = 5,
};

enum eShadowShaderFx_Flag : int {
  FX_SHADOW_USE_OBJECT = (1 << 0),
  FX_SHADOW_USE_WAVE = (1 << 1),
};
ENUM_OPERATORS(eShadowShaderFx_Flag)

enum eSwirlShaderFx_Flag : int {
  FX_SWIRL_MAKE_TRANSPARENT = (1 << 0),
};
ENUM_OPERATORS(eSwirlShaderFx_Flag)

/* WARNING ALERT! TYPEDEF VALUES ARE WRITTEN IN FILES! SO DO NOT CHANGE!
 * (ONLY ADD NEW ITEMS AT THE END)
 */

enum ShaderFxType : int {
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

struct ShaderFxData {
  struct ShaderFxData *next = nullptr, *prev = nullptr;

  ShaderFxType type = eShaderFxType_None;
  ShaderFxMode mode = {};
  char _pad0[4] = {};
  ShaderFxFlag flag = {};
  /* An "expand" bit for each of the constraint's (sub)panels (uiPanelDataExpansion). */
  short ui_expand_flag = 0;
  char name[/*MAX_NAME*/ 64] = "";

  char *error = nullptr;
};

/** Runtime temp data. */
struct ShaderFxData_Runtime {
  float loc[3] = {};
  char _pad[4] = {};
  struct DRWShadingGroup *fx_sh = nullptr;
  struct DRWShadingGroup *fx_sh_b = nullptr;
  struct DRWShadingGroup *fx_sh_c = nullptr;
};

struct BlurShaderFxData {
  ShaderFxData shaderfx;
  float radius[2] = {};
  /** Flags. */
  eBlurShaderFx_Flag flag = {};
  /** Number of samples. */
  int samples = 0;
  /** Rotation of blur effect. */
  float rotation = 0;
  char _pad[4] = {};

  ShaderFxData_Runtime runtime;
};

struct ColorizeShaderFxData {
  ShaderFxData shaderfx;
  ColorizeShaderFxModes mode = eShaderFxColorizeMode_GrayScale;
  float low_color[4] = {};
  float high_color[4] = {};
  float factor = 0;
  /** Flags. */
  int flag = 0;
  char _pad[4] = {};

  ShaderFxData_Runtime runtime;
};

struct FlipShaderFxData {
  ShaderFxData shaderfx;
  /** Flags. */
  eFlipShaderFx_Flag flag = {};
  /** Internal, not visible in rna. */
  int flipmode = 0;
  ShaderFxData_Runtime runtime;
};

struct GlowShaderFxData {
  ShaderFxData shaderfx;
  float glow_color[4] = {};
  float select_color[3] = {};
  float threshold = 0;
  /** Flags. */
  eGlowShaderFx_Flag flag = {};
  GlowShaderFxModes mode = eShaderFxGlowMode_Luminance;
  float blur[2] = {};
  int samples = 0;
  /** Rotation of effect. */
  float rotation = 0;
  /** Blend modes. */
  int blend_mode = 0;
  char _pad[4] = {};

  ShaderFxData_Runtime runtime;
};

struct PixelShaderFxData {
  ShaderFxData shaderfx;
  /** Last element used for shader only. */
  int size[3] = {};
  /** Flags. */
  ePixelShaderFx_Flag flag = {};
  float rgba[4] = {};
  ShaderFxData_Runtime runtime;
};

struct RimShaderFxData {
  ShaderFxData shaderfx;
  int offset[2] = {};
  /** Flags. */
  int flag = 0;
  float rim_rgb[3] = {};
  float mask_rgb[3] = {};
  RimShaderFxModes mode = eShaderFxRimMode_Normal;
  int blur[2] = {};
  int samples = 0;
  char _pad[4] = {};
  ShaderFxData_Runtime runtime;
};

struct ShadowShaderFxData {
  ShaderFxData shaderfx;
  struct Object *object = nullptr;
  int offset[2] = {};
  /** Flags. */
  eShadowShaderFx_Flag flag = {};
  float shadow_rgba[4] = {};
  float amplitude = 0;
  float period = 0;
  float phase = 0;
  int orientation = 0;
  float scale[2] = {};
  float rotation = 0;
  int blur[2] = {};
  int samples = 0;
  char _pad[4] = {};
  ShaderFxData_Runtime runtime;
};

struct SwirlShaderFxData {
  ShaderFxData shaderfx;
  struct Object *object = nullptr;
  /** Flags. */
  eSwirlShaderFx_Flag flag = {};
  int radius = 0;
  float angle = 0;
  /** Not visible in rna. */
  int transparent = 0;
  ShaderFxData_Runtime runtime;
};

struct WaveShaderFxData {
  ShaderFxData shaderfx;
  float amplitude = 0;
  float period = 0;
  float phase = 0;
  int orientation = 0;
  /** Flags. */
  int flag = 0;
  char _pad[4] = {};
  ShaderFxData_Runtime runtime;
};

}  // namespace blender
