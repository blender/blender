/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

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

/**
 * The struct that holds the data for an individual Shape Key. Depending on which object owns the
 * `Key`, the contained data type can vary (see `void *data;`).
 */
typedef struct KeyBlock {
  struct KeyBlock *next, *prev;

  /**
   * A point in time used in case of `(Key::type == KEY_NORMAL)` only,
   * for historic reasons this is relative to (Key::ctime / 100),
   * so this value increments by 0.1f per frame.
   */
  float pos;
  /** Influence (typically [0 - 1] but can be more), `(Key::type == KEY_RELATIVE)` only. */
  float curval;

  /** Interpolation type. Used for `(Key::type == KEY_NORMAL)` only (KeyInterpolationType). */
  short type;
  char _pad1[2];

  /** `relative == 0` means first key is reference, otherwise the index of Key::blocks. */
  short relative;
  /* KeyBlockFlag */
  short flag;

  /** Total number of items in the keyblock (compare with mesh/curve verts to check we match). */
  int totelem;
  /** For meshes only, match the unique number with the customdata layer. */
  int uid;

  /** Array of shape key values, size is `(Key::elemsize * KeyBlock->totelem)`.
   * E.g. meshes use float3. */
  void *data;
  /** Unique name, user assigned. */
  char name[/*MAX_NAME*/ 64];
  /** Optional vertex group, array gets allocated into 'weights' when set. */
  char vgroup[/*MAX_VGROUP_NAME*/ 64];

  /** Ranges, for RNA and UI only to clamp 'curval'. */
  float slidermin;
  float slidermax;

} KeyBlock;

typedef struct Key {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_KE;
#endif

  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt;

  /**
   * Commonly called 'Basis', `(Key::type == KEY_RELATIVE)` only.
   * Looks like this is _always_ 'key->block.first',
   * perhaps later on it could be defined as some other KeyBlock - campbell.
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

  /** A list of KeyBlock's. */
  ListBase block;

  ID *from;

  /** (totkey == BLI_listbase_count(&key->block)). */
  int totkey;
  /* ShapekeyContainerFlag */
  short flag;
  /** Absolute or relative shape key (ShapekeyContainerType). */
  char type;
  char _pad2;

  /** Only used when (Key::type == KEY_NORMAL), this value is used as a time slider,
   * rather than using the scene's time, this value can be animated to give greater control */
  float ctime;

  /**
   * Can never be 0, this is used for detecting old data.
   * current free UID for key-blocks.
   */
  int uidgen;
} Key;

/* **************** KEY ********************* */

/* Key::type: KeyBlocks are interpreted as... */
typedef enum ShapekeyContainerType {
  /* Sequential positions over time (using KeyBlock::pos and Key::ctime) */
  KEY_NORMAL = 0,

  /* States to blend between (default) */
  KEY_RELATIVE = 1,
} ShapekeyContainerType;

/* Key::flag */
typedef enum ShapekeyContainerFlag {
  KEY_DS_EXPAND = 1,
} ShapekeyContainerFlag;

/* The obvious name would be `KeyBlockType` but this enum is actually used in places outside of
 * Shape Keys (NURBS, particles, etc.). */
typedef enum KeyInterpolationType {
  KEY_LINEAR = 0,
  KEY_CARDINAL = 1,
  KEY_BSPLINE = 2,
  KEY_CATMULL_ROM = 3,
} KeyInterpolationType;

typedef enum KeyBlockFlag {
  KEYBLOCK_MUTE = (1 << 0),
  KEYBLOCK_SEL = (1 << 1),
  KEYBLOCK_LOCKED = (1 << 2),
  KEYBLOCK_LOCKED_SHAPE = (1 << 3),
} KeyBlockFlag;

#define KEYELEM_FLOAT_LEN_COORD 3

/* Curve key data layout constants */
#define KEYELEM_ELEM_SIZE_CURVE 3

#define KEYELEM_ELEM_LEN_BPOINT 2
#define KEYELEM_FLOAT_LEN_BPOINT (KEYELEM_ELEM_LEN_BPOINT * KEYELEM_ELEM_SIZE_CURVE)

#define KEYELEM_ELEM_LEN_BEZTRIPLE 4
#define KEYELEM_FLOAT_LEN_BEZTRIPLE (KEYELEM_ELEM_LEN_BEZTRIPLE * KEYELEM_ELEM_SIZE_CURVE)
