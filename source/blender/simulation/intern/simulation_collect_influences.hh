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

#ifndef __SIM_SIMULATION_COLLECT_INFLUENCES_HH__
#define __SIM_SIMULATION_COLLECT_INFLUENCES_HH__

#include "NOD_derived_node_tree.hh"

#include "BLI_resource_collector.hh"

#include "simulation_solver.hh"

namespace blender::sim {

struct SimulationStatesInfo {
  VectorSet<std::string> particle_simulation_names;
};

void collect_simulation_influences(Simulation &simulation,
                                   ResourceCollector &resources,
                                   SimulationInfluences &r_influences,
                                   SimulationStatesInfo &r_states_info);

}  // namespace blender::sim

#endif /* __SIM_SIMULATION_COLLECT_INFLUENCES_HH__ */
