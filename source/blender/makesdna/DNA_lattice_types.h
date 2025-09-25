/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_defs.h"

struct AnimData;
struct BPoint;
struct Key;
struct MDeformVert;

#
#
typedef struct EditLatt {
  DNA_DEFINE_CXX_METHODS(EditLatt)

  struct Lattice *latt;

  int shapenr;

  /**
   * ID data is older than edit-mode data.
   * Set #Main.is_memfile_undo_flush_needed when enabling.
   */
  char needs_flush_to_id;
} EditLatt;

typedef struct Lattice {
#ifdef __cplusplus
  DNA_DEFINE_CXX_METHODS(Lattice)
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_LT;
#endif

  ID id;
  struct AnimData *adt;

  short pntsu, pntsv, pntsw, flag;
  short opntsu, opntsv, opntsw;
  char _pad2[3];
  /* KeyInterpolationType */
  char typeu, typev, typew;
  /** Active element index, unset with LT_ACTBP_NONE. */
  int actbp;

  float fu, fv, fw, du, dv, dw;

  struct BPoint *def;

  struct Key *key;

  struct MDeformVert *dvert;
  /** Multiply the influence. */
  char vgroup[/*MAX_VGROUP_NAME*/ 64];
  /** List of bDeformGroup names and flag only. */
  ListBase vertex_group_names;
  int vertex_group_active_index;

  char _pad0[4];

  struct EditLatt *editlatt;
  void *batch_cache;
} Lattice;

/* ***************** LATTICE ********************* */

/** #Lattice::flag */
enum {
  LT_GRID = 1 << 0,
  LT_OUTSIDE = 1 << 1,

  LT_DS_EXPAND = 1 << 2,
};

#define LT_ACTBP_NONE -1
