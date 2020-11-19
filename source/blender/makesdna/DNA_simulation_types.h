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
 */

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
  ID id;
  struct AnimData *adt; /* animation data (must be immediately after id) */

  /* This nodetree is embedded into the data block. */
  struct bNodeTree *nodetree;

  uint32_t flag;
  char _pad[4];
} Simulation;

/* Simulation.flag */
enum {
  SIM_DS_EXPAND = (1 << 0),
};

#ifdef __cplusplus
}
#endif
