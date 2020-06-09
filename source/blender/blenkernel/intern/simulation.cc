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

#include "BLI_array_ref.hh"
#include "BLI_compiler_compat.h"
#include "BLI_float3.hh"
#include "BLI_listbase.h"
#include "BLI_math.h"
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

#include "NOD_simulation.h"

#include "BLT_translation.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

using blender::ArrayRef;
using blender::float3;
using blender::MutableArrayRef;

static void simulation_init_data(ID *id)
{
  Simulation *simulation = (Simulation *)id;
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(simulation, id));

  MEMCPY_STRUCT_AFTER(simulation, DNA_struct_default_get(Simulation), id);

  bNodeTree *ntree = ntreeAddTree(nullptr, "Simulation Nodetree", ntreeType_Simulation->idname);
  simulation->nodetree = ntree;

  /* Add a default particle simulation state for now. */
  ParticleSimulationState *state = (ParticleSimulationState *)MEM_callocN(
      sizeof(ParticleSimulationState), __func__);
  CustomData_reset(&state->attributes);

  state->point_cache = BKE_ptcache_add(&state->ptcaches);
  BLI_addtail(&simulation->states, state);
}

static void simulation_copy_data(Main *bmain, ID *id_dst, const ID *id_src, const int flag)
{
  Simulation *simulation_dst = (Simulation *)id_dst;
  Simulation *simulation_src = (Simulation *)id_src;

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
    switch ((eSimulationStateType)state_src->type) {
      case SIM_STATE_TYPE_PARTICLES: {
        ParticleSimulationState *particle_state_dst = (ParticleSimulationState *)MEM_callocN(
            sizeof(ParticleSimulationState), __func__);
        CustomData_reset(&particle_state_dst->attributes);

        BLI_addtail(&simulation_dst->states, particle_state_dst);
        break;
      }
    }
  }
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

  LISTBASE_FOREACH_MUTABLE (SimulationState *, state, &simulation->states) {
    switch ((eSimulationStateType)state->type) {
      case SIM_STATE_TYPE_PARTICLES: {
        ParticleSimulationState *particle_state = (ParticleSimulationState *)state;
        CustomData_free(&particle_state->attributes, particle_state->tot_particles);
        BKE_ptcache_free_list(&particle_state->ptcaches);
        break;
      }
    }
    MEM_freeN(state);
  }
}

static void simulation_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Simulation *simulation = (Simulation *)id;
  if (simulation->nodetree) {
    /* nodetree **are owned by IDs**, treat them as mere sub-data and not real ID! */
    BKE_library_foreach_ID_embedded(data, (ID **)&simulation->nodetree);
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

static MutableArrayRef<float3> get_particle_positions(ParticleSimulationState *state)
{
  return MutableArrayRef<float3>(
      (float3 *)CustomData_get_layer_named(&state->attributes, CD_LOCATION, "Position"),
      state->tot_particles);
}

static void ensure_attributes_exist(ParticleSimulationState *state)
{
  if (CustomData_get_layer_named(&state->attributes, CD_LOCATION, "Position") == nullptr) {
    CustomData_add_layer_named(
        &state->attributes, CD_LOCATION, CD_CALLOC, nullptr, state->tot_particles, "Position");
  }
}

static void copy_particle_state_to_cow(ParticleSimulationState *state_orig,
                                       ParticleSimulationState *state_cow)
{
  ensure_attributes_exist(state_cow);
  CustomData_free(&state_cow->attributes, state_cow->tot_particles);
  CustomData_copy(&state_orig->attributes,
                  &state_cow->attributes,
                  CD_MASK_ALL,
                  CD_DUPLICATE,
                  state_orig->tot_particles);
  state_cow->current_frame = state_orig->current_frame;
  state_cow->tot_particles = state_orig->tot_particles;
}

void BKE_simulation_data_update(Depsgraph *depsgraph, Scene *scene, Simulation *simulation)
{
  int current_frame = scene->r.cfra;

  ParticleSimulationState *state_cow = (ParticleSimulationState *)simulation->states.first;
  ParticleSimulationState *state_orig = (ParticleSimulationState *)state_cow->head.orig_state;

  if (current_frame == state_cow->current_frame) {
    return;
  }

  /* Number of particles should be stored in the cache, but for now assume it is constant. */
  state_cow->tot_particles = state_orig->tot_particles;
  CustomData_realloc(&state_cow->attributes, state_orig->tot_particles);
  ensure_attributes_exist(state_cow);

  PTCacheID pid_cow;
  BKE_ptcache_id_from_sim_particles(&pid_cow, state_cow);
  BKE_ptcache_id_time(&pid_cow, scene, current_frame, nullptr, nullptr, nullptr);

  /* If successfull, this will read the state directly into the cow state. */
  int cache_result = BKE_ptcache_read(&pid_cow, current_frame, true);
  if (cache_result == PTCACHE_READ_EXACT) {
    state_cow->current_frame = current_frame;
    return;
  }

  /* Below we modify the original state/cache. Only the active depsgraph is allowed to do that. */
  if (!DEG_is_active(depsgraph)) {
    return;
  }

  PTCacheID pid_orig;
  BKE_ptcache_id_from_sim_particles(&pid_orig, state_orig);
  BKE_ptcache_id_time(&pid_orig, scene, current_frame, nullptr, nullptr, nullptr);

  if (current_frame == 1) {
    state_orig->tot_particles = 100;
    state_orig->current_frame = 1;
    CustomData_realloc(&state_orig->attributes, state_orig->tot_particles);
    ensure_attributes_exist(state_orig);

    MutableArrayRef<float3> positions = get_particle_positions(state_orig);
    for (uint i : positions.index_range()) {
      positions[i] = {i / 10.0f, 0, 0};
    }

    BKE_ptcache_write(&pid_orig, current_frame);
    copy_particle_state_to_cow(state_orig, state_cow);
  }
  else if (current_frame == state_orig->current_frame + 1) {
    state_orig->current_frame = current_frame;
    ensure_attributes_exist(state_orig);
    MutableArrayRef<float3> positions = get_particle_positions(state_orig);
    for (float3 &position : positions) {
      position.z += 0.1f;
    }

    BKE_ptcache_write(&pid_orig, current_frame);
    copy_particle_state_to_cow(state_orig, state_cow);
  }
}
