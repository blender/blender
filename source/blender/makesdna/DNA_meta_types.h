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

#include "DNA_ID.h"
#include "DNA_defs.h"
#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AnimData;
struct BoundBox;
struct Ipo;
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
  /** Old, only used for backwards compat. use dimensions now. */
  float len;

  /** Matrix and inverted matrix. */
  float *mat, *imat;
} MetaElem;

typedef struct MetaBall {
  ID id;
  struct AnimData *adt;

  ListBase elems;
  ListBase disp;
  /** Not saved in files, note we use pointer for editmode check. */
  ListBase *editelems;
  /** Old animation system, deprecated for 2.5. */
  struct Ipo *ipo DNA_DEPRECATED;

  /* material of the mother ball will define the material used of all others */
  struct Material **mat;

  /** Flag is enum for updates, flag2 is bitflags for settings. */
  char flag, flag2;
  short totcol;
  /** Used to store MB_AUTOSPACE. */
  short texflag;
  char _pad[1];

  /**
   * ID data is older than edit-mode data (TODO: move to edit-mode struct).
   * Set #Main.is_memfile_undo_flush_needed when enabling.
   */
  char needs_flush_to_id;

  /* texture space, copied as one block in editobject.c */
  float loc[3];
  float size[3];
  float rot[3];

  /** Display and render res. */
  float wiresize, rendersize;

  /* bias elements to have an offset volume.
   * mother ball changes will effect other objects thresholds,
   * but these may also have their own thresh as an offset */
  float thresh;

  /* used in editmode */
  // ListBase edit_elems;
  MetaElem *lastelem;

  void *batch_cache;
} MetaBall;

/* **************** METABALL ********************* */

/* texflag */
#define MB_AUTOSPACE 1

/* mb->flag */
#define MB_UPDATE_ALWAYS 0
#define MB_UPDATE_HALFRES 1
#define MB_UPDATE_FAST 2
#define MB_UPDATE_NEVER 3

/* mb->flag2 */
#define MB_DS_EXPAND (1 << 0)

/* ml->type */
#define MB_BALL 0
#define MB_TUBEX 1 /* deprecated. */
#define MB_TUBEY 2 /* deprecated. */
#define MB_TUBEZ 3 /* deprecated. */
#define MB_TUBE 4
#define MB_PLANE 5
#define MB_ELIPSOID 6
#define MB_CUBE 7

#define MB_TYPE_SIZE_SQUARED(type) (type == MB_ELIPSOID)

/* ml->flag */
#define MB_NEGATIVE 2
#define MB_HIDE 8
#define MB_SCALE_RAD 16

#ifdef __cplusplus
}
#endif
