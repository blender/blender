/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_shader_fx_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_SHADERFX_TYPES_H__
#define __DNA_SHADERFX_TYPES_H__

#include "DNA_defs.h"
#include "DNA_listBase.h"

struct DRWShadingGroup;

/* WARNING ALERT! TYPEDEF VALUES ARE WRITTEN IN FILES! SO DO NOT CHANGE!
 * (ONLY ADD NEW ITEMS AT THE END)
 */

typedef enum ShaderFxType {
	eShaderFxType_None      = 0,
	eShaderFxType_Blur      = 1,
	eShaderFxType_Flip      = 2,
	eShaderFxType_Light     = 3,
	eShaderFxType_Pixel     = 4,
	eShaderFxType_Swirl     = 5,
	eShaderFxType_Wave      = 6,
	eShaderFxType_Rim       = 7,
	eShaderFxType_Colorize  = 8,
	NUM_SHADER_FX_TYPES
} ShaderFxType;

typedef enum ShaderFxMode {
	eShaderFxMode_Realtime          = (1 << 0),
	eShaderFxMode_Render            = (1 << 1),
	eShaderFxMode_Editmode          = (1 << 2),
	eShaderFxMode_Expanded          = (1 << 3),
} ShaderFxMode;

typedef enum {
	/* This fx has been inserted in local override, and hence can be fully edited. */
	eShaderFxFlag_StaticOverride_Local = (1 << 0),
} ShaderFxFlag;

typedef struct ShaderFxData {
	struct ShaderFxData *next, *prev;

	int type, mode;
	int stackindex;
	short flag;
	short pad;
	char name[64];  /* MAX_NAME */

	char *error;
} ShaderFxData;

/* Runtime temp data */
typedef struct ShaderFxData_runtime {
	struct DRWShadingGroup *fx_sh;
	struct DRWShadingGroup *fx_sh_b;
	struct DRWShadingGroup *fx_sh_c;
} ShaderFxData_runtime;

typedef struct BlurShaderFxData {
	ShaderFxData shaderfx;
	int radius[2];
	int flag;                    /* flags */
	int samples;                 /* number of samples */
	float coc;                   /* circle of confusion */
	int blur[2];                 /* not visible in rna */
	char pad[4];
	ShaderFxData_runtime runtime;
} BlurShaderFxData;

typedef enum eBlurShaderFx_Flag {
	FX_BLUR_DOF_MODE = (1 << 0)
} eBlurShaderFx_Flag;

typedef struct ColorizeShaderFxData {
	ShaderFxData shaderfx;
	int   mode;
	float low_color[4];
	float high_color[4];
	float factor;
	int flag;                    /* flags */
	char pad[4];
	ShaderFxData_runtime runtime;
} ColorizeShaderFxData;

typedef enum ColorizeShaderFxModes {
	eShaderFxColorizeMode_GrayScale = 0,
	eShaderFxColorizeMode_Sepia = 1,
	eShaderFxColorizeMode_BiTone = 2,
	eShaderFxColorizeMode_Custom = 3,
	eShaderFxColorizeMode_Transparent = 4,
} ColorizeShaderFxModes;

typedef struct FlipShaderFxData {
	ShaderFxData shaderfx;
	int flag;                    /* flags */
	int flipmode;  /* internal, not visible in rna */
	ShaderFxData_runtime runtime;
} FlipShaderFxData;

typedef enum eFlipShaderFx_Flag {
	FX_FLIP_HORIZONTAL = (1 << 0),
	FX_FLIP_VERTICAL = (1 << 1),
} eFlipShaderFx_Flag;

typedef struct LightShaderFxData {
	ShaderFxData shaderfx;
	struct Object *object;
	int flag;                    /* flags */
	float energy;
	float ambient;
	float loc[4]; /* internal, not visible in rna */ 
	char pad[4];
	ShaderFxData_runtime runtime;
} LightShaderFxData;

typedef struct PixelShaderFxData {
	ShaderFxData shaderfx;
	int size[3];                 /* last element used for shader only */
	int flag;                    /* flags */
	float rgba[4];
	ShaderFxData_runtime runtime;
} PixelShaderFxData;

typedef enum ePixelShaderFx_Flag {
	FX_PIXEL_USE_LINES = (1 << 0),
} ePixelShaderFx_Flag;

typedef struct RimShaderFxData {
	ShaderFxData shaderfx;
	int offset[2];
	int flag;                    /* flags */
	float rim_rgb[3];
	float mask_rgb[3];
	int   mode;
	int   blur[2];
	int   samples;
	char pad[4];
	ShaderFxData_runtime runtime;
} RimShaderFxData;

typedef enum RimShaderFxModes {
	eShaderFxRimMode_Normal = 0,
	eShaderFxRimMode_Overlay = 1,
	eShaderFxRimMode_Add = 2,
	eShaderFxRimMode_Subtract = 3,
	eShaderFxRimMode_Multiply = 4,
	eShaderFxRimMode_Divide = 5,
} RimShaderFxModes;

typedef struct SwirlShaderFxData {
	ShaderFxData shaderfx;
	struct Object *object;
	int flag;                    /* flags */
	int radius;
	float angle;
	int transparent;  /* not visible in rna */
	ShaderFxData_runtime runtime;
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
	int flag;                    /* flags */
	char pad[4];
	ShaderFxData_runtime runtime;
} WaveShaderFxData;
#endif  /* __DNA_SHADERFX_TYPES_H__ */
