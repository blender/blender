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
 * \ingroup DNA
 */

#pragma once

#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Ipo;
struct Object;
struct bAction;

/* simple uniform modifier structure, assumed it can hold all type info */
typedef struct bActionModifier {
  struct bActionModifier *next, *prev;
  short type, flag;
  char channel[32];

  /* noise modifier */
  float noisesize, turbul;
  short channels;

  /* path deform modifier */
  short no_rot_axis;
  struct Object *ob;
} bActionModifier;

/* NLA-Modifier Types (UNUSED) */
// #define ACTSTRIP_MOD_DEFORM      0
// #define ACTSTRIP_MOD_NOISE       1

typedef struct bActionStrip {
  struct bActionStrip *next, *prev;
  short flag, mode;
  /** Axis 0=x, 1=y, 2=z. */
  short stride_axis;
  /** Current modifier for buttons. */
  short curmod;

  /** Blending ipo - was used for some old NAN era experiments. Non-functional currently. */
  struct Ipo *ipo;
  /** The action referenced by this strip. */
  struct bAction *act;
  /** For groups, the actual object being nla'ed. */
  struct Object *object;
  /** The range of frames covered by this strip. */
  float start, end;
  /** The range of frames taken from the action. */
  float actstart, actend;
  /** Offset within action, for cycles and striding. */
  float actoffs;
  /** The stridelength (considered when flag & ACT_USESTRIDE). */
  float stridelen;
  /** The number of times to repeat the action range. */
  float repeat;
  /** The amount the action range is scaled by. */
  float scale;

  /** The number of frames on either end of the strip's length to fade in/out. */
  float blendin, blendout;

  /** Instead of stridelen, it uses an action channel. */
  char stridechannel[32];
  /** If repeat, use this bone/channel for defining offset. */
  char offs_bone[32];

  /** Modifier stack. */
  ListBase modifiers;
} bActionStrip;

/* strip->mode (these defines aren't really used, but are here for reference) */
#define ACTSTRIPMODE_BLEND 0
#define ACTSTRIPMODE_ADD 1

/* strip->flag */
typedef enum eActStrip_Flag {
  ACTSTRIP_SELECT = (1 << 0),
  ACTSTRIP_USESTRIDE = (1 << 1),
  /* Not implemented. Is not used anywhere */
  /* ACTSTRIP_BLENDTONEXT = (1 << 2), */ /* UNUSED */
  ACTSTRIP_HOLDLASTFRAME = (1 << 3),
  ACTSTRIP_ACTIVE = (1 << 4),
  ACTSTRIP_LOCK_ACTION = (1 << 5),
  ACTSTRIP_MUTE = (1 << 6),
  /* This has yet to be implemented. To indicate that a strip should be played backwards */
  ACTSTRIP_REVERSE = (1 << 7),
  ACTSTRIP_AUTO_BLENDS = (1 << 11),
} eActStrip_Flag;

#ifdef __cplusplus
}
#endif
