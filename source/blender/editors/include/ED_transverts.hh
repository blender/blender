/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

struct Object;
struct bContext;

typedef struct TransVert {
  float *loc;
  float oldloc[3], maploc[3];
  float normal[3];
  int flag;
} TransVert;

typedef struct TransVertStore {
  struct TransVert *transverts;
  int transverts_tot;
  int mode;
} TransVertStore;

/**
 * \param obedit: When `mode` has the #TM_CALC_MAPLOC flag set, `obedit` must be evaluated,
 * to access evaluated vertices.
 */
void ED_transverts_create_from_obedit(TransVertStore *tvs, const struct Object *obedit, int mode);
void ED_transverts_update_obedit(TransVertStore *tvs, struct Object *obedit);
void ED_transverts_free(TransVertStore *tvs);
bool ED_transverts_check_obedit(const struct Object *obedit);
bool ED_transverts_poll(struct bContext *C);

/* currently only used for bmesh index values */
enum {
  /** Tag to make trans verts. */
  TM_INDEX_ON = 1,
  /** Don't make verts. */
  TM_INDEX_OFF = 0,
  /** Don't make verts (when the index values point to trans-verts). */
  TM_INDEX_SKIP = -1,
};

/* mode flags: */
enum {
  /** all joints (for bones only) */
  TM_ALL_JOINTS = (1 << 0),
  /** skip handles when control point is selected (for curves only) */
  TM_SKIP_HANDLES = (1 << 1),
  /** fill in normals when available */
  TM_CALC_NORMALS = (1 << 2),
  /** Calculates #TransVert.maploc where possible. */
  TM_CALC_MAPLOC = (1 << 2),
};

enum {
  /* SELECT == (1 << 0) */
  /** Calculated when #TM_CALC_MAPLOC is set. */
  TX_VERT_USE_MAPLOC = (1 << 1),
  /** Calculated when #TM_CALC_NORMALS is set, avoid nonzero check. */
  TX_VERT_USE_NORMAL = (1 << 2),
};
