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

#include "BLT_translation.h"

#include "FN_attributes_ref.hh"
#include "FN_multi_function_network_evaluation.hh"
#include "FN_multi_function_network_optimization.hh"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "SIM_simulation_update.hh"

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
}

static void free_simulation_state_head(SimulationState *state)
{
  MEM_freeN(state->name);
}

static void free_particle_simulation_state(ParticleSimulationState *state)
{
  free_simulation_state_head(&state->head);
  CustomData_free(&state->attributes, state->tot_particles);
  BKE_ptcache_free_list(&state->ptcaches);
  MEM_freeN(state);
}

SimulationState *BKE_simulation_state_add(Simulation *simulation,
                                          eSimulationStateType type,
                                          const char *name)
{
  BLI_assert(simulation != nullptr);
  BLI_assert(name != nullptr);

  bool is_cow_simulation = DEG_is_evaluated_id(&simulation->id);

  switch (type) {
    case SIM_STATE_TYPE_PARTICLES: {
      ParticleSimulationState *state = (ParticleSimulationState *)MEM_callocN(sizeof(*state), AT);
      state->head.type = SIM_STATE_TYPE_PARTICLES;
      state->head.name = BLI_strdup(name);
      CustomData_reset(&state->attributes);

      if (!is_cow_simulation) {
        state->point_cache = BKE_ptcache_add(&state->ptcaches);
      }

      BLI_addtail(&simulation->states, state);
      return &state->head;
    }
  }

  BLI_assert(false);
  return nullptr;
}

void BKE_simulation_state_remove(Simulation *simulation, SimulationState *state)
{
  BLI_assert(simulation != nullptr);
  BLI_assert(state != nullptr);
  BLI_assert(BLI_findindex(&simulation->states, state) >= 0);

  BLI_remlink(&simulation->states, state);
  switch ((eSimulationStateType)state->type) {
    case SIM_STATE_TYPE_PARTICLES: {
      free_particle_simulation_state((ParticleSimulationState *)state);
      break;
    }
  }
}

void BKE_simulation_state_remove_all(Simulation *simulation)
{
  BLI_assert(simulation != nullptr);
  while (!BLI_listbase_is_empty(&simulation->states)) {
    BKE_simulation_state_remove(simulation, (SimulationState *)simulation->states.first);
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

  BKE_simulation_state_remove_all(simulation);
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

void BKE_simulation_data_update(Depsgraph *depsgraph, Scene *scene, Simulation *simulation)
{
  blender::sim::update_simulation_in_depsgraph(depsgraph, scene, simulation);
}
