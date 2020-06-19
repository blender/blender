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

#ifndef __DNA_SHADER_FX_TYPES_H__
#define __DNA_SHADER_FX_TYPES_H__

#include "DNA_defs.h"
#include "DNA_listBase.h"

struct DRWShadingGroup;

/* WARNING ALERT! TYPEDEF VALUES ARE WRITTEN IN FILES! SO DO NOT CHANGE!
 * (ONLY ADD NEW ITEMS AT THE END)
 */

typedef enum ShaderFxType {
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
} ShaderFxType;

typedef enum ShaderFxMode {
  eShaderFxMode_Realtime = (1 << 0),
  eShaderFxMode_Render = (1 << 1),
  eShaderFxMode_Editmode = (1 << 2),
  eShaderFxMode_Expanded_DEPRECATED = (1 << 3),
} ShaderFxMode;

typedef enum {
  /* This fx has been inserted in local override, and hence can be fully edited. */
  eShaderFxFlag_OverrideLibrary_Local = (1 << 0),
} ShaderFxFlag;

typedef struct ShaderFxData {
  struct ShaderFxData *next, *prev;

  int type, mode;
  int stackindex;
  short flag;
  /* Expansion for shader effect panels and subpanels. */
  short ui_expand_flag;
  /** MAX_NAME. */
  char name[64];

  char *error;
} ShaderFxData;

/* Runtime temp data */
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
  /** Rotation of blur effect.  */
  float rotation;
  char _pad[4];

  ShaderFxData_Runtime runtime;
} BlurShaderFxData;

typedef enum eBlurShaderFx_Flag {
  FX_BLUR_DOF_MODE = (1 << 0),
} eBlurShaderFx_Flag;

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

typedef enum ColorizeShaderFxModes {
  eShaderFxColorizeMode_GrayScale = 0,
  eShaderFxColorizeMode_Sepia = 1,
  eShaderFxColorizeMode_Duotone = 2,
  eShaderFxColorizeMode_Custom = 3,
  eShaderFxColorizeMode_Transparent = 4,
} ColorizeShaderFxModes;

typedef struct FlipShaderFxData {
  ShaderFxData shaderfx;
  /** Flags. */
  int flag;
  /** Internal, not visible in rna. */
  int flipmode;
  ShaderFxData_Runtime runtime;
} FlipShaderFxData;

typedef enum eFlipShaderFx_Flag {
  FX_FLIP_HORIZONTAL = (1 << 0),
  FX_FLIP_VERTICAL = (1 << 1),
} eFlipShaderFx_Flag;

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
  /** Rotation of effect.  */
  float rotation;
  /** Blend modes. */
  int blend_mode;
  char _pad[4];

  ShaderFxData_Runtime runtime;
} GlowShaderFxData;

typedef enum GlowShaderFxModes {
  eShaderFxGlowMode_Luminance = 0,
  eShaderFxGlowMode_Color = 1,
} GlowShaderFxModes;

typedef enum eGlowShaderFx_Flag {
  FX_GLOW_USE_ALPHA = (1 << 0),
} eGlowShaderFx_Flag;

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

typedef enum RimShaderFxModes {
  eShaderFxRimMode_Normal = 0,
  eShaderFxRimMode_Overlay = 1,
  eShaderFxRimMode_Add = 2,
  eShaderFxRimMode_Subtract = 3,
  eShaderFxRimMode_Multiply = 4,
  eShaderFxRimMode_Divide = 5,
} RimShaderFxModes;

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

typedef enum eShadowShaderFx_Flag {
  FX_SHADOW_USE_OBJECT = (1 << 0),
  FX_SHADOW_USE_WAVE = (1 << 1),
} eShadowShaderFx_Flag;

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

typedef enum eSwirlShaderFx_Flag {
  FX_SWIRL_MAKE_TRANSPARENT = (1 << 0),
} eSwirlShaderFx_Flag;

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
#endif /* __DNA_SHADER_FX_TYPES_H__ */
