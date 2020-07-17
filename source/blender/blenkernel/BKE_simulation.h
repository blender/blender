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

#ifndef __BKE_SIMULATION_H__
#define __BKE_SIMULATION_H__

#include "DNA_simulation_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Depsgraph;
struct Main;
struct Scene;

void *BKE_simulation_add(struct Main *bmain, const char *name);

void BKE_simulation_data_update(struct Depsgraph *depsgraph,
                                struct Scene *scene,
                                struct Simulation *simulation);

SimulationState *BKE_simulation_state_add(Simulation *simulation,
                                          eSimulationStateType type,
                                          const char *name);
void BKE_simulation_state_remove(Simulation *simulation, SimulationState *state);
void BKE_simulation_state_remove_all(Simulation *simulation);

#ifdef __cplusplus
}
#endif

#endif /* __BKE_SIMULATION_H__ */
