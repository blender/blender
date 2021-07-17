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

#ifdef __cplusplus
extern "C" {
#endif

struct AnimData;
struct BPoint;
struct Ipo;
struct Key;
struct MDeformVert;

#
#
typedef struct EditLatt {
  struct Lattice *latt;

  int shapenr;

  /**
   * ID data is older than edit-mode data.
   * Set #Main.is_memfile_undo_flush_needed when enabling.
   */
  char needs_flush_to_id;
} EditLatt;

typedef struct Lattice {
  ID id;
  struct AnimData *adt;

  short pntsu, pntsv, pntsw, flag;
  short opntsu, opntsv, opntsw;
  char _pad2[3];
  char typeu, typev, typew;
  /** Active element index, unset with LT_ACTBP_NONE. */
  int actbp;

  float fu, fv, fw, du, dv, dw;

  struct BPoint *def;

  /** Old animation system, deprecated for 2.5. */
  struct Ipo *ipo DNA_DEPRECATED;
  struct Key *key;

  struct MDeformVert *dvert;
  /** Multiply the influence, MAX_VGROUP_NAME. */
  char vgroup[64];
  /** List of bDeformGroup names and flag only. */
  ListBase vertex_group_names;
  int vertex_group_active_index;

  char _pad0[4];

  struct EditLatt *editlatt;
  void *batch_cache;
} Lattice;

/* ***************** LATTICE ********************* */

/* flag */
#define LT_GRID 1
#define LT_OUTSIDE 2

#define LT_DS_EXPAND 4

#define LT_ACTBP_NONE -1

#ifdef __cplusplus
}
#endif
