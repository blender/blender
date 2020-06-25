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
#ifndef __DNA_KEY_TYPES_H__
#define __DNA_KEY_TYPES_H__

/** \file
 * \ingroup DNA
 *
 * This file defines structures for Shape-Keys (not animation keyframes),
 * attached to Mesh, Curve and Lattice Data. Even though Key's are ID blocks they
 * aren't intended to be shared between multiple data blocks as with other ID types.
 */

#include "DNA_ID.h"
#include "DNA_defs.h"
#include "DNA_listBase.h"

struct AnimData;
struct Ipo;

typedef struct KeyBlock {
  struct KeyBlock *next, *prev;

  /**
   * point in time   (Key->type == KEY_NORMAL) only,
   * for historic reasons this is relative to (Key->ctime / 100),
   * so this value increments by 0.1f per frame.
   */
  float pos;
  /** influence (typically [0 - 1] but can be more), (Key->type == KEY_RELATIVE) only.*/
  float curval;

  /** interpolation type (Key->type == KEY_NORMAL) only. */
  short type;
  char _pad1[2];

  /** relative == 0 means first key is reference, otherwise the index of Key->blocks */
  short relative;
  short flag;

  /** total number if items in the keyblock (compare with mesh/curve verts to check we match) */
  int totelem;
  /** for meshes only, match the unique number with the customdata layer */
  int uid;

  /** array of shape key values, size is (Key->elemsize * KeyBlock->totelem) */
  void *data;
  /** MAX_NAME (unique name, user assigned) */
  char name[64];
  /** MAX_VGROUP_NAME (optional vertex group), array gets allocated into 'weights' when set */
  char vgroup[64];

  /** ranges, for RNA and UI only to clamp 'curval' */
  float slidermin;
  float slidermax;

} KeyBlock;

typedef struct Key {
  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt;

  /**
   * commonly called 'Basis', (Key->type == KEY_RELATIVE) only.
   * Looks like this is  _always_ 'key->block.first',
   * perhaps later on it could be defined as some other KeyBlock - campbell
   */
  KeyBlock *refkey;

  /**
   * This is not a regular string, although it is \0 terminated
   * this is an array of (element_array_size, element_type) pairs
   * (each one char) used for calculating shape key-blocks. */
  char elemstr[32];
  /** Size of each element in #KeyBlock.data, use for allocation and stride. */
  int elemsize;
  char _pad[4];

  /** list of KeyBlock's */
  ListBase block;
  /** old animation system, deprecated for 2.5 */
  struct Ipo *ipo DNA_DEPRECATED;

  ID *from;

  /** (totkey == BLI_listbase_count(&key->block)) */
  int totkey;
  short flag;
  /** absolute or relative shape key */
  char type;
  char _pad2;

  /** Only used when (Key->type == KEY_NORMAL), this value is used as a time slider,
   * rather then using the scenes time, this value can be animated to give greater control */
  float ctime;

  /**
   * Can never be 0, this is used for detecting old data.
   * current free UID for key-blocks.
   */
  int uidgen;
} Key;

/* **************** KEY ********************* */

/* Key->type: KeyBlocks are interpreted as... */
enum {
  /* Sequential positions over time (using KeyBlock->pos and Key->ctime) */
  KEY_NORMAL = 0,

  /* States to blend between (default) */
  KEY_RELATIVE = 1,
};

/* Key->flag */
enum {
  KEY_DS_EXPAND = 1,
};

/* KeyBlock->type */
enum {
  KEY_LINEAR = 0,
  KEY_CARDINAL = 1,
  KEY_BSPLINE = 2,
  KEY_CATMULL_ROM = 3,
};

/* KeyBlock->flag */
enum {
  KEYBLOCK_MUTE = (1 << 0),
  KEYBLOCK_SEL = (1 << 1),
  KEYBLOCK_LOCKED = (1 << 2),
};

#define KEYELEM_FLOAT_LEN_COORD 3

/* Curve key data layout constants */
#define KEYELEM_ELEM_SIZE_CURVE 3

#define KEYELEM_ELEM_LEN_BPOINT 2
#define KEYELEM_FLOAT_LEN_BPOINT (KEYELEM_ELEM_LEN_BPOINT * KEYELEM_ELEM_SIZE_CURVE)

#define KEYELEM_ELEM_LEN_BEZTRIPLE 4
#define KEYELEM_FLOAT_LEN_BEZTRIPLE (KEYELEM_ELEM_LEN_BEZTRIPLE * KEYELEM_ELEM_SIZE_CURVE)

#endif /* __DNA_KEY_TYPES_H__  */
