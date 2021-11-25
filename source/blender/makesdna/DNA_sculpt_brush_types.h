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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup DNA
 *
 * Structs used for the sculpt brush system
 */
#pragma once

#include "DNA_color_types.h"
#include "DNA_listBase.h"

struct GHash;

typedef struct BrushMapping {
  char name[64];

  /*reference to a cached curve, see BKE_curvemapping_cache*/
  CurveMapping *curve;
  float factor;
  short blendmode;
  short input_channel;
  int flag, type;

  float min, max;
  float premultiply;  // premultiply input data
  int mapfunc;
  float func_cutoff;
  char inherit_mode, _pad[3];
} BrushMapping;

typedef struct BrushCurve {
  CurveMapping *curve;
  int preset;  // see eBrushCurvePreset, this differs from the one in BrushMappingDef
  char preset_slope_negative;
  char _pad[3];
} BrushCurve;

typedef struct BrushChannel {
  struct BrushChannel *next, *prev;

  char idname[64];
  char name[64];
  char *category;  // if NULL, def->category will be used

  struct BrushChannelType *def;

  float fvalue;
  int ivalue;
  float vector[4];
  BrushCurve curve;

  BrushMapping mappings[7];  // should always be BRUSH_MAPPING_MAX

  short type, ui_order;
  int flag;
} BrushChannel;

typedef struct BrushChannelSet {
  ListBase channels;
  int totchannel, _pad[1];
  struct GHash *namemap;
} BrushChannelSet;

#define BRUSH_CHANNEL_MAX_IDNAME sizeof(((BrushChannel){0}).idname)

/* BrushMapping->flag */
enum {
  BRUSH_MAPPING_ENABLED = 1 << 0,
  BRUSH_MAPPING_INVERT = 1 << 1,
  BRUSH_MAPPING_UI_EXPANDED = 1 << 2,
};

/* BrushMapping->inherit_mode */
enum {
  /* never inherit */
  BRUSH_MAPPING_INHERIT_NEVER,
  /* always inherit */
  BRUSH_MAPPING_INHERIT_ALWAYS,
  /* use channel's inheritance mode */
  BRUSH_MAPPING_INHERIT_CHANNEL
};

/* BrushMapping->mapfunc */
typedef enum {
  BRUSH_MAPFUNC_NONE,
  BRUSH_MAPFUNC_SAW,
  BRUSH_MAPFUNC_TENT,
  BRUSH_MAPFUNC_COS,
  BRUSH_MAPFUNC_CUTOFF,
  BRUSH_MAPFUNC_SQUARE,
} BrushMappingFunc;

// mapping types
typedef enum {
  BRUSH_MAPPING_PRESSURE = 0,
  BRUSH_MAPPING_XTILT = 1,
  BRUSH_MAPPING_YTILT = 2,
  BRUSH_MAPPING_ANGLE = 3,
  BRUSH_MAPPING_SPEED = 4,
  BRUSH_MAPPING_RANDOM = 5,
  BRUSH_MAPPING_STROKE_T = 6,
  BRUSH_MAPPING_MAX = 7  // see BrushChannel.mappings
} BrushMappingType;

#ifndef __GNUC__
static_assert(offsetof(BrushChannel, type) - offsetof(BrushChannel, mappings) ==
                  sizeof(BrushMapping) * BRUSH_MAPPING_MAX,
              "BrushChannel.mappings must == BRUSH_MAPPING_MAX");
#endif

// BrushChannel->flag
enum {
  BRUSH_CHANNEL_INHERIT = 1 << 0,
  BRUSH_CHANNEL_INHERIT_IF_UNSET = 1 << 1,
  BRUSH_CHANNEL_NO_MAPPINGS = 1 << 2,
  BRUSH_CHANNEL_UI_EXPANDED = 1 << 3,
  BRUSH_CHANNEL_APPLY_MAPPING_TO_ALPHA = 1 << 4,
  BRUSH_CHANNEL_SHOW_IN_WORKSPACE = 1 << 6,
  BRUSH_CHANNEL_SHOW_IN_HEADER = 1 << 7,
  BRUSH_CHANNEL_SHOW_IN_CONTEXT_MENU = 1 << 8,
};

// BrushChannelType->type
enum {
  BRUSH_CHANNEL_TYPE_FLOAT = 1 << 0,
  BRUSH_CHANNEL_TYPE_INT = 1 << 1,
  BRUSH_CHANNEL_TYPE_ENUM = 1 << 2,
  BRUSH_CHANNEL_TYPE_BITMASK = 1 << 3,
  BRUSH_CHANNEL_TYPE_BOOL = 1 << 4,
  BRUSH_CHANNEL_TYPE_VEC3 = 1 << 5,
  BRUSH_CHANNEL_TYPE_VEC4 = 1 << 6,
  BRUSH_CHANNEL_TYPE_CURVE = 1 << 7
};

/* clang-format off */
enum {
  BRUSH_CHANNEL_NONE,
  BRUSH_CHANNEL_COLOR,
  BRUSH_CHANNEL_FACTOR,
  BRUSH_CHANNEL_PERCENT,
  BRUSH_CHANNEL_PIXEL
};
/* clang-format on */
