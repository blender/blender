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
#include "BKE_object.h"
#include "BKE_simulation.h"

#include "DNA_modifier_types.h"
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

#include "NOD_node_tree_dependencies.hh"

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

class SampledDependencyAnimations : public DependencyAnimations {
 private:
  TimeInterval simulation_time_interval_;
  MultiValueMap<Object *, float4x4> object_transforms_cache_;

 public:
  SampledDependencyAnimations(TimeInterval simulation_time_interval)
      : simulation_time_interval_(simulation_time_interval)
  {
  }

  void add_object_transforms(Object &object, Span<float4x4> transforms)
  {
    object_transforms_cache_.add_multiple(&object, transforms);
  }

  bool is_object_transform_changing(Object &object) const
  {
    return object_transforms_cache_.lookup(&object).size() >= 2;
  }

  void get_object_transforms(Object &object,
                             Span<float> simulation_times,
                             MutableSpan<float4x4> r_transforms) const
  {
    assert_same_size(simulation_times, r_transforms);
    Span<float4x4> cached_transforms = object_transforms_cache_.lookup(&object);
    if (cached_transforms.size() == 0) {
      r_transforms.fill(object.obmat);
      return;
    }
    if (cached_transforms.size() == 1) {
      r_transforms.fill(cached_transforms[0]);
      return;
    }

    for (int i : simulation_times.index_range()) {
      const float simulation_time = simulation_times[i];
      if (simulation_time <= simulation_time_interval_.start()) {
        r_transforms[i] = cached_transforms.first();
        continue;
      }
      if (simulation_time >= simulation_time_interval_.stop()) {
        r_transforms[i] = cached_transforms.last();
        continue;
      }
      const float factor = simulation_time_interval_.factor_at_time(simulation_time);
      BLI_assert(factor > 0.0f && factor < 1.0f);
      const float scaled_factor = factor * (cached_transforms.size() - 1);
      const int lower_sample = static_cast<int>(scaled_factor);
      const int upper_sample = lower_sample + 1;
      const float mix_factor = scaled_factor - lower_sample;
      r_transforms[i] = float4x4::interpolate(
          cached_transforms[lower_sample], cached_transforms[upper_sample], mix_factor);
    }
  }
};

static void sample_object_transforms(Object &object,
                                     Depsgraph &depsgraph,
                                     Scene &scene,
                                     TimeInterval scene_frame_interval,
                                     MutableSpan<float4x4> r_transforms)
{
  if (r_transforms.size() == 0) {
    return;
  }
  if (r_transforms.size() == 1) {
    r_transforms[0] = object.obmat;
    return;
  }

  Array<float> frames(r_transforms.size());
  scene_frame_interval.compute_uniform_samples(frames);

  for (int i : frames.index_range()) {
    float frame = frames[i];
    const int recursion_depth = 5;
    BKE_object_modifier_update_subframe(
        &depsgraph, &scene, &object, false, recursion_depth, frame, eModifierType_None);
    r_transforms[i] = object.obmat;
  }
}

template<typename T> static bool all_values_equal(Span<T> values)
{
  if (values.size() == 0) {
    return true;
  }
  for (const T &value : values.drop_front(1)) {
    if (value != values[0]) {
      return false;
    }
  }
  return true;
}

static void prepare_dependency_animations(Depsgraph &depsgraph,
                                          Scene &scene,
                                          Simulation &simulation,
                                          TimeInterval scene_frame_interval,
                                          SampledDependencyAnimations &r_dependency_animations)
{
  LISTBASE_FOREACH (SimulationDependency *, dependency, &simulation.dependencies) {
    ID *id_cow = DEG_get_evaluated_id(&depsgraph, dependency->id);
    if (id_cow == nullptr) {
      continue;
    }
    if (GS(id_cow->name) != ID_OB) {
      continue;
    }
    Object &object_cow = *reinterpret_cast<Object *>(id_cow);
    constexpr int sample_count = 10;
    Array<float4x4, sample_count> transforms(sample_count);
    sample_object_transforms(object_cow, depsgraph, scene, scene_frame_interval, transforms);

    /* If all samples are the same, only store one. */
    Span<float4x4> transforms_to_use = (all_values_equal(transforms.as_span())) ?
                                           transforms.as_span().take_front(1) :
                                           transforms.as_span();

    r_dependency_animations.add_object_transforms(object_cow, transforms_to_use);
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

  Simulation *simulation_orig = reinterpret_cast<Simulation *>(
      DEG_get_original_id(&simulation_cow->id));

  ResourceCollector resources;
  SimulationInfluences influences;
  RequiredStates required_states;

  collect_simulation_influences(*simulation_cow, resources, influences, required_states);

  bke::PersistentDataHandleMap handle_map;
  LISTBASE_FOREACH (SimulationDependency *, dependency, &simulation_orig->dependencies) {
    ID *id_cow = DEG_get_evaluated_id(depsgraph, dependency->id);
    if (id_cow != nullptr) {
      handle_map.add(dependency->handle, *id_cow);
    }
  }

  if (current_frame == 1) {
    reinitialize_empty_simulation_states(simulation_orig, required_states);

    initialize_simulation_states(*simulation_orig, *depsgraph, influences, handle_map);
    simulation_orig->current_frame = 1;

    copy_states_to_cow(simulation_orig, simulation_cow);
  }
  else if (current_frame == simulation_orig->current_frame + 1) {
    update_simulation_state_list(simulation_orig, required_states);

    const float fps = scene_cow->r.frs_sec / scene_cow->r.frs_sec_base;
    const float time_step = 1.0f / fps;
    TimeInterval scene_frame_interval(current_frame - 1, 1);
    TimeInterval simulation_time_interval(simulation_orig->current_simulation_time, time_step);
    SampledDependencyAnimations dependency_animations{simulation_time_interval};
    prepare_dependency_animations(
        *depsgraph, *scene_cow, *simulation_orig, scene_frame_interval, dependency_animations);

    solve_simulation_time_step(
        *simulation_orig, *depsgraph, influences, handle_map, dependency_animations, time_step);
    simulation_orig->current_frame = current_frame;

    copy_states_to_cow(simulation_orig, simulation_cow);
  }
}

/* Returns true when dependencies have changed. */
bool update_simulation_dependencies(Simulation *simulation)
{
  nodes::NodeTreeDependencies dependencies = nodes::find_node_tree_dependencies(
      *simulation->nodetree);

  ListBase *dependency_list = &simulation->dependencies;

  bool dependencies_changed = false;

  Map<ID *, SimulationDependency *> dependency_by_id;
  Map<SimulationDependency *, int> old_flag_by_dependency;
  Set<int> used_handles;

  /* Remove unused handle items and clear flags that are reinitialized later. */
  LISTBASE_FOREACH_MUTABLE (SimulationDependency *, dependency, dependency_list) {
    if (dependencies.depends_on(dependency->id)) {
      dependency_by_id.add_new(dependency->id, dependency);
      used_handles.add_new(dependency->handle);
      old_flag_by_dependency.add_new(dependency, dependency->flag);
      dependency->flag &= ~(SIM_DEPENDS_ON_TRANSFORM | SIM_DEPENDS_ON_GEOMETRY);
    }
    else {
      if (dependency->id != nullptr) {
        id_us_min(dependency->id);
      }
      BLI_remlink(dependency_list, dependency);
      MEM_freeN(dependency);
      dependencies_changed = true;
    }
  }

  /* Add handle items for new id dependencies. */
  int next_handle = 0;
  for (ID *id : dependencies.id_dependencies()) {
    dependency_by_id.lookup_or_add_cb(id, [&]() {
      while (used_handles.contains(next_handle)) {
        next_handle++;
      }
      used_handles.add_new(next_handle);

      SimulationDependency *dependency = static_cast<SimulationDependency *>(
          MEM_callocN(sizeof(*dependency), AT));
      id_us_plus(id);
      dependency->id = id;
      dependency->handle = next_handle;
      BLI_addtail(dependency_list, dependency);

      return dependency;
    });
  }

  /* Set appropriate dependency flags. */
  for (Object *object : dependencies.transform_dependencies()) {
    SimulationDependency *dependency = dependency_by_id.lookup(&object->id);
    dependency->flag |= SIM_DEPENDS_ON_TRANSFORM;
  }
  for (Object *object : dependencies.geometry_dependencies()) {
    SimulationDependency *dependency = dependency_by_id.lookup(&object->id);
    dependency->flag |= SIM_DEPENDS_ON_GEOMETRY;
  }

  if (!dependencies_changed) {
    /* Check if any flags have changed. */
    LISTBASE_FOREACH (SimulationDependency *, dependency, dependency_list) {
      uint32_t old_flag = old_flag_by_dependency.lookup_default(dependency, 0);
      uint32_t new_flag = dependency->flag;
      if (old_flag != new_flag) {
        dependencies_changed = true;
        break;
      }
    }
  }

  return dependencies_changed;
}

}  // namespace blender::sim
