struct DRWShadingGroup;
namespace blender {

/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

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

struct ShaderFxData {
  struct ShaderFxData *next = nullptr, *prev = nullptr;

  int type = 0, mode = 0;
  char _pad0[4] = {};
  short flag = 0;
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
  int flag = 0;
  /** Number of samples. */
  int samples = 0;
  /** Rotation of blur effect. */
  float rotation = 0;
  char _pad[4] = {};

  ShaderFxData_Runtime runtime;
};

struct ColorizeShaderFxData {
  ShaderFxData shaderfx;
  int mode = 0;
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
  int flag = 0;
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
  int flag = 0;
  int mode = 0;
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
  int flag = 0;
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
  int mode = 0;
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
  int flag = 0;
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
  int flag = 0;
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
