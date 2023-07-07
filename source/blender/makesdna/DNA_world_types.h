/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  char _pad3[2];

  /** Eevee settings. */
  /**
   * Resolution of the world probe when baked to a texture. Contains `eLightProbeResolution`.
   */
  int probe_resolution;

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

/** #World::mode */
enum {
  WO_MIST = 1 << 0,
  WO_MODE_UNUSED_1 = 1 << 1, /* cleared */
  WO_MODE_UNUSED_2 = 1 << 2, /* cleared */
  WO_MODE_UNUSED_3 = 1 << 3, /* cleared */
  WO_MODE_UNUSED_4 = 1 << 4, /* cleared */
  WO_MODE_UNUSED_5 = 1 << 5, /* cleared */
  WO_AMB_OCC = 1 << 6,
  WO_MODE_UNUSED_7 = 1 << 7, /* cleared */
};

/** #World::mistype */
enum {
  WO_MIST_QUADRATIC = 0,
  WO_MIST_LINEAR = 1,
  WO_MIST_INVERSE_QUADRATIC = 2,
};

/** #World::flag */
enum {
  WO_DS_EXPAND = 1 << 0,
  /**
   * NOTE: this must have the same value as #MA_DS_SHOW_TEXS,
   * otherwise anim-editors will not read correctly.
   */
  WO_DS_SHOW_TEXS = 1 << 2,
};

#ifdef __cplusplus
}
#endif
