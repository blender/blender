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
#include "BKE_node_tree_multi_function.hh"
#include "BKE_pointcache.h"
#include "BKE_simulation.h"

#include "NOD_simulation.h"

#include "BLT_translation.h"

#include "FN_attributes_ref.hh"
#include "FN_cpp_types.hh"
#include "FN_multi_function_network_evaluation.hh"
#include "FN_multi_function_network_optimization.hh"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

extern "C" {
void WM_clipboard_text_set(const char *buf, bool selection);
}

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
        free_particle_simulation_state((ParticleSimulationState *)state);
        break;
      }
    }
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

namespace blender::bke {

static void ensure_attributes_exist(ParticleSimulationState *state)
{
  if (CustomData_get_layer_named(&state->attributes, CD_LOCATION, "Position") == nullptr) {
    CustomData_add_layer_named(
        &state->attributes, CD_LOCATION, CD_CALLOC, nullptr, state->tot_particles, "Position");
  }
  if (CustomData_get_layer_named(&state->attributes, CD_LOCATION, "Velocity") == nullptr) {
    CustomData_add_layer_named(
        &state->attributes, CD_LOCATION, CD_CALLOC, nullptr, state->tot_particles, "Velocity");
  }
}

static void copy_states_to_cow(Simulation *simulation_orig, Simulation *simulation_cow)
{
  LISTBASE_FOREACH_MUTABLE (SimulationState *, state_cow, &simulation_cow->states) {
    switch ((eSimulationStateType)state_cow->type) {
      case SIM_STATE_TYPE_PARTICLES: {
        BLI_remlink(&simulation_cow->states, state_cow);
        free_particle_simulation_state((ParticleSimulationState *)state_cow);
        break;
      }
    }
  }
  simulation_cow->current_frame = simulation_orig->current_frame;

  LISTBASE_FOREACH (SimulationState *, state_orig, &simulation_orig->states) {
    switch ((eSimulationStateType)state_orig->type) {
      case SIM_STATE_TYPE_PARTICLES: {
        ParticleSimulationState *particle_state_orig = (ParticleSimulationState *)state_orig;
        ParticleSimulationState *particle_state_cow = (ParticleSimulationState *)MEM_callocN(
            sizeof(*particle_state_cow), AT);
        particle_state_cow->tot_particles = particle_state_orig->tot_particles;
        particle_state_cow->head.name = BLI_strdup(state_orig->name);
        CustomData_copy(&particle_state_orig->attributes,
                        &particle_state_cow->attributes,
                        CD_MASK_ALL,
                        CD_DUPLICATE,
                        particle_state_orig->tot_particles);
        BLI_addtail(&simulation_cow->states, particle_state_cow);
        break;
      }
    }
  }
}

using AttributeNodeMap = Map<fn::MFDummyNode *, std::pair<std::string, fn::MFDataType>>;

static AttributeNodeMap deduplicate_attribute_nodes(fn::MFNetwork &network,
                                                    MFNetworkTreeMap &network_map,
                                                    const DerivedNodeTree &tree)
{
  Span<const DNode *> attribute_dnodes = tree.nodes_by_type("SimulationNodeParticleAttribute");
  uint amount = attribute_dnodes.size();
  if (amount == 0) {
    return {};
  }

  Vector<fn::MFInputSocket *> name_sockets;
  for (const DNode *dnode : attribute_dnodes) {
    fn::MFInputSocket &name_socket = network_map.lookup_dummy(dnode->input(0));
    name_sockets.append(&name_socket);
  }

  fn::MFNetworkEvaluator network_fn{{}, name_sockets.as_span()};

  fn::MFParamsBuilder params{network_fn, 1};

  Array<std::string> attribute_names{amount, NoInitialization()};
  for (uint i : IndexRange(amount)) {
    params.add_uninitialized_single_output(
        fn::GMutableSpan(fn::CPPType_string, attribute_names.data() + i, 1));
  }

  fn::MFContextBuilder context;
  /* Todo: Check that the names don't depend on dummy nodes. */
  network_fn.call({0}, params, context);

  Map<std::pair<std::string, fn::MFDataType>, Vector<fn::MFNode *>>
      attribute_nodes_by_name_and_type;
  for (uint i : IndexRange(amount)) {
    attribute_nodes_by_name_and_type
        .lookup_or_add_default({attribute_names[i], name_sockets[i]->data_type()})
        .append(&name_sockets[i]->node());
  }

  AttributeNodeMap final_attribute_nodes;
  for (auto item : attribute_nodes_by_name_and_type.items()) {
    StringRef attribute_name = item.key.first;
    fn::MFDataType data_type = item.key.second;
    Span<fn::MFNode *> nodes = item.value;

    fn::MFOutputSocket &new_attribute_socket = network.add_input(
        "Attribute '" + attribute_name + "'", data_type);
    for (fn::MFNode *node : nodes) {
      network.relink(node->output(0), new_attribute_socket);
    }
    network.remove(nodes);

    final_attribute_nodes.add_new(&new_attribute_socket.node().as_dummy(), item.key);
  }

  return final_attribute_nodes;
}

class CustomDataAttributesRef {
 private:
  Vector<void *> buffers_;
  uint size_;
  std::unique_ptr<fn::AttributesInfo> info_;

 public:
  CustomDataAttributesRef(CustomData &custom_data, uint size)
  {
    fn::AttributesInfoBuilder builder;
    for (const CustomDataLayer &layer : Span(custom_data.layers, custom_data.totlayer)) {
      buffers_.append(layer.data);
      builder.add<float3>(layer.name, {0, 0, 0});
    }
    info_ = std::make_unique<fn::AttributesInfo>(builder);
    size_ = size;
  }

  operator fn::MutableAttributesRef()
  {
    return fn::MutableAttributesRef(*info_, buffers_, size_);
  }

  operator fn::AttributesRef() const
  {
    return fn::AttributesRef(*info_, buffers_, size_);
  }
};

static std::string dnode_to_path(const DNode &dnode)
{
  std::string path;
  for (const DParentNode *parent = dnode.parent(); parent; parent = parent->parent()) {
    path = parent->node_ref().name() + "/" + path;
  }
  path = path + dnode.name();
  return path;
}

static void remove_unused_states(Simulation *simulation, const VectorSet<std::string> &state_names)
{
  LISTBASE_FOREACH_MUTABLE (SimulationState *, state, &simulation->states) {
    if (!state_names.contains(state->name)) {
      BLI_remlink(&simulation->states, state);
      free_particle_simulation_state((ParticleSimulationState *)state);
    }
  }
}

static void reset_states(Simulation *simulation)
{
  LISTBASE_FOREACH (SimulationState *, state, &simulation->states) {
    switch ((eSimulationStateType)state->type) {
      case SIM_STATE_TYPE_PARTICLES: {
        ParticleSimulationState *particle_state = (ParticleSimulationState *)state;
        CustomData_free(&particle_state->attributes, particle_state->tot_particles);
        particle_state->tot_particles = 0;
        break;
      }
    }
  }
}

static SimulationState *try_find_state_by_name(Simulation *simulation, StringRef name)
{
  LISTBASE_FOREACH (SimulationState *, state, &simulation->states) {
    if (state->name == name) {
      return state;
    }
  }
  return nullptr;
}

static void add_missing_particle_states(Simulation *simulation, Span<std::string> state_names)
{
  for (StringRefNull name : state_names) {
    SimulationState *state = try_find_state_by_name(simulation, name);
    if (state != nullptr) {
      BLI_assert(state->type == SIM_STATE_TYPE_PARTICLES);
      continue;
    }

    ParticleSimulationState *particle_state = (ParticleSimulationState *)MEM_callocN(
        sizeof(*particle_state), AT);
    particle_state->head.type = SIM_STATE_TYPE_PARTICLES;
    particle_state->head.name = BLI_strdup(name.data());
    CustomData_reset(&particle_state->attributes);
    particle_state->point_cache = BKE_ptcache_add(&particle_state->ptcaches);
    BLI_addtail(&simulation->states, particle_state);
  }
}

static void reinitialize_empty_simulation_states(Simulation *simulation,
                                                 const DerivedNodeTree &tree)
{
  VectorSet<std::string> state_names;
  for (const DNode *dnode : tree.nodes_by_type("SimulationNodeParticleSimulation")) {
    state_names.add(dnode_to_path(*dnode));
  }

  remove_unused_states(simulation, state_names);
  reset_states(simulation);
  add_missing_particle_states(simulation, state_names);
}

static void update_simulation_state_list(Simulation *simulation, const DerivedNodeTree &tree)
{
  VectorSet<std::string> state_names;
  for (const DNode *dnode : tree.nodes_by_type("SimulationNodeParticleSimulation")) {
    state_names.add(dnode_to_path(*dnode));
  }

  remove_unused_states(simulation, state_names);
  add_missing_particle_states(simulation, state_names);
}

static void simulation_data_update(Depsgraph *depsgraph, Scene *scene, Simulation *simulation_cow)
{
  int current_frame = scene->r.cfra;
  if (simulation_cow->current_frame == current_frame) {
    return;
  }

  /* Below we modify the original state/cache. Only the active depsgraph is allowed to do that. */
  if (!DEG_is_active(depsgraph)) {
    return;
  }

  Simulation *simulation_orig = (Simulation *)DEG_get_original_id(&simulation_cow->id);

  NodeTreeRefMap tree_refs;
  /* TODO: Use simulation_cow, but need to add depsgraph relations before that. */
  const DerivedNodeTree tree{simulation_orig->nodetree, tree_refs};
  fn::MFNetwork network;
  ResourceCollector resources;
  MFNetworkTreeMap network_map = insert_node_tree_into_mf_network(network, tree, resources);
  AttributeNodeMap attribute_node_map = deduplicate_attribute_nodes(network, network_map, tree);
  fn::mf_network_optimization::constant_folding(network, resources);
  fn::mf_network_optimization::common_subnetwork_elimination(network);
  fn::mf_network_optimization::dead_node_removal(network);
  UNUSED_VARS(attribute_node_map);
  // WM_clipboard_text_set(network.to_dot().c_str(), false);

  if (current_frame == 1) {
    reinitialize_empty_simulation_states(simulation_orig, tree);

    RNG *rng = BLI_rng_new(0);

    simulation_orig->current_frame = 1;
    LISTBASE_FOREACH (ParticleSimulationState *, state, &simulation_orig->states) {
      state->tot_particles = 100;
      CustomData_realloc(&state->attributes, state->tot_particles);
      ensure_attributes_exist(state);

      CustomDataAttributesRef custom_data_attributes{state->attributes,
                                                     (uint)state->tot_particles};

      fn::MutableAttributesRef attributes = custom_data_attributes;
      MutableSpan<float3> positions = attributes.get<float3>("Position");
      MutableSpan<float3> velocities = attributes.get<float3>("Velocity");

      for (uint i : positions.index_range()) {
        positions[i] = {i / 10.0f, 0, 0};
        velocities[i] = {0, BLI_rng_get_float(rng), BLI_rng_get_float(rng) * 2 + 1};
      }
    }

    BLI_rng_free(rng);

    copy_states_to_cow(simulation_orig, simulation_cow);
  }
  else if (current_frame == simulation_orig->current_frame + 1) {
    update_simulation_state_list(simulation_orig, tree);
    float time_step = 1.0f / 24.0f;
    simulation_orig->current_frame = current_frame;

    LISTBASE_FOREACH (ParticleSimulationState *, state, &simulation_orig->states) {
      ensure_attributes_exist(state);

      CustomDataAttributesRef custom_data_attributes{state->attributes,
                                                     (uint)state->tot_particles};

      fn::MutableAttributesRef attributes = custom_data_attributes;
      MutableSpan<float3> positions = attributes.get<float3>("Position");
      MutableSpan<float3> velocities = attributes.get<float3>("Velocity");

      for (uint i : positions.index_range()) {
        velocities[i].z += -1.0f * time_step;
        positions[i] += velocities[i] * time_step;
      }
    }

    copy_states_to_cow(simulation_orig, simulation_cow);
  }
}

}  // namespace blender::bke

void BKE_simulation_data_update(Depsgraph *depsgraph, Scene *scene, Simulation *simulation)
{
  blender::bke::simulation_data_update(depsgraph, scene, simulation);
}
