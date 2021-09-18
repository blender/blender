#pragma once

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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 * \brief New brush engine for sculpt
 */
#ifdef __cplusplus
extern "C" {
#endif

#include "RNA_types.h"

/*
The new brush engine is based on command lists.  These lists
will eventually be created by a node editor.

Key is the concept of BrushChannels.  A brush channel is
a logical parameter with a type, input settings (e.g. pen),
a falloff curve, etc.

Brush channels have a concept of inheritance.  There is a
BrushChannelSet (collection of channels) in Sculpt,
in Brush, and in BrushCommand.  Inheritence behavior
is controller via BrushChannel->flag.

This should completely replace UnifiedPaintSettings.
*/
struct BrushChannel;

#include "BLO_read_write.h"
#include "DNA_sculpt_brush_types.h"

typedef struct BrushMappingDef {
  int curve;
  bool enabled;
  bool inv;
  float min, max;
  int blendmode;
} BrushMappingDef;

typedef struct BrushMappingPreset {
  // must match order of BRUSH_MAPPING_XXX enums
  struct BrushMappingDef pressure, xtilt, ytilt, angle, speed;
} BrushMappingPreset;

#define MAX_BRUSH_ENUM_DEF 32

typedef struct BrushEnumDef {
  EnumPropertyItem items[MAX_BRUSH_ENUM_DEF];
} BrushEnumDef;

typedef struct BrushChannelType {
  char name[32], idname[32];
  float min, max, soft_min, soft_max;
  BrushMappingPreset mappings;

  int type, flag;
  int ivalue;
  float fvalue;
  BrushEnumDef enumdef;  // if an enum type
} BrushChannelType;

typedef struct BrushCommand {
  int tool;
  struct BrushChannelSet *params;
  struct BrushChannelSet *params_final;
  int totparam;
} BrushCommand;

typedef struct BrushCommandList {
  BrushCommand *commands;
  int totcommand;
} BrushCommandList;

void BKE_brush_channel_free(BrushChannel *ch);
void BKE_brush_channel_copy_data(BrushChannel *dst, BrushChannel *src);
void BKE_brush_channel_init(BrushChannel *ch, BrushChannelType *def);

BrushChannelSet *BKE_brush_channelset_create();
BrushChannelSet *BKE_brush_channelset_copy(BrushChannelSet *src);
void BKE_brush_channelset_free(BrushChannelSet *chset);

// makes a copy of ch
void BKE_brush_channelset_add(BrushChannelSet *chset, BrushChannel *ch);

// checks is a channel with existing->idname exists; if not a copy of existing is made and inserted
void BKE_brush_channelset_ensure_existing(BrushChannelSet *chset, BrushChannel *existing);

BrushChannel *BKE_brush_channelset_lookup(BrushChannelSet *chset, const char *idname);

bool BKE_brush_channelset_has(BrushChannelSet *chset, const char *idname);

void BKE_brush_channelset_add_builtin(BrushChannelSet *chset, const char *idname);
bool BKE_brush_channelset_ensure_builtin(BrushChannelSet *chset, const char *idname);

void BKE_brush_channelset_merge(BrushChannelSet *dst,
                                BrushChannelSet *child,
                                BrushChannelSet *parent);

void BKE_brush_resolve_channels(struct Brush *brush, struct Sculpt *sd);
int BKE_brush_channel_get_int(BrushChannelSet *chset, char *idname);
float BKE_brush_channel_get_float(BrushChannelSet *chset, char *idname);
float BKE_brush_channel_set_float(BrushChannelSet *chset, char *idname, float val);
void BKE_brush_init_toolsettings(struct Sculpt *sd);
void BKE_brush_builtin_create(struct Brush *brush, int tool);
BrushCommandList *BKE_brush_commandlist_create();
void BKE_brush_commandlist_free(BrushCommandList *cl);
BrushCommand *BKE_brush_commandlist_add(BrushCommandList *cl);
BrushCommand *BKE_brush_command_init(BrushCommand *command, int tool);
void BKE_builtin_commandlist_create(BrushChannelSet *chset, BrushCommandList *cl, int tool);
void BKE_brush_channelset_read(BlendDataReader *reader, BrushChannelSet *cset);
void BKE_brush_channelset_write(BlendWriter *writer, BrushChannelSet *cset);
void BKE_brush_mapping_copy_data(BrushMapping *dst, BrushMapping *src);
const char *BKE_brush_mapping_type_to_str(BrushMappingType mapping);
const char *BKE_brush_mapping_type_to_typename(BrushMappingType mapping);

#ifdef __cplusplus
}
#endif
