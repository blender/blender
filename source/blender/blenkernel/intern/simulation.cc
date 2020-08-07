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
 * \ingroup bke
 */

#include <iostream>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_defaults.h"
#include "DNA_scene_types.h"
#include "DNA_simulation_types.h"

#include "BLI_compiler_compat.h"
#include "BLI_float3.hh"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_span.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_anim_data.h"
#include "BKE_animsys.h"
#include "BKE_customdata.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_lib_remap.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_pointcache.h"
#include "BKE_simulation.h"

#include "NOD_node_tree_multi_function.hh"
#include "NOD_simulation.h"

#include "BLI_map.hh"
#include "BLT_translation.h"

#include "FN_attributes_ref.hh"
#include "FN_multi_function_network_evaluation.hh"
#include "FN_multi_function_network_optimization.hh"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "SIM_simulation_update.hh"

using StateInitFunction = void (*)(SimulationState *state);
using StateResetFunction = void (*)(SimulationState *state);
using StateRemoveFunction = void (*)(SimulationState *state);
using StateCopyFunction = void (*)(const SimulationState *src, SimulationState *dst);

struct SimulationStateType {
  const char *name;
  int size;
  StateInitFunction init;
  StateResetFunction reset;
  StateRemoveFunction remove;
  StateCopyFunction copy;
};

static const SimulationStateType *try_get_state_type(blender::StringRefNull type_name);

static void simulation_init_data(ID *id)
{
  Simulation *simulation = (Simulation *)id;
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(simulation, id));

  MEMCPY_STRUCT_AFTER(simulation, DNA_struct_default_get(Simulation), id);

  bNodeTree *ntree = ntreeAddTree(nullptr, "Simulation Nodetree", ntreeType_Simulation->idname);
  simulation->nodetree = ntree;
}

static void simulation_copy_data(Main *bmain, ID *id_dst, const ID *id_src, const int flag)
{
  Simulation *simulation_dst = (Simulation *)id_dst;
  const Simulation *simulation_src = (const Simulation *)id_src;

  /* We always need allocation of our private ID data. */
  const int flag_private_id_data = flag & ~LIB_ID_CREATE_NO_ALLOCATE;

  if (simulation_src->nodetree) {
    BKE_id_copy_ex(bmain,
                   (ID *)simulation_src->nodetree,
                   (ID **)&simulation_dst->nodetree,
                   flag_private_id_data);
  }

  BLI_listbase_clear(&simulation_dst->states);
  LISTBASE_FOREACH (const SimulationState *, state_src, &simulation_src->states) {
    SimulationState *state_dst = BKE_simulation_state_add(
        simulation_dst, state_src->type, state_src->name);
    BKE_simulation_state_copy_data(state_src, state_dst);
  }

  BLI_listbase_clear(&simulation_dst->dependencies);
  BLI_duplicatelist(&simulation_dst->dependencies, &simulation_src->dependencies);
}

static void simulation_free_data(ID *id)
{
  Simulation *simulation = (Simulation *)id;

  BKE_animdata_free(&simulation->id, false);

  if (simulation->nodetree) {
    ntreeFreeEmbeddedTree(simulation->nodetree);
    MEM_freeN(simulation->nodetree);
    simulation->nodetree = nullptr;
  }

  BKE_simulation_state_remove_all(simulation);

  BLI_freelistN(&simulation->dependencies);
}

static void simulation_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Simulation *simulation = (Simulation *)id;
  if (simulation->nodetree) {
    /* nodetree **are owned by IDs**, treat them as mere sub-data and not real ID! */
    BKE_library_foreach_ID_embedded(data, (ID **)&simulation->nodetree);
  }
  LISTBASE_FOREACH (SimulationDependency *, dependency, &simulation->dependencies) {
    BKE_LIB_FOREACHID_PROCESS_ID(data, dependency->id, IDWALK_CB_USER);
  }
}

IDTypeInfo IDType_ID_SIM = {
    /* id_code */ ID_SIM,
    /* id_filter */ FILTER_ID_SIM,
    /* main_listbase_index */ INDEX_ID_SIM,
    /* struct_size */ sizeof(Simulation),
    /* name */ "Simulation",
    /* name_plural */ "simulations",
    /* translation_context */ BLT_I18NCONTEXT_ID_SIMULATION,
    /* flags */ 0,

    /* init_data */ simulation_init_data,
    /* copy_data */ simulation_copy_data,
    /* free_data */ simulation_free_data,
    /* make_local */ nullptr,
    /* foreach_id */ simulation_foreach_id,
};

void *BKE_simulation_add(Main *bmain, const char *name)
{
  Simulation *simulation = (Simulation *)BKE_libblock_alloc(bmain, ID_SIM, name, 0);

  simulation_init_data(&simulation->id);

  return simulation;
}

SimulationState *BKE_simulation_state_add(Simulation *simulation,
                                          const char *type,
                                          const char *name)
{
  BLI_assert(simulation != nullptr);
  BLI_assert(name != nullptr);

  const SimulationStateType *state_type = try_get_state_type(type);
  BLI_assert(state_type != nullptr);

  SimulationState *state = (SimulationState *)MEM_callocN(state_type->size, AT);
  state->type = BLI_strdup(type);
  state->name = BLI_strdup(name);

  state_type->init(state);
  BLI_addtail(&simulation->states, state);
  return state;
}

void BKE_simulation_state_remove(Simulation *simulation, SimulationState *state)
{
  BLI_assert(simulation != nullptr);
  BLI_assert(state != nullptr);
  BLI_assert(BLI_findindex(&simulation->states, state) >= 0);

  BLI_remlink(&simulation->states, state);
  const SimulationStateType *state_type = try_get_state_type(state->type);
  BLI_assert(state_type != nullptr);
  state_type->remove(state);
  MEM_freeN(state->name);
  MEM_freeN(state->type);
  MEM_freeN(state);
}

void BKE_simulation_state_remove_all(Simulation *simulation)
{
  BLI_assert(simulation != nullptr);

  while (!BLI_listbase_is_empty(&simulation->states)) {
    BKE_simulation_state_remove(simulation, (SimulationState *)simulation->states.first);
  }
}

void BKE_simulation_state_reset(Simulation *UNUSED(simulation), SimulationState *state)
{
  BLI_assert(state != nullptr);

  const SimulationStateType *state_type = try_get_state_type(state->type);
  BLI_assert(state_type != nullptr);
  state_type->reset(state);
}

void BKE_simulation_state_reset_all(Simulation *simulation)
{
  BLI_assert(simulation != nullptr);

  LISTBASE_FOREACH (SimulationState *, state, &simulation->states) {
    BKE_simulation_state_reset(simulation, state);
  }
}

void BKE_simulation_state_copy_data(const SimulationState *src_state, SimulationState *dst_state)
{
  BLI_assert(src_state != nullptr);
  BLI_assert(dst_state != nullptr);
  BLI_assert(STREQ(src_state->type, dst_state->type));

  const SimulationStateType *state_type = try_get_state_type(src_state->type);
  BLI_assert(state_type != nullptr);
  state_type->copy(src_state, dst_state);
}

SimulationState *BKE_simulation_state_try_find_by_name(Simulation *simulation, const char *name)
{
  if (simulation == nullptr) {
    return nullptr;
  }
  if (name == nullptr) {
    return nullptr;
  }

  LISTBASE_FOREACH (SimulationState *, state, &simulation->states) {
    if (STREQ(state->name, name)) {
      return state;
    }
  }
  return nullptr;
}

SimulationState *BKE_simulation_state_try_find_by_name_and_type(Simulation *simulation,
                                                                const char *name,
                                                                const char *type)
{
  if (type == nullptr) {
    return nullptr;
  }

  SimulationState *state = BKE_simulation_state_try_find_by_name(simulation, name);
  if (state == nullptr) {
    return nullptr;
  }
  if (STREQ(state->type, type)) {
    return state;
  }
  return nullptr;
}

void BKE_simulation_data_update(Depsgraph *depsgraph, Scene *scene, Simulation *simulation)
{
  blender::sim::update_simulation_in_depsgraph(depsgraph, scene, simulation);
}

void BKE_simulation_update_dependencies(Simulation *simulation, Main *bmain)
{
  bool dependencies_changed = blender::sim::update_simulation_dependencies(simulation);
  if (dependencies_changed) {
    DEG_relations_tag_update(bmain);
  }
}

using StateTypeMap = blender::Map<std::string, std::unique_ptr<SimulationStateType>>;

template<typename T>
static void add_state_type(StateTypeMap &map,
                           void (*init)(T *state),
                           void (*reset)(T *state),
                           void (*remove)(T *state),
                           void (*copy)(const T *src, T *dst))
{
  SimulationStateType state_type{
      BKE_simulation_get_state_type_name<T>(),
      static_cast<int>(sizeof(T)),
      (StateInitFunction)init,
      (StateResetFunction)reset,
      (StateRemoveFunction)remove,
      (StateCopyFunction)copy,
  };
  map.add_new(state_type.name, std::make_unique<SimulationStateType>(state_type));
}

static StateTypeMap init_state_types()
{
  StateTypeMap map;
  add_state_type<ParticleSimulationState>(
      map,
      [](ParticleSimulationState *state) { CustomData_reset(&state->attributes); },
      [](ParticleSimulationState *state) {
        CustomData_free(&state->attributes, state->tot_particles);
        state->tot_particles = 0;
        state->next_particle_id = 0;
      },
      [](ParticleSimulationState *state) {
        CustomData_free(&state->attributes, state->tot_particles);
      },
      [](const ParticleSimulationState *src, ParticleSimulationState *dst) {
        CustomData_free(&dst->attributes, dst->tot_particles);
        dst->tot_particles = src->tot_particles;
        dst->next_particle_id = src->next_particle_id;
        CustomData_copy(
            &src->attributes, &dst->attributes, CD_MASK_ALL, CD_DUPLICATE, src->tot_particles);
      });

  add_state_type<ParticleMeshEmitterSimulationState>(
      map,
      [](ParticleMeshEmitterSimulationState *UNUSED(state)) {},
      [](ParticleMeshEmitterSimulationState *state) { state->last_birth_time = 0.0f; },
      [](ParticleMeshEmitterSimulationState *UNUSED(state)) {},
      [](const ParticleMeshEmitterSimulationState *src, ParticleMeshEmitterSimulationState *dst) {
        dst->last_birth_time = src->last_birth_time;
      });
  return map;
}

static StateTypeMap &get_state_types()
{
  static StateTypeMap state_type_map = init_state_types();
  return state_type_map;
}

static const SimulationStateType *try_get_state_type(blender::StringRefNull type_name)
{
  std::unique_ptr<SimulationStateType> *type = get_state_types().lookup_ptr_as(type_name);
  if (type == nullptr) {
    return nullptr;
  }
  return type->get();
}

template<> const char *BKE_simulation_get_state_type_name<ParticleSimulationState>()
{
  return SIM_TYPE_NAME_PARTICLE_SIMULATION;
}

template<> const char *BKE_simulation_get_state_type_name<ParticleMeshEmitterSimulationState>()
{
  return SIM_TYPE_NAME_PARTICLE_MESH_EMITTER;
}
