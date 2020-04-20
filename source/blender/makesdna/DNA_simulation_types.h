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

#ifndef __DNA_SIMULATION_TYPES_H__
#define __DNA_SIMULATION_TYPES_H__

#include "DNA_ID.h"

typedef struct Simulation {
  ID id;
  struct AnimData *adt; /* animation data (must be immediately after id) */

  struct bNodeTree *nodetree;

  int flag;
  int _pad1[1];
} Simulation;

/* Simulation.flag */
enum {
  SIM_DS_EXPAND = (1 << 0),
};

#endif /* __DNA_SIMULATION_TYPES_H__ */
