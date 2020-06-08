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
#include "DNA_customdata_types.h"

typedef struct Simulation {
  ID id;
  struct AnimData *adt; /* animation data (must be immediately after id) */

  struct bNodeTree *nodetree;

  int flag;
  int _pad;

  /** List containing SimulationState objects. */
  struct ListBase states;
} Simulation;

typedef struct SimulationState {
  struct SimulationState *next;
  struct SimulationState *prev;

  /** This is only initialized on cow copies of the simulation. It points to the state on the
   * original data block. That is where the cache is stored.  */
  struct SimulationState *orig_state;

  /** eSimulationStateType */
  int type;
  int _pad;

  char name[64];
} SimulationState;

typedef struct ParticleSimulationState {
  SimulationState head;

  /** Contains the state of the particles at time current_frame. */
  float current_frame;
  int tot_particles;
  struct CustomData attributes;

  /** Caches the state of the particles over time. The cache only exists on the original data
   * block, not on cow copies. */
  struct PointCache *point_cache;
  struct ListBase ptcaches;
} ParticleSimulationState;

/* Simulation.flag */
enum {
  SIM_DS_EXPAND = (1 << 0),
};

/* SimulationCache.type */
typedef enum eSimulationStateType {
  SIM_STATE_TYPE_PARTICLES = 0,
} eSimulationStateType;

#endif /* __DNA_SIMULATION_TYPES_H__ */
