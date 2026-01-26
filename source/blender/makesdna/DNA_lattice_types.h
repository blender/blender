/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_defs.h"
#include "DNA_key_types.h"

namespace blender {

struct AnimData;
struct BPoint;
struct Key;
struct MDeformVert;

namespace draw {
struct LatticeBatchCache;
}

/** #Lattice::flag */
enum {
  LT_GRID = 1 << 0,
  LT_OUTSIDE = 1 << 1,

  LT_DS_EXPAND = 1 << 2,
};

#define LT_ACTBP_NONE -1

#
#
struct EditLatt {
  DNA_DEFINE_CXX_METHODS(EditLatt)

  struct Lattice *latt = nullptr;

  int shapenr = 0;

  /**
   * ID data is older than edit-mode data.
   * Set #Main.is_memfile_undo_flush_needed when enabling.
   */
  char needs_flush_to_id = 0;
};

struct Lattice {
#ifdef __cplusplus
  DNA_DEFINE_CXX_METHODS(Lattice)
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_LT;
#endif

  ID id;
  struct AnimData *adt = nullptr;

  short pntsu = 0, pntsv = 0, pntsw = 0, flag = LT_GRID;
  short opntsu = 0, opntsv = 0, opntsw = 0;
  char _pad2[3] = {};
  /* KeyInterpolationType */
  char typeu = KEY_BSPLINE, typev = KEY_BSPLINE, typew = KEY_BSPLINE;
  /** Active element index, unset with LT_ACTBP_NONE. */
  int actbp = LT_ACTBP_NONE;

  float fu = 0, fv = 0, fw = 0, du = 0, dv = 0, dw = 0;

  struct BPoint *def = nullptr;

  struct Key *key = nullptr;

  struct MDeformVert *dvert = nullptr;
  /** Multiply the influence. */
  char vgroup[/*MAX_VGROUP_NAME*/ 64] = "";
  /** List of bDeformGroup names and flag only. */
  ListBaseT<struct bDeformGroup> vertex_group_names = {nullptr, nullptr};
  int vertex_group_active_index = 0;

  char _pad0[4] = {};

  struct EditLatt *editlatt = nullptr;
  draw::LatticeBatchCache *batch_cache = nullptr;
};

}  // namespace blender
