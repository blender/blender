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
  float current_frame;
  float current_simulation_time;
  char _pad[4];

  /** List containing SimulationState objects. */
  struct ListBase states;

  /** List containing PersistentDataHandleItem objects. */
  struct ListBase persistent_data_handles;
} Simulation;

typedef struct SimulationState {
  struct SimulationState *next;
  struct SimulationState *prev;

  char *type;
  char *name;
} SimulationState;

typedef struct ParticleSimulationState {
  SimulationState head;

  /** Contains the state of the particles at time Simulation->current_frame. */
  int tot_particles;
  int next_particle_id;
  struct CustomData attributes;
} ParticleSimulationState;

typedef struct ParticleMeshEmitterSimulationState {
  SimulationState head;

  float last_birth_time;
  char _pad[4];
} ParticleMeshEmitterSimulationState;

/** Stores a mapping between an integer handle and a corresponding ID data block. */
typedef struct PersistentDataHandleItem {
  struct PersistentDataHandleItem *next;
  struct PersistentDataHandleItem *prev;
  struct ID *id;
  int handle;
  int flag;
} PersistentDataHandleItem;

/* Simulation.flag */
enum {
  SIM_DS_EXPAND = (1 << 0),
};

/* PersistentDataHandleItem.flag */
enum {
  SIM_HANDLE_DEPENDS_ON_TRANSFORM = (1 << 0),
  SIM_HANDLE_DEPENDS_ON_GEOMETRY = (1 << 1),
};

#define SIM_TYPE_NAME_PARTICLE_SIMULATION "Particle Simulation"
#define SIM_TYPE_NAME_PARTICLE_MESH_EMITTER "Particle Mesh Emitter"

#endif /* __DNA_SIMULATION_TYPES_H__ */
