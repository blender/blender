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

#include "SIM_simulation_update.hh"

#include "BKE_customdata.h"
#include "BKE_lib_id.h"
#include "BKE_simulation.h"

#include "DNA_scene_types.h"
#include "DNA_simulation_types.h"

#include "DEG_depsgraph_query.h"

#include "BLI_array.hh"
#include "BLI_float3.hh"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_rand.h"
#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "particle_function.hh"
#include "simulation_collect_influences.hh"
#include "simulation_solver.hh"

namespace blender::sim {

static void copy_states_to_cow(const Simulation *simulation_orig, Simulation *simulation_cow)
{
  BKE_simulation_state_remove_all(simulation_cow);
  simulation_cow->current_frame = simulation_orig->current_frame;

  LISTBASE_FOREACH (const SimulationState *, state_orig, &simulation_orig->states) {
    SimulationState *state_cow = BKE_simulation_state_add(
        simulation_cow, state_orig->type, state_orig->name);
    BKE_simulation_state_copy_data(state_orig, state_cow);
  }
}

static void remove_unused_states(Simulation *simulation, const RequiredStates &required_states)
{
  LISTBASE_FOREACH_MUTABLE (SimulationState *, state, &simulation->states) {
    if (!required_states.is_required(state->name, state->type)) {
      BKE_simulation_state_remove(simulation, state);
    }
  }
}

static void add_missing_states(Simulation *simulation, const RequiredStates &required_states)
{
  for (auto &&item : required_states.states().items()) {
    const char *name = item.key.c_str();
    const char *type = item.value;

    SimulationState *state = BKE_simulation_state_try_find_by_name_and_type(
        simulation, name, type);

    if (state == nullptr) {
      BKE_simulation_state_add(simulation, type, name);
    }
  }
}

static void reinitialize_empty_simulation_states(Simulation *simulation,
                                                 const RequiredStates &required_states)
{
  remove_unused_states(simulation, required_states);
  BKE_simulation_state_reset_all(simulation);
  add_missing_states(simulation, required_states);
}

static void update_simulation_state_list(Simulation *simulation,
                                         const RequiredStates &required_states)
{
  remove_unused_states(simulation, required_states);
  add_missing_states(simulation, required_states);
}

/* TODO: It might be better to do this as part of ntreeUpdateTree, so that the information
 * about referenced data blocks is available when building the depsgraph. */
static void update_persistent_data_handles(Simulation &simulation_orig,
                                           const UsedPersistentData &used_persistent_data)
{
  Set<ID *> contained_ids;
  Set<int> used_handles;

  /* Remove handles that have been invalidated. */
  LISTBASE_FOREACH_MUTABLE (
      PersistentDataHandleItem *, handle_item, &simulation_orig.persistent_data_handles) {
    if (handle_item->id == nullptr) {
      BLI_remlink(&simulation_orig.persistent_data_handles, handle_item);
      MEM_freeN(handle_item);
      continue;
    }
    if (!used_persistent_data.used_ids().contains(handle_item->id)) {
      id_us_min(handle_item->id);
      BLI_remlink(&simulation_orig.persistent_data_handles, handle_item);
      MEM_freeN(handle_item);
      continue;
    }
    contained_ids.add_new(handle_item->id);
    used_handles.add_new(handle_item->handle);
  }

  /* Add new handles that are not in the list yet. */
  int next_handle = 0;
  for (ID *id : used_persistent_data.used_ids()) {
    if (contained_ids.contains(id)) {
      continue;
    }

    /* Find the next available handle. */
    while (used_handles.contains(next_handle)) {
      next_handle++;
    }
    used_handles.add_new(next_handle);

    PersistentDataHandleItem *handle_item = (PersistentDataHandleItem *)MEM_callocN(
        sizeof(*handle_item), AT);
    /* Cannot store const pointers in DNA. */
    id_us_plus(id);
    handle_item->id = id;
    handle_item->handle = next_handle;

    BLI_addtail(&simulation_orig.persistent_data_handles, handle_item);
  }
}

void update_simulation_in_depsgraph(Depsgraph *depsgraph,
                                    Scene *scene_cow,
                                    Simulation *simulation_cow)
{
  int current_frame = scene_cow->r.cfra;
  if (simulation_cow->current_frame == current_frame) {
    return;
  }

  /* Below we modify the original state/cache. Only the active depsgraph is allowed to do that. */
  if (!DEG_is_active(depsgraph)) {
    return;
  }

  Simulation *simulation_orig = (Simulation *)DEG_get_original_id(&simulation_cow->id);

  ResourceCollector resources;
  SimulationInfluences influences;
  RequiredStates required_states;
  UsedPersistentData used_persistent_data;

  collect_simulation_influences(
      *simulation_cow, resources, influences, required_states, used_persistent_data);
  update_persistent_data_handles(*simulation_orig, used_persistent_data);

  bke::PersistentDataHandleMap handle_map;
  LISTBASE_FOREACH (
      PersistentDataHandleItem *, handle_item, &simulation_orig->persistent_data_handles) {
    ID *id_cow = DEG_get_evaluated_id(depsgraph, handle_item->id);
    handle_map.add(handle_item->handle, *id_cow);
  }

  if (current_frame == 1) {
    reinitialize_empty_simulation_states(simulation_orig, required_states);

    initialize_simulation_states(*simulation_orig, *depsgraph, influences, handle_map);
    simulation_orig->current_frame = 1;

    copy_states_to_cow(simulation_orig, simulation_cow);
  }
  else if (current_frame == simulation_orig->current_frame + 1) {
    update_simulation_state_list(simulation_orig, required_states);

    float time_step = 1.0f / 24.0f;
    solve_simulation_time_step(*simulation_orig, *depsgraph, influences, handle_map, time_step);
    simulation_orig->current_frame = current_frame;

    copy_states_to_cow(simulation_orig, simulation_cow);
  }
}

}  // namespace blender::sim
