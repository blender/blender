/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_customdata_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Simulation {
  DNA_DEFINE_CXX_METHODS(Simulation)

  ID id;
  struct AnimData *adt; /* animation data (must be immediately after id) */

  /* This nodetree is embedded into the data block. */
  struct bNodeTree *nodetree;

  uint32_t flag;
  char _pad[4];
} Simulation;

/** #Simulation.flag */
enum {
  SIM_DS_EXPAND = (1 << 0),
};

#ifdef __cplusplus
}
#endif
