/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_listBase.h"

struct Object;
struct bAction;

/** Simple uniform modifier structure, assumed it can hold all type info. */
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

// /* NLA-Modifier Types (UNUSED) */
// enum {
// 	ACTSTRIP_MOD_DEFORM = 0,
// 	ACTSTRIP_MOD_NOISE = 1,
// };

typedef struct bActionStrip {
  struct bActionStrip *next, *prev;
  short flag, mode;
  /** Axis 0=x, 1=y, 2=z. */
  short stride_axis;
  /** Current modifier for buttons. */
  short curmod;

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
  /** The stride-length (considered when flag & ACT_USESTRIDE). */
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

/** #Strip::mode (these defines aren't really used, but are here for reference) */
enum {
  ACTSTRIPMODE_BLEND = 0,
  ACTSTRIPMODE_ADD = 1,
};

/** #bActionStrip.flag */
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
