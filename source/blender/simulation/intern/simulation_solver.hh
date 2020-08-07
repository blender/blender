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

#pragma once

#include "simulation_collect_influences.hh"

struct Depsgraph;

namespace blender::sim {

void initialize_simulation_states(Simulation &simulation,
                                  Depsgraph &depsgraph,
                                  const SimulationInfluences &influences,
                                  const bke::PersistentDataHandleMap &handle_map);

void solve_simulation_time_step(Simulation &simulation,
                                Depsgraph &depsgraph,
                                const SimulationInfluences &influences,
                                const bke::PersistentDataHandleMap &handle_map,
                                const DependencyAnimations &dependency_animations,
                                float time_step);

}  // namespace blender::sim
