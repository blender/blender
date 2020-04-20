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

#ifndef __NOD_SIMULATION_H__
#define __NOD_SIMULATION_H__

#ifdef __cplusplus
extern "C" {
#endif

extern struct bNodeTreeType *ntreeType_Simulation;

void register_node_tree_type_sim(void);

void register_node_type_sim_group(void);

void register_node_type_sim_particle_simulation(void);
void register_node_type_sim_force(void);
void register_node_type_sim_set_particle_attribute(void);
void register_node_type_sim_particle_birth_event(void);
void register_node_type_sim_particle_time_step_event(void);
void register_node_type_sim_execute_condition(void);
void register_node_type_sim_multi_execute(void);
void register_node_type_sim_particle_mesh_emitter(void);
void register_node_type_sim_particle_mesh_collision_event(void);
void register_node_type_sim_emit_particles(void);
void register_node_type_sim_time(void);
void register_node_type_sim_particle_attribute(void);

#ifdef __cplusplus
}
#endif

#endif /* __NOD_SIMULATION_H__ */
