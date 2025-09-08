/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_defs.h"
#include "DNA_listBase.h"

struct AnimData;
struct BoundBox;
struct Material;

typedef struct MetaElem {
  struct MetaElem *next, *prev;

  /** Bound Box of MetaElem. */
  struct BoundBox *bb;

  short type, flag;
  char _pad[4];
  /** Position of center of MetaElem. */
  float x, y, z;
  /** Rotation of MetaElem (MUST be kept normalized). */
  float quat[4];
  /** Dimension parameters, used for some types like cubes. */
  float expx;
  float expy;
  float expz;
  /** Radius of the meta element. */
  float rad;
  /** Temp field, used only while processing. */
  float rad2;
  /** Stiffness, how much of the element to fill. */
  float s;
  /** Old, only used for backwards compatibility. use dimensions now. */
  float len;

  /** Matrix and inverted matrix. */
  float *mat, *imat;
} MetaElem;

typedef struct MetaBall {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_MB;
#endif

  ID id;
  struct AnimData *adt;

  ListBase elems;
  /** Not saved in files, note we use pointer for editmode check. */
  ListBase *editelems;

  /* material of the mother ball will define the material used of all others */
  struct Material **mat;

  /** Flag is enum for updates, flag2 is bit-flags for settings. */
  char flag, flag2;
  short totcol;
  /** Used to store #MB_TEXTURE_FLAG_AUTO. */
  char texspace_flag;
  char _pad[2];

  /**
   * ID data is older than edit-mode data (TODO: move to edit-mode struct).
   * Set #Main.is_memfile_undo_flush_needed when enabling.
   */
  char needs_flush_to_id;

  float texspace_location[3];
  float texspace_size[3];

  /** Display and render res. */
  float wiresize, rendersize;

  /* bias elements to have an offset volume.
   * mother ball changes will effect other objects thresholds,
   * but these may also have their own thresh as an offset */
  float thresh;

  char _pad0[4];

  /** The active meta-element (used in edit-mode). */
  MetaElem *lastelem;

} MetaBall;

/* **************** METABALL ********************* */

/** #MetaBall::texspace_flag */
enum {
  MB_TEXSPACE_FLAG_AUTO = 1 << 0,
};

/** #MetaBall::flag */
enum {
  MB_UPDATE_ALWAYS = 0,
  MB_UPDATE_HALFRES = 1,
  MB_UPDATE_FAST = 2,
  MB_UPDATE_NEVER = 3,
};

/** #MetaBall::flag2 */
enum {
  MB_DS_EXPAND = 1 << 0,
};

/** #MetaElem::type */
enum {
  MB_BALL = 0,
  MB_TUBEX = 1, /* Deprecated. */
  MB_TUBEY = 2, /* Deprecated. */
  MB_TUBEZ = 3, /* Deprecated. */
  MB_TUBE = 4,
  MB_PLANE = 5,
  MB_ELIPSOID = 6,
  MB_CUBE = 7,
};

#define MB_TYPE_SIZE_SQUARED(type) ((type) == MB_ELIPSOID)

/** #MetaElem::flag */
enum {
  MB_NEGATIVE = 1 << 1,
  MB_HIDE = 1 << 3,
  MB_SCALE_RAD = 1 << 4,
};
