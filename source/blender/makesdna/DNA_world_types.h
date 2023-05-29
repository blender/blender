/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

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
struct Ipo;
struct LightgroupMembership;
struct bNodeTree;

#ifndef MAX_MTEX
#  define MAX_MTEX 18
#endif

/**
 * World defines general modeling data such as a background fill,
 * gravity, color model etc. It mixes rendering data and modeling data. */
typedef struct World {
  DNA_DEFINE_CXX_METHODS(World)

  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt;
  /* runtime (must be immediately after id for utilities to use it). */
  DrawDataList drawdata;

  char _pad0[4];
  short texact, mistype;

  float horr, horg, horb;

  /**
   * Exposure is a multiplication factor. Unused now, but maybe back later.
   * Kept in to be upward compatible.
   */
  float exposure, exp, range;

  /**
   * Some world modes
   * bit 0: Do mist
   */
  short mode;
  char _pad2[6];

  float misi, miststa, mistdist, misthi;

  /** Ambient occlusion. */
  float aodist, aoenergy;

  /** Assorted settings. */
  short flag;
  char _pad3[6];

  /** Old animation system, deprecated for 2.5. */
  struct Ipo *ipo DNA_DEPRECATED;
  short pr_texture, use_nodes;
  char _pad[4];

  /* previews */
  struct PreviewImage *preview;

  /* nodes */
  struct bNodeTree *nodetree;

  /** Light-group membership information. */
  struct LightgroupMembership *lightgroup;

  /** Runtime. */
  ListBase gpumaterial;
} World;

/* **************** WORLD ********************* */

/* mode */
#define WO_MIST (1 << 0)
#define WO_MODE_UNUSED_1 (1 << 1) /* cleared */
#define WO_MODE_UNUSED_2 (1 << 2) /* cleared */
#define WO_MODE_UNUSED_3 (1 << 3) /* cleared */
#define WO_MODE_UNUSED_4 (1 << 4) /* cleared */
#define WO_MODE_UNUSED_5 (1 << 5) /* cleared */
#define WO_AMB_OCC (1 << 6)
#define WO_MODE_UNUSED_7 (1 << 7) /* cleared */

enum {
  WO_MIST_QUADRATIC = 0,
  WO_MIST_LINEAR = 1,
  WO_MIST_INVERSE_QUADRATIC = 2,
};

/* flag */
#define WO_DS_EXPAND (1 << 0)
/* NOTE: this must have the same value as MA_DS_SHOW_TEXS,
 * otherwise anim-editors will not read correctly
 */
#define WO_DS_SHOW_TEXS (1 << 2)

#ifdef __cplusplus
}
#endif
